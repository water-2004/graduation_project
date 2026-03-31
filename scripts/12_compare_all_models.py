#!/usr/bin/env python3
"""全量模型对比脚本：支持任意数量模型，生成五章节对比报告。"""

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

    # 推断输入形状描述
    input_shape = "未知"
    model_type = raw.get("model_type", display_name)
    if "STFT" in model_type or "2D" in model_type or "Paper" in model_type:
        nperseg = raw.get("nperseg", 32)
        noverlap = raw.get("noverlap", 16)
        input_shape = f"[1,1,17,11] (STFT nperseg={nperseg}, noverlap={noverlap})"
    elif "1D" in model_type or "CNN" in model_type:
        input_shape = "[1,1,187] (原始心拍)"
    elif "Forest" in model_type:
        input_shape = "[187] (展平特征向量)"
    elif "XGB" in model_type:
        input_shape = "[187] (展平特征向量)"

    return {
        "display_name": display_name,
        "source_file": str(path),
        "model_type": model_type,
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
        "input_shape": input_shape,
    }


def build_report(
    models: list[dict[str, Any]],
    baseline_name: str | None,
    optimized_name: str | None,
) -> str:
    """生成五章节中文 Markdown 对比报告。"""
    ranked = sorted(
        models,
        key=lambda m: (m["macro_f1"], m["accuracy"], m["macro_recall"]),
        reverse=True,
    )
    best = ranked[0]
    classes = ranked[0]["classes"]

    lines: list[str] = []

    # ── 第1章：核心指标排名表 ──
    lines += [
        "# 心电异常识别全量模型对比报告",
        "",
        "## 1. 核心指标排名表",
        "",
        "按 Macro-F1 降序排列：",
        "",
        "| 排名 | 模型 | Accuracy | Macro-F1 | Macro-Recall |",
        "| ---: | --- | ---: | ---: | ---: |",
    ]
    for rank, m in enumerate(ranked, 1):
        lines.append(
            f"| {rank} | {m['display_name']} | "
            f"{m['accuracy']:.6f} | "
            f"{m['macro_f1']:.6f} | "
            f"{m['macro_recall']:.6f} |"
        )

    # ── 第2章：各类别召回率矩阵 ──
    class_headers = " | ".join(f"类别{c}" for c in classes)
    lines += [
        "",
        "## 2. 各类别召回率矩阵",
        "",
        f"| 模型 | {class_headers} |",
        "| --- |" + " ---: |" * len(classes),
    ]
    for m in ranked:
        recall_map = {cls: rec for cls, rec in zip(m["classes"], m["per_class_recall"])}
        cells = " | ".join(f"{recall_map.get(c, 0.0):.6f}" for c in classes)
        lines.append(f"| {m['display_name']} | {cells} |")

    # ── 第3章：部署就绪度评估 ──
    lines += [
        "",
        "## 3. 部署就绪度评估",
        "",
        "| 模型 | ONNX 可用 | 输入形状 | 边缘端兼容 |",
        "| --- | --- | --- | --- |",
    ]
    for m in ranked:
        onnx_status = "是" if m["onnx_ready"] else "否"
        edge_compat = "是" if m["onnx_ready"] else "否"
        lines.append(f"| {m['display_name']} | {onnx_status} | {m['input_shape']} | {edge_compat} |")

    if not any(m["onnx_ready"] for m in ranked):
        lines += ["", "当前所有模型均未导出 ONNX，暂时无法直接部署到边缘端。"]

    # ── 第4章：模型选型分析 ──
    deployable = [m for m in ranked if m["onnx_ready"]]
    best_deployable = deployable[0] if deployable else None

    lines += [
        "",
        "## 4. 模型选型分析",
        "",
        "### 4.1 综合最优",
        "",
        f"- 综合指标最优的模型是 **{best['display_name']}**。",
        f"- Macro-F1：`{best['macro_f1']:.6f}`，Accuracy：`{best['accuracy']:.6f}`，"
        f"Macro-Recall：`{best['macro_recall']:.6f}`。",
    ]
    if best["is_quick_experiment"]:
        lines.append("- 该结果来自快速实验（部分训练集），暂不作为最终定稿结论。")
    else:
        lines.append("- 该结果来自全量训练，可作为论文主要参考。")

    lines += [
        "",
        "### 4.2 部署推荐",
        "",
    ]
    if best_deployable:
        lines += [
            f"- 推荐部署模型：**{best_deployable['display_name']}**。",
            f"- 该模型已导出 ONNX，可直接用于 `edge_infer_service` 推理。",
            f"- Macro-F1：`{best_deployable['macro_f1']:.6f}`。",
        ]
    else:
        lines.append("- 当前无模型已导出 ONNX，需补充导出流程。")

    # 找论文主线模型
    paper_models = [m for m in ranked if "论文" in m["display_name"] or "Paper" in m["display_name"] or "paper" in m["model_type"]]
    lines += [
        "",
        "### 4.3 论文主线",
        "",
    ]
    if paper_models:
        paper_best = paper_models[0]
        lines += [
            f"- 论文主线模型中综合最优的是 **{paper_best['display_name']}**。",
            f"- Macro-F1：`{paper_best['macro_f1']:.6f}`。",
        ]
        if len(paper_models) > 1:
            lines.append("- 论文主线涉及的模型变体：" + "、".join(f"`{m['display_name']}`" for m in paper_models) + "。")
    else:
        lines.append("- 未检测到论文主线相关模型。")

    # ── 第5章：优化改进分析 ──
    baseline_model = None
    optimized_model = None
    if baseline_name and optimized_name:
        for m in models:
            if m["display_name"] == baseline_name:
                baseline_model = m
            if m["display_name"] == optimized_name:
                optimized_model = m

    if baseline_model and optimized_model:
        lines += [
            "",
            "## 5. 优化改进分析",
            "",
            f"基线模型：**{baseline_model['display_name']}**",
            f"优化模型：**{optimized_model['display_name']}**",
            "",
            "### 5.1 核心指标对比",
            "",
            "| 指标 | 基线 | 优化后 | 变化 |",
            "| --- | ---: | ---: | ---: |",
        ]
        for metric_name, key in [
            ("Accuracy", "accuracy"),
            ("Macro-F1", "macro_f1"),
            ("Macro-Recall", "macro_recall"),
        ]:
            base_val = baseline_model[key]
            opt_val = optimized_model[key]
            delta = opt_val - base_val
            sign = "+" if delta >= 0 else ""
            lines.append(
                f"| {metric_name} | {base_val:.6f} | {opt_val:.6f} | {sign}{delta:.6f} |"
            )

        lines += [
            "",
            "### 5.2 各类别召回率对比",
            "",
            f"| 类别 | 基线 | 优化后 | 变化 |",
            "| --- | ---: | ---: | ---: |",
        ]
        base_recall_map = dict(zip(baseline_model["classes"], baseline_model["per_class_recall"]))
        opt_recall_map = dict(zip(optimized_model["classes"], optimized_model["per_class_recall"]))
        for cls in classes:
            b = base_recall_map.get(cls, 0.0)
            o = opt_recall_map.get(cls, 0.0)
            d = o - b
            sign = "+" if d >= 0 else ""
            lines.append(f"| 类别{cls} | {b:.6f} | {o:.6f} | {sign}{d:.6f} |")

        f1_delta = optimized_model["macro_f1"] - baseline_model["macro_f1"]
        if f1_delta > 0:
            lines += [
                "",
                f"优化后 Macro-F1 提升了 **{f1_delta:.4f}**，优化措施有效。",
            ]
        elif f1_delta == 0:
            lines += [
                "",
                "优化后 Macro-F1 与基线持平，优化措施效果有限。",
            ]
        else:
            lines += [
                "",
                f"优化后 Macro-F1 下降了 **{abs(f1_delta):.4f}**，需进一步调参。",
            ]

    lines.append("")
    return "\n".join(lines)


def parse_metric_arg(value: str) -> tuple[str, Path]:
    """解析 'name:path' 格式的参数。"""
    if ":" not in value:
        raise argparse.ArgumentTypeError(f"格式错误：'{value}'，应为 '模型名:metrics.json路径'")
    name, path_str = value.split(":", 1)
    path = Path(path_str)
    if not path.exists():
        raise argparse.ArgumentTypeError(f"文件不存在：{path}")
    return name.strip(), path


def main() -> int:
    parser = argparse.ArgumentParser(
        description="全量模型对比脚本",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""示例:
  python scripts/12_compare_all_models.py \\
    --metrics "1D-CNN:artifacts/cnn_mitbih_v2/metrics.json" \\
              "Random Forest:artifacts/random_forest_mitbih_full/metrics.json" \\
              "XGBoost:artifacts/xgboost_mitbih_full/metrics.json" \\
              "论文风格CNN:artifacts/paper_style_cnn_full_v1/metrics.json" \\
              "参考论文CNN:artifacts/paper_reference_cnn_full_v1/metrics.json" \\
              "优化后论文CNN:artifacts/paper_optimized_cnn/metrics.json" \\
    --baseline-name "参考论文CNN" \\
    --optimized-name "优化后论文CNN"
""",
    )
    parser.add_argument(
        "--metrics",
        nargs="+",
        required=True,
        help="模型指标文件列表，格式为 '模型名:metrics.json路径'",
    )
    parser.add_argument(
        "--baseline-name",
        type=str,
        default=None,
        help="基线模型名称（用于第5章优化改进分析）",
    )
    parser.add_argument(
        "--optimized-name",
        type=str,
        default=None,
        help="优化模型名称（用于第5章优化改进分析）",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("artifacts/model_compare_all"),
    )
    args = parser.parse_args()

    models: list[dict[str, Any]] = []
    for item in args.metrics:
        name, path = parse_metric_arg(item)
        models.append(load_metrics(path, name))

    if len(models) < 2:
        print("错误：至少需要 2 个模型进行对比。", flush=True)
        return 1

    ranked = sorted(
        models,
        key=lambda m: (m["macro_f1"], m["accuracy"], m["macro_recall"]),
        reverse=True,
    )

    args.output_dir.mkdir(parents=True, exist_ok=True)
    (args.output_dir / "comparison.json").write_text(
        json.dumps(ranked, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )
    report = build_report(models, args.baseline_name, args.optimized_name)
    (args.output_dir / "模型对比报告.md").write_text(report, encoding="utf-8")

    print("模型排名（按 Macro-F1、Accuracy、Macro-Recall 排序）：", flush=True)
    for index, item in enumerate(ranked, 1):
        print(
            f"  {index}. {item['display_name']} | "
            f"Accuracy={item['accuracy']:.4f} | "
            f"Macro-F1={item['macro_f1']:.4f} | "
            f"Macro-Recall={item['macro_recall']:.4f}",
            flush=True,
        )
    print(f"对比报告已保存到：{args.output_dir / '模型对比报告.md'}", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
