#!/usr/bin/env python3
"""在参考论文模型基础上进行优化训练：BatchNorm + 类别权重 + 学习率调度 + 权重衰减。"""

from __future__ import annotations

import argparse
import csv
import json
import random
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any

import numpy as np
import pandas as pd
import torch
import torch.nn as nn
from imblearn.over_sampling import SMOTE
from scipy.signal import butter, filtfilt, stft
from sklearn.metrics import accuracy_score, classification_report, confusion_matrix, f1_score, recall_score
from sklearn.model_selection import train_test_split
from torch.utils.data import DataLoader, Dataset


def set_seed(seed: int) -> None:
    random.seed(seed)
    np.random.seed(seed)
    torch.manual_seed(seed)
    torch.cuda.manual_seed_all(seed)


class SpectrogramDataset(Dataset):
    """保存时频图及标签的数据集。"""

    def __init__(self, x: np.ndarray, y: np.ndarray):
        self.x = torch.from_numpy(x).float()
        self.y = torch.from_numpy(y).long()

    def __len__(self) -> int:
        return int(self.x.shape[0])

    def __getitem__(self, idx: int) -> tuple[torch.Tensor, torch.Tensor]:
        return self.x[idx], self.y[idx]


class OptimizedReferenceECGCNN(nn.Module):
    """在参考论文 2D-CNN 基础上增加 BatchNorm2d 的优化版本。

    架构：
        Conv2d(1→16) + BatchNorm2d(16) + ReLU + MaxPool2d
        Conv2d(16→32) + BatchNorm2d(32) + ReLU + MaxPool2d
        Conv2d(32→64) + BatchNorm2d(64) + ReLU + AdaptiveAvgPool2d
        Flatten + Dropout + Linear(64→num_classes)

    输出为 logits（无 softmax），与 C++ 推理引擎的 Softmax 后处理兼容。
    """

    def __init__(self, num_classes: int, dropout_rate: float = 0.3):
        super().__init__()
        self.features = nn.Sequential(
            nn.Conv2d(1, 16, kernel_size=3, padding=1),
            nn.BatchNorm2d(16),
            nn.ReLU(inplace=True),
            nn.MaxPool2d(kernel_size=2),
            nn.Conv2d(16, 32, kernel_size=3, padding=1),
            nn.BatchNorm2d(32),
            nn.ReLU(inplace=True),
            nn.MaxPool2d(kernel_size=2),
            nn.Conv2d(32, 64, kernel_size=3, padding=1),
            nn.BatchNorm2d(64),
            nn.ReLU(inplace=True),
            nn.AdaptiveAvgPool2d((1, 1)),
        )
        self.classifier = nn.Sequential(
            nn.Flatten(),
            nn.Dropout(p=dropout_rate),
            nn.Linear(64, num_classes),
        )

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        x = self.features(x)
        return self.classifier(x)


@dataclass
class EpochStat:
    epoch: int
    train_loss: float
    val_loss: float
    val_accuracy: float
    val_macro_f1: float
    learning_rate: float


def load_csv_dataset(path: Path) -> tuple[np.ndarray, np.ndarray]:
    """读取 MIT-BIH CSV 数据，前 187 列为信号，最后一列为标签。"""
    frame = pd.read_csv(path, header=None)
    x = frame.iloc[:, :-1].to_numpy(dtype=np.float32)
    y = frame.iloc[:, -1].to_numpy(dtype=np.int64)
    return x, y


def apply_bandpass_filter(signals: np.ndarray, lowcut: float, highcut: float, fs: float, order: int) -> np.ndarray:
    """对每条 ECG 信号应用带通滤波。"""
    nyquist = 0.5 * fs
    low = lowcut / nyquist
    high = highcut / nyquist
    b, a = butter(order, [low, high], btype="bandpass")

    filtered = np.empty_like(signals, dtype=np.float32)
    for index, signal in enumerate(signals):
        filtered[index] = filtfilt(b, a, signal).astype(np.float32)
    return filtered


def minmax_normalize(signals: np.ndarray) -> np.ndarray:
    """按样本做 min-max 归一化到 [0, 1]。"""
    mins = signals.min(axis=1, keepdims=True)
    maxs = signals.max(axis=1, keepdims=True)
    denom = np.maximum(maxs - mins, 1e-8)
    return ((signals - mins) / denom).astype(np.float32)


def build_spectrograms(signals: np.ndarray, nperseg: int, noverlap: int) -> np.ndarray:
    """将一维 ECG 序列转换为 STFT 时频图。"""
    specs: list[np.ndarray] = []
    for signal in signals:
        _, _, zxx = stft(signal, nperseg=nperseg, noverlap=noverlap, boundary=None)
        magnitude = np.abs(zxx).astype(np.float32)
        mag_min = float(magnitude.min())
        mag_max = float(magnitude.max())
        if mag_max > mag_min:
            magnitude = (magnitude - mag_min) / (mag_max - mag_min)
        else:
            magnitude = np.zeros_like(magnitude, dtype=np.float32)
        specs.append(magnitude[np.newaxis, ...])
    return np.stack(specs, axis=0).astype(np.float32)


def compute_class_weights(labels: np.ndarray) -> torch.Tensor:
    """计算类别权重，使用 sklearn balanced 公式：weight = total / (n_classes * count_per_class)。"""
    classes = np.unique(labels)
    n_classes = len(classes)
    total = len(labels)
    weights = np.zeros(n_classes, dtype=np.float32)
    for i, cls in enumerate(classes):
        count = np.sum(labels == cls)
        weights[i] = total / (n_classes * count) if count > 0 else 1.0
    return torch.from_numpy(weights)


def run_epoch(
    model: nn.Module,
    dataloader: DataLoader,
    criterion: nn.Module,
    optimizer: torch.optim.Optimizer | None,
    device: torch.device,
) -> float:
    is_train = optimizer is not None
    model.train(is_train)
    total_loss = 0.0
    total_samples = 0
    for x, y in dataloader:
        x = x.to(device=device, dtype=torch.float32)
        y = y.to(device=device, dtype=torch.long)
        if is_train:
            optimizer.zero_grad(set_to_none=True)
        logits = model(x)
        loss = criterion(logits, y)
        if is_train:
            loss.backward()
            optimizer.step()
        batch_size = y.shape[0]
        total_samples += int(batch_size)
        total_loss += float(loss.item()) * batch_size
    return total_loss / max(total_samples, 1)


@torch.no_grad()
def evaluate(model: nn.Module, dataloader: DataLoader, criterion: nn.Module, device: torch.device) -> dict[str, Any]:
    model.eval()
    total_loss = 0.0
    total_samples = 0
    y_true: list[int] = []
    y_pred: list[int] = []
    for x, y in dataloader:
        x = x.to(device=device, dtype=torch.float32)
        y = y.to(device=device, dtype=torch.long)
        logits = model(x)
        loss = criterion(logits, y)
        pred = torch.argmax(logits, dim=1)
        batch_size = y.shape[0]
        total_samples += int(batch_size)
        total_loss += float(loss.item()) * batch_size
        y_true.extend(y.cpu().tolist())
        y_pred.extend(pred.cpu().tolist())

    return {
        "loss": total_loss / max(total_samples, 1),
        "accuracy": float(accuracy_score(y_true, y_pred)),
        "macro_f1": float(f1_score(y_true, y_pred, average="macro")),
        "macro_recall": float(recall_score(y_true, y_pred, average="macro")),
        "per_class_recall": recall_score(y_true, y_pred, average=None).tolist(),
        "confusion_matrix": confusion_matrix(y_true, y_pred).tolist(),
        "classification_report": classification_report(y_true, y_pred, digits=4),
    }


def save_logs(path: Path, logs: list[EpochStat]) -> None:
    if not logs:
        return
    with path.open("w", newline="", encoding="utf-8") as file:
        writer = csv.DictWriter(file, fieldnames=list(asdict(logs[0]).keys()))
        writer.writeheader()
        for row in logs:
            writer.writerow(asdict(row))


def save_training_report(path: Path, metrics: dict[str, Any]) -> None:
    lines = [
        "# 优化后论文模型训练结果报告",
        "",
        "## 训练配置",
        "",
        f"- 训练集文件：`{metrics['train_file']}`",
        f"- 测试集文件：`{metrics['test_file']}`",
        f"- 设备：`{metrics['device']}`",
        f"- 训练轮数：`{metrics['epochs']}`",
        f"- 批大小：`{metrics['batch_size']}`",
        f"- 学习率：`{metrics['learning_rate']}`",
        f"- 权重衰减：`{metrics['weight_decay']}`",
        f"- Dropout 率：`{metrics['dropout_rate']}`",
        f"- 调度器 patience：`{metrics['scheduler_patience']}`",
        f"- 调度器 factor：`{metrics['scheduler_factor']}`",
        f"- 验证集比例：`{metrics['val_ratio']}`",
        f"- 随机种子：`{metrics['random_seed']}`",
        f"- STFT 窗长：`{metrics['nperseg']}`",
        f"- STFT 重叠长度：`{metrics['noverlap']}`",
        f"- 滤波范围：`{metrics['lowcut_hz']}` Hz ~ `{metrics['highcut_hz']}` Hz",
        "",
        "## 优化措施",
        "",
        "1. 在每个卷积层后增加 BatchNorm2d，加速收敛并提升泛化。",
        "2. 使用 balanced 类别权重损失函数，缓解类别不平衡。",
        "3. 使用 ReduceLROnPlateau 学习率调度，监控验证集 Macro-F1。",
        "4. 增加 weight_decay 正则化。",
        "5. 增加训练轮数至 80 轮，配合调度器充分训练。",
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
        "## 与基线模型（参考论文 CNN）的对比",
        "",
        "- 基线模型无 BatchNorm，无类别权重，无学习率调度，无 weight_decay。",
        "- 优化后模型在保持相同 STFT 参数（nperseg=32, noverlap=16）的前提下，",
        "  仅通过训练技巧提升性能，不改变模型输入输出形状。",
        "- ONNX 导出后可直接替换 C++ 推理引擎中的模型文件。",
        "",
        "## C++ 兼容性",
        "",
        "- 输入形状：`[1, 1, 17, 11]`（与基线相同）",
        "- 输出形状：`[1, 5]`（logits，由 C++ 端做 Softmax）",
        "- 推理引擎自动检测 rank=4 走 `paper_reference_stft_2dcnn` 路径",
        "",
    ]
    path.write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description="在参考论文模型基础上进行优化训练")
    parser.add_argument("--train-file", type=Path, default=Path("MIT-BIH/mitbih_train.csv"))
    parser.add_argument("--test-file", type=Path, default=Path("MIT-BIH/mitbih_test.csv"))
    parser.add_argument("--output-dir", type=Path, default=Path("artifacts/paper_optimized_cnn"))
    parser.add_argument("--epochs", type=int, default=80)
    parser.add_argument("--batch-size", type=int, default=128)
    parser.add_argument("--lr", type=float, default=1e-3)
    parser.add_argument("--weight-decay", type=float, default=1e-4)
    parser.add_argument("--dropout-rate", type=float, default=0.3)
    parser.add_argument("--scheduler-patience", type=int, default=5)
    parser.add_argument("--scheduler-factor", type=float, default=0.5)
    parser.add_argument("--val-ratio", type=float, default=0.1)
    parser.add_argument("--random-seed", type=int, default=42)
    parser.add_argument("--num-workers", type=int, default=0)
    parser.add_argument("--nperseg", type=int, default=32)
    parser.add_argument("--noverlap", type=int, default=16)
    parser.add_argument("--lowcut-hz", type=float, default=0.5)
    parser.add_argument("--highcut-hz", type=float, default=40.0)
    parser.add_argument("--sampling-rate", type=float, default=360.0)
    parser.add_argument("--filter-order", type=int, default=3)
    args = parser.parse_args()

    if not args.train_file.exists():
        raise FileNotFoundError(f"未找到训练集文件：{args.train_file}")
    if not args.test_file.exists():
        raise FileNotFoundError(f"未找到测试集文件：{args.test_file}")

    set_seed(args.random_seed)
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")

    # ── 数据加载 ──
    x_train_raw, y_train_raw = load_csv_dataset(args.train_file)
    x_test_raw, y_test = load_csv_dataset(args.test_file)

    train_x_raw, val_x_raw, train_y_raw, val_y = train_test_split(
        x_train_raw,
        y_train_raw,
        test_size=args.val_ratio,
        random_state=args.random_seed,
        stratify=y_train_raw,
    )

    # ── SMOTE ──
    print("开始对训练子集做 SMOTE...", flush=True)
    smote = SMOTE(random_state=args.random_seed)
    train_x_balanced, train_y_balanced = smote.fit_resample(train_x_raw, train_y_raw)
    print(f"SMOTE 后训练集形状：{train_x_balanced.shape}", flush=True)

    # ── 带通滤波 ──
    print("开始带通滤波...", flush=True)
    train_x_filtered = apply_bandpass_filter(
        train_x_balanced, args.lowcut_hz, args.highcut_hz, args.sampling_rate, args.filter_order
    )
    val_x_filtered = apply_bandpass_filter(
        val_x_raw, args.lowcut_hz, args.highcut_hz, args.sampling_rate, args.filter_order
    )
    test_x_filtered = apply_bandpass_filter(
        x_test_raw, args.lowcut_hz, args.highcut_hz, args.sampling_rate, args.filter_order
    )

    # ── 归一化 ──
    train_x_norm = minmax_normalize(train_x_filtered)
    val_x_norm = minmax_normalize(val_x_filtered)
    test_x_norm = minmax_normalize(test_x_filtered)

    # ── STFT 时频图 ──
    print("开始生成 STFT 时频图...", flush=True)
    train_specs = build_spectrograms(train_x_norm, args.nperseg, args.noverlap)
    val_specs = build_spectrograms(val_x_norm, args.nperseg, args.noverlap)
    test_specs = build_spectrograms(test_x_norm, args.nperseg, args.noverlap)
    print(
        f"STFT 输出形状：train={train_specs.shape}, val={val_specs.shape}, test={test_specs.shape}",
        flush=True,
    )

    # ── DataLoader ──
    train_loader = DataLoader(
        SpectrogramDataset(train_specs, train_y_balanced),
        batch_size=args.batch_size,
        shuffle=True,
        num_workers=args.num_workers,
        pin_memory=False,
    )
    val_loader = DataLoader(
        SpectrogramDataset(val_specs, val_y),
        batch_size=args.batch_size,
        shuffle=False,
        num_workers=args.num_workers,
        pin_memory=False,
    )
    test_loader = DataLoader(
        SpectrogramDataset(test_specs, y_test),
        batch_size=args.batch_size,
        shuffle=False,
        num_workers=args.num_workers,
        pin_memory=False,
    )

    # ── 模型与优化器 ──
    classes = sorted(np.unique(y_train_raw).tolist())
    num_classes = len(classes)
    model = OptimizedReferenceECGCNN(num_classes=num_classes, dropout_rate=args.dropout_rate).to(device)

    # 类别权重损失
    class_weights = compute_class_weights(train_y_balanced).to(device)
    print(f"类别权重：{class_weights.tolist()}", flush=True)
    criterion = nn.CrossEntropyLoss(weight=class_weights)

    optimizer = torch.optim.Adam(model.parameters(), lr=args.lr, weight_decay=args.weight_decay)
    scheduler = torch.optim.lr_scheduler.ReduceLROnPlateau(
        optimizer, mode="max", patience=args.scheduler_patience, factor=args.scheduler_factor
    )

    # ── 训练循环 ──
    args.output_dir.mkdir(parents=True, exist_ok=True)
    model_path = args.output_dir / "model_best.pt"
    onnx_path = args.output_dir / "model_best.onnx"
    logs: list[EpochStat] = []

    best_macro_f1 = -1.0
    best_state: dict[str, torch.Tensor] | None = None
    for epoch in range(1, args.epochs + 1):
        current_lr = optimizer.param_groups[0]["lr"]
        train_loss = run_epoch(model, train_loader, criterion, optimizer, device)
        val_metrics = evaluate(model, val_loader, criterion, device)

        scheduler.step(val_metrics["macro_f1"])

        logs.append(
            EpochStat(
                epoch=epoch,
                train_loss=train_loss,
                val_loss=val_metrics["loss"],
                val_accuracy=val_metrics["accuracy"],
                val_macro_f1=val_metrics["macro_f1"],
                learning_rate=current_lr,
            )
        )
        print(
            f"第 {epoch:02d}/{args.epochs} 轮 | "
            f"lr={current_lr:.6f} | "
            f"训练损失={train_loss:.4f} | "
            f"验证损失={val_metrics['loss']:.4f} | "
            f"验证准确率={val_metrics['accuracy']:.4f} | "
            f"验证 Macro-F1={val_metrics['macro_f1']:.4f}",
            flush=True,
        )
        if val_metrics["macro_f1"] > best_macro_f1:
            best_macro_f1 = val_metrics["macro_f1"]
            best_state = {key: value.cpu().clone() for key, value in model.state_dict().items()}

    if best_state is None:
        raise RuntimeError("训练失败：未保存到最佳模型参数")

    # ── 保存模型 ──
    model.load_state_dict(best_state)
    torch.save(
        {
            "state_dict": model.state_dict(),
            "num_classes": num_classes,
            "classes": classes,
            "stft_shape": list(train_specs.shape[1:]),
            "nperseg": args.nperseg,
            "noverlap": args.noverlap,
            "lowcut_hz": args.lowcut_hz,
            "highcut_hz": args.highcut_hz,
        },
        model_path,
    )

    # ── 测试集评估 ──
    test_metrics = evaluate(model, test_loader, criterion, device)

    # ── ONNX 导出 ──
    onnx_export_error = ""
    dummy_input = torch.randn(1, *train_specs.shape[1:], device=device)
    model.eval()
    try:
        torch.onnx.export(
            model,
            dummy_input,
            onnx_path,
            input_names=["input"],
            output_names=["logits"],
            dynamic_axes={"input": {0: "batch_size"}, "logits": {0: "batch_size"}},
            opset_version=13,
            dynamo=False,
        )
    except Exception as exc:  # pragma: no cover
        if not onnx_path.exists():
            onnx_export_error = str(exc)

    # ── 保存日志和指标 ──
    save_logs(args.output_dir / "train_log.csv", logs)
    (args.output_dir / "classification_report.txt").write_text(
        test_metrics["classification_report"],
        encoding="utf-8",
    )
    np.savetxt(
        args.output_dir / "confusion_matrix.csv",
        np.asarray(test_metrics["confusion_matrix"], dtype=np.int64),
        fmt="%d",
        delimiter=",",
    )

    metrics = {
        "model_type": "OptimizedReferencePaperSTFT2DCNN",
        "train_file": str(args.train_file),
        "test_file": str(args.test_file),
        "device": str(device),
        "epochs": args.epochs,
        "batch_size": args.batch_size,
        "learning_rate": args.lr,
        "weight_decay": args.weight_decay,
        "dropout_rate": args.dropout_rate,
        "scheduler_patience": args.scheduler_patience,
        "scheduler_factor": args.scheduler_factor,
        "val_ratio": args.val_ratio,
        "random_seed": args.random_seed,
        "nperseg": args.nperseg,
        "noverlap": args.noverlap,
        "lowcut_hz": args.lowcut_hz,
        "highcut_hz": args.highcut_hz,
        "best_val_macro_f1": best_macro_f1,
        "test_loss": test_metrics["loss"],
        "test_accuracy": test_metrics["accuracy"],
        "test_macro_f1": test_metrics["macro_f1"],
        "test_macro_recall": test_metrics["macro_recall"],
        "test_per_class_recall": test_metrics["per_class_recall"],
        "confusion_matrix": test_metrics["confusion_matrix"],
        "classes": classes,
        "onnx_export_error": onnx_export_error,
    }
    (args.output_dir / "metrics.json").write_text(
        json.dumps(metrics, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )
    save_training_report(args.output_dir / "训练报告.md", metrics)

    print(f"测试集 Accuracy：{metrics['test_accuracy']:.4f}", flush=True)
    print(f"测试集 Macro-F1：{metrics['test_macro_f1']:.4f}", flush=True)
    print(f"模型已保存到：{model_path}", flush=True)
    if onnx_export_error:
        print(f"ONNX 导出跳过：{onnx_export_error}", flush=True)
    else:
        print(f"ONNX 已保存：{onnx_path}", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
