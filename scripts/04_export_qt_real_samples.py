#!/usr/bin/env python3
"""导出适合 Qt 客户端联调的真实 MIT-BIH 测试样本。"""

from __future__ import annotations

import argparse
import re
import socket
from collections import defaultdict
from pathlib import Path

import numpy as np


RESPONSE_RE = re.compile(
    r"^OK\s+pred=(\S+)\s+conf=([-\d\.eE\+]+)\s+alert=(\S+)\s+latency_ms=([-\d\.eE\+]+)$"
)


LABEL_NAME = {
    0: "N-正常",
    1: "S-室上性异常",
    2: "V-室性异常",
    3: "F-融合波",
    4: "Q-未知/其他",
}


def build_payload(features: np.ndarray) -> str:
    return ",".join(f"{float(v):.10g}" for v in features.tolist())


def query_service(host: str, port: int, payload: str) -> dict[str, str]:
    with socket.create_connection((host, port), timeout=3) as conn:
        conn.sendall(f"PREDICT {payload}\n".encode("utf-8"))
        response = conn.recv(4096).decode("utf-8", errors="ignore").strip()

    match = RESPONSE_RE.match(response)
    if not match:
        raise RuntimeError(f"服务端返回异常: {response}")

    return {
        "raw": response,
        "pred": match.group(1),
        "conf": match.group(2),
        "alert": match.group(3),
        "latency_ms": match.group(4),
    }


def export_samples(
    prepared_file: Path,
    output_dir: Path,
    host: str,
    port: int,
    samples_per_label: int,
    require_correct: bool,
    search_limit_per_label: int,
) -> list[dict[str, str]]:
    data = np.load(prepared_file)
    x_test = data["x_test"]
    y_test = data["y_test"]

    output_dir.mkdir(parents=True, exist_ok=True)

    exported: list[dict[str, str]] = []
    found_count: dict[int, int] = defaultdict(int)
    scanned_count: dict[int, int] = defaultdict(int)

    for idx, (features, label) in enumerate(zip(x_test, y_test)):
        label = int(label)
        if found_count[label] >= samples_per_label:
            continue
        if scanned_count[label] >= search_limit_per_label:
            continue

        scanned_count[label] += 1
        payload = build_payload(features)

        service_result: dict[str, str] | None = None
        try:
            service_result = query_service(host, port, payload)
        except Exception as exc:
            service_result = {"raw": f"ERROR: {exc}", "pred": "?", "conf": "?", "alert": "?", "latency_ms": "?"}

        if require_correct and service_result["pred"].isdigit():
            if int(service_result["pred"]) != label:
                continue

        file_name = f"label_{label}_sample_{found_count[label] + 1}_idx_{idx}.txt"
        file_path = output_dir / file_name
        file_path.write_text(payload, encoding="utf-8")

        item = {
            "label": str(label),
            "label_name": LABEL_NAME.get(label, str(label)),
            "sample_index": str(idx),
            "file_name": file_name,
            "pred": service_result["pred"],
            "conf": service_result["conf"],
            "alert": service_result["alert"],
            "latency_ms": service_result["latency_ms"],
            "raw": service_result["raw"],
        }
        exported.append(item)
        found_count[label] += 1

        if all(found_count[lbl] >= samples_per_label for lbl in sorted(set(y_test.tolist()))):
            break

    summary_lines = [
        "# Qt 真实样本联调清单",
        "",
        f"- 数据文件：`{prepared_file}`",
        f"- 服务端：`{host}:{port}`",
        f"- 每类样本数：`{samples_per_label}`",
        f"- 是否要求预测正确：`{require_correct}`",
        "",
        "| 真实标签 | 标签说明 | 测试集索引 | 文件名 | 服务端预测 | 置信度 | 告警等级 | 时延(ms) |",
        "| --- | --- | ---: | --- | ---: | ---: | --- | ---: |",
    ]

    for item in exported:
        summary_lines.append(
            f"| {item['label']} | {item['label_name']} | {item['sample_index']} | {item['file_name']} | "
            f"{item['pred']} | {item['conf']} | {item['alert']} | {item['latency_ms']} |"
        )

    (output_dir / "sample_summary.md").write_text("\n".join(summary_lines), encoding="utf-8")
    return exported


def main() -> int:
    parser = argparse.ArgumentParser(description="导出 Qt 联调用真实样本")
    parser.add_argument("--prepared-file", type=Path, default=Path("processed/mitbih_prepared.npz"))
    parser.add_argument("--output-dir", type=Path, default=Path("results/qt_real_samples"))
    parser.add_argument("--host", type=str, default="127.0.0.1")
    parser.add_argument("--port", type=int, default=9000)
    parser.add_argument("--samples-per-label", type=int, default=1)
    parser.add_argument("--search-limit-per-label", type=int, default=300)
    parser.add_argument("--allow-mismatch", action="store_true", default=False)
    args = parser.parse_args()

    if not args.prepared_file.exists():
        raise FileNotFoundError(f"未找到处理后的数据文件: {args.prepared_file}")

    exported = export_samples(
        prepared_file=args.prepared_file,
        output_dir=args.output_dir,
        host=args.host,
        port=args.port,
        samples_per_label=args.samples_per_label,
        require_correct=not args.allow_mismatch,
        search_limit_per_label=args.search_limit_per_label,
    )

    print(f"已导出 {len(exported)} 条样本到: {args.output_dir}")
    for item in exported:
        print(
            f"label={item['label']} idx={item['sample_index']} pred={item['pred']} "
            f"conf={item['conf']} alert={item['alert']} file={item['file_name']}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
