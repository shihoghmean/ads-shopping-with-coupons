"""生成 ADS Project 5 (Shopping With Coupons) 四大基准实验族数据。

所有随机输入使用固定种子 20261123，输出到仓库根的 data/ 目录（不入库、不进提交包），
任意机器运行本脚本都能逐字节复现报告第 3、4 章的全部测试输入。

用法（在仓库根执行）：
    python code/tools/gen_cases.py
"""
import os
import random

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DATA_DIR = os.path.join(ROOT, "data")


def write_case(subdir, name, prices, coupons, budget):
    out_dir = os.path.join(DATA_DIR, subdir)
    os.makedirs(out_dir, exist_ok=True)
    path = os.path.join(out_dir, name + ".in")
    # 校验题面硬约束 max(c) < min(p)，避免生成器自身出错污染实验
    assert len(prices) == len(coupons) and len(prices) > 0
    assert max(coupons) < min(prices), f"invalid case {name}: max(c)={max(coupons)} >= min(p)={min(prices)}"
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(f"{len(prices)} {budget}\n")
        f.write(" ".join(map(str, prices)) + "\n")
        f.write(" ".join(map(str, coupons)) + "\n")
    return path


def gen_scale(rng):
    """实验族 A：规模扩展（N 从 1000 到 100000，固定 D=10^6，低价高购买量）"""
    for n in [1000, 2000, 5000, 10000, 20000, 50000, 100000]:
        coupons = [rng.randint(9800, 10000) for _ in range(n)]
        max_c = max(coupons)
        prices = [rng.randint(max_c + 1, max_c + 200) for _ in range(n)]
        write_case("scale", f"S_n{n}", prices, coupons, 1_000_000)


def gen_k_sweep(rng):
    """实验族 B：固定 N=100000，改变实付价区间与预算，使实际购买量 K 从 ~100 扫到 ~1000000"""
    n = 100000
    configs = [
        # (name, span_c, gap, span_p, budget)
        ("K1_tiny", 5000, 9000, 5000, 1_000_000),      # 最小价约 9000，K 约 100
        ("K2_small", 5000, 800, 5000, 1_000_000),      # K 约 1000
        ("K3_mid1", 2000, 50, 2000, 1_000_000),        # K 约 1.5 万
        ("K4_mid2", 500, 10, 500, 1_000_000),          # K 约 5~8 万
        ("K5_large", 200, 1, 200, 1_000_000),          # K 约 20 万
        ("K6_xlarge", 50, 1, 50, 1_000_000),           # K 约 50 万
        ("K7_max1e6", 3, 1, 3, 1_000_000),             # 实付价集中在 1~7，K 逼近 100 万上限
    ]
    for name, span_c, gap, span_p, budget in configs:
        base_c = 100000
        coupons = [rng.randint(base_c - span_c, base_c) for _ in range(n)]
        max_c = max(coupons)
        prices = [rng.randint(max_c + gap, max_c + gap + span_p) for _ in range(n)]
        write_case("k_sweep", name, prices, coupons, budget)


def gen_dist(rng):
    """实验族 C：固定 N=100000, D=10^6，五种结构差异显著的输入分布"""
    n = 100000
    budget = 1_000_000

    # D1: 均匀低价（高购买量）
    c1 = [rng.randint(99000, 100000) for _ in range(n)]
    mc1 = max(c1)
    p1 = [rng.randint(mc1 + 1, mc1 + 500) for _ in range(n)]
    write_case("dist", "D1_uniform_cheap", p1, c1, budget)

    # D2: 均匀高价（低购买量）
    c2 = [rng.randint(1, 50000) for _ in range(n)]
    mc2 = max(c2)
    p2 = [rng.randint(mc2 + 2000, mc2 + 500000) for _ in range(n)]
    write_case("dist", "D2_uniform_exp", p2, c2, budget)

    # D3: 全相等平地（全部配对实付价恒为 5，K=200000）
    write_case("dist", "D3_all_equal", [1000] * n, [995] * n, budget)

    # D4: 阶梯离散（仅 3 种价格与 3 种优惠券，但值域跨度大）
    p4_vals = [100_000_001, 100_000_050, 500_000_000]
    c4_vals = [100_000_000, 99_999_990, 10_000_000]
    p4 = [rng.choice(p4_vals) for _ in range(n)]
    c4 = [rng.choice(c4_vals) for _ in range(n)]
    write_case("dist", "D4_step_3levels", p4, c4, budget)

    # D5: 两极断层（前 1% 商品很便宜，后 99% 商品贵达 10^9）
    c5 = [rng.randint(99000, 100000) for _ in range(n)]
    mc5 = max(c5)
    p5 = []
    for i in range(n):
        if i < n // 100:
            p5.append(rng.randint(mc5 + 1, mc5 + 20))
        else:
            p5.append(rng.randint(500_000_000, 1_000_000_000))
    rng.shuffle(p5)
    write_case("dist", "D5_heavy_gap", p5, c5, budget)


def gen_huge_budget(rng):
    """实验族 D：突破题面 D<=10^6，固定 N=100000，把预算推高到 10^14，检验 O(K log N) 的崩塌"""
    n = 100000
    coupons = [rng.randint(1, 100000) for _ in range(n)]
    max_c = max(coupons)
    prices = [rng.randint(max_c + 1, max_c + 100000) for _ in range(n)]
    for tag, bud in [
        ("B1_1e6", 10**6),
        ("B2_1e7", 10**7),
        ("B3_1e8", 10**8),
        ("B4_1e10", 10**10),
        ("B5_1e12", 10**12),
        ("B6_1e14", 10**14),
    ]:
        write_case("huge_budget", tag, prices, coupons, bud)


def main():
    rng = random.Random(20261123)
    gen_scale(rng)
    gen_k_sweep(rng)
    gen_dist(rng)
    gen_huge_budget(rng)
    print(f"已生成四大实验族输入到: {DATA_DIR}")


if __name__ == "__main__":
    main()
