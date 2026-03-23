#!/usr/bin/env python3
"""
Phase R2 sweep report generator for LoRa CSV logs.

Reads one or more lora_log.csv files and prints a matrix grouped by (sf, ack_timeout_ms),
including success rate, retry rate, and median RTT over ACK outcomes.

Usage:
  python r2_sweep_report.py path/to/lora_log.csv [path/to/another.csv ...]
"""

from __future__ import annotations

import argparse
import csv
import re
import statistics
from collections import defaultdict
from dataclasses import dataclass, field
from pathlib import Path
from typing import DefaultDict, Iterable


ACK_OK_PATTERN = re.compile(r"^ACK_OK_R(\d+)$")
ACK_TIMEOUT_PATTERN = re.compile(r"^ACK_TIMEOUT_R(\d+)$")


@dataclass
class Bucket:
    attempts: int = 0
    ack_ok: int = 0
    timeout: int = 0
    retry_attempts: int = 0
    rtt_values_ms: list[float] = field(default_factory=list)

    def consume(self, status: str, rtt_ms: float) -> None:
        ok_match = ACK_OK_PATTERN.match(status)
        timeout_match = ACK_TIMEOUT_PATTERN.match(status)
        if ok_match:
            self.attempts += 1
            self.ack_ok += 1
            retry_idx = int(ok_match.group(1))
            self.retry_attempts += retry_idx
            if rtt_ms >= 0:
                self.rtt_values_ms.append(rtt_ms)
        elif timeout_match:
            self.attempts += 1
            self.timeout += 1
            retry_idx = int(timeout_match.group(1))
            self.retry_attempts += retry_idx
            if rtt_ms >= 0:
                self.rtt_values_ms.append(rtt_ms)


Key = tuple[int, int]


def parse_int(value: str, fallback: int) -> int:
    try:
        return int(value)
    except (TypeError, ValueError):
        return fallback


def parse_float(value: str, fallback: float) -> float:
    try:
        return float(value)
    except (TypeError, ValueError):
        return fallback


def iter_rows(csv_paths: Iterable[Path]):
    for path in csv_paths:
        with path.open("r", newline="", encoding="utf-8") as handle:
            reader = csv.DictReader(handle)
            for row in reader:
                yield path, row


def aggregate(csv_paths: list[Path]) -> DefaultDict[Key, Bucket]:
    buckets: DefaultDict[Key, Bucket] = defaultdict(Bucket)
    for _, row in iter_rows(csv_paths):
        status = (row.get("status") or "").strip()
        sf = parse_int((row.get("sf") or "").strip(), -1)
        timeout_ms = parse_int((row.get("ack_timeout_ms") or "").strip(), -1)
        rtt_ms = parse_float((row.get("rtt_ms") or "").strip(), -1.0)

        if sf < 0 or timeout_ms < 0:
            # Skip legacy rows without R2 metadata columns.
            continue

        buckets[(sf, timeout_ms)].consume(status, rtt_ms)

    return buckets


def print_report(buckets: DefaultDict[Key, Bucket]) -> None:
    if not buckets:
        print("No Phase R2 rows found. Ensure CSV has sf and ack_timeout_ms columns.")
        return

    print("sf,ack_timeout_ms,attempts,ack_ok,timeouts,success_rate_pct,retry_rate_pct,median_rtt_ms")
    for (sf, timeout_ms) in sorted(buckets.keys()):
        bucket = buckets[(sf, timeout_ms)]
        success_rate = (100.0 * bucket.ack_ok / bucket.attempts) if bucket.attempts else 0.0
        retry_rate = (100.0 * bucket.retry_attempts / bucket.attempts) if bucket.attempts else 0.0
        median_rtt = statistics.median(bucket.rtt_values_ms) if bucket.rtt_values_ms else -1.0

        print(
            f"{sf},{timeout_ms},{bucket.attempts},{bucket.ack_ok},{bucket.timeout},"
            f"{success_rate:.2f},{retry_rate:.2f},{median_rtt:.2f}"
        )


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate Phase R2 SF/timeout sweep matrix from CSV logs.")
    parser.add_argument("csv", nargs="+", help="One or more lora_log.csv files")
    args = parser.parse_args()

    csv_paths = [Path(p).resolve() for p in args.csv]
    missing = [str(p) for p in csv_paths if not p.exists()]
    if missing:
        print("Missing CSV files:")
        for item in missing:
            print(f"  - {item}")
        return 2

    buckets = aggregate(csv_paths)
    print_report(buckets)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
