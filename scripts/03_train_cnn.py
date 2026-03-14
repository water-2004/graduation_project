#!/usr/bin/env python3
"""在准备好的 MIT-BIH 数据上训练轻量 1D-CNN 模型。"""

from __future__ import annotations

import argparse
import csv
import json
import random
from dataclasses import asdict, dataclass
from pathlib import Path

import numpy as np
import torch
import torch.nn as nn
from sklearn.metrics import accuracy_score, classification_report, confusion_matrix, f1_score, recall_score
from sklearn.model_selection import train_test_split
from torch.utils.data import DataLoader, Dataset


def set_seed(seed: int) -> None:
    random.seed(seed)
    np.random.seed(seed)
    torch.manual_seed(seed)
    torch.cuda.manual_seed_all(seed)


class IndexedBeatDataset(Dataset):
    def __init__(self, x: torch.Tensor, y: torch.Tensor, indices: np.ndarray):
        self.x = x
        self.y = y
        self.indices = torch.from_numpy(indices.astype(np.int64))

    def __len__(self) -> int:
        return int(self.indices.shape[0])

    def __getitem__(self, idx: int) -> tuple[torch.Tensor, torch.Tensor]:
        real_idx = self.indices[idx]
        return self.x[real_idx], self.y[real_idx]


class FullBeatDataset(Dataset):
    def __init__(self, x: torch.Tensor, y: torch.Tensor):
        self.x = x
        self.y = y

    def __len__(self) -> int:
        return int(self.x.shape[0])

    def __getitem__(self, idx: int) -> tuple[torch.Tensor, torch.Tensor]:
        return self.x[idx], self.y[idx]


class ECGCNN(nn.Module):
    def __init__(self, num_classes: int):
        super().__init__()
        self.features = nn.Sequential(
            nn.Conv1d(1, 16, kernel_size=5, padding=2),
            nn.BatchNorm1d(16),
            nn.ReLU(inplace=True),
            nn.MaxPool1d(kernel_size=2),
            nn.Conv1d(16, 32, kernel_size=5, padding=2),
            nn.BatchNorm1d(32),
            nn.ReLU(inplace=True),
            nn.MaxPool1d(kernel_size=2),
            nn.Conv1d(32, 64, kernel_size=3, padding=1),
            nn.BatchNorm1d(64),
            nn.ReLU(inplace=True),
            nn.AdaptiveAvgPool1d(1),
        )
        self.classifier = nn.Sequential(
            nn.Flatten(),
            nn.Dropout(p=0.2),
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
def evaluate(model: nn.Module, dataloader: DataLoader, criterion: nn.Module, device: torch.device) -> dict:
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
    with path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(asdict(logs[0]).keys()))
        writer.writeheader()
        for row in logs:
            writer.writerow(asdict(row))


def save_training_report(path: Path, metrics: dict) -> None:
    lines = [
        "# CNN 训练结果报告",
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
        "",
        "## 核心指标（测试集）",
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

    lines += ["", "## ONNX 导出状态", ""]
    if metrics["onnx_export_error"]:
        lines.append(f"- 导出失败：`{metrics['onnx_export_error']}`")
    else:
        lines.append("- 导出成功")
    lines.append("")
    path.write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description="在处理后的 MIT-BIH 数据上训练 1D-CNN")
    parser.add_argument("--prepared-file", type=Path, default=Path("processed/mitbih_prepared.npz"))
    parser.add_argument("--output-dir", type=Path, default=Path("artifacts/cnn_mitbih"))
    parser.add_argument("--epochs", type=int, default=20)
    parser.add_argument("--batch-size", type=int, default=512)
    parser.add_argument("--lr", type=float, default=1e-3)
    parser.add_argument("--weight-decay", type=float, default=1e-4)
    parser.add_argument("--val-ratio", type=float, default=0.1)
    parser.add_argument("--random-seed", type=int, default=42)
    parser.add_argument("--num-workers", type=int, default=0)
    args = parser.parse_args()

    if not args.prepared_file.exists():
        raise FileNotFoundError(f"未找到处理后的数据文件：{args.prepared_file}")

    set_seed(args.random_seed)
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")

    data = np.load(args.prepared_file)
    x_train = torch.from_numpy(data["x_train"]).unsqueeze(1)
    y_train = torch.from_numpy(data["y_train"]).long()
    x_test = torch.from_numpy(data["x_test"]).unsqueeze(1)
    y_test = torch.from_numpy(data["y_test"]).long()

    classes = sorted(torch.unique(y_train).tolist())
    num_classes = len(classes)
    indices = np.arange(y_train.shape[0])
    train_idx, val_idx = train_test_split(
        indices,
        test_size=args.val_ratio,
        random_state=args.random_seed,
        stratify=y_train.numpy(),
    )

    train_loader = DataLoader(
        IndexedBeatDataset(x_train, y_train, train_idx),
        batch_size=args.batch_size,
        shuffle=True,
        num_workers=args.num_workers,
        pin_memory=False,
    )
    val_loader = DataLoader(
        IndexedBeatDataset(x_train, y_train, val_idx),
        batch_size=args.batch_size,
        shuffle=False,
        num_workers=args.num_workers,
        pin_memory=False,
    )
    test_loader = DataLoader(
        FullBeatDataset(x_test, y_test),
        batch_size=args.batch_size,
        shuffle=False,
        num_workers=args.num_workers,
        pin_memory=False,
    )

    model = ECGCNN(num_classes=num_classes).to(device)
    criterion = nn.CrossEntropyLoss()
    optimizer = torch.optim.Adam(model.parameters(), lr=args.lr, weight_decay=args.weight_decay)

    args.output_dir.mkdir(parents=True, exist_ok=True)
    model_path = args.output_dir / "model_best.pt"
    onnx_path = args.output_dir / "model_best.onnx"
    logs: list[EpochStat] = []

    best_macro_f1 = -1.0
    best_state: dict | None = None
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
            f"第 {epoch:02d}/{args.epochs} 轮 "
            f"训练损失={train_loss:.4f} "
            f"验证损失={val_metrics['loss']:.4f} "
            f"验证准确率={val_metrics['accuracy']:.4f} "
            f"验证Macro-F1={val_metrics['macro_f1']:.4f}"
        )

        if val_metrics["macro_f1"] > best_macro_f1:
            best_macro_f1 = val_metrics["macro_f1"]
            best_state = {k: v.cpu().clone() for k, v in model.state_dict().items()}

    if best_state is None:
        raise RuntimeError("训练失败：未捕获到最佳模型参数。")

    model.load_state_dict(best_state)
    torch.save(
        {
            "state_dict": model.state_dict(),
            "num_classes": num_classes,
            "classes": classes,
            "input_shape": [1, 1, int(x_train.shape[-1])],
        },
        model_path,
    )

    test_metrics = evaluate(model, test_loader, criterion, device)
    onnx_export_error = ""
    dummy_input = torch.randn(1, 1, int(x_train.shape[-1]), device=device)
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

    metrics = {
        "device": str(device),
        "epochs": args.epochs,
        "batch_size": args.batch_size,
        "learning_rate": args.lr,
        "weight_decay": args.weight_decay,
        "val_ratio": args.val_ratio,
        "random_seed": args.random_seed,
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

    save_logs(args.output_dir / "train_log.csv", logs)
    (args.output_dir / "metrics.json").write_text(json.dumps(metrics, indent=2), encoding="utf-8")
    (args.output_dir / "classification_report.txt").write_text(test_metrics["classification_report"], encoding="utf-8")
    np.savetxt(args.output_dir / "confusion_matrix.csv", np.array(test_metrics["confusion_matrix"]), fmt="%d", delimiter=",")
    save_training_report(args.output_dir / "训练报告.md", metrics)

    print(f"最佳模型已保存：{model_path}")
    if onnx_export_error:
        print(f"ONNX 导出跳过：{onnx_export_error}")
    else:
        print(f"ONNX 已保存：{onnx_path}")
    print(f"测试集准确率：{metrics['test_accuracy']:.4f}")
    print(f"测试集 Macro-F1：{metrics['test_macro_f1']:.4f}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
