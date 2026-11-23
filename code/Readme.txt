Shopping With Coupons —— Project 5 的 code/ 说明（build & run）
================================================================

code/ 内部结构
--------------
src/          主程序源码，三个文件，C++17
  solvers.h     四个求解器的统一接口与结果结构体声明
  solvers.cpp   四个求解器的实现：
                brute（N^2 展开排序基准）
                heap_full（标准 N 路归并最小堆）
                heap_lazy（网格前沿懒扩展最小堆）
                bsearch（值域二分 + 单调双指针 + 前缀和）
  main.cpp      标准 stdin/stdout 入口、内建自检 test、求解 solve、基准 bench
samples/      题面样例与三组边界样例（.in 与对应的 .out）
tools/        Python 3 辅助脚本：生成基准数据、跑基准、画图、统计注释率与自检
Readme.txt    本文件

浏览顺序：src/solvers.h -> src/main.cpp -> src/solvers.cpp。

环境要求
--------
g++ 支持 C++17（Windows 下 MinGW-w64 g++ 与 Linux 下 g++ 均适用），
需要 64 位编译器支持 __int128（GCC/Clang 均原生支持）。
只使用 C++17 标准库，不依赖第三方库。

tools/ 中的脚本使用 Python 3，除 plot.py 需要 matplotlib 外仅用标准库；
主程序本身不依赖 Python，只编译 C++ 即可运行全部求解与自检功能。

编译
----
源码为 C++17（.cpp 与 .h）。在本目录（code/）内用一条命令即可编译：

    g++ -O2 -std=c++17 -Wall -Wextra src/main.cpp src/solvers.cpp -o coupons.exe

Linux / macOS 下将输出名改为 coupons 即可。
-O2 必须保留：报告中的运行时间全部在 -O2 下测得，换优化级别耗时不可比。

如果使用完整仓库（含仓库根目录的 CMakeLists.txt 与 CMakePresets.json），
也可以用 CMake + Ninja 构建，产物位于仓库根的 build/coupons.exe：

    cmake --preset default
    cmake --build --preset default

运行
----
以下示例均假设在 code/ 目录内执行、可执行文件为当前目录的 coupons.exe。
若采用 CMake 构建，请将 .\coupons.exe 换为 ..\build\coupons.exe（或在仓库根运行）。

1) 标准评测模式（无命令行参数，按题面格式从 stdin 读入并输出「最大件数 最大余额」）：

       .\coupons.exe < samples\sample1.in

   期望输出：

       8 2

   其余边界样例（sample2_equal.in、sample3_gap.in、sample4_largeval.in）同理，
   对应标准答案写在同名 .out 文件中。

2) 内建正确性自检（不需要任何外部数据文件）：

       .\coupons.exe test

   依次运行 10 组手工边界用例、300 组固定种子（20261123）随机小规模四算法暴力对拍、
   30 组中大规模三算法对拍与 20 组大预算四算法暴力对拍。全部通过时打印汇总行并以 0 退出；
   任一算法结果不一致会打印冲突用例并以 1 退出。

3) 单文件求解（可指定单个算法或 all 同步校验）：

       .\coupons.exe solve samples\sample1.in all

4) 计时与工作量统计：

       .\coupons.exe bench <输入文件> [算法或算法组] [重复次数] [--csv]

       算法/组    brute | heap_full | heap_lazy | bsearch | all | fast | heaps
                  缺省 fast（运行除 brute 外的三个高效算法）
       重复次数   缺省 1；单次耗时较短时设 10 以上以平滑 clock() 毫秒粒度
       --csv      输出 12 列 CSV：
                  algo,case,n,budget,count,rem,repeat,sort_ms,solve_ms,total_ms,ops,rounds

   同一次 bench 内所有算法会自动校验 (count, rem) 一致，不一致直接报错并以 1 退出。

辅助脚本（tools/，在仓库根执行）
--------------------------------
    python code/tools/gen_cases.py    按固定种子 20261123 生成四大实验族数据到 data/
    python code/tools/run_bench.py    批量运行全部基准实验，写出 CSV 到 documents/results/
    python code/tools/plot.py         读取 CSV，生成三张统计图表到 documents/results/figures/
    python code/tools/check_pkg.py    检查源码注释率与匿名打包自检
