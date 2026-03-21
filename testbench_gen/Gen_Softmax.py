#!/usr/bin/env python3
"""Generate fp16 Softmax test vectors and golden outputs.

Usage:
	python Gen_Softmax.py --data_len 128 --random_seed 42
"""

from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
import torch


def parse_args() -> argparse.Namespace:
	parser = argparse.ArgumentParser(description="Generate fp16 Softmax test data")
	parser.add_argument(
		"--data_len",
		type=int,
		required=True,
		help="Length of 1D input vector",
	)
	parser.add_argument(
		"--random_seed",
		type=int,
		required=True,
		help="Random seed for reproducible test vectors",
	)
	parser.add_argument(
		"--output_dir",
		type=str,
		default="output",
		help='Output directory (default: "output")',
	)
	return parser.parse_args()


def main() -> None:
	args = parse_args()

	if args.data_len <= 0:
		raise ValueError("data_len must be > 0")

	out_dir = Path(args.output_dir)
	out_dir.mkdir(parents=True, exist_ok=True)

	torch.manual_seed(args.random_seed)
	np.random.seed(args.random_seed)

	# Use a moderate value range to avoid extreme exponent behavior.
	input_torch = (torch.rand(args.data_len, dtype=torch.float32) * 16.0) - 8.0
	golden_torch = torch.softmax(input_torch, dim=0)

	input_fp16 = input_torch.detach().cpu().numpy().astype(np.float16)
	golden_fp16 = golden_torch.detach().cpu().numpy().astype(np.float16)

	input_path = out_dir / f"softmax_input_len{args.data_len}_seed{args.random_seed}.bin"
	golden_path = out_dir / f"softmax_golden_len{args.data_len}_seed{args.random_seed}.bin"

	input_fp16.tofile(input_path)
	golden_fp16.tofile(golden_path)

	print(f"[Softmax] data_len={args.data_len}, random_seed={args.random_seed}")
	print(f"[Softmax] input binary  : {input_path}")
	print(f"[Softmax] golden binary : {golden_path}")


if __name__ == "__main__":
	main()
