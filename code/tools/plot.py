"""读取 documents/results/ 下的四份基准 CSV，绘制报告第 3、4 章使用的三张高清统计图。

输出目录：documents/results/figures/
  - fig1_scale.png         规模扩展（总耗时与剔除排序后的核心求解耗时）
  - fig2_k_sweep.png       购买件数 K 梯度反转与排序/求解占比拆解
  - fig3_huge_budget.png   超大预算拓展（D 从 10^6 到 10^14 的量级分化）

用法（在仓库根执行）：
    python code/tools/plot.py
"""
import csv
import os
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
RES_DIR = os.path.join(ROOT, "documents", "results")
FIG_DIR = os.path.join(RES_DIR, "figures")

# 配置中文字体与学术风格全局参数
plt.rcParams["font.sans-serif"] = ["Microsoft YaHei", "SimHei", "DejaVu Sans"]
plt.rcParams["axes.unicode_minus"] = False
plt.rcParams["axes.edgecolor"] = "#333333"
plt.rcParams["axes.linewidth"] = 0.9
plt.rcParams["grid.color"] = "#D8DCE3"
plt.rcParams["grid.linestyle"] = "--"
plt.rcParams["grid.linewidth"] = 0.6

ALGO_STYLE = {
    "heap_full": {"label": "标准 N 路堆 (heap_full)", "color": "#C44E52", "marker": "s", "ls": "-"},
    "heap_lazy": {"label": "前沿懒扩展堆 (heap_lazy)", "color": "#DD8452", "marker": "^", "ls": "--"},
    "bsearch":   {"label": "值域二分双指针 (bsearch)", "color": "#2E5E8C", "marker": "o", "ls": "-"},
    "brute":     {"label": "朴素展开排序 (brute)",     "color": "#7F7F7F", "marker": "x", "ls": ":"},
}


def load_csv(name):
    path = os.path.join(RES_DIR, name)
    rows = []
    with open(path, "r", encoding="utf-8") as f:
        for r in csv.DictReader(f):
            r["n"] = int(r["n"])
            r["budget"] = int(r["budget"])
            r["count"] = int(r["count"])
            r["rem"] = int(r["rem"])
            r["repeat"] = int(r["repeat"])
            r["sort_ms"] = float(r["sort_ms"])
            r["solve_ms"] = float(r["solve_ms"])
            r["total_ms"] = float(r["total_ms"])
            r["ops"] = int(r["ops"])
            r["rounds"] = int(r["rounds"])
            rows.append(r)
    return rows


def plot_scale():
    rows = load_csv("scale.csv")
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(11.2, 4.3), dpi=300)

    for algo in ["brute", "heap_full", "heap_lazy", "bsearch"]:
        sub = sorted([r for r in rows if r["algo"] == algo], key=lambda x: x["n"])
        if not sub:
            continue
        xs = [r["n"] for r in sub]
        ytot = [max(r["total_ms"], 1e-3) for r in sub]
        st = ALGO_STYLE[algo]
        ax1.plot(xs, ytot, label=st["label"], color=st["color"], marker=st["marker"],
                 linestyle=st["ls"], linewidth=1.8, markersize=5.5)

    ax1.set_xscale("log")
    ax1.set_yscale("log")
    ax1.set_xlabel("商品 / 优惠券种类数 N（对数坐标）", fontsize=10)
    ax1.set_ylabel("单次总耗时 (ms，对数坐标)", fontsize=10)
    ax1.set_title("(a) 总耗时随规模 N 的变化（含暴力对照）", fontsize=10.5, fontweight="bold")
    ax1.grid(True, which="major", alpha=0.8)
    ax1.legend(fontsize=8.4, frameon=True, framealpha=0.92, loc="lower right")

    for algo in ["heap_full", "heap_lazy", "bsearch"]:
        sub = sorted([r for r in rows if r["algo"] == algo], key=lambda x: x["n"])
        xs = [r["n"] / 1000.0 for r in sub]
        ysol = [r["solve_ms"] for r in sub]
        st = ALGO_STYLE[algo]
        ax2.plot(xs, ysol, label=st["label"], color=st["color"], marker=st["marker"],
                 linestyle=st["ls"], linewidth=1.8, markersize=5.5)

    ax2.set_xlabel(r"规模 $N$（千，$\times 10^3$）", fontsize=10)
    ax2.set_ylabel("核心求解耗时 solve_ms (ms)", fontsize=10)
    ax2.set_title("(b) 剔除排序后的核心求解耗时对比", fontsize=10.5, fontweight="bold")
    ax2.grid(True, alpha=0.8)
    ax2.legend(fontsize=8.4, frameon=True, framealpha=0.92, loc="upper left")

    fig.tight_layout(pad=1.4)
    out = os.path.join(FIG_DIR, "fig1_scale.png")
    fig.savefig(out, dpi=300, bbox_inches="tight")
    plt.close(fig)
    print(f"-> saved {out}")


def plot_k_sweep():
    rows = load_csv("k_sweep.csv")
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(11.4, 4.4), dpi=300)

    for algo in ["heap_full", "heap_lazy", "bsearch"]:
        sub = [r for r in rows if r["algo"] == algo]
        pts = []
        seen_k = set()
        for r in sub:
            if r["count"] not in seen_k:
                pts.append((r["count"], r["solve_ms"]))
                seen_k.add(r["count"])
        st = ALGO_STYLE[algo]
        ax1.plot([p[0] for p in pts], [p[1] for p in pts], label=st["label"],
                 color=st["color"], marker=st["marker"], linestyle=st["ls"],
                 linewidth=1.9, markersize=6.0)

    ax1.set_xscale("log")
    ax1.set_ylim(-0.2, 3.3)
    ax1.set_xlabel(r"实际购买件数 $K$（$N=10^5,\,D=10^6$，对数坐标）", fontsize=10)
    ax1.set_ylabel("核心求解耗时 solve_ms (ms)", fontsize=10)
    ax1.set_title("(a) 购买件数 K 梯度下的求解耗时反转", fontsize=10.5, fontweight="bold")
    ax1.grid(True, alpha=0.8)
    ax1.legend(fontsize=8.5, frameon=True, framealpha=0.95, loc="upper left")

    pick_cases = [("K1_tiny.in", r"$K\approx 10^2$"), ("K4_mid2.in", r"$K\approx 9.5\times 10^4$"),
                  ("K5_large.in", r"$K\approx 6.3\times 10^5$"), ("K7_max1e6.in", r"$K=10^6$")]
    algos = ["heap_full", "heap_lazy", "bsearch"]
    x = np.arange(len(pick_cases))
    width = 0.24

    for idx, algo in enumerate(algos):
        sorts, solves = [], []
        for cname, _ in pick_cases:
            rec = next(r for r in rows if r["case"] == cname and r["algo"] == algo)
            sorts.append(rec["sort_ms"])
            solves.append(rec["solve_ms"])
        offset = (idx - 1.0) * width
        col = ALGO_STYLE[algo]["color"]
        ax2.bar(x + offset, sorts, width, color=col, alpha=0.35, edgecolor=col, linewidth=0.8)
        ax2.bar(x + offset, solves, width, bottom=sorts, color=col, alpha=0.95,
                edgecolor="#222222", linewidth=0.5, label=algo)

    ax2.set_xticks(x)
    ax2.set_xticklabels([lbl for _, lbl in pick_cases], fontsize=9.5)
    ax2.set_ylabel("单次耗时 (ms)：浅色=排序，深色=求解", fontsize=9.5)
    ax2.set_title("(b) 总耗时拆解：排序阶段 vs 核心求解阶段", fontsize=10.5, fontweight="bold")
    ax2.grid(True, axis="y", alpha=0.8)
    ax2.legend(fontsize=8.5, frameon=True, framealpha=0.95, loc="upper right")

    fig.tight_layout(pad=1.4)
    out = os.path.join(FIG_DIR, "fig2_k_sweep.png")
    fig.savefig(out, dpi=300, bbox_inches="tight")
    plt.close(fig)
    print(f"-> saved {out}")


def plot_huge_budget():
    rows = load_csv("huge_budget.csv")
    fig, ax = plt.subplots(figsize=(7.8, 4.3), dpi=300)

    for algo in ["heap_full", "heap_lazy", "bsearch"]:
        sub = sorted([r for r in rows if r["algo"] == algo], key=lambda x: x["budget"])
        xs = [r["budget"] for r in sub]
        ys = [r["total_ms"] for r in sub]
        st = ALGO_STYLE[algo]
        ax.plot(xs, ys, label=st["label"], color=st["color"], marker=st["marker"],
                linestyle=st["ls"], linewidth=2.0, markersize=6.2)

    b_map = {}
    for r in rows:
        if r["algo"] == "bsearch":
            b_map[r["budget"]] = (r["count"], r["total_ms"])
    if 10**10 in b_map:
        hf_b4 = next(r for r in rows if r["algo"] == "heap_full" and r["budget"] == 10**10)
        ax.annotate(f"K={hf_b4['count']:,}\n堆总耗时 {hf_b4['total_ms']:.0f} ms",
                    xy=(10**10, hf_b4["total_ms"]), xytext=(4e11, 125),
                    arrowprops=dict(arrowstyle="->", color="#C44E52", lw=1.2),
                    fontsize=8.6, color="#902B2F",
                    bbox=dict(boxstyle="round,pad=0.28", fc="#FFF5F5", ec="#C44E52", lw=0.8))
    if 10**14 in b_map:
        cnt6, ms6 = b_map[10**14]
        ax.annotate(rf"$K={cnt6/1e9:.2f}\times 10^9$" + f"\nbsearch {ms6:.1f} ms",
                    xy=(10**14, ms6), xytext=(1.0e12, 45),
                    arrowprops=dict(arrowstyle="->", color="#2E5E8C", lw=1.2),
                    fontsize=8.6, color="#1D3E5E",
                    bbox=dict(boxstyle="round,pad=0.28", fc="#F2F7FC", ec="#2E5E8C", lw=0.8))

    ax.axvline(10**6, color="#888888", linestyle=":", linewidth=1.2)
    ax.text(1.5e6, 45, "题面上限\n" + r"$D=10^6$", fontsize=8.4, color="#555555")

    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_ylim(8, 280)
    ax.set_xlabel(r"预算 $D$（$N=10^5$，突破题面 $10^6$ 上限拓展至 $10^{14}$，对数坐标）", fontsize=10)
    ax.set_ylabel("单次总耗时 (ms，对数坐标)", fontsize=10)
    ax.set_title("超大预算拓展下堆算法与值域二分的量级分化", fontsize=11, fontweight="bold")
    ax.grid(True, which="major", alpha=0.8)
    ax.legend(fontsize=8.8, frameon=True, framealpha=0.95, loc="upper left")

    fig.tight_layout(pad=1.2)
    out = os.path.join(FIG_DIR, "fig3_huge_budget.png")
    fig.savefig(out, dpi=300, bbox_inches="tight")
    plt.close(fig)
    print(f"-> saved {out}")


def main():
    os.makedirs(FIG_DIR, exist_ok=True)
    plot_scale()
    plot_k_sweep()
    plot_huge_budget()
    print("全部三张图表已生成。")


if __name__ == "__main__":
    main()
