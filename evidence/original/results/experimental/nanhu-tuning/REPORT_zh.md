本次搜索得到的最高已验证中位数为 **229.531 cycles/s**，配置是 `nu-aligned-all-s1`。范围为 228.820–230.501 cycles/s。它在本轮同核重复比较中是 OU 的 **2.030 倍**，吞吐为同源 Verilator 的 **25.18%**。

这里的“最高”限定为本报告实际搜索的**单线程 CPU 内核、展开因子、编译选项和内存布局**，使用固定 Nanhu tensor 与 `checked-loop-1000.bin`。没有证明其他算法、硬件、工作负载或更大参数空间不存在更快实现。接近的配置需要结合样本范围判断；不能把最高一次样本当作稳定最大值。

第二名为 `psu8-aligned-all-s1`，中位数差 0.569%，两组样本范围重叠。样本范围不是置信区间；若差异很小且范围重叠，当前证据支持它们处于同一性能水平，不能认定某个内核在共享主机上唯一、稳定地最优。

**固定设计与测量口径。**

- XiangShan Nanhu `0a68ebc13b3d96a78d8663679393ecbea5b8fb73`，升级 Chisel 3.6.1 前的提交。单核 `DefaultConfig`，真实 L1/L2/L3；没有缩小目标 RTL。
- Chisel 实际依赖 3.5.1 / FIRRTL 1.5.1；RTeAAL 编译端 FIRRTL 1.5.6。沿用先前已检查的时钟、外设和宽层实验适配。
- tensor：2,096,953 个操作、1,036 层、461,324 个数据槽。运算数量和每周期执行范围保持一致。
- node007，AMD EPYC 7773X。最终样本都绑定 CPU 91，SMT sibling 为 219；同一时刻只运行一个本实验的测量进程。最终比较中的 sibling 忙碌比例为 5.20%–9.19%；共享主机仍有其他负载。
- Clang 19.1.1，`-std=c++17 -O3 -march=native`。普通 NU/PSU 在启动时验证 scatter 索引后使用无边界检查的存取。OU 对照保留原检查方式和普通编译选项，反映本轮整个优化组合的收益，不能把比例全部归因于一种改动。
- 每次完整运行共 10,644 周期、8,011 条退休指令；首退休为第 630 周期。测量其后的 10,013 周期，排除 tensor 加载、复位、oracle、trace 和状态哈希，包含启动/cache 停顿与每周期计时开销；不是纯热循环稳态。

**最终交替比较。**

候选配置按预先设置的轮次顺序轮换；每个配置运行 3 次完整程序，保留全部样本。原始顺序、时间戳、CPU 使用率与二进制 SHA-256 见 [final-comparison.json](final-comparison.json)。

| 配置 | 各轮 cycles/s | 中位数 cycles/s | 最小–最大 cycles/s |
| --- | --- | ---: | ---: |
| NU，scatter=1，全部热数组对齐到大页 | 229.531 / 230.501 / 228.820 | **229.531** | 228.820–230.501 |
| 上游 PSU，运算展开 8，scatter=1，全部热数组对齐到大页 | 229.417 / 220.017 / 228.232 | **228.232** | 220.017–229.417 |
| NU，scatter=1，普通分配 | 215.779 / 193.640 / 216.573 | **215.779** | 193.640–216.573 |
| 同源 Verilator 对照 | 911.716 / 871.684 / 936.030 | **911.716** | 871.684–936.030 |
| OU 对照，普通分配 | 112.673 / 113.555 / 113.078 | **113.078** | 112.673–113.555 |

历史 RU 三轮中位数为 18.162 cycles/s；本轮最优中位数与其数值比为 12.64。历史实验在 CPU 95，本轮在 CPU 91，**该比值不是同轮配对加速比**。历史 RU/Verilator 结果、tensor 和原 runner 均保留。

**搜索范围与收敛证据。**

本目录保留 55 个带配置和哈希清单的构建，以及 107 个检查通过的结果（包含前缀筛选、完整运行和明确标注的诊断结果）。[summary.json](summary.json) 逐项列出实际命令、配置与样本；没有把训练或采样插桩的吞吐用于最终排名。

| 方向 | 实际比较及结果 |
| --- | --- |
| RU → OU | 前一轮证实 RU 每周期 2,655,071 次临时 `new`，分配/释放约占 55% self 样本；OU 消除此开销。见 [原诊断报告](../nanhu-profile/REPORT_zh.md)。 |
| NU 与 PSU | 32 位分组循环；派生 PSU 运算展开 2/4/8/16/32/64，另测上游原 PSU 的展开 8。大展开没有稳定优势。 |
| scatter | 比较 1/8/16/24/32；大页版本也比较 1/16。当前数据支持配置 1。配置 1 是源码参数；反汇编显示 `-O3` 仍会自动展开部分循环，不能解读成机器码完全不展开。 |
| IU | 每层静态边界，测试运算因子 1/8。完整运行与短窗口均未超过最终候选。 |
| SU / TI | 完整生成静态操作数索引、消除安全的输出缓冲，按 512 个操作/存储分函数以控制编译规模。两者完成完整程序检查；机器码明显变大且更慢。这里是分区实现，不能称为未经修改的上游整体 TI。 |
| 紧凑 SU 混合实现 | 同样安全消除输出缓冲，但保留按类型循环；测试展开 1/4/8/16/32，以及 scatter 1/8/16/24、无分支 mux、PGO。最好的早期完整样本约 207 cycles/s，未超过最后的大页 NU/PSU。 |
| 编译器与软件预取 | Clang 与 GCC 13.3、`-O2`/`-O3`、`-funroll-loops`、关闭向量化、对更多类型展开、PGO、预取距离 8/16/32。未找到优于最终配置的证据。CPU 117 的早期筛选后来受到 sibling 占用影响，未混入最终中位数。 |
| 显式 AVX2 | 4/8/16 路 gather，保留标量尾部及其他上游运算；独立随机算子和 sanitizer 检查通过。CPU 91 的 1,024 周期窗口约 156–160 cycles/s，对照 NU 为 218.7，停止该方向。 |
| 本地大页 | 普通分配上 `madvise`、仅 live-state 对齐、以及所有热数组按 2 MiB 对齐。完整对齐覆盖 in/rptr/sptr/output/muxJT，启动日志确认 36 MiB `AnonHugePages`；未修改全局系统设置。 |

部分筛选样本如下。它们用于判断方向，和最终三轮中位数分开；不同时间的单次结果不能直接当成严格配对加速比。

| 内核/配置 | cycles/s | 条件 | 数据 |
| --- | ---: | --- | --- |
| `nu-native-s1` | 203.510 | CPU 91，单次完整运行 | [原始结果](runs/nu-s1-full91.json) |
| `nu-native-s1` | 211.001 | CPU 91，单次完整运行 | [原始结果](runs/nu91-control-full6.json) |
| `nu-native-thp-s1` | 220.389 | CPU 91，单次完整运行 | [原始结果](runs/nu-thp-full.json) |
| `upstream-psu8-native-s1` | 208.103 | CPU 91，单次完整运行 | [原始结果](runs/upstream-psu8-full.json) |
| `upstream-psu8-thp-s1` | 219.580 | CPU 91，单次完整运行 | [原始结果](runs/upstream-psu8-thp-full.json) |
| `nu-native-thp-s16` | 211.993 | CPU 91，单次完整运行 | [原始结果](runs/nu-thp-s16-full.json) |
| `nu-native-s8` | 205.230 | CPU 91，单次完整运行 | [原始结果](runs/nu-s8-full91.json) |
| `nu-native-s16` | 204.008 | CPU 91，单次完整运行 | [原始结果](runs/nu-s16-full91.json) |
| `hybrid1-s8` | 190.232 | CPU 91，单次完整运行 | [原始结果](runs/hybrid-full.json) |
| `hybrid4-s8` | 206.657 | CPU 91，单次完整运行 | [原始结果](runs/hybrid4-full.json) |
| `hybrid4-s8` | 188.326 | CPU 91，单次完整运行 | [原始结果](runs/hybrid4-full91-repeat.json) |
| `hybrid8-s16` | 203.748 | CPU 91，单次完整运行 | [原始结果](runs/hybrid8-full.json) |
| `su512-native` | 149.217 | CPU 91，单次完整运行 | [原始结果](runs/su512-full.json) |
| `ti512-native` | 69.783 | CPU 91，单次完整运行 | [原始结果](runs/ti512-full.json) |

**当前瓶颈与为什么停止增加展开。**

普通 native NU 的 1,024 周期诊断中，每仿真周期约 2,291 万条宿主指令，IPC 约 1.50；分支误预测约 1.23%。perf 源码行采样中 scatter 约占 26%，mux 表达式约占 24%；临时操作数分配已不再是主要热点。[计数](runs/nu-latest-stat.stat.csv)、[源码行采样](runs/nu-latest-record.lines.txt) 与 [原始 perf 数据](runs/nu-latest-record.perf.data) 均保留。PMU 计数覆盖约 83%，perf 已缩放；record 无丢样，只有约一千个样本，比例属于诊断近似值。

两次另外的 1,024 周期诊断中，普通 NU 与 live-state 对齐大页版本的通用 `dTLB-load-misses` 分别为 12,206,088 和 1,737,749（约 11,920 / 1,697 每仿真周期），宿主指令总量近似一致。这与地址翻译相关开销减少一致。没有把 perf 自动打印的 TLB 百分比当成完整内存访问 miss rate，也没有据通用 cache 事件宣称 DRAM 带宽饱和。[普通页](runs/nu-tlb-stat.stat.csv)、[大页](runs/nu-aligned-tlb-stat.stat.csv)。

SU/TI 每周期 1,412,555 个操作可直接写 data，679,781 个输出字仍需缓冲；紧凑混合调度每周期有 12,069 段类型循环。静态 SU/TI 的 text 分别约 57 / 96 MB，而紧凑 NU 约 0.39 MB。较少的间接索引和 scatter 并没有转化为更高吞吐；巨大机器码和编译规模是明确成本，但本轮没有用完整前端 PMU 分解证明全部性能损失来自指令缓存。继续扩大展开与已测结果相悖，因此最终采用紧凑候选并验证大页布局。

**正确性与隔离。**

1. [layout.json](layout.json) 将全部分组操作数、scatter 目的地、每层 58 个边界项与已接受 RU tensor 的解码逐项核对。fptr 是操作计数，不是宽位移的输出字数。9 个宽层超过 65,535，最大 fptr 为 189,109；NU/PSU 的相关 16 位类型扩为 32 位。
2. 上游分组约定省略 5,428 次 memw 占位输出。槽位复用使得“这些位置从不被读”不成立。[check_liveness.py](check_liveness.py) 证明每个被省略的值在下一次读取前均被真实写覆盖，覆盖周期回绕、mux 查表和观测信号；不安全写为 0。[liveness.json](liveness.json) 是有效结论，早期 naive 检查作为历史保留。
3. 分组算子独立比较原 OU 与分组求值，覆盖 29 类操作、4 个随机种子、每组约 14.5 万操作/17 万输出、宽层和尾部。原版 PSU 与显式 SIMD 另有对应检查；ASan/UBSan 检查通过。为保留上游已有移位语义关闭 shift sanitizer，沙箱不支持 LeakSanitizer，未把它们描述为已通过。
4. 每次最终完整运行都由独立 RV64I oracle 检查退休 PC/指令、已定义 GPR、退出条件；8,011 行周期/PC/指令 trace 与同源 Verilator **逐字节一致**。RTeAAL 样本另外与 NU 完整参考比较数据及 memory64/memory8 的 64 位指纹。数据指纹排除了 memw 占位目的槽，不是全状态逐字节等价证明。
5. 对齐版本与各自普通版本的 evaluator header **逐字节一致**，见 [aligned-kernel-integrity.json](aligned-kernel-integrity.json)。对齐/大页处理在加载后、复位和计时前执行，数据值保持一致；36 MiB 是热数组的大页覆盖量，不是整个模拟器的内存占用。
6. 新实验全部位于本目录；未修改 home 下原 XiangShan 工作区。原 [tensor-manifest.json](../../nanhu/tensor-manifest.json) 的 20 个文件重新核对全部通过，包含原 RU 源码和二进制。最优结果仍是当前实验时钟/外设适配、固定 RV64I 程序上的性能；没有宣称全 ISA 或完整设计形式等价。

**复现。**

从 `/nfs/home/zhangziqing/RTeAAL` 执行，使用新的结果标签保存全部样本：

```bash
python3 results/experimental/nanhu-tuning/run.py nu-aligned-all-s1 reproduce-best --cycles 0 --cpu 91 --reference nu-full
python3 results/experimental/nanhu-tuning/compare.py nu-aligned-all-s1 psu8-aligned-all-s1 nu-native-s1 --include-ou --prefix reproduce --rounds 3 --cpu 91
```

`build.py` 构建 OU/NU/派生 PSU/上游 PSU/IU；`build_static.py` 构建分区 SU/TI；`build_hybrid.py` 和 `clone_hybrid.py` 构建紧凑混合调度；`build_simd.py` 构建 AVX2 实验；`build_aligned.py SOURCE NAME --all-arrays` 添加完整热数组对齐。每个 `build/NAME/manifest.json` 保留实际命令、配置、源码和二进制哈希。[environment.json](environment.json) 固定宿主、编译器与大页策略。

`run.py` 与 `compare.py` 通过本目录文件锁串行测量；加载后运行固定程序并验证结果，不复用缓存的仿真答案。`profile.py` 在首退休后启用 PMU，仅用于诊断，不进入最终排名。`summarize.py` 校验产物后汇总，`make_report.py` 生成本文。
