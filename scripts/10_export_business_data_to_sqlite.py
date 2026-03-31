#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""将业务服务端的 TSV 数据导出为 SQLite 数据库。"""

from __future__ import annotations

import argparse
import csv
import sqlite3
from pathlib import Path
from typing import Iterable, List


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="将业务服务端 TSV 数据导出为 SQLite 数据库")
    parser.add_argument(
        "--data-dir",
        type=Path,
        default=Path("data") / "business_service",
        help="业务服务端 TSV 数据目录，默认 data/business_service",
    )
    parser.add_argument(
        "--output-db",
        type=Path,
        default=Path("artifacts") / "business_service_snapshot.db",
        help="导出的 SQLite 数据库文件路径",
    )
    return parser.parse_args()


def read_tsv_rows(file_path: Path) -> List[List[str]]:
    if not file_path.exists():
        return []
    with file_path.open("r", encoding="utf-8") as file:
        reader = csv.reader(file, delimiter="\t")
        return [row for row in reader if row]


def ensure_parent_dir(file_path: Path) -> None:
    file_path.parent.mkdir(parents=True, exist_ok=True)


def recreate_table(conn: sqlite3.Connection, table_sql: str) -> None:
    conn.executescript(table_sql)


def insert_many(
    conn: sqlite3.Connection,
    sql: str,
    rows: Iterable[Iterable[str]],
) -> int:
    row_list = list(rows)
    if not row_list:
        return 0
    conn.executemany(sql, row_list)
    return len(row_list)


def normalize_alert_row(row: List[str]) -> List[str]:
    # 兼容旧版 9 列报警记录和新版 11 列报警记录。
    if len(row) >= 11:
        return row[:11]
    padded = row[:9]
    while len(padded) < 11:
        padded.append("")
    return padded


def export_to_sqlite(data_dir: Path, output_db: Path) -> None:
    ensure_parent_dir(output_db)
    if output_db.exists():
        output_db.unlink()

    users_rows = read_tsv_rows(data_dir / "users.tsv")
    patients_rows = read_tsv_rows(data_dir / "patients.tsv")
    monitor_rows = read_tsv_rows(data_dir / "monitor_records.tsv")
    alert_rows = [normalize_alert_row(row) for row in read_tsv_rows(data_dir / "alerts.tsv")]

    conn = sqlite3.connect(output_db)
    try:
        recreate_table(
            conn,
            """
            DROP TABLE IF EXISTS users;
            CREATE TABLE users (
                username TEXT PRIMARY KEY,
                password TEXT NOT NULL,
                role TEXT NOT NULL,
                display_name TEXT NOT NULL
            );

            DROP TABLE IF EXISTS patients;
            CREATE TABLE patients (
                patient_id TEXT PRIMARY KEY,
                name TEXT NOT NULL,
                gender TEXT NOT NULL,
                age INTEGER NOT NULL,
                phone TEXT,
                remark TEXT
            );

            DROP TABLE IF EXISTS monitor_records;
            CREATE TABLE monitor_records (
                record_id TEXT PRIMARY KEY,
                patient_id TEXT NOT NULL,
                recorded_at TEXT NOT NULL,
                pred_label TEXT NOT NULL,
                confidence TEXT,
                alert_level TEXT,
                latency_ms TEXT,
                source TEXT,
                sample_name TEXT,
                true_label TEXT,
                FOREIGN KEY(patient_id) REFERENCES patients(patient_id)
            );

            DROP TABLE IF EXISTS alerts;
            CREATE TABLE alerts (
                alert_id TEXT PRIMARY KEY,
                patient_id TEXT NOT NULL,
                created_at TEXT NOT NULL,
                alert_level TEXT NOT NULL,
                pred_label TEXT NOT NULL,
                confidence TEXT,
                source TEXT,
                sample_name TEXT,
                status TEXT NOT NULL,
                confirmed_at TEXT,
                confirmed_by TEXT,
                FOREIGN KEY(patient_id) REFERENCES patients(patient_id)
            );

            CREATE INDEX idx_monitor_records_patient_id ON monitor_records(patient_id);
            CREATE INDEX idx_alerts_patient_id ON alerts(patient_id);
            CREATE INDEX idx_alerts_status ON alerts(status);
            """,
        )

        users_count = insert_many(
            conn,
            "INSERT INTO users(username, password, role, display_name) VALUES (?, ?, ?, ?)",
            [row[:4] for row in users_rows if len(row) >= 4],
        )
        patients_count = insert_many(
            conn,
            "INSERT INTO patients(patient_id, name, gender, age, phone, remark) VALUES (?, ?, ?, ?, ?, ?)",
            [row[:6] for row in patients_rows if len(row) >= 6],
        )
        monitor_count = insert_many(
            conn,
            """
            INSERT INTO monitor_records(
                record_id, patient_id, recorded_at, pred_label, confidence,
                alert_level, latency_ms, source, sample_name, true_label
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            """,
            [row[:10] for row in monitor_rows if len(row) >= 10],
        )
        alert_count = insert_many(
            conn,
            """
            INSERT INTO alerts(
                alert_id, patient_id, created_at, alert_level, pred_label,
                confidence, source, sample_name, status, confirmed_at, confirmed_by
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            """,
            alert_rows,
        )

        conn.commit()
    finally:
        conn.close()

    print(f"数据目录: {data_dir}")
    print(f"输出数据库: {output_db}")
    print(f"users: {users_count}")
    print(f"patients: {patients_count}")
    print(f"monitor_records: {monitor_count}")
    print(f"alerts: {alert_count}")


def main() -> None:
    args = parse_args()
    export_to_sqlite(args.data_dir.resolve(), args.output_db.resolve())


if __name__ == "__main__":
    main()
