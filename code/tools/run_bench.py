"""批量运行基准测试并将原始 CSV 写入 documents/results/。

每个实验族单独产出一个 CSV 文件，便于后续画图脚本与报告表格直接按原始数据核对。

用法（在仓库根执行）：
    python code/tools/run_bench.py
"""
import csv
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
EXE = os.path.join(ROOT, "build", "coupons.exe")
DATA_DIR = os.path.join(ROOT, "data")
RES_DIR = os.path.join(ROOT, "documents", "results")

HEADER = [
    "algo", "case", "n", "budget", "count", "rem",
    "repeat", "sort_ms", "solve_ms", "total_ms", "ops", "rounds"
]


def run_one(case_path, algo_group, repeat):
    cmd = [EXE, "bench", case_path, algo_group, str(repeat), "--csv"]
    proc = subprocess.run(cmd, capture_output=True, text=True, check=False)
    if proc.returncode != 0:
        print(f"[ERROR] cmd failed: {' '.join(cmd)}", file=sys.stderr)
        print(proc.stderr, file=sys.stderr)
        sys.exit(1)
    rows = []
    for line in proc.stdout.strip().splitlines():
        if not line.strip():
            continue
        rows.append(line.strip().split(","))
    return rows


def run_suite(csv_name, tasks):
    os.makedirs(RES_DIR, exist_ok=True)
    out_path = os.path.join(RES_DIR, csv_name)
    all_rows = []
    for rel_path, algo_group, repeat in tasks:
        full_path = os.path.join(DATA_DIR, rel_path)
        print(f"  running {rel_path} [{algo_group}] x{repeat} ...")
        rows = run_one(full_path, algo_group, repeat)
        all_rows.extend(rows)
    with open(out_path, "w", encoding="utf-8", newline="") as f:
        w = csv.writer(f)
        w.writerow(HEADER)
        w.writerows(all_rows)
    print(f"-> saved {out_path} ({len(all_rows)} rows)")


def main():
    if not os.path.exists(EXE):
        print(f"[ERROR] 找不到可执行文件: {EXE}，请先构建项目", file=sys.stderr)
        sys.exit(1)

    print("=== Suite A: scale ===")
    run_suite("scale.csv", [
        ("scale/S_n1000.in", "brute", 5),
        ("scale/S_n1000.in", "fast", 50),
        ("scale/S_n2000.in", "brute", 3),
        ("scale/S_n2000.in", "fast", 30),
        ("scale/S_n5000.in", "fast", 30),
        ("scale/S_n10000.in", "fast", 25),
        ("scale/S_n20000.in", "fast", 15),
        ("scale/S_n50000.in", "fast", 10),
        ("scale/S_n100000.in", "fast", 10),
    ])

    print("=== Suite B: k_sweep ===")
    run_suite("k_sweep.csv", [
        ("k_sweep/K1_tiny.in", "fast", 10),
        ("k_sweep/K2_small.in", "fast", 10),
        ("k_sweep/K3_mid1.in", "fast", 10),
        ("k_sweep/K4_mid2.in", "fast", 10),
        ("k_sweep/K5_large.in", "fast", 10),
        ("k_sweep/K6_xlarge.in", "fast", 10),
        ("k_sweep/K7_max1e6.in", "fast", 10),
    ])

    print("=== Suite C: dist ===")
    run_suite("dist.csv", [
        ("dist/D1_uniform_cheap.in", "fast", 10),
        ("dist/D2_uniform_exp.in", "fast", 10),
        ("dist/D3_all_equal.in", "fast", 15),
        ("dist/D4_step_3levels.in", "fast", 15),
        ("dist/D5_heavy_gap.in", "fast", 10),
    ])

    print("=== Suite D: huge_budget ===")
    run_suite("huge_budget.csv", [
        ("huge_budget/B1_1e6.in", "fast", 10),
        ("huge_budget/B2_1e7.in", "fast", 10),
        ("huge_budget/B3_1e8.in", "fast", 5),
        ("huge_budget/B4_1e10.in", "fast", 2),
        ("huge_budget/B5_1e12.in", "nologk", 10),
        ("huge_budget/B6_1e14.in", "nologk", 10),
    ])

    print("=== Suite E: huge_value ===")
    run_suite("huge_value.csv", [
        ("huge_value/V1_x1e0.in", "nologk", 15),
        ("huge_value/V2_x1e3.in", "nologk", 15),
        ("huge_value/V3_x1e6.in", "nologk", 15),
        ("huge_value/V4_x1e9.in", "nologk", 15),
        ("huge_value/V5_x1e11.in", "nologk", 15),
    ])

    print("全部基准实验完成。")


if __name__ == "__main__":
    main()
