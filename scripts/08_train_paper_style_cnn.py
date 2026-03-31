#!/usr/bin/env python3
"""基于 STFT + 2D-CNN 的论文风格心电分类训练脚本。"""

from __future__ import annotations

import argparse
import csv
import json
import random
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any

import numpy as np
import torch
import torch.nn as nn
from scipy.signal import stft
from sklearn.metrics import accuracy_score, classification_report, confusion_matrix, f1_score, recall_score
from sklearn.model_selection import train_test_split
from torch.utils.data import DataLoader, Dataset


def set_seed(seed: int) -> None:
    random.seed(seed)
    np.random.seed(seed)
    torch.manual_seed(seed)
    torch.cuda.manual_seed_all(seed)


class SpectrogramDataset(Dataset):
    """保存 STFT 时频图及其标签的数据集。"""

    def __init__(self, x: np.ndarray, y: np.ndarray):
        self.x = torch.from_numpy(x).float()
        self.y = torch.from_numpy(y).long()

    def __len__(self) -> int:
        return int(self.x.shape[0])

    def __getitem__(self, idx: int) -> tuple[torch.Tensor, torch.Tensor]:
        return self.x[idx], self.y[idx]


class PaperStyleECGCNN(nn.Module):
    """接近论文描述的 2D-CNN 结构：卷积 + 池化 + Dropout + 全连接。"""

    def __init__(self, num_classes: int):
        super().__init__()
        self.features = nn.Sequential(
            nn.Conv2d(1, 16, kernel_size=3, padding=1),
            nn.ReLU(inplace=True),
            nn.MaxPool2d(kernel_size=2),
            nn.Conv2d(16, 32, kernel_size=3, padding=1),
            nn.ReLU(inplace=True),
            nn.MaxPool2d(kernel_size=2),
            nn.Conv2d(32, 64, kernel_size=3, padding=1),
            nn.ReLU(inplace=True),
            nn.AdaptiveAvgPool2d((1, 1)),
        )
        self.classifier = nn.Sequential(
            nn.Flatten(),
            nn.Dropout(p=0.3),
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


def build_spectrograms(signals: np.ndarray, nperseg: int, noverlap: int) -> np.ndarray:
    """将一维心拍序列转换为 STFT 时频图。"""
    specs: list[np.ndarray] = []
    for signal in signals:
        _, _, zxx = stft(signal, nperseg=nperseg, noverlap=noverlap, boundary=None)
        magnitude = np.abs(zxx).astype(np.float32)
        magnitude_min = float(magnitude.min())
        magnitude_max = float(magnitude.max())
        if magnitude_max > magnitude_min:
            magnitude = (magnitude - magnitude_min) / (magnitude_max - magnitude_min)
        else:
            magnitude = np.zeros_like(magnitude, dtype=np.float32)
        specs.append(magnitude[np.newaxis, ...])
    return np.stack(specs, axis=0).astype(np.float32)


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
        "# 论文风格 STFT + 2D-CNN 训练结果报告",
        "",
        "## 训练配置",
        "",
        f"- 设备：`{metrics['device']}`",
        f"- 训练轮数：`{metrics['epochs']}`",
        f"- 批大小：`{metrics['batch_size']}`",
        f"- 学习率：`{metrics['learning_rate']}`",
        f"- 权重衰减：`{metrics['weight_decay']}`",
        f"- 验证集比例：`{metrics['val_ratio']}`",
        f"- 随机种子：`{metrics['random_seed']}`",
        f"- STFT 窗长：`{metrics['nperseg']}`",
        f"- STFT 重叠长度：`{metrics['noverlap']}`",
        f"- 训练集抽样比例：`{metrics['train_fraction']}`",
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
        "## 与论文的一致点",
        "",
        "- 使用 STFT 将一维心拍转换为时频图。",
        "- 使用 2D-CNN 而不是直接对原始一维序列做卷积。",
        "- 卷积核大小使用 3，包含池化层、Dropout、全连接层。",
        "- 默认超参数与论文接近：Adam、学习率 0.001、批大小 32、50 轮。",
        "",
        "## 与论文的差异点",
        "",
        "- 当前仍基于本项目已有的单拍 187 点样本，而不是论文中的 10 秒滑窗。",
        "- 当前脚本未接入 SMOTE 平衡、剪枝和量化流程。",
        "- 因此该脚本应视为论文风格近似复现，而不是严格逐项复现。",
        "",
        "## ONNX 导出状态",
        "",
    ]
    if metrics["onnx_export_error"]:
        lines.append(f"- 导出失败：`{metrics['onnx_export_error']}`")
    else:
        lines.append("- 导出成功")
    lines.append("")
    path.write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description="训练论文风格的 STFT + 2D-CNN 模型")
    parser.add_argument("--prepared-file", type=Path, default=Path("processed/mitbih_prepared.npz"))
    parser.add_argument("--output-dir", type=Path, default=Path("artifacts/paper_style_cnn"))
    parser.add_argument("--epochs", type=int, default=50)
    parser.add_argument("--batch-size", type=int, default=32)
    parser.add_argument("--lr", type=float, default=1e-3)
    parser.add_argument("--weight-decay", type=float, default=1e-4)
    parser.add_argument("--val-ratio", type=float, default=0.1)
    parser.add_argument("--random-seed", type=int, default=42)
    parser.add_argument("--num-workers", type=int, default=0)
    parser.add_argument("--nperseg", type=int, default=32)
    parser.add_argument("--noverlap", type=int, default=16)
    parser.add_argument("--train-fraction", type=float, default=1.0)
    args = parser.parse_args()

    if not args.prepared_file.exists():
        raise FileNotFoundError(f"未找到处理后的数据文件：{args.prepared_file}")
    if not (0 < args.train_fraction <= 1.0):
        raise ValueError("--train-fraction 必须在 (0, 1] 范围内")

    set_seed(args.random_seed)
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")

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

    print("开始生成 STFT 时频图...", flush=True)
    train_specs = build_spectrograms(train_x, args.nperseg, args.noverlap)
    val_specs = build_spectrograms(val_x, args.nperseg, args.noverlap)
    test_specs = build_spectrograms(x_test, args.nperseg, args.noverlap)
    print(
        f"STFT 输出形状：train={train_specs.shape}, val={val_specs.shape}, test={test_specs.shape}",
        flush=True,
    )

    train_loader = DataLoader(
        SpectrogramDataset(train_specs, train_y),
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

    classes = sorted(np.unique(y_train).tolist())
    num_classes = len(classes)
    model = PaperStyleECGCNN(num_classes=num_classes).to(device)
    criterion = nn.CrossEntropyLoss()
    optimizer = torch.optim.Adam(model.parameters(), lr=args.lr, weight_decay=args.weight_decay)

    args.output_dir.mkdir(parents=True, exist_ok=True)
    model_path = args.output_dir / "model_best.pt"
    onnx_path = args.output_dir / "model_best.onnx"
    logs: list[EpochStat] = []

    best_macro_f1 = -1.0
    best_state: dict[str, torch.Tensor] | None = None
    for epoch in range(1, args.epochs + 1):
        train_loss = run_epoch(model, train_loader, criterion, optimizer, device)
        val_metrics = evaluate(model, val_loader, criterion, device)
        logs.append(
            EpochStat(
                epoch=epoch,
                train_loss=train_loss,
                val_loss=val_metrics["loss"],
                val_accuracy=val_metrics["accuracy"],
                val_macro_f1=val_metrics["macro_f1"],
            )
        )
        print(
            f"第 {epoch:02d}/{args.epochs} 轮 | "
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

    model.load_state_dict(best_state)
    torch.save(
        {
            "state_dict": model.state_dict(),
            "num_classes": num_classes,
            "classes": classes,
            "stft_shape": list(train_specs.shape[1:]),
            "nperseg": args.nperseg,
            "noverlap": args.noverlap,
        },
        model_path,
    )

    test_metrics = evaluate(model, test_loader, criterion, device)
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
        "model_type": "PaperStyleSTFT2DCNN",
        "device": str(device),
        "epochs": args.epochs,
        "batch_size": args.batch_size,
        "learning_rate": args.lr,
        "weight_decay": args.weight_decay,
        "val_ratio": args.val_ratio,
        "random_seed": args.random_seed,
        "nperseg": args.nperseg,
        "noverlap": args.noverlap,
        "train_fraction": args.train_fraction,
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
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
