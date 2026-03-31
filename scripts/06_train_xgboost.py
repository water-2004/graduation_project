#!/usr/bin/env python3
"""在处理后的 MIT-BIH 数据上训练 XGBoost 基线模型。"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any

import numpy as np
from sklearn.metrics import (
    accuracy_score,
    classification_report,
    confusion_matrix,
    f1_score,
    log_loss,
    recall_score,
)
from sklearn.model_selection import train_test_split
from xgboost import XGBClassifier


def evaluate(model: XGBClassifier, x: np.ndarray, y: np.ndarray) -> dict[str, Any]:
    """计算分类模型在给定数据上的统一评估指标。"""
    y_pred = model.predict(x)
    y_prob = model.predict_proba(x)
    labels = np.asarray(model.classes_, dtype=np.int64)

    return {
        "loss": float(log_loss(y, y_prob, labels=labels)),
        "accuracy": float(accuracy_score(y, y_pred)),
        "macro_f1": float(f1_score(y, y_pred, average="macro")),
        "macro_recall": float(recall_score(y, y_pred, average="macro")),
        "per_class_recall": recall_score(y, y_pred, average=None, labels=labels).tolist(),
        "confusion_matrix": confusion_matrix(y, y_pred, labels=labels).tolist(),
        "classification_report": classification_report(y, y_pred, digits=4),
    }


def build_report(metrics: dict[str, Any]) -> str:
    """生成中文 Markdown 训练报告，便于直接写入文档。"""
    lines = [
        "# XGBoost 训练结果报告",
        "",
        "## 训练配置",
        "",
        f"- 训练集文件：`{metrics['prepared_file']}`",
        f"- 随机种子：`{metrics['random_seed']}`",
        f"- 验证集比例：`{metrics['val_ratio']}`",
        f"- 训练集抽样比例：`{metrics['train_fraction']}`",
        f"- 并行线程数：`{metrics['n_jobs']}`",
        f"- 候选参数组数：`{metrics['candidate_count']}`",
        f"- 最优参数：`{json.dumps(metrics['best_params'], ensure_ascii=False)}`",
        "",
        "## 测试集核心指标",
        "",
        f"- 测试损失：`{metrics['test_loss']:.6f}`",
        f"- 准确率（Accuracy）：`{metrics['test_accuracy']:.6f}`",
        f"- 宏平均 F1（Macro-F1）：`{metrics['test_macro_f1']:.6f}`",
        f"- 宏平均召回率（Macro-Recall）：`{metrics['test_macro_recall']:.6f}`",
        f"- 最佳验证 Macro-F1：`{metrics['best_val_macro_f1']:.6f}`",
        "",
        "## 各类别召回率",
        "",
        "| 类别 | 召回率 |",
        "| --- | ---: |",
    ]
    for cls, rec in zip(metrics["classes"], metrics["test_per_class_recall"]):
        lines.append(f"| {cls} | {rec:.6f} |")

    lines += [
        "",
        "## 说明",
        "",
        "- 本模型作为梯度提升树基线，用于与 1D-CNN、随机森林等方法做横向对比。",
        "- 当训练集抽样比例小于 1.0 时，应将结果视为快速实验结果，而不是最终定稿结果。",
        "- 当前脚本仅负责训练与评估，不负责导出 ONNX 部署模型。",
        "",
    ]
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description="在处理后的 MIT-BIH 数据上训练 XGBoost")
    parser.add_argument("--prepared-file", type=Path, default=Path("processed/mitbih_prepared.npz"))
    parser.add_argument("--output-dir", type=Path, default=Path("artifacts/xgboost_mitbih"))
    parser.add_argument("--val-ratio", type=float, default=0.1)
    parser.add_argument("--random-seed", type=int, default=42)
    parser.add_argument("--train-fraction", type=float, default=1.0)
    parser.add_argument("--n-jobs", type=int, default=1)
    args = parser.parse_args()

    if not args.prepared_file.exists():
        raise FileNotFoundError(f"未找到处理后的数据文件：{args.prepared_file}")
    if not (0 < args.train_fraction <= 1.0):
        raise ValueError("--train-fraction 必须在 (0, 1] 范围内")

    data = np.load(args.prepared_file)
    x_train = data["x_train"].astype(np.float32)
    y_train = data["y_train"].astype(np.int64)
    x_test = data["x_test"].astype(np.float32)
    y_test = data["y_test"].astype(np.int64)

    train_x, val_x, train_y, val_y = train_test_split(
        x_train,
        y_train,
        test_size=args.val_ratio,
        random_state=args.random_seed,
        stratify=y_train,
    )

    if args.train_fraction < 1.0:
        train_x, _, train_y, _ = train_test_split(
            train_x,
            train_y,
            train_size=args.train_fraction,
            random_state=args.random_seed,
            stratify=train_y,
        )

    classes = sorted(np.unique(y_train).tolist())
    num_class = len(classes)

    # 参数数量先控制在 3 组，保证能较快得到可比较的结果。
    candidates = [
        {
            "n_estimators": 120,
            "max_depth": 6,
            "learning_rate": 0.08,
            "subsample": 0.9,
            "colsample_bytree": 0.9,
        },
        {
            "n_estimators": 180,
            "max_depth": 8,
            "learning_rate": 0.08,
            "subsample": 0.9,
            "colsample_bytree": 0.9,
        },
        {
            "n_estimators": 240,
            "max_depth": 8,
            "learning_rate": 0.05,
            "subsample": 0.95,
            "colsample_bytree": 0.9,
        },
    ]

    best_model: XGBClassifier | None = None
    best_params: dict[str, Any] | None = None
    best_val_metrics: dict[str, Any] | None = None
    best_val_macro_f1 = -1.0

    for index, params in enumerate(candidates, start=1):
        model = XGBClassifier(
            objective="multi:softprob",
            num_class=num_class,
            eval_metric="mlogloss",
            tree_method="hist",
            random_state=args.random_seed,
            n_jobs=args.n_jobs,
            reg_lambda=1.0,
            **params,
        )
        model.fit(train_x, train_y, verbose=False)
        val_metrics = evaluate(model, val_x, val_y)

        print(
            f"参数组 {index}/{len(candidates)}: {params} | "
            f"验证 Accuracy={val_metrics['accuracy']:.4f} | "
            f"验证 Macro-F1={val_metrics['macro_f1']:.4f}",
            flush=True,
        )

        if val_metrics["macro_f1"] > best_val_macro_f1:
            best_val_macro_f1 = val_metrics["macro_f1"]
            best_model = model
            best_params = params
            best_val_metrics = val_metrics

    if best_model is None or best_params is None or best_val_metrics is None:
        raise RuntimeError("XGBoost 训练失败：未获得有效模型")

    test_metrics = evaluate(best_model, x_test, y_test)

    args.output_dir.mkdir(parents=True, exist_ok=True)
    best_model.save_model(args.output_dir / "model_best.json")
    np.savetxt(
        args.output_dir / "confusion_matrix.csv",
        np.asarray(test_metrics["confusion_matrix"], dtype=np.int64),
        fmt="%d",
        delimiter=",",
    )
    (args.output_dir / "classification_report.txt").write_text(
        test_metrics["classification_report"],
        encoding="utf-8",
    )

    metrics = {
        "model_type": "XGBClassifier",
        "prepared_file": str(args.prepared_file),
        "random_seed": args.random_seed,
        "val_ratio": args.val_ratio,
        "train_fraction": args.train_fraction,
        "n_jobs": args.n_jobs,
        "candidate_count": len(candidates),
        "best_params": best_params,
        "best_val_macro_f1": best_val_macro_f1,
        "best_val_accuracy": best_val_metrics["accuracy"],
        "test_loss": test_metrics["loss"],
        "test_accuracy": test_metrics["accuracy"],
        "test_macro_f1": test_metrics["macro_f1"],
        "test_macro_recall": test_metrics["macro_recall"],
        "test_per_class_recall": test_metrics["per_class_recall"],
        "confusion_matrix": test_metrics["confusion_matrix"],
        "classes": classes,
        "onnx_export_error": "XGBoost 当前未接入本项目的 ONNX 导出流程，脚本未导出 ONNX。",
    }
    (args.output_dir / "metrics.json").write_text(
        json.dumps(metrics, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )
    (args.output_dir / "训练报告.md").write_text(build_report(metrics), encoding="utf-8")

    print(f"最优参数：{best_params}", flush=True)
    print(f"测试集 Accuracy：{metrics['test_accuracy']:.4f}", flush=True)
    print(f"测试集 Macro-F1：{metrics['test_macro_f1']:.4f}", flush=True)
    print(f"模型已保存到：{args.output_dir / 'model_best.json'}", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
