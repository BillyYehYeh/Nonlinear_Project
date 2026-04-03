#!/usr/bin/env python3
import argparse
import csv
import os
import sys


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Plot SOLE execution time vs input size with a summary table.")
    parser.add_argument("--input", required=True, help="Input data TXT/CSV path")
    parser.add_argument("--output", required=True, help="Output PNG path")
    parser.add_argument("--title", default="SOLE Input Count vs Execution Time", help="Plot title")
    return parser.parse_args()


def to_float(value: str):
    try:
        return float(value)
    except (TypeError, ValueError):
        return None


def linear_fit(xs, ys):
    n = len(xs)
    if n < 2:
        return None, None
    mean_x = sum(xs) / n
    mean_y = sum(ys) / n
    denom = sum((x - mean_x) ** 2 for x in xs)
    if denom == 0:
        return None, None
    slope = sum((x - mean_x) * (y - mean_y) for x, y in zip(xs, ys)) / denom
    intercept = mean_y - slope * mean_x
    return slope, intercept


def main() -> int:
    args = parse_args()

    try:
        import matplotlib.pyplot as plt
        from matplotlib import gridspec
    except Exception as exc:
        print(f"[ERROR] matplotlib import failed: {exc}", file=sys.stderr)
        return 1

    if not os.path.isfile(args.input):
        print(f"[ERROR] Input file not found: {args.input}", file=sys.stderr)
        return 1

    rows = []
    with open(args.input, "r", newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            rows.append(row)

    if not rows:
        print("[ERROR] No data rows found in input file", file=sys.stderr)
        return 1

    within_limit_rows = [r for r in rows if r.get("case_type") == "within_limit"]
    x_vals = []
    y_vals = []
    for r in within_limit_rows:
        x = to_float(r.get("input_count", ""))
        y = to_float(r.get("execution_time_ns", ""))
        if x is not None and y is not None:
            x_vals.append(x)
            y_vals.append(y)

    if not x_vals:
        print("[ERROR] No valid numeric rows for within_limit data", file=sys.stderr)
        return 1

    fig = plt.figure(figsize=(14, 12), constrained_layout=False)
    gs = gridspec.GridSpec(2, 1, figure=fig, height_ratios=[3, 2.3], hspace=0.45)

    ax = fig.add_subplot(gs[0])
    ax.plot(x_vals, y_vals, marker="o", linewidth=2, color="#1f77b4", label="Measured execution time")
    ax.set_title(args.title, fontsize=14, pad=12)
    ax.set_xlabel("Input data count")
    ax.set_ylabel("SOLE execution time (ns)")
    ax.grid(True, linestyle="--", alpha=0.4)

    slope, intercept = linear_fit(x_vals, y_vals)
    if slope is not None and intercept is not None:
        fit_vals = [slope * x + intercept for x in x_vals]
        ax.plot(x_vals, fit_vals, linestyle="--", color="#d62728", label=f"Linear trend: y={slope:.4f}x+{intercept:.2f}")

    ax.legend(loc="upper left")

    y_min = min(y_vals)
    y_max = max(y_vals)
    ax.text(
        0.99,
        0.01,
        f"Cases={len(x_vals)} | min={y_min:.0f} ns | max={y_max:.0f} ns",
        transform=ax.transAxes,
        ha="right",
        va="bottom",
        fontsize=10,
        bbox={"boxstyle": "round,pad=0.3", "facecolor": "#f7f7f7", "edgecolor": "#cccccc"},
    )

    table_ax = fig.add_subplot(gs[1])
    table_ax.axis("off")
    table_ax.text(
        0.5,
        1.08,
        "Per-case summary table",
        transform=table_ax.transAxes,
        ha="center",
        va="bottom",
        fontsize=12,
        fontweight="bold",
    )

    table_rows = []
    for r in rows:
        table_rows.append([
            r.get("case_type", ""),
            r.get("input_count", ""),
            r.get("execution_time_ns", ""),
            r.get("cosine_similarity", ""),
            r.get("cosine_gt_threshold", ""),
            r.get("timeout_detected", ""),
        ])

    col_labels = [
        "CaseType",
        "InputCount",
        "ExecTime(ns)",
        "Cosine",
        "CosinePass",
        "Timeout",
    ]

    table = table_ax.table(
        cellText=table_rows,
        colLabels=col_labels,
        loc="upper center",
        bbox=[0.0, 0.0, 1.0, 0.95],
        cellLoc="center",
    )
    table.auto_set_font_size(False)
    table.set_fontsize(9)
    table.scale(1, 1.05)

    for (row_idx, col_idx), cell in table.get_celld().items():
        if row_idx == 0:
            cell.set_text_props(weight="bold")
            cell.set_facecolor("#e8eef7")
            continue
        if col_idx == 0 and table_rows[row_idx - 1][0] == "over_limit":
            cell.set_facecolor("#ffe9e9")

    output_dir = os.path.dirname(args.output)
    if output_dir:
        os.makedirs(output_dir, exist_ok=True)

    fig.subplots_adjust(top=0.95, bottom=0.06, left=0.05, right=0.98)
    fig.savefig(args.output, dpi=150, bbox_inches="tight", pad_inches=0.25)
    plt.close(fig)

    print(f"[DONE] Plot written: {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
