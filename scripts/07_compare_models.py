#!/usr/bin/env python3
"""汇总多个模型结果并生成中文对比报告。"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any


def load_metrics(path: Path, display_name: str) -> dict[str, Any]:
    """读取单个模型的指标文件并转换为统一结构。"""
    raw = json.loads(path.read_text(encoding="utf-8"))
    onnx_error = str(raw.get("onnx_export_error", "")).strip()
    train_fraction = float(raw.get("train_fraction", 1.0))

    return {
        "display_name": display_name,
        "source_file": str(path),
        "model_type": raw.get("model_type", display_name),
        "accuracy": float(raw["test_accuracy"]),
        "macro_f1": float(raw["test_macro_f1"]),
        "macro_recall": float(raw["test_macro_recall"]),
        "best_val_macro_f1": float(raw.get("best_val_macro_f1", 0.0)),
        "classes": raw.get("classes", []),
        "per_class_recall": raw.get("test_per_class_recall", []),
        "train_fraction": train_fraction,
        "is_quick_experiment": train_fraction < 0.999,
        "onnx_ready": onnx_error == "",
        "onnx_export_error": onnx_error,
    }


def build_report(models: list[dict[str, Any]]) -> str:
    """生成中文 Markdown 对比报告。"""
    ranked_models = sorted(
        models,
        key=lambda item: (item["macro_f1"], item["accuracy"], item["macro_recall"]),
        reverse=True,
    )
    best_model = ranked_models[0]
    classes = ranked_models[0]["classes"]

    lines = [
        "# 心电异常识别模型对比报告",
        "",
        "## 对比说明",
        "",
        "- 数据集：MIT-BIH Arrhythmia Database 处理结果 `processed/mitbih_prepared.npz`。",
        "- 对比目标：在统一数据划分口径下，比较不同模型在心拍五分类任务中的效果。",
        "- 评价重点：优先关注 `Macro-F1`、`Macro-Recall` 与异常类召回率，而不是只看整体准确率。",
        "",
        "## 核心指标对比",
        "",
        "| 模型 | Accuracy | Macro-F1 | Macro-Recall | 训练集抽样比例 | 是否快速实验 | 是否已接入 ONNX 部署 |",
        "| --- | ---: | ---: | ---: | ---: | --- | --- |",
    ]

    for item in ranked_models:
        lines.append(
            f"| {item['display_name']} | "
            f"{item['accuracy']:.6f} | "
            f"{item['macro_f1']:.6f} | "
            f"{item['macro_recall']:.6f} | "
            f"{item['train_fraction']:.2f} | "
            f"{'是' if item['is_quick_experiment'] else '否'} | "
            f"{'是' if item['onnx_ready'] else '否'} |"
        )

    lines += [
        "",
        "## 各类别召回率对比",
        "",
        "| 模型 | 类别0 | 类别1 | 类别2 | 类别3 | 类别4 |",
        "| --- | ---: | ---: | ---: | ---: | ---: |",
    ]

    for item in ranked_models:
        recall_cells = []
        recall_map = {cls: rec for cls, rec in zip(item["classes"], item["per_class_recall"])}
        for cls in classes:
            recall_cells.append(f"{recall_map.get(cls, 0.0):.6f}")
        lines.append(f"| {item['display_name']} | " + " | ".join(recall_cells) + " |")

    lines += [
        "",
        "## 当前结论",
        "",
        f"- 按当前实验结果，综合指标最优的模型是 `{best_model['display_name']}`。",
        f"- 该模型当前的 Macro-F1 为 `{best_model['macro_f1']:.6f}`，Accuracy 为 `{best_model['accuracy']:.6f}`。",
    ]

    if best_model["is_quick_experiment"]:
        lines.append("- 但该结果来自快速实验，只使用了部分训练集，暂时不能直接作为最终定稿结论。")
    else:
        lines.append("- 该结果来自全量训练，可作为论文与系统实现中的主要候选模型。")

    deployable_models = [item["display_name"] for item in ranked_models if item["onnx_ready"]]
    if deployable_models:
        lines.append(
            f"- 当前已直接接入本项目 ONNX 推理链路的模型有：`{', '.join(deployable_models)}`。"
        )
    else:
        lines.append("- 当前对比模型还没有直接接入本项目 ONNX 推理链路，需要后续补部署导出。")

    lines += [
        "",
        "## 建议汇报说法",
        "",
        "1. 现阶段已经完成系统闭环，包括数据预处理、模型训练、边缘端推理服务和 Qt 可视化客户端。",
        "2. 当前不再只使用单一模型，而是在统一数据集上对 1D-CNN、随机森林、XGBoost 进行横向对比。",
        "3. 评价指标不只看准确率，还重点关注 Macro-F1 和异常类别召回率，以避免模型偏向正常类。",
        "4. 若快速实验中某个传统模型表现更好，下一步应先做全量复现，再决定是否替换当前部署模型。",
        "",
    ]
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description="对比多个心电分类模型的结果")
    parser.add_argument("--cnn-metrics", type=Path, required=True)
    parser.add_argument("--rf-metrics", type=Path, required=True)
    parser.add_argument("--xgb-metrics", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, default=Path("artifacts/model_compare"))
    args = parser.parse_args()

    models = [
        load_metrics(args.cnn_metrics, "1D-CNN"),
        load_metrics(args.rf_metrics, "Random Forest"),
        load_metrics(args.xgb_metrics, "XGBoost"),
    ]
    ranked_models = sorted(
        models,
        key=lambda item: (item["macro_f1"], item["accuracy"], item["macro_recall"]),
        reverse=True,
    )

    args.output_dir.mkdir(parents=True, exist_ok=True)
    (args.output_dir / "comparison.json").write_text(
        json.dumps(ranked_models, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )
    (args.output_dir / "模型对比报告.md").write_text(
        build_report(models),
        encoding="utf-8",
    )

    print("模型排名（按 Macro-F1、Accuracy、Macro-Recall 排序）：", flush=True)
    for index, item in enumerate(ranked_models, start=1):
        print(
            f"{index}. {item['display_name']} | "
            f"Accuracy={item['accuracy']:.4f} | "
            f"Macro-F1={item['macro_f1']:.4f} | "
            f"Macro-Recall={item['macro_recall']:.4f}",
            flush=True,
        )
    print(f"对比报告已保存到：{args.output_dir / '模型对比报告.md'}", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
