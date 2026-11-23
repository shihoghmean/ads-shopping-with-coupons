# Shopping With Coupons — ADS Project 5

浙江大学《高级数据结构与算法分析》(ADS) Project 5 工程仓库。

## 问题简介

给定 $N$ 种商品价格 $p_i$、$N$ 种优惠券面值 $c_j$（满足 $\max c_j < \min p_i \le 10^9$）与总预算 $D$（$N \le 10^5, D \le 10^6$）。每种商品可买多次、每种优惠券可用多次，但同一个 `(商品, 优惠券)` 配对最多使用一次。求在预算 $D$ 内最多能买到的商品件数 $K$，以及买到 $K$ 件时最多剩余的金额 $R$。

## 实现的四种算法

| 算法标识 | 思路 | 时间复杂度 | 空间复杂度 | 角色定位 |
|---|---|---|---|---|
| `brute` | 展开全部 $N^2$ 配对并排序 | $O(N^2 \log N)$ | $O(N^2)$ | 小规模独立真值基准 |
| `heap_full` | 排序后 $N$ 路归并最小堆（全行首入堆） | $O(N \log N + K \log N)$ | $O(N)$ | 教材经典堆解法 |
| `heap_lazy` | 排序后网格前沿懒扩展最小堆（无重生成树） | $O(N \log N + K \log \min(N, K))$ | $O(N)$（堆 $O(\min(N, K))$） | $K \ll N$ 场景优化堆 |
| `bsearch` | 值域二分 + 单调双指针 + 前缀和 + 收尾定理 | $O(N \log N + N \log(\min(V, D)+1))$ | $O(N)$ | 工程综合最快，摆脱对 $K$ 依赖 |

## 目录结构

```text
P5-shopping-with-coupons/
├── CMakeLists.txt          # CMake 构建配置（固定 C++17 / -O2 / -Wall -Wextra）
├── CMakePresets.json       # MinGW g++ / Ninja 预设
├── code/
│   ├── Readme.txt          # 编译与运行说明（随提交包交付）
│   ├── src/                # 平铺源码：solvers.h / solvers.cpp / main.cpp
│   ├── samples/            # 题面样例与三组边界样例
│   └── tools/              # 数据生成、批量测速、画图与注释率/打包自检脚本
└── documents/
    ├── readme.txt          # documents/ 浏览说明
    ├── report.tex          # XeLaTeX 报告源码
    ├── report.pdf          # 编译完成的 PDF 报告
    └── results/            # 四份基准 CSV 与三张统计图
```

## 快速构建与复现

```powershell
# 1. CMake + Ninja 构建（或进 code/ 执行单条 g++ 命令，见 code/Readme.txt）
cmake --preset default
cmake --build --preset default

# 2. 运行全套内置正确性自检（10 固定边界 + 350 随机/大预算对拍）
.\build\coupons.exe test

# 3. 题面标准输入输出测试
Get-Content code\samples\sample1.in | .\build\coupons.exe

# 4. 复现全部基准实验与图表
python code\tools\gen_cases.py
python code\tools\run_bench.py
python code\tools\plot.py
```
