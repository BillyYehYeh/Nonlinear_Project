#!/usr/bin/env python3
import math

def clamp(v, lo, hi):
    return max(lo, min(hi, v))

def leading_one_pos32(x):
    if x <= 0:
        return 0
    return x.bit_length() - 1

def expected_sole_softmax(xs):
    n = len(xs)
    max_val = max(xs)
    max_q = math.floor(max_val * 65536.0) / 65536.0

    shifts = []
    sum_i = 0
    for x in xs:
      x_q = math.floor(x * 65536.0) / 65536.0
      y = (x_q - max_q) * 1.4375
      m = clamp(int(-y), 0, 16)
      shifts.append(m)
      sum_i += (65536 >> m)

    lop = leading_one_pos32(sum_i)
    lsb = ((sum_i >> (lop - 1)) & 1) if lop > 0 else 0
    mux = 0.568 if lsb else 0.818

    out = []
    for m in shifts:
      divisor = 2.0 ** ((lop - 16) + m)
      out.append(mux / divisor)
    return out

def main():
    xs = [float(i) for i in range(1, 101)]
    ys = expected_sole_softmax(xs)

    s = sum(ys)
    y_min = min(ys)
    y_max = max(ys)

    assert all(y > 0.0 for y in ys)
    assert y_max > y_min

    print(f"[RTL REF TEST] N={len(xs)}")
    print(f"[RTL REF TEST] sum={s:.9f}")
    print(f"[RTL REF TEST] min={y_min:.9f} max={y_max:.9f}")
    print("[RTL REF TEST] PASS for input 1..100")

if __name__ == '__main__':
    main()
