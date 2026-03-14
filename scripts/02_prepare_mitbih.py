#!/usr/bin/env python3
"""准备 MIT-BIH CSV 数据，用于后续模型训练。"""

from __future__ import annotations

import argparse
import json
from collections import Counter
from pathlib import Path

import numpy as np
import pandas as pd
from imblearn.over_sampling import SMOTE
from sklearn.preprocessing import MinMaxScaler


def load_csv(path: Path) -> tuple[np.ndarray, np.ndarray]:
    df = pd.read_csv(path, header=None)
    x = df.iloc[:, :-1].to_numpy(dtype=np.float32)
    y = df.iloc[:, -1].to_numpy(dtype=np.int64)
    return x, y


def format_counter(counter: Counter) -> dict[str, int]:
    return {str(int(k)): int(v) for k, v in sorted(counter.items(), key=lambda t: t[0])}


def save_report(path: Path, stats: dict) -> None:
    lines = [
        "# MIT-BIH 数据准备报告",
        "",
        f"- 训练集文件：`{stats['train_file']}`",
        f"- 测试集文件：`{stats['test_file']}`",
        f"- 随机种子：`{stats['random_seed']}`",
        f"- 是否使用 SMOTE：`{stats['use_smote']}`",
        "",
        "## 训练集标签分布（处理前）",
        "",
    ]
    for k, v in stats["train_dist_before"].items():
        lines.append(f"- 标签 {k}：{v}")
    lines += ["", "## 训练集标签分布（处理后）", ""]
    for k, v in stats["train_dist_after"].items():
        lines.append(f"- 标签 {k}：{v}")
    lines += ["", "## 测试集标签分布", ""]
    for k, v in stats["test_dist"].items():
        lines.append(f"- 标签 {k}：{v}")
    lines += [
        "",
        "## 数据形状",
        "",
        f"- x_train: `{stats['x_train_shape']}`",
        f"- y_train: `{stats['y_train_shape']}`",
        f"- x_test: `{stats['x_test_shape']}`",
        f"- y_test: `{stats['y_test_shape']}`",
        "",
    ]
    path.write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description="准备 MIT-BIH 数据集")
    parser.add_argument("--dataset-dir", type=Path, default=Path("MIT-BIH"))
    parser.add_argument("--train-file", type=str, default="mitbih_train.csv")
    parser.add_argument("--test-file", type=str, default="mitbih_test.csv")
    parser.add_argument("--output-dir", type=Path, default=Path("processed"))
    parser.add_argument("--use-smote", action="store_true", default=False)
    parser.add_argument("--random-seed", type=int, default=42)
    args = parser.parse_args()

    train_path = args.dataset_dir / args.train_file
    test_path = args.dataset_dir / args.test_file
    if not train_path.exists() or not test_path.exists():
        missing = [str(p) for p in [train_path, test_path] if not p.exists()]
        raise FileNotFoundError(f"缺少文件：{missing}")

    x_train, y_train = load_csv(train_path)
    x_test, y_test = load_csv(test_path)

    scaler = MinMaxScaler()
    x_train = scaler.fit_transform(x_train).astype(np.float32)
    x_test = scaler.transform(x_test).astype(np.float32)

    train_dist_before = Counter(y_train.tolist())
    if args.use_smote:
        sampler = SMOTE(random_state=args.random_seed)
        x_train, y_train = sampler.fit_resample(x_train, y_train)
    train_dist_after = Counter(y_train.tolist())
    test_dist = Counter(y_test.tolist())

    args.output_dir.mkdir(parents=True, exist_ok=True)
    data_out = args.output_dir / "mitbih_prepared.npz"
    np.savez_compressed(
        data_out,
        x_train=x_train.astype(np.float32),
        y_train=y_train.astype(np.int64),
        x_test=x_test.astype(np.float32),
        y_test=y_test.astype(np.int64),
    )

    stats = {
        "train_file": str(train_path),
        "test_file": str(test_path),
        "random_seed": args.random_seed,
        "use_smote": args.use_smote,
        "train_dist_before": format_counter(train_dist_before),
        "train_dist_after": format_counter(train_dist_after),
        "test_dist": format_counter(test_dist),
        "x_train_shape": list(map(int, x_train.shape)),
        "y_train_shape": list(map(int, y_train.shape)),
        "x_test_shape": list(map(int, x_test.shape)),
        "y_test_shape": list(map(int, y_test.shape)),
    }

    (args.output_dir / "prepare_stats.json").write_text(
        json.dumps(stats, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )
    save_report(args.output_dir / "prepare_report.md", stats)
    np.save(args.output_dir / "scaler_min.npy", scaler.data_min_.astype(np.float32))
    np.save(args.output_dir / "scaler_max.npy", scaler.data_max_.astype(np.float32))

    print(f"数据准备完成，已保存：{data_out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
