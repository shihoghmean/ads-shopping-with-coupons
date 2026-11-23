#include"solvers.h"

#include<algorithm>
#include<ctime>
#include<functional>
#include<stdexcept>

using namespace std;

/*
内部累加统一使用 128 位有符号整数 __int128：
  1. 在题面约束 N<=10^5、p<=10^9 下，全矩阵 N^2 个配对的总花费最高可达 10^19，
     已经超出 64 位有符号整数上限（约 9.22e18）；
  2. 在拓展的超大值域实验（p<=10^16）里，单行前缀和与全矩阵总花费更会达到 10^26。
用 __int128 存前缀和与矩阵累加和，可以从根源消除溢出，避免为省字节去写易错的饱和加法。
*/
using i128=__int128;

namespace{

/*
计时函数沿用标准 C 的 clock()，与课程其他工程保持一致。
Windows 下 CLOCKS_PER_SEC 为 1000（1 ms 粒度），单次耗时不足 1 ms 时容易量化为 0；
因此外层 runTimed 会通过重复执行 repeat 次再取均值来平滑量化误差。
*/
double elapsedMs(clock_t start,clock_t end){
    return (double)(end-start)*1000.0/CLOCKS_PER_SEC;
}

//---------- 手写二叉最小堆 ----------
/*
两个堆算法（标准 N 路归并堆与前沿懒扩展堆）共用这份手写二叉最小堆，而不用 std::priority_queue：
  1. 统计口径透明：能在 siftDown / siftUp 内精确累加每一次键值比较与搬动步数 ops，
     从而在报告第 4 章把「耗时差」还原为「操作次数差」与「单次代价差」；
  2. 消除冗余往返：归并时绝大多数弹出后紧跟一次同行右移入堆，提供 replaceTop 原地覆写堆顶
     后直接单趟下沉，比先 pop（末元素上提再下沉）再 push（新元素入尾再上滤）少一半树高遍历；
  3. 保证公平对照：两种堆除了「初始装载与后继扩展策略」之外底层容器完全一致，
     测出来的差异才能明确归因到算法策略而非容器实现差异。
*/
struct HeapNode{
    long long val;// 当前配对的实付价格 p[r]-c[col]，作为小根堆排序键
    int r;// 排序后商品价格数组的下标（行号）
    int col;// 排序后优惠券面值数组的下标（列号）
};

class MinHeap{
private:
    /*
    采用 1-indexed 连续数组存储堆结点（0 号位为哨兵）：
    父结点下标为 idx>>1，左子为 idx<<1，右子为 (idx<<1)|1，
    全部结点连续排布在同一块内存里，缓存局部性远优于指针链表式堆。
    */
    vector<HeapNode> a;

    /*
    向下调整（空穴法）：先把待下沉结点暂存在局部变量 cur 里，
    每层只做一次单向覆盖 a[idx]=a[ch] 而不是三次赋值的 swap，
    最终落位后再写回 a[idx]=cur，减少约三分之二的内存写操作。
    */
    long long siftDown(int idx){
        int n=(int)a.size()-1;
        HeapNode cur=a[idx];
        long long steps=0;
        while((idx<<1)<=n){
            int ch=idx<<1;
            // 在左右孩子中挑出实付价更小的一个，确保替换上去后仍维持父<=子
            if(ch<n&&a[ch+1].val<a[ch].val) ch++;
            steps++;
            // 若当前结点已经不大于较小孩子，说明堆序已恢复，立即停止下沉
            if(cur.val<=a[ch].val) break;
            a[idx]=a[ch];
            idx=ch;
        }
        a[idx]=cur;
        return steps;
    }

    /*
    向上调整（同样采用空穴法）：新结点插入数组末尾后，
    只要父结点键值严格大于 cur.val，就把父结点下拉，直至根或满足堆序。
    */
    long long siftUp(int idx){
        HeapNode cur=a[idx];
        long long steps=0;
        while(idx>1){
            int p=idx>>1;
            steps++;
            // 父结点已经 <= cur 时提前结束，避免无意义继续上溯
            if(a[p].val<=cur.val) break;
            a[idx]=a[p];
            idx=p;
        }
        a[idx]=cur;
        return steps;
    }

public:
    MinHeap(){
        a.reserve(64);// 预留少量空间，避免极小用例前几次插入反复扩容
        a.push_back({0,0,0});// 写入 0 号哨兵，使根固定在下标 1
    }

    // 预分配容量：消除已知上限下的动态倍增拷贝开销
    void reserve(size_t cap){
        a.reserve(cap+1);
    }

    // 数组中仅剩 0 号哨兵即表示堆空
    bool empty() const{
        return a.size()<=1;
    }

    // 读取堆顶最小元素，调用方需确保非空
    const HeapNode& top() const{
        return a[1];
    }

    /*
    批量装载已经升序排列的初始行首：
    因为对任意 1<=i<j<=N 都有 a[i].val<=a[j].val，父下标 i>>1 恒小于子下标 i，
    故顺序追加进数组后天然满足小根堆不变式，O(N) 零比较零交换。
    */
    void appendSorted(const HeapNode& node){
        a.push_back(node);
    }

    // 常规插入：追加到末尾并上滤，返回比较/搬动步数
    long long push(const HeapNode& node){
        a.push_back(node);
        return siftUp((int)a.size()-1);
    }

    // 常规弹出：当某一行走到最后一列没有后继时调用，把末尾元素移至堆顶后下沉
    long long pop(){
        if(a.size()<=2){
            // 只剩 0 或 1 个有效元素时直接弹尾即可，无需下沉
            if(a.size()==2) a.pop_back();
            return 1;
        }
        a[1]=a.back();
        a.pop_back();
        return siftDown(1);
    }

    /*
    原地替换堆顶并单趟下沉：
    归并主循环中每弹出一个 (r,col) 且 col+1<n 时直接调用本方法，
    比「pop 再 push」节省一次末尾元素上提与一次新元素上滤。
    */
    long long replaceTop(const HeapNode& node){
        a[1]=node;
        return siftDown(1);
    }
};

//---------- 公共排序与预处理 ----------

/*
将商品价格按升序排为 p[0..n-1]，优惠券面值按降序排为 c[0..n-1]。
这样做之后，虚拟二维实付价矩阵 M[i][j] = p[i] - c[j] 同时满足：
  - 同一行内随列 j 增大而单调不降（优惠券面值越来越小，实付越来越贵）；
  - 同一列内随行 i 增大而单调不降（商品标价越来越贵，实付越来越贵）。
传入 const 引用并在内部拷贝排序，是为了防止基准测试中上一个算法排完序影响下一个算法计时。
*/
void sortInputs(const vector<long long>& prices,const vector<long long>& coupons,vector<long long>& p,vector<long long>& c){
    p=prices;
    c=coupons;
    sort(p.begin(),p.end());
    sort(c.begin(),c.end(),greater<long long>());
}

/*
预处理降序优惠券数组的前缀和 prefC[0..n]，满足 prefC[m] = sum_{t=0}^{m-1} c[t]。
在值域二分与矩阵剪枝中，第 i 行前 m 个配对的总实付花费为：
  sum_{t=0}^{m-1} (p[i] - c[t]) = m * p[i] - prefC[m]，
从而能把单行求和从 O(m) 压成 O(1)，保证整张矩阵的单趟判定严格保持 O(N)。
*/
void buildPrefC(const vector<long long>& c,vector<i128>& prefC){
    int n=(int)c.size();
    prefC.assign(n+1,0);
    for(int i=0;i<n;i++) prefC[i+1]=prefC[i]+(i128)c[i];
}

/*
闭式计算全矩阵全部 N^2 个配对的总花费：
  sum_{i=0}^{n-1} sum_{j=0}^{n-1} (p[i] - c[j]) = N * sum(p) - N * sum(c)。
利用该 O(N) 闭式公式可以直接判掉「预算足够买光全部 N^2 件」的边界情形，
避免后续二分或剪枝在边界上做无效迭代，也保证后续存在严格超出预算的临界价位。
*/
i128 calcTotalMatrixCost(const vector<long long>& p,const vector<i128>& prefC){
    int n=(int)p.size();
    i128 sumP=0;
    for(int i=0;i<n;i++) sumP+=(i128)p[i];
    return (i128)n*sumP-(i128)n*prefC[n];
}

//---------- BFPRT 确定性线性加权中位数 ----------
/*
算法 4（强多项式矩阵剪枝）每轮需从至多 N 个活性行中点里选出加权中位数。
为什么不用更短的 std::sort 或 std::nth_element？
  1. 用 std::sort 单轮需 O(m log m)，总时间退化到 O(N log^2 N)；
  2. 用随机 Quickselect 虽然期望 O(m)，但最坏时间无法做到确定性线性；
  3. 本实现采用经典 BFPRT（5-分组中位数的中位数）按元素个数选定划分枢轴 pivot，
     再配合荷兰国旗三路划分并同步统计左段与等值段权重，决定向哪一段收缩。

关键正确性细节：
选枢轴必须走「无权」BFPRT（按元素个数取中位数），绝不能按权重选枢轴；
否则某个权重极大的单元素可能位于值域角落，导致按元素个数划分极不均衡而退化为 O(m^2)。
走无权 BFPRT 选枢轴后，严格小于 pivot 与严格大于 pivot 两段的元素个数均不超过 0.7m，
因此无论权重引导递归进入左段还是右段，总元素个数都按常数比例收缩，
满足递推 T(m) <= T(ceil(m/5)) + T(7m/10) + O(m)，严格最坏线性时间 O(m)。
*/
struct WeightedItem{
    long long val;// 该活性行中点格子的实付价 M[r][mid]
    long long w;// 该行当前未决活性列数 rightCol[r]-leftCol[r]
};

// 仅记录有效元素区间 [l,r]，调用方负责保证范围合法
void insertionSortItems(vector<WeightedItem>& a,int l,int r,long long& ops){
    for(int i=l+1;i<=r;i++){
        WeightedItem key=a[i];// 暂存待插项，避免内层循环每次整结构体交换
        int j=i-1;
        while(j>=l&&a[j].val>key.val){
            a[j+1]=a[j];// 比 key 大的项统一右移一格腾位
            j--;
            ops++;// 记入 BFPRT 内部比较步数，确保 ops 完整反映剪枝总开销
        }
        ops++;// 记入退出 while 的最后一次比较
        a[j+1]=key;
    }
}

// 供无权 BFPRT 对 5 元素小组及基例区排序使用
void insertionSortVals(vector<long long>& a,int l,int r,long long& ops){
    for(int i=l+1;i<=r;i++){
        long long key=a[i];
        int j=i-1;
        while(j>=l&&a[j]>key){
            a[j+1]=a[j];
            j--;
            ops++;
        }
        ops++;
        a[j+1]=key;
    }
}

/*
无权 BFPRT 线性选择：在 a[l..r] 中确定性选出第 k 小的值（0-indexed）。
外层写成迭代循环（尾递归消除）减少栈帧开销；内部必须使用三路划分而非二路划分，
因为当数据存在大量重复价格（如阶梯分布、全相等分布）时，二路划分会让等值元素全落在一侧，
破坏 <=0.7m 的收缩保证，三路划分则能把所有等值元素一次性留在中间段直接命中或剔除。
*/
long long selectUnweightedBFPRT(vector<long long>& a,int l,int r,int k,long long& ops){
    while(true){
        int len=r-l+1;
        if(len<=10){
            // 规模足够小时直接插入排序并按下标取第 k 小
            insertionSortVals(a,l,r,ops);
            return a[l+k];
        }
        // 每 5 个一组排序并取出各组中位数，收集到独立数组 meds 中求中位数的中位数
        int numG=(len+4)/5;
        vector<long long> meds(numG);
        for(int g=0;g<numG;g++){
            int gl=l+g*5;
            int gr=min(gl+4,r);
            insertionSortVals(a,gl,gr,ops);
            meds[g]=a[gl+(gr-gl)/2];
        }
        long long pivot=selectUnweightedBFPRT(meds,0,numG-1,numG/2,ops);

        /*
        荷兰国旗三路划分不变式：
          [l,lt) 严格小于 pivot；[lt,i) 等于 pivot；(gt,r] 严格大于 pivot。
        划分完成后两端的长度均保证不超过 7*len/10 + 6。
        */
        int lt=l,i=l,gt=r;
        while(i<=gt){
            ops++;
            if(a[i]<pivot){
                swap(a[lt],a[i]);
                lt++;
                i++;
            }else if(a[i]>pivot){
                swap(a[i],a[gt]);
                gt--;
            }else{
                i++;
            }
        }
        int leftCnt=lt-l;
        int eqCnt=gt-lt+1;
        if(k<leftCnt){
            // 目标落在左段，收缩右边界
            r=lt-1;
        }else if(k<leftCnt+eqCnt){
            // 目标恰好落在等值段，直接返回 pivot
            return pivot;
        }else{
            // 目标落在右段，扣除左段与等值段个数后收缩左边界
            k-=leftCnt+eqCnt;
            l=gt+1;
        }
    }
}

/*
带权 BFPRT 线性选择：在 a[l..r] 中找出满足
  sum_{val<x} w < targetW  且  sum_{val<=x} w >= targetW
的阈值 x。
调用方取 targetW = (totalW+1)/2 时，返回值 x 同时满足：
  - 所有 val<=x 的行权和 >= totalW/2；
  - 所有 val>=x 的行权和 >= totalW/2。
又因每行取的是活性区间中点（左右两半各至少占该行权重的 1/2），
故全矩阵活性元素中至少有 totalW/4 个 <=x，也至少有 totalW/4 个 >=x，
由此保证矩阵剪枝每轮至少砍掉四分之一的活性元素。
*/
long long selectWeightedBFPRT(vector<WeightedItem>& a,int l,int r,long long targetW,long long& ops){
    while(true){
        int len=r-l+1;
        if(len<=10){
            // 规模极小时直接排序并顺序累加权重定位加权中位数
            insertionSortItems(a,l,r,ops);
            long long acc=0;
            for(int i=l;i<=r;i++){
                acc+=a[i].w;
                if(acc>=targetW) return a[i].val;
            }
            return a[r].val;
        }
        // 用各元素 val 按 5-分组取无权中位数，再递归找中位数的中位数作均衡枢轴
        int numG=(len+4)/5;
        vector<long long> meds(numG);
        for(int g=0;g<numG;g++){
            int gl=l+g*5;
            int gr=min(gl+4,r);
            insertionSortItems(a,gl,gr,ops);
            meds[g]=a[gl+(gr-gl)/2].val;
        }
        long long pivot=selectUnweightedBFPRT(meds,0,numG-1,numG/2,ops);

        // 三路划分并同步累计严格小于 pivot 的权和 leftW 与等于 pivot 的权和 eqW
        int lt=l,i=l,gt=r;
        long long leftW=0,eqW=0;
        while(i<=gt){
            ops++;
            if(a[i].val<pivot){
                leftW+=a[i].w;
                swap(a[lt],a[i]);
                lt++;
                i++;
            }else if(a[i].val>pivot){
                swap(a[i],a[gt]);
                gt--;
            }else{
                eqW+=a[i].w;
                i++;
            }
        }
        if(targetW<=leftW){
            // 加权中位数在严格小于 pivot 的左段内
            r=lt-1;
        }else if(targetW<=leftW+eqW){
            // 跨过 targetW 的正是 pivot 本身，直接返回
            return pivot;
        }else{
            // 扣除左段与等值段总权后进入右段继续寻找
            targetW-=leftW+eqW;
            l=gt+1;
        }
    }
}

}// namespace

//---------- 算法 0：朴素暴力 ----------
/*
直接枚举全部 N^2 个 (商品,优惠券) 组合并全排序，随后按贪心选择性质顺序累加至预算耗尽。
该实现故意不依赖任何单调矩阵、双指针或收尾定理，只直接体现「前 K 小前缀」的定义本身，
用作小规模（N<=4000）对拍的独立真值基准，杜绝「被测算法与参照算法犯同一类错误」。
*/
SolveResult solveBrute(const vector<long long>& prices,const vector<long long>& coupons,long long budget){
    int n=(int)prices.size();
    // 拦截过大规模：N=10^5 时 N^2 个 8 字节整数需 80 GB 内存，会直接耗尽系统内存
    if(n>4000) throw runtime_error("brute solver refused for N>4000 to avoid O(N^2) memory overflow");

    clock_t t0=clock();
    vector<long long> all;
    all.reserve((size_t)n*(size_t)n);// 一次性预留 N^2 空间，避免 push_back 多次倍增重分配
    for(int i=0;i<n;i++){
        for(int j=0;j<n;j++) all.push_back(prices[i]-coupons[j]);
    }
    sort(all.begin(),all.end());
    clock_t t1=clock();

    // 贪心顺序扫前缀：题目保证所有实付价 >=1，遇第一个买不起的即可立即终止
    long long cnt=0,rem=budget;
    for(long long w:all){
        if(w>rem) break;
        rem-=w;
        cnt++;
    }
    clock_t t2=clock();

    SolveResult res{};
    res.count=cnt;
    res.rem=rem;
    res.ops=(long long)n*(long long)n;// 展开并参与排序的配对总数
    res.rounds=cnt;// 实际买下的件数
    res.sortMs=elapsedMs(t0,t1);
    res.solveMs=elapsedMs(t1,t2);
    res.totalMs=elapsedMs(t0,t2);
    return res;
}

//---------- 算法 1：标准 N 路归并最小堆 ----------
/*
将排序后的实付价矩阵视为 N 条各自单调不降的有序序列（第 r 行为 p[r]-c[0..n-1]）。
初始把全部 N 个行首装入最小堆，每轮取出全局最小项扣减预算，再把同行的下一列补回堆顶。
由于每个配对实付价 >=1，最多弹出 K <= min(N^2,D) 次，总时间 O(N log N + K log N)。
缺点在于：即使实际只能买几十件（K<<N），初始阶段依然要把全部 N 个行首装入堆中，
且后续每次下沉的堆高始终维持在 log2(N) 级别。
*/
SolveResult solveHeapFull(const vector<long long>& prices,const vector<long long>& coupons,long long budget){
    int n=(int)prices.size();
    clock_t t0=clock();
    vector<long long> p,c;
    sortInputs(prices,coupons,p,c);
    clock_t t1=clock();

    MinHeap heap;
    heap.reserve((size_t)n);
    /*
    把全部 N 个行首 (p[i]-c[0],i,0) 装入最小堆。
    因为 p 已经升序排列，p[0]-c[0] <= p[1]-c[0] <= ... <= p[n-1]-c[0] 天然满足小根堆堆序，
    按序追加只需 O(N) 线性写入，无需逐个 push 做 O(N log N) 上滤。
    */
    for(int i=0;i<n;i++) heap.appendSorted({p[i]-c[0],i,0});

    long long cnt=0,rem=budget,ops=n,rounds=0;
    while(!heap.empty()){
        HeapNode cur=heap.top();// 取出当前全局最便宜配对，暂存值与坐标以便原地覆写
        // 堆顶是尚未购买的所有配对里最便宜的一件，连它都买不起时即可结束
        if(cur.val>rem) break;
        rem-=cur.val;// 扣减余额：实付价严格为正，rem 严格递减，必然有限步终止
        cnt++;// 累计成功买入件数
        rounds++;// 记录弹出轮次，等价于实际购买件数 K
        if(cur.col+1<n){
            // 同行还有下一张面值更小的优惠券：原地替换堆顶并下沉，省去 pop+push 往返
            ops+=heap.replaceTop({p[cur.r]-c[cur.col+1],cur.r,cur.col+1});
        }else{
            // 该商品已配完全部 N 张优惠券，该路耗尽直接弹出
            ops+=heap.pop();
        }
    }
    clock_t t2=clock();// 求解段计时终点

    SolveResult res{};// 打包结果与工作量指标返回调用方
    res.count=cnt;
    res.rem=rem;
    res.ops=ops;
    res.rounds=rounds;
    res.sortMs=elapsedMs(t0,t1);// 仅含两次排序，不含初始 O(N) 装堆
    res.solveMs=elapsedMs(t1,t2);// 含初始装堆与全部 K 轮弹出调整
    res.totalMs=elapsedMs(t0,t2);
    return res;
}

//---------- 算法 2：前沿懒扩展最小堆 ----------
/*
针对算法 1 在 K<<N 时白白建满 N 结点堆的缺点，改用按需扩展前沿：
仅把左上角全局最小格 (0,0) 放入初始堆，随后按无重生成树规则向外扩展：
  - 每个格 (r,col) 的唯一父亲定义为：col==0 时是 (r-1,0)，col>0 时是 (r,col-1)；
  - 因为行列均单调不降，父亲的实付价必 <= 儿子的实付价，故父亲必先于儿子出堆；
  - 于是仅在出堆时向右推 (r,col+1)，仅当 col==0 时额外向下推 (r+1,0)，
    既覆盖全矩阵又不产生任何重复入堆，无需维护 visited 哈希表。
堆中活跃结点数不超过 min(N,K+1)，核心求解时间降为 O(K log min(N,K))。
*/
SolveResult solveHeapLazy(const vector<long long>& prices,const vector<long long>& coupons,long long budget){
    int n=(int)prices.size();
    clock_t t0=clock();
    vector<long long> p,c;
    sortInputs(prices,coupons,p,c);
    clock_t t1=clock();

    MinHeap heap;
    // 只先预留小块空间，避免 K<<N 时像算法 1 那样立刻分配 O(N) 的堆内存
    heap.reserve((size_t)min(n,1024));
    heap.push({p[0]-c[0],0,0});

    long long cnt=0,rem=budget,ops=1,rounds=0;
    while(!heap.empty()){
        HeapNode cur=heap.top();
        // 堆顶为当前边界前沿的最小值，也是全矩阵剩余未买元素的最小值
        if(cur.val>rem) break;
        rem-=cur.val;
        cnt++;
        rounds++;

        bool hasRight=(cur.col+1<n);
        bool hasDown=(cur.col==0&&cur.r+1<n);
        if(hasRight&&hasDown){
            // 第 0 列元素有两个生成树儿子：先原地覆写推右儿子，再常规 push 下儿子
            ops+=heap.replaceTop({p[cur.r]-c[cur.col+1],cur.r,cur.col+1});
            ops+=heap.push({p[cur.r+1]-c[0],cur.r+1,0});
        }else if(hasRight){
            // 非第 0 列元素只有右儿子，原地替换一次下沉即可
            ops+=heap.replaceTop({p[cur.r]-c[cur.col+1],cur.r,cur.col+1});
        }else if(hasDown){
            // 只有 N==1 时无右有下（实际 N==1 也无下），保留分支以保证结构对称完整
            ops+=heap.replaceTop({p[cur.r+1]-c[0],cur.r+1,0});
        }else{
            // 无任何后继，直接弹出堆顶
            ops+=heap.pop();
        }
    }
    clock_t t2=clock();

    SolveResult res{};
    res.count=cnt;
    res.rem=rem;
    res.ops=ops;
    res.rounds=rounds;
    res.sortMs=elapsedMs(t0,t1);
    res.solveMs=elapsedMs(t1,t2);
    res.totalMs=elapsedMs(t0,t2);
    return res;
}

//---------- 算法 3：值域二分 + 单调双指针 + 前缀和 ----------
/*
两个堆算法的核心瓶颈在于必须逐件弹出，时间复杂度不可避免地带有 Omega(K)：
当题目放宽预算至 D=10^12 或 10^14 时，购买件数 K 可达数亿甚至数十亿，逐件弹堆彻底不可行。
值域二分绕开「逐件枚举」，转而在价格阈值 T 上搜索最大可行全买门槛 T*：
  1. 给定 T，利用矩阵行列单调性，用单调递减指针 j 在 O(N) 内统计所有 M[i][j]<=T 的配对总和；
  2. 二分确定满足 sum(T)<=D 的最大整数 T*（轮数至多 ceil(log2(min(V,D))) 轮）；
  3. 再用重复值收尾定理 O(1) 算出价位等于 T*+1 上能补买的精确件数与余钱。
总时间 O(N log N + N log min(V,D))，空间 O(N)，与实际购买件数 K 完全无关。
*/
SolveResult solveBinarySearch(const vector<long long>& prices,const vector<long long>& coupons,long long budget){
    int n=(int)prices.size();
    clock_t t0=clock();
    vector<long long> p,c;
    sortInputs(prices,coupons,p,c);
    vector<i128> prefC;
    buildPrefC(c,prefC);
    clock_t t1=clock();

    long long ops=0,rounds=0;
    long long loVal=p[0]-c[0];// 全局最小实付价
    long long hiVal=p[n-1]-c[n-1];// 全局最大实付价

    // 边界 1：连全局最便宜的一件都买不起，直接返回 0 件、原样保留全部预算
    if(loVal>budget){
        clock_t t2=clock();
        return {0,budget,1,0,elapsedMs(t0,t1),elapsedMs(t1,t2),elapsedMs(t0,t2)};
    }

    // 边界 2：全矩阵 N^2 件的总花费都不超过预算，全部买光后直接返回，保证后续存在临界价位
    i128 totalAll=calcTotalMatrixCost(p,prefC);
    ops+=n;
    if(totalAll<=(i128)budget){
        clock_t t2=clock();
        long long totalCnt=(long long)n*(long long)n;
        long long rem=(long long)((i128)budget-totalAll);
        return {totalCnt,rem,ops,0,elapsedMs(t0,t1),elapsedMs(t1,t2),elapsedMs(t0,t2)};
    }

    /*
    单调双指针可行性判定：检查全矩阵所有 <=T 的配对总花费是否不超过 budget。
    随行号 i 递增，p[i] 单调不降，故满足 p[i]-c[j]<=T 的最大列号 j 单调不增，
    j 在整趟循环里最多左移 N 次，整体严格 O(N)。
    两条提前终止不改变结果但显著压低常数：
      1. 若 p[i]-c[0]>T，说明第 i 行及之后所有行的最小实付价都已超过 T，直接 break；
      2. 若累计花费已严格大于 budget，不必再扫后面的行，直接返回 false。
    */
    /*
    双指针扫描与早停的精确实现：
    j 从最右列 n-1 开始随行号 i 单调左移，整个函数内 while 最多执行 n 次，
    因此单次判定严格 O(N) 时间、O(1) 额外空间。
    */
    auto checkBudget=[&](long long T)->bool{
        int j=n-1;// 当前行允许到达的最右列下标，随 i 递增单调不增
        i128 sum=0;// 用 128 位累加防大值域或高预算下溢出
        for(int i=0;i<n;i++){
            if(p[i]-c[0]>T){
                ops++;// 记入导致提前终止的判定比较
                break;// 第 i 行连最大优惠券都超阈值 T，后续行更贵，直接退出
            }
            while(j>=0&&p[i]-c[j]>T){
                j--;// 当前列实付价超过 T，向左回退寻找更大面值优惠券
                ops++;
            }
            ops++;// 记入每行结束回退时的最后一次比较
            long long m=j+1;// 第 i 行满足实付价 <=T 的配对共有 0..j 共 j+1 个
            sum+=(i128)m*p[i]-prefC[m];// O(1) 累加整行实付花费
            if(sum>(i128)budget) return false;// 超过预算立即早停，不必继续扫描剩余行
        }
        return true;// 所有 <=T 配对总和均在预算内，阈值 T 可行
    };

    /*
    二分上界可安全收紧到 min(hiVal-1,budget)：
      - 因为上面已排除全买光（sum(hiVal)>budget），所以完全买得起的最大阈值必 <= hiVal-1；
      - 若矩阵价格在 budget 附近有大断层，使 bestT 卡在上界 budget 上，
        则所有 <=budget 的配对已全部买入，剩余未买配对价格均 >= budget+1 > remBefore，
        后续收尾公式 extra = remBefore/(budget+1) 恰好算得 0，结果依然严格正确。
    注意 bestT 初值设为 loVal-1，以覆盖「连最小价位 loVal 都不能全部买完（重复很多）」的情形：
    此时 bestT 保持 loVal-1，扫描得 cnt=0,sum=0，收尾公式自动按 nextPrice=loVal 整除计算。
    */
    long long lo=loVal;// 搜索左端点：最小合法实付价
    long long hi=min(hiVal-1,budget);// 搜索右端点：收紧至不超过预算，把轮数压进 log2(min(V,D))
    long long bestT=loVal-1;// 记录目前确认可行的最大全买阈值
    while(lo<=hi){
        rounds++;// 统计二分判定轮数，用于验证 O(log min(V,D)) 阶
        // 用 lo+((hi-lo)>>1) 防 (lo+hi) 溢出 64 位整数（大值域实验里 hi 可达 10^16）
        long long mid=lo+((hi-lo)>>1);
        if(checkBudget(mid)){
            bestT=mid;// mid 可行，尝试向右探寻更高阈值
            lo=mid+1;
        }else{
            hi=mid-1;// mid 超预算，收缩上界
        }
    }

    // 对最终锁定的最大可行阈值 bestT 做一次完整扫描，取回精确配对数 cnt 与总花费 sum
    long long cnt=0;
    i128 sum=0;
    if(bestT>=loVal){
        int j=n-1;// 复用同一套单调双指针逻辑，保证与判定口径完全一致
        for(int i=0;i<n;i++){
            if(p[i]-c[0]>bestT){
                ops++;
                break;// 后续行无 <=bestT 配对，提前结束
            }
            while(j>=0&&p[i]-c[j]>bestT){
                j--;
                ops++;
            }
            ops++;
            long long m=j+1;// 第 i 行全部买入的件数
            cnt+=m;// 累加已确定买入件数
            sum+=(i128)m*p[i]-prefC[m];// 累加已确定花费
        }
    }

    /*
    重复值收尾定理：
    设 q 为矩阵中价格恰好等于 bestT+1 的配对个数。
    在 bestT < min(hiVal-1,budget) 或 hi==hiVal-1 时，必有 sum(bestT+1) > budget，
    即 q*(bestT+1) > budget - sum(bestT) = remBefore，故整数 q >= floor(remBefore/(bestT+1)) + 1。
    这保证了价格为 bestT+1 的配对必然足够买下 extra = floor(remBefore/(bestT+1)) 件，
    买完后余额 remBefore % (bestT+1) < bestT+1，再也买不起任何剩余配对。
    因此无需二次扫描统计 q，O(1) 整除取模即可收尾。
    */
    long long remBefore=(long long)((i128)budget-sum);
    long long nextPrice=bestT+1;
    long long extra=remBefore/nextPrice;
    long long finalRem=remBefore%nextPrice;
    cnt+=extra;
    clock_t t2=clock();

    SolveResult res{};
    res.count=cnt;
    res.rem=finalRem;
    res.ops=ops;
    res.rounds=rounds;
    res.sortMs=elapsedMs(t0,t1);
    res.solveMs=elapsedMs(t1,t2);
    res.totalMs=elapsedMs(t0,t2);
    return res;
}

//---------- 算法 4：强多项式矩阵加权中位数剪枝 ----------
/*
算法 3 虽已摆脱对 K 的依赖，但其轮数 O(log min(V,D)) 仍依赖数值大小（弱多项式）：
当价格跨度扩展到高精度大数甚至任意实数时，值域二分会变慢甚至不可用。
算法 4 借鉴 Johnson & Mizoguchi (1978) 在 X+Y 矩阵中选第 k 小的思路，将其改造为预算约束版：
  1. 维护每行未决列区间 [leftCol[r],rightCol[r]) 与活性行表 activeRows；
  2. 每轮对各活性行取区间中点 M[r][mid]，按该行活性长度 w 赋权，用 BFPRT 在 O(m) 求出加权中位数 x；
  3. 用双指针在活性行上 O(m) 算出全矩阵 <x 与 <=x 的精确总花费 sumLt、sumLe：
       - 若 sumLt > budget：临界价必 <x，淘汰所有 >=x 的活性元素（至少占总数 1/4）；
       - 若 sumLe <= budget：所有 <=x 必买，并入确定区并淘汰所有 <=x 的活性元素（至少占总数 1/4）；
       - 若 sumLt <= budget < sumLe：直接命中唯一临界价 x，整除收尾返回！
因为每轮至少淘汰 1/4 活性元素，总轮数严格 <= log_{4/3}(N^2) ≈ 4.82 log2(N)，
配合 O(m) 的 BFPRT 与双指针，最坏时间复杂度严格为 O(N log N)，完全与 V、D、K 无关。
*/
SolveResult solvePruneSearch(const vector<long long>& prices,const vector<long long>& coupons,long long budget){
    int n=(int)prices.size();
    clock_t t0=clock();
    vector<long long> p,c;
    sortInputs(prices,coupons,p,c);
    vector<i128> prefC;
    buildPrefC(c,prefC);
    clock_t t1=clock();

    long long ops=0,rounds=0;
    // 边界 1：最便宜的一件都买不起，直接返回 0 件
    if(p[0]-c[0]>budget){
        clock_t t2=clock();
        return {0,budget,1,0,elapsedMs(t0,t1),elapsedMs(t1,t2),elapsedMs(t0,t2)};
    }

    // 边界 2：全部 N^2 件总花费不超过预算，闭式直接返回，确保后续必存在跨预算临界价
    i128 totalAll=calcTotalMatrixCost(p,prefC);
    ops+=n;
    if(totalAll<=(i128)budget){
        clock_t t2=clock();
        long long totalCnt=(long long)n*(long long)n;
        long long rem=(long long)((i128)budget-totalAll);
        return {totalCnt,rem,ops,0,elapsedMs(t0,t1),elapsedMs(t1,t2),elapsedMs(t0,t2)};
    }

    /*
    维护三段划分不变式：存在严格开区间 (a,b) 使得
      - [0,leftCol[r]) 恰为第 r 行 <=a 的元素，全矩阵 <=a 总花费 <= budget；
      - [rightCol[r],n) 恰为第 r 行 >=b 的元素，全矩阵 <b 总花费 > budget；
      - [leftCol[r],rightCol[r]) 恰为第 r 行落在 (a,b) 内的活性未决元素。
    当某行收缩到 leftCol[r]==rightCol[r] 时，该行对所有后续枢轴 x in (a,b) 的贡献恒定为前 leftCol[r] 列，
    立即结算进 fixedCnt / fixedSum 并踢出 activeRows，后续每轮只需在剩余活性行上操作。
    */
    vector<int> leftCol(n,0),rightCol(n,n);
    vector<int> activeRows(n);
    for(int i=0;i<n;i++) activeRows[i]=i;

    long long fixedCnt=0;
    i128 fixedSum=0;

    // 复用的工作缓冲区：放在循环外预分配，避免每轮重新向堆内存申请释放
    vector<WeightedItem> items;
    vector<int> p1,p2,nextActive;
    items.reserve(n);
    p1.reserve(n);
    p2.reserve(n);
    nextActive.reserve(n);

    long long finalCnt=0,finalRem=budget;
    while(!activeRows.empty()){
        rounds++;
        items.clear();
        long long totalW=0;
        for(int r:activeRows){
            int w=rightCol[r]-leftCol[r];
            // 取区间右中点 leftCol+w/2：无论 w 奇偶，左右两半（含中点）均至少含 ceil(w/2)>=w/2 个元素
            int mid=leftCol[r]+(w>>1);
            items.push_back({p[r]-c[mid],(long long)w});
            totalW+=w;
        }
        ops+=(long long)activeRows.size();

        // BFPRT 确定性线性时间求加权中位数，确保本轮淘汰 >= totalW/4 个活性元素
        long long targetW=(totalW+1)>>1;
        long long x=selectWeightedBFPRT(items,0,(int)items.size()-1,targetW,ops);

        /*
        在活性行上单调双指针统计全矩阵严格 <x 与 <=x 的花费：
        因为 activeRows 保持行号严格递增，且 leftCol[r]、rightCol[r] 均随行号单调不增，
        所以上一行的分界列 j1、j2 必然大于等于下一行的真实分界列，直接继承单调左移不会漏扫。
        */
        int m=(int)activeRows.size();
        p1.resize(m);
        p2.resize(m);
        long long cntLt=fixedCnt;
        i128 sumLt=fixedSum,sumLe=fixedSum;

        int j1=rightCol[activeRows[0]];
        int j2=rightCol[activeRows[0]];
        for(int idx=0;idx<m;idx++){
            int r=activeRows[idx];
            int loC=leftCol[r],hiC=rightCol[r];
            // 利用本行右界直接截断，跳过已经淘汰的右侧列
            if(j1>hiC) j1=hiC;
            while(j1>loC&&p[r]-c[j1-1]>=x){
                j1--;
                ops++;
            }
            if(j2>hiC) j2=hiC;
            while(j2>loC&&p[r]-c[j2-1]>x){
                j2--;
                ops++;
            }
            ops++;
            p1[idx]=j1;
            p2[idx]=j2;
            cntLt+=j1;
            sumLt+=(i128)j1*p[r]-prefC[j1];
            sumLe+=(i128)j2*p[r]-prefC[j2];
        }

        if(sumLt>(i128)budget){
            // 分支 A：连严格小于 x 的都超预算，说明临界价严格小于 x，淘汰所有 >=x 的元素
            nextActive.clear();
            for(int idx=0;idx<m;idx++){
                int r=activeRows[idx];
                rightCol[r]=p1[idx];
                if(leftCol[r]==rightCol[r]){
                    // 该行未决区间缩空，永久结算进 fixedCnt / fixedSum
                    int keep=leftCol[r];
                    fixedCnt+=keep;
                    fixedSum+=(i128)keep*p[r]-prefC[keep];
                }else{
                    nextActive.push_back(r);
                }
            }
            activeRows.swap(nextActive);
        }else if(sumLe<=(i128)budget){
            // 分支 B：所有 <=x 的元素总花费都不超预算，全部确定买入并淘汰所有 <=x 的活性元素
            nextActive.clear();
            for(int idx=0;idx<m;idx++){
                int r=activeRows[idx];
                leftCol[r]=p2[idx];
                if(leftCol[r]==rightCol[r]){
                    // 该行未决区间缩空，永久结算进 fixedCnt / fixedSum
                    int keep=leftCol[r];
                    fixedCnt+=keep;
                    fixedSum+=(i128)keep*p[r]-prefC[keep];
                }else{
                    nextActive.push_back(r);
                }
            }
            activeRows.swap(nextActive);
        }else{
            // 分支 C：恰好命中唯一跨过预算 D 的临界价格 x（sumLt <= budget < sumLe），直接整除收尾
            long long remBefore=(long long)((i128)budget-sumLt);
            long long extra=remBefore/x;
            finalCnt=cntLt+extra;
            finalRem=remBefore%x;
            clock_t t2=clock();
            return {finalCnt,finalRem,ops,rounds,elapsedMs(t0,t1),elapsedMs(t1,t2),elapsedMs(t0,t2)};
        }
    }

    // 理论防线：排除全买光后必然先命中分支 C，此处兜底返回全部已结算买入
    finalCnt=fixedCnt;
    finalRem=(long long)((i128)budget-fixedSum);
    clock_t t2=clock();
    return {finalCnt,finalRem,ops,rounds,elapsedMs(t0,t1),elapsedMs(t1,t2),elapsedMs(t0,t2)};
}

//---------- 统一定时包装器 ----------
/*
把单次执行包装成 repeat 次连续测量：
  1. 每次重复都传入未经修改的原始 prices/coupons，避免第二趟起排序面对已排好序的数组；
  2. 外层用整段 repeat 轮的总 clock() 除以 repeat，压低 1 ms 时钟粒度的截断误差；
  3. 再按内层累计的 sortMs 与 solveMs 比例拆回两段耗时，使 sortMs + solveMs == totalMs。
*/
SolveResult runTimed(const string& algo,const vector<long long>& prices,const vector<long long>& coupons,long long budget,int repeat){
    if(repeat<1) repeat=1;
    SolveResult last{};
    double sumSort=0.0,sumSolve=0.0;
    clock_t wall0=clock();

    for(int it=0;it<repeat;it++){
        if(algo=="brute") last=solveBrute(prices,coupons,budget);
        else if(algo=="heap_full") last=solveHeapFull(prices,coupons,budget);
        else if(algo=="heap_lazy") last=solveHeapLazy(prices,coupons,budget);
        else if(algo=="bsearch") last=solveBinarySearch(prices,coupons,budget);
        else if(algo=="prune") last=solvePruneSearch(prices,coupons,budget);
        else throw runtime_error("unknown algorithm: "+algo);
        sumSort+=last.sortMs;
        sumSolve+=last.solveMs;
    }

    clock_t wall1=clock();
    double avgTotal=elapsedMs(wall0,wall1)/repeat;
    double innerSum=sumSort+sumSolve;
    if(innerSum>1e-9){
        // 按内层实测占比缩放到外层平滑总时间，保持分段和与总耗时一致
        double scale=avgTotal/(innerSum/repeat);
        last.sortMs=(sumSort/repeat)*scale;
        last.solveMs=(sumSolve/repeat)*scale;
    }else{
        // 极小用例连 repeat 次总耗时仍不足 1 个时钟滴答，避免人为造出虚假比例
        last.sortMs=0.0;
        last.solveMs=0.0;
    }
    last.totalMs=avgTotal;
    return last;
}
