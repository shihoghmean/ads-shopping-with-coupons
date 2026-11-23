#ifndef SOLVERS_H
#define SOLVERS_H

#include<string>
#include<vector>

/*
本头文件定义「Shopping With Coupons」五个求解器的统一输入输出接口。
五个实现共享同一份只读原始数组（价格 p 与优惠券 c），各自在内部拷贝后排序或展开，
避免前一个求解器原地改序让后一个求解器占到预处理便宜，保证基准对比口径一致。
*/

// 单个求解器的返回结果：除答案外附带操作计数与分段耗时，供第 3、4 章做机制拆解
struct SolveResult{
    long long count;// 最多能买到的商品件数
    long long rem;// 在买到最大件数的前提下，最多剩余的金额
    long long ops;// 核心工作量计数：堆算法记下沉/上滤步数，二分与剪枝记指针扫描与比较步数
    long long rounds;// 迭代轮数：堆记弹出次数，二分记二分轮数，剪枝记矩阵剪枝轮数
    double sortMs;// 排序与前缀和预处理耗时（毫秒，单次）
    double solveMs;// 核心求解阶段耗时（毫秒，单次）
    double totalMs;// 单次总耗时（毫秒）
};

//---------- 五个求解器声明 ----------

/*
暴力展开基准：直接枚举全部 N^2 个 (商品,优惠券) 配对并全排序。
时间 O(N^2 log N)、空间 O(N^2)，仅用于小规模（N<=4000）对拍，提供独立真值。
*/
SolveResult solveBrute(const std::vector<long long>& prices,const std::vector<long long>& coupons,long long budget);

/*
标准 N 路归并最小堆：将排序后的 N*N 价格矩阵视为 N 条升序行，
初始把全部 N 个行首压入最小堆，每轮弹出全局最小配对并沿同行右移一列。
时间 O(N log N + K log N)，空间 O(N)，其中 K<=min(N^2,D) 为实际购买件数。
*/
SolveResult solveHeapFull(const std::vector<long long>& prices,const std::vector<long long>& coupons,long long budget);

/*
前沿懒扩展最小堆：不预先建满 N 个行首，仅从全局最小格 (0,0) 启动。
每次弹出 (r,col) 后只向右推 (r,col+1)，仅当 col==0 时再向下推 (r+1,0)，
形成覆盖全矩阵的无重生成树，堆大小上限压到 min(N,K+1)，无需哈希去重。
时间 O(N log N + K log min(N,K))，空间 O(min(N,K))。
*/
SolveResult solveHeapLazy(const std::vector<long long>& prices,const std::vector<long long>& coupons,long long budget);

/*
值域二分 + 单调双指针 + 前缀和：对实付价格阈值 T 做二分，
利用矩阵行列单调性在 O(N) 内统计 <=T 的配对个数与总花费（__int128 防溢出），
最后用重复值收尾公式在 O(1) 内补齐临界价位 T*+1 的件数。
时间 O(N log N + N log min(V,D))，空间 O(N)，彻底摆脱对购买件数 K 的依赖。
*/
SolveResult solveBinarySearch(const std::vector<long long>& prices,const std::vector<long long>& coupons,long long budget);

/*
强多项式矩阵加权中位数剪枝（基于 Johnson-Mizoguchi 1978 与 BFPRT 线性选择）：
维护每行未决列区间 [leftCol[r],rightCol[r])，每轮取各活性行中点按活性长度加权，
用确定性 BFPRT 在 O(m) 内求出加权中位数 x，再经单调双指针 O(m) 判定并淘汰至少 1/4 活性元素。
最坏时间 O(N log N)，空间 O(N)，完全不依赖值域 V、预算 D 与购买件数 K。
*/
SolveResult solvePruneSearch(const std::vector<long long>& prices,const std::vector<long long>& coupons,long long budget);

/*
统一定时包装器：重复运行同一求解器 repeat 次并累加 clock()，
用于压低单次耗时在毫秒级以下时的时钟量化误差。每次重复都会传入原始未排序输入。
*/
SolveResult runTimed(const std::string& algo,const std::vector<long long>& prices,const std::vector<long long>& coupons,long long budget,int repeat);

#endif
