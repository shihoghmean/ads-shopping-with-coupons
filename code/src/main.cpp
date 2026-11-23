#include"solvers.h"

#include<algorithm>
#include<cstdio>
#include<cstdlib>
#include<cstring>
#include<iostream>
#include<random>
#include<string>
#include<vector>

using namespace std;

/*
主程序只编译出单一可执行文件 coupons.exe，承接四种入口：
  1. 无命令行参数：完全兼容题面标准输入输出，从 stdin 读入并只输出「最大件数 最大余额」；
  2. test：内建全套正确性自检，涵盖题面样例、十类边界用例、300 组随机五算法暴力对拍、
     中大规模四算法对拍以及超大预算/超大值域下的二分与剪枝对拍；
  3. solve <file> [algo]：从文本文件读入单组数据求解；
  4. bench <file> [algo] [repeat] [--csv]：批量计时与工作量统计，供实验脚本汇总画图。

设计原则：
  - 任何一处参数非法、文件打不开、输入违背约束或算法校验不一致，都必须打印明确中文诊断
    并以非零退出码终止，防止上层批处理脚本吞掉异常数据；
  - 将自检折进 test 子命令而不另设 tests/ 目录，保证单可执行文件即可完成评测与验证。
*/

namespace{

/*
自定义 64 位整数快读：
满规模输入 N=10^5 包含 20 万个大整数，std::cin 在不关同步时解析开销偏高，
而课程风格不使用 ios::sync_with_stdio；直接用 fgetc 走 stdio 缓冲区既快又简洁，
同时兼容任意文件流（stdin 或 fopen 打开的数据文件）。
*/
bool readLL(FILE* fp,long long& out){
    int ch=fgetc(fp);
    // 跳过任意空白或换行符，兼容 Windows CRLF 与 Unix LF
    while(ch!=EOF&&(ch<'0'||ch>'9')&&ch!='-') ch=fgetc(fp);
    if(ch==EOF) return false;
    long long sgn=1;
    if(ch=='-'){
        sgn=-1;
        ch=fgetc(fp);
    }
    long long val=0;
    while(ch>='0'&&ch<='9'){
        val=val*10+(ch-'0');
        ch=fgetc(fp);
    }
    out=val*sgn;
    return true;
}

/*
读取并校验一组标准输入实例：
除了检查文件是否截断外，还会主动核验题面核心前提 max(coupons) < min(prices)。
若输入文件被损坏或生成器有误导致出现 <=0 的实付价，宁可当场报错停机，
也不能把错误输入喂进依赖「实付价严格为正」的收尾公式和上界假设里。
*/
bool loadInstance(FILE* fp,vector<long long>& prices,vector<long long>& coupons,long long& budget,string& err){
    long long nLL=0;
    if(!readLL(fp,nLL)||!readLL(fp,budget)){
        err="文件为空或缺少首行 N D";
        return false;
    }
    if(nLL<=0||budget<0){
        err="N 必须为正整数且 D 不能为负";
        return false;
    }
    int n=(int)nLL;
    prices.resize(n);
    coupons.resize(n);
    long long minP=-1,maxC=-1;
    for(int i=0;i<n;i++){
        if(!readLL(fp,prices[i])){
            err="商品价格数量不足 N 个";
            return false;
        }
        if(i==0||prices[i]<minP) minP=prices[i];
    }
    for(int i=0;i<n;i++){
        if(!readLL(fp,coupons[i])){
            err="优惠券面值数量不足 N 个";
            return false;
        }
        if(i==0||coupons[i]>maxC) maxC=coupons[i];
    }
    // 守住题面硬前提：任何一件商品配任何一张优惠券都必须严格付费 >= 1 元
    if(maxC>=minP){
        err="违反题面约束：最高优惠券面值必须严格小于最低商品价格";
        return false;
    }
    return true;
}

// 截掉路径前缀只留文件名：防止绝对路径写进 CSV 结果文件，既简洁也符合匿名要求
string baseName(const string& path){
    size_t pos=path.find_last_of("/\\");
    return pos==string::npos?path:path.substr(pos+1);
}

/*
将命令行算法组名映射为具体求解器列表：
  - all：N<=4000 时五算法全跑，超出时自动跳过暴力避免 80 GB 内存溢出；
  - fast：除暴力外四个多项式/近线性算法；
  - nologk：仅跑与 K 无关的 bsearch 与 prune，用于 D>=10^12 的超大预算拓展实验。
*/
vector<string> expandAlgos(const string& name,int n){
    if(name=="all"){
        if(n<=4000) return {"brute","heap_full","heap_lazy","bsearch","prune"};
        return {"heap_full","heap_lazy","bsearch","prune"};
    }
    if(name=="fast") return {"heap_full","heap_lazy","bsearch","prune"};
    if(name=="nologk") return {"bsearch","prune"};
    if(name=="heaps") return {"heap_full","heap_lazy"};
    return {name};
}

//---------- 内建正确性自检 ----------

// 单条固定测试案例：同时携带期望答案与允许运行的算法范围
struct FixedCase{
    string name;// 用例编号与测试目的标签
    vector<long long> prices;// 商品价格表
    vector<long long> coupons;// 优惠券面值表
    long long budget;// 预算 D
    long long expCnt;// 期望最大件数
    long long expRem;// 期望最大余额
    bool runBrute;// 是否同时运行暴力基准
    bool runHeaps;// 是否同时运行两种堆算法
};

/*
通用对拍检查器：
在给定输入上依次运行所有待测算法，若提供了人工/暴力期望值则逐个比对期望值，
否则以列表中第一个算法（小规模时即独立暴力 brute）作为真值基准逐项比对。
任何一对 (count,rem) 不符立刻输出冲突详情并返回 false。
*/
bool checkCase(const string& tag,const vector<long long>& prices,const vector<long long>& coupons,long long budget,
               bool withBrute,bool withHeaps,bool hasExp,long long expCnt,long long expRem){
    vector<string> algos;
    if(withBrute) algos.push_back("brute");
    if(withHeaps){
        algos.push_back("heap_full");
        algos.push_back("heap_lazy");
    }
    algos.push_back("bsearch");
    algos.push_back("prune");

    long long refCnt=expCnt,refRem=expRem;
    bool hasRef=hasExp;
    string refAlgo=hasExp?"expected":"";

    for(const string& algo:algos){
        SolveResult res=runTimed(algo,prices,coupons,budget,1);
        if(!hasRef){
            refCnt=res.count;
            refRem=res.rem;
            refAlgo=algo;
            hasRef=true;
        }else if(res.count!=refCnt||res.rem!=refRem){
            printf("[失败] %s: %s 得到 (%lld,%lld)，与 %s 的 (%lld,%lld) 不一致\n",
                   tag.c_str(),algo.c_str(),res.count,res.rem,refAlgo.c_str(),refCnt,refRem);
            return false;
        }
    }
    return true;
}

/*
四层递进正确性自检（无外部文件依赖，随机种子固定为 20261123 保证完全可复现）：
  1. 10 组手工构造的边界用例：覆盖题面样例、零买、全买、全等平地、价格断层与大数防溢出；
  2. 300 组随机小规模五算法全对拍：以独立暴力 brute 为真值，涵盖均匀、高重复、大断层、10^9 值域；
  3. 30 组中大规模四算法对拍（N 到 3000）：检验堆、二分、剪枝在更大规模下无边界偏差；
  4. 20 组超大预算/超大值域对拍（D 与值域到 10^15）：验证 __int128 与 BFPRT 在极限尺度下逐位一致。
*/
int runSelfTest(){
    vector<FixedCase> fixed={
        {"T01_sample",{12,20,15,10},{9,6,8,7},30,8,2,true,true},// 题面官方样例
        {"T02_single_exact",{100},{99},1,1,0,true,true},// N=1 最小规模且预算恰好清零
        {"T03_zero_buy",{10,12,15},{5,4,3},4,0,4,true,true},// 最便宜商品也买不起，结果应为 0 件
        {"T04_buy_all",{10,12},{9,8},100,4,90,true,true},// 预算充裕买光全部 N^2=4 件
        {"T05_all_equal_partial",{10,10,10,10},{7,7,7,7},20,6,2,true,true},// 全等平地且不能整除
        {"T06_all_equal_exact",{10,10,10},{7,7,7},27,9,0,true,true},// 全等平地且恰好买光全矩阵
        {"T07_price_gap_over_D",{10,1000},{9,8},100,2,97,true,true},// 巨大价格断层，专门检验二分上界收紧到 D
        {"T08_tie_at_threshold",{10,11,12},{9,8,7},10,4,2,true,true},// 临界价位 3 出现三次但只够买其中一件
        {"T09_large_vals_1e9",{1000000000LL,999999999LL,999999998LL},{999999997LL,999999996LL,999999995LL},10,4,2,true,true},// 贴近 10^9 上限验证 64 位无符号/有符号差值
        {"T10_huge_budget_1e12",{100000000LL,100000001LL,100000005LL},{99999999LL,99999998LL,99999990LL},1000000000000LL,9,999999999943LL,true,true}// 超大预算 10^12 检验 64 位余额与闭式总花费
    };

    for(const FixedCase& tc:fixed){
        if(!checkCase(tc.name,tc.prices,tc.coupons,tc.budget,tc.runBrute,tc.runHeaps,true,tc.expCnt,tc.expRem)){
            return 1;
        }
    }

    // 固定种子随机生成器：确保任意环境复跑测试覆盖同一批压力分布
    mt19937_64 rng(20261123ULL);
    /*
    第二部分：300 组随机小规模五算法全对拍（N in [1,80]）。
    为什么刻意轮换四类分布而不是只做单一均匀随机？
      - 单纯均匀随机几乎撞不上「临界价大量重复」与「断层跨过预算 D」，
        而这两处恰恰是值域二分收尾公式与 BFPRT 三路划分最容易写错的边界；
      - 与独立暴力 brute 逐项比对，可彻底排除四个高效算法共同犯错的可能。
    */
    for(int tc=1;tc<=300;tc++){
        int n=(int)(rng()%80)+1;// 覆盖 N=1 到 80，兼顾极小退化与中小规模矩阵
        int mode=tc%4;// 四类分布轮换，避免纯均匀随机漏掉重复值与断层分支
        vector<long long> prices(n),coupons(n);
        long long baseC=0,spanC=0,gap=1,spanP=0,budget=0;
        if(mode==0){
            // 常规均匀分布：模拟一般随机测试点，验证平均形态下的全流程一致性
            baseC=1; spanC=500; gap=1+(long long)(rng()%20); spanP=500;
            budget=(long long)(rng()%50000)+1;
        }else if(mode==1){
            // 极小值域高重复分布：高频触发临界价重复值收尾与 BFPRT 三路等值段
            baseC=1; spanC=5; gap=1; spanP=5;
            budget=(long long)(rng()%3000)+1;
        }else if(mode==2){
            // 价格断层分布：测试二分上界收紧到 D 与行循环提前 break
            baseC=1; spanC=100; gap=1; spanP=100000;
            budget=(long long)(rng()%2000)+1;
        }else{
            // 贴近 10^9 值域上限，验证不出现隐式 32 位截断
            baseC=900000000LL; spanC=50000; gap=1+(long long)(rng()%10); spanP=50000;
            budget=(long long)(rng()%1000000)+1;
        }
        long long maxC=0;// 先生成优惠券并记最大面值，再在其上抬高商品价格
        for(int i=0;i<n;i++){
            coupons[i]=baseC+(long long)(rng()%spanC);
            if(coupons[i]>maxC) maxC=coupons[i];
        }
        // 所有商品价格从 maxC+gap 起生成，天然满足 max(c) < min(p)
        long long minBaseP=maxC+gap;
        for(int i=0;i<n;i++){
            // 断层模式下故意令 20% 商品紧贴底价、其余大幅跳升，制造宽空隙
            if(mode==2&&i%5==0) prices[i]=minBaseP+(long long)(rng()%10);
            else prices[i]=minBaseP+(long long)(rng()%spanP);
        }
        if(tc%15==0) budget=0;// 周期性注入零预算，测试「买 0 件」分支
        if(tc%23==0) budget=1000000000000LL;// 周期性注入超大预算，测试「全矩阵买光」分支

        // 任一组小规模随机用例出现分歧即刻停止并报错，避免掩盖首错现场
        if(!checkCase("small_rand_"+to_string(tc),prices,coupons,budget,true,true,false,0,0)){
            return 1;
        }
    }

    /*
    第三部分：30 组中大规模四算法对拍（N in [500,3000], D<=10^6）。
    跳过 brute 是因为 N=3000 时 N^2=9e6 反复排序耗时过长；
    保留两种堆与二分、剪枝互相对拍，可重点覆盖动态堆扩容、多层 BFPRT 递归与活性行剔除。
    */
    for(int tc=1;tc<=30;tc++){
        int n=500+(int)(rng()%2501);// 覆盖 500 到 3000 的中大规模区间
        vector<long long> prices(n),coupons(n);
        long long maxC=0;
        for(int i=0;i<n;i++){
            coupons[i]=1+(long long)(rng()%10000);// 保持万级跨度以便产生中等规模购买量 K
            if(coupons[i]>maxC) maxC=coupons[i];
        }
        for(int i=0;i<n;i++){
            prices[i]=maxC+1+(long long)(rng()%10000);// 确保最小实付价 >= 1
        }
        long long budget=(long long)(rng()%1000000)+1;// 覆盖题面 10^6 预算上限范围
        if(!checkCase("mid_rand_"+to_string(tc),prices,coupons,budget,false,true,false,0,0)){
            return 1;
        }
    }

    /*
    第四部分：20 组超大预算与超大值域对拍（D 最高 10^15，单价跨度最高 10^12）。
    在此尺度下 K 可达数百万以上，逐件弹堆已极慢，专门校验 bsearch 与 prune 两类独立范式：
      - 若 64 位整数在 mid=(lo+hi)/2 或前缀和累计中发生静默溢出，两算法分支不同极易暴露分歧；
      - 偶数用例侧重超大值域，奇数用例侧重超大购买件数 K。
    */
    for(int tc=1;tc<=20;tc++){
        int n=2000;// 固定 2000 行使矩阵共有 400 万配对，足以测透百万级 K 与大值域
        vector<long long> prices(n),coupons(n);
        long long span=(tc%2==0)?1000000000000LL:1000000LL;// 交替切换 10^12 大值域与 10^6 常规值域
        long long maxC=0;
        for(int i=0;i<n;i++){
            coupons[i]=1+(long long)(rng()%span);
            if(coupons[i]>maxC) maxC=coupons[i];
        }
        for(int i=0;i<n;i++){
            prices[i]=maxC+1+(long long)(rng()%span);
        }
        long long budget=(tc%2==0)?1000000000000000LL:100000000000LL;// 与对应值域匹配以落在矩阵内部
        if(!checkCase("huge_rand_"+to_string(tc),prices,coupons,budget,false,false,false,0,0)){
            return 1;
        }
    }

    printf("自检通过: 10 组固定边界用例 + 300 组随机五算法暴力对拍 + 30 组中大规模四算法对拍 + 20 组超大预算/值域对拍全部一致\n");
    return 0;
}

// 命令行帮助信息：保持简洁，完整说明留在 code/Readme.txt
void printUsage(){
    printf("用法:\n");
    printf("  coupons                                      标准输入输出模式（默认使用 bsearch）\n");
    printf("  coupons test                                 运行全套内置正确性自检与对拍\n");
    printf("  coupons solve <file> [algo]                  读文件求解（algo: brute|heap_full|heap_lazy|bsearch|prune|all）\n");
    printf("  coupons bench <file> [algo] [repeat] [--csv] 计时与工作量统计（algo 另支持 fast|nologk|heaps）\n");
}

}// namespace

int main(int argc,char** argv){
    /*
    无参数时严格遵循 PTA 题面交互协议：
    从 stdin 读取单一测试用例，用实测综合最快且不依赖 K 的 bsearch 求解，
    仅向 stdout 打印「最大件数 最大余额」两个空格分隔的整数，不附带任何多余提示文字。
    */
    if(argc==1){
        vector<long long> prices,coupons;
        long long budget=0;
        string err;
        if(!loadInstance(stdin,prices,coupons,budget,err)){
            fprintf(stderr,"[错误] %s\n",err.c_str());
            return 1;
        }
        SolveResult res=solveBinarySearch(prices,coupons,budget);
        printf("%lld %lld\n",res.count,res.rem);
        return 0;
    }

    string cmd=argv[1];
    if(cmd=="test"){
        return runSelfTest();
    }

    if(cmd=="solve"||cmd=="bench"){
        /*
        solve 与 bench 共用一套文件读取、算法展开与一致性校验管线：
        两者只在输出格式与默认算法组上不同，共用解析路径可防止一边漏校验。
        */
        if(argc<3){
            printUsage();// 缺少必选输入文件路径时打印用法并报错退出
            return 1;
        }
        string path=argv[2];
        string algoArg=(cmd=="solve")?"bsearch":"fast";// solve 默认单跑最快解，bench 默认四算法对照
        int repeat=1;// 默认单次执行，短用例由外部显式提高以平滑时钟误差
        bool csvMode=false;// 默认人类可读排版，加 --csv 后切为严格 12 列逗号分隔

        // 解析可选参数：允许 --csv 放在参数列表任意位置，便于脚本拼接命令
        for(int i=3;i<argc;i++){
            string tok=argv[i];
            if(tok=="--csv"){
                csvMode=true;
            }else if(i==3){
                algoArg=tok;// 第 3 个位置非 --csv 时视作算法或算法组名
            }else{
                repeat=atoi(tok.c_str());
                if(repeat<1) repeat=1;// 防止非法或非正重复次数导致除零
            }
        }

        // 以二进制只读打开：避免 Windows 文本模式对 CRLF 做额外转换影响 fgetc 吞吐
        FILE* fp=fopen(path.c_str(),"rb");
        if(!fp){
            fprintf(stderr,"[错误] 无法打开输入文件 %s\n",path.c_str());
            return 1;
        }
        vector<long long> prices,coupons;
        long long budget=0;
        string err;
        bool ok=loadInstance(fp,prices,coupons,budget,err);
        fclose(fp);// 读取完立即关句柄，不把文件描述符留到漫长的计算阶段
        if(!ok){
            fprintf(stderr,"[错误] 读取 %s 失败: %s\n",path.c_str(),err.c_str());
            return 1;
        }

        vector<string> algos=expandAlgos(algoArg,(int)prices.size());
        long long refCnt=-1,refRem=-1;// 记录组内首个算法的输出作为一致性参照
        string refName;
        string cname=baseName(path);

        try{
            for(const string& algo:algos){
                SolveResult res=runTimed(algo,prices,coupons,budget,repeat);
                // 同一输入下所有被测算法必须互相校验一致，防止带病基准进入 CSV
                if(refName.empty()){
                    refCnt=res.count;
                    refRem=res.rem;
                    refName=algo;
                }else if(res.count!=refCnt||res.rem!=refRem){
                    fprintf(stderr,"[错误] 校验不一致: %s=(%lld,%lld) 与 %s=(%lld,%lld)\n",
                            algo.c_str(),res.count,res.rem,refName.c_str(),refCnt,refRem);
                    return 1;
                }

                if(cmd=="solve"&&algos.size()==1){
                    // 单算法 solve 与标准 stdin 模式保持同样两整数输出，方便脚本比对 .out
                    printf("%lld %lld\n",res.count,res.rem);
                }else if(csvMode){
                    // CSV 输出不含表头，便于上层批处理脚本把多组文件结果拼接进同一张表
                    printf("%s,%s,%zu,%lld,%lld,%lld,%d,%.4f,%.4f,%.4f,%lld,%lld\n",
                           algo.c_str(),cname.c_str(),prices.size(),budget,res.count,res.rem,repeat,
                           res.sortMs,res.solveMs,res.totalMs,res.ops,res.rounds);
                }else{
                    // 终端交互视图：同时给出分段耗时与工作量指标，便于现场人工排查
                    printf("%-10s count=%-12lld rem=%-12lld total=%8.4f ms (sort=%7.4f, solve=%7.4f) ops=%-12lld rounds=%lld\n",
                           algo.c_str(),res.count,res.rem,res.totalMs,res.sortMs,res.solveMs,res.ops,res.rounds);
                }
            }
        }catch(const exception& e){
            // 捕获未知算法名或 brute 超 N>4000 保护异常，打印错误并返回 1
            fprintf(stderr,"[错误] %s\n",e.what());
            return 1;
        }

        // 多算法交互模式末尾追加汇总校验行，--csv 下不输出免得破坏列结构
        if(!csvMode&&algos.size()>1){
            printf("校验一致: %zu 个算法的结果均为 count=%lld, rem=%lld\n",algos.size(),refCnt,refRem);
        }
        return 0;
    }

    printUsage();
    return 1;
}
