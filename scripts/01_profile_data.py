#!/usr/bin/env python3
"""统计 MIT-BIH/PTBDB CSV 数据集并导出 Markdown 报告。"""

from __future__ import annotations

import argparse
from datetime import datetime
from pathlib import Path

import numpy as np
import pandas as pd

DEFAULT_FILES = [
    "mitbih_train.csv",
    "mitbih_test.csv",
    "ptbdb_normal.csv",
    "ptbdb_abnormal.csv",
]


def fmt_label(value: float) -> str:
    if pd.isna(value):
        return "NaN"
    ivalue = int(value)
    if abs(value - ivalue) < 1e-9:
        return str(ivalue)
    return f"{value:.4f}"


def profile_csv(path: Path) -> dict:
    df = pd.read_csv(path, header=None)
    feature_df = df.iloc[:, :-1]
    label_series = df.iloc[:, -1]

    total = len(label_series)
    label_counts = label_series.value_counts().sort_index()
    label_stats = []
    for label, count in label_counts.items():
        ratio = (float(count) / total) * 100 if total else 0.0
        label_stats.append(
            {
                "label": fmt_label(float(label)),
                "count": int(count),
                "ratio": ratio,
            }
        )

    arr = feature_df.to_numpy(dtype=np.float64)
    zero_ratio = float((arr == 0).sum() / arr.size) if arr.size else 0.0

    return {
        "file": path.name,
        "rows": int(df.shape[0]),
        "cols": int(df.shape[1]),
        "feature_cols": int(feature_df.shape[1]),
        "label_col": int(df.shape[1] - 1),
        "missing_total": int(df.isna().sum().sum()),
        "feature_min": float(np.min(arr)) if arr.size else float("nan"),
        "feature_max": float(np.max(arr)) if arr.size else float("nan"),
        "feature_mean": float(np.mean(arr)) if arr.size else float("nan"),
        "feature_std": float(np.std(arr)) if arr.size else float("nan"),
        "feature_zero_ratio": zero_ratio,
        "label_stats": label_stats,
    }


def render_markdown(dataset_dir: Path, profiles: list[dict]) -> str:
    lines = []
    lines.append("# 数据探查报告")
    lines.append("")
    lines.append(f"生成时间：{datetime.now().isoformat(timespec='seconds')}")
    lines.append(f"数据目录：`{dataset_dir}`")
    lines.append("")
    lines.append("## 文件概览")
    lines.append("")
    lines.append("| 文件 | 行数 | 列数 | 特征列数 | 标签列索引 | 缺失值总数 | 最小值 | 最大值 | 均值 | 标准差 | 零值占比 |")
    lines.append("| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |")

    for p in profiles:
        lines.append(
            "| {file} | {rows} | {cols} | {feature_cols} | {label_col} | {missing_total} | "
            "{feature_min:.6f} | {feature_max:.6f} | {feature_mean:.6f} | {feature_std:.6f} | "
            "{feature_zero_ratio:.6f} |".format(**p)
        )

    for p in profiles:
        lines.append("")
        lines.append(f"## 标签分布 - {p['file']}")
        lines.append("")
        lines.append("| 标签 | 数量 | 占比（%） |")
        lines.append("| --- | ---: | ---: |")
        for item in p["label_stats"]:
            lines.append(f"| {item['label']} | {item['count']} | {item['ratio']:.4f} |")

    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description="统计 MIT-BIH/PTBDB CSV 文件")
    parser.add_argument(
        "--dataset-dir",
        type=Path,
        default=Path("MIT-BIH"),
        help="包含数据集 CSV 文件的目录",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("results/data_profile.md"),
        help="输出 Markdown 报告路径",
    )
    args = parser.parse_args()

    dataset_dir = args.dataset_dir
    missing_files = [name for name in DEFAULT_FILES if not (dataset_dir / name).exists()]
    if missing_files:
        names = ", ".join(missing_files)
        raise FileNotFoundError(f"缺少数据文件：{names}")

    profiles = [profile_csv(dataset_dir / name) for name in DEFAULT_FILES]

    args.output.parent.mkdir(parents=True, exist_ok=True)
    report = render_markdown(dataset_dir.resolve(), profiles)
    args.output.write_text(report, encoding="utf-8")

    print(f"报告已写入：{args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
