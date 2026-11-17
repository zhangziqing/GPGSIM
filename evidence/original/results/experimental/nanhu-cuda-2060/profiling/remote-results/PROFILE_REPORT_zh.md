# RTX 2060 Nanhu 长尾时间线实测

已安装项目内 NVIDIA Nsight Systems 2024.6.2.225，并完成 CUDA Graph 节点时间线和独立 CUPTI 12.6.80 Activity 采集。最终 **939 层 suffix 占 kernel 执行时间 34.68%–34.80%**；全部 968 个窄层合计占 37.41%–37.53%。三窗口各 256 个真实 CoreMark 周期，CUPTI 丢失计数全部为 **0**。原始、同一采样二进制关闭 tracing、打开 CUPTI、打开 Nsight 的完整 100K 均通过在线逐周期比较与最终指纹。

## 配置与映射

Windows 10 / RTX 2060 6GB / driver 565.90；CUDA 12.6.85、MSVC 19.29。配置固定为 Graph / threads=128 / tail_threshold=256。原二进制 SHA256 `7f08fd127477db8527d4a074064f781430247c0dfb26ed4efe595f13b90be25f` 保持不变。所有结果均在本独立目录，原封存报告和 summary 未修改。

`layers.bin` 按小端 4×uint32 解码，生成 `layers.csv`、`schedule.csv`、`layer-map.json`。层号使用零起始、半开区间。每周期 ordinal0 为 set_reset，ordinal1–134 为层[0,67)的 compute/commit，ordinal135 为 [67,96) 29 层融合段（2676 nodes / 2694 outputs），ordinal136/137 是第96层（443 nodes），ordinal138 为 [97,1036) 939 层 suffix（2966 nodes / 2965 outputs，最大39 nodes/层），ordinal139 为 gather。总计140个kernel、2段融合、68个宽层。每个采样周期逐一核对 kernel 名称、顺序、graphNodeId、grid/block 和 stream，140个唯一 graphNodeId 稳定。最终suffix约占全图0.1414% nodes和90.64%层，这些静态比例没有用作时间估算。

长尾定义分别为最终939层连续suffix，以及满足实际调度条件 nodes≤256且outputs≤256的全部968层。

## 三阶段实测（CUPTI 零丢失数据为主）

采样非reset周期使用零起始：[1024,1280)、[20000,20256)、[90000,90256)。对应master周期加64。所有 kernel 时间为设备活动的 end-start；kernel占比的分母是该窗口全部kernel duration之和。端到端占比以窗口loop wall（含主机观察/参考比较）为分母。

| 非reset窗口 | suffix µs/周期 | suffix占kernel | 29层段 µs/周期 | 全部窄层占kernel | suffix占loop wall |
|---|---:|---:|---:|---:|---:|
| 1024–1279 | 1186.87 | 34.68% | 93.30 | 37.41% | 31.05% |
| 20000–20255 | 1194.22 | 34.75% | 93.55 | 37.47% | 31.04% |
| 90000–90255 | 1194.33 | 34.80% | 93.60 | 37.53% | 31.12% |

| 窗口 | compute µs/周期（占kernel） | commit µs/周期（占kernel） | reset/gather µs/周期 | D2H µs/周期 | Graph内正空隙 µs/周期 |
|---|---:|---:|---:|---:|---:|
| 1024 | 754.61 (22.05%) | 1382.70 (40.41%) | 1.54 / 3.00 | 1.60 | 74.66 |
| 20000 | 755.68 (21.99%) | 1388.70 (40.41%) | 1.51 / 3.06 | 1.61 | 74.44 |
| 90000 | 752.36 (21.92%) | 1387.05 (40.42%) | 1.46 / 3.10 | 1.67 | 75.37 |

D2H 每周期4144字节。下表把GPU活动、周期间空隙和host等待分开；host等待与GPU执行重叠，不能相加。Graph到D2H和D2H到下一周期包含提交、同步、CUDA events、主机观察和参考比较等，未把整个空隙归因给单一组件。

| 窗口 | Graph到D2H gap µs/周期 | D2H到下一kernel gap µs/间隔 | cudaStreamSynchronize µs/周期 | host graph-launch API µs/周期 | loop wall ms |
|---|---:|---:|---:|---:|---:|
| 1024 | 73.72 | 241.39 | 3562.02 | 182.83 | 978.39 |
| 20000 | 71.41 | 255.62 | 3564.27 | 191.27 | 984.77 |
| 90000 | 70.12 | 250.48 | 3558.44 | 193.53 | 982.41 |

各 kernel 的均值/中位数/范围、层区间、graphNodeId、绝对起止、空隙及原始活动记录均保留。时间戳在少量相邻依赖kernel间存在重叠，无法据此断言真实并行；正空隙、重叠量、kernel duration之和与活动区间并集均在 JSON 单独记录。

端到端窗口占比补充（以各窗口loop wall为分母；host同步为重叠量）：

| 窗口 | reset/gather | D2H | Graph内正空隙 | Graph到D2H空隙 | 周期间空隙 | host同步（重叠） |
|---|---:|---:|---:|---:|---:|---:|
| 1024 | 0.040% / 0.079% | 0.042% | 1.95% | 1.93% | 6.29% | 93.20% |
| 20000 | 0.039% / 0.080% | 0.042% | 1.93% | 1.86% | 6.62% | 92.66% |
| 90000 | 0.038% / 0.081% | 0.044% | 1.96% | 1.83% | 6.50% | 92.73% |

这些量不全部互斥：host同步覆盖GPU执行，全部窄层也包含两个子融合段；不能逐列求和。

## 最慢调度段（中期窗口）

| ordinal | 类型/层区间 | 平均 µs/周期 | 占kernel |
|---:|---|---:|---:|
| 138 | small_layers [97,1036) | 1194.22 | 34.75% |
| 12 | commit [5,6) | 301.08 | 8.76% |
| 16 | commit [7,8) | 143.33 | 4.17% |
| 4 | commit [1,2) | 125.79 | 3.66% |
| 135 | small_layers [67,96) | 93.55 | 2.72% |
| 10 | commit [4,5) | 70.30 | 2.05% |
| 7 | compute [3,4) | 54.25 | 1.58% |
| 9 | compute [4,5) | 54.24 | 1.58% |
| 8 | commit [3,4) | 50.21 | 1.46% |
| 11 | compute [5,6) | 48.40 | 1.41% |
| 6 | commit [2,3) | 45.65 | 1.33% |
| 62 | commit [30,31) | 43.92 | 1.28% |

## 采集扰动和正确性

| 运行 | cycles/s（仅对照，不作新排名） | 完整100K指纹 |
|---|---:|---|
| original-control | 268.457 | passed |
| profiling-v2-control | 268.101 | passed |
| cupti-full100K | 271.085 | passed |
| nsight-v2-three-windows | 266.480 | passed |

同一采样二进制的窗口step wall包括提交/同步/紧凑观察回传，排除启停控制；loop wall再包括主机观察和逐周期参考比较。`start_control_seconds`/`stop_control_seconds`另列，包含flush/序列化开销，不能隐去。

| 窗口 | tracing关闭step ms | CUPTI打开step ms | step扰动 | CUPTI启停开销 ms | Nsight打开step ms |
|---|---:|---:|---:|---:|---:|
| 1024 | 949.94 | 964.99 | +1.59% | 702.05 | 986.28 |
| 20000 | 952.71 | 968.52 | +1.66% | 677.76 | 984.36 |
| 90000 | 955.79 | 967.53 | +1.23% | 712.06 | 986.93 |

每组只跑一轮，环境频率/温度变化与运行次序会影响单次差异，不能把负扰动视为加速。原始与采样二进制的结果均保持64 reset +100000 cycles、167375 commits、171898 retired；三项指纹为 `10550482425419111625` / `15503565812696885107` / `656100811014868721`。规范化轨迹SHA256 `aac4bdad014633cadab8d50497e1555c60e05272eb091e0c3445c32237e6c3ae`；UART `Running CoreMark for 2 iterations\r\n`。

## 丢失、权限与方法限制

CUPTI 每窗口预期/实收均为35840 kernels、256次D2H、1536条runtime API。每个完成buffer按官方API调用 `cuptiActivityGetNumDroppedRecords`，窗口结束再检查context及全局队列，计数全为0，解析无错误，没有强制flush不完整记录。host buffer 16MiB、device buffer 16MiB、pool limit 64。保留原始buffer二进制、逐条JSONL、每buffer丢失查询和分析CSV。

Nsight三个窗口也均有完整140节点×256周期及256次回传，但保留了“Not all CUDA events might have been collected”泛化告警；它还未包含每周期的cudaEventElapsedTime runtime条目。未以节点数量齐全冒充所有内部事件均无丢失，正式时间占比取独立CUPTI零丢失数据，Nsight .nsys-rep/SQLite用于时间线交叉检查。先前定时停止和输出转发失败的预实验亦保留，并排除在正式验收之外。

工具来自NVIDIA官方redistrib，官方manifest与实际SHA256均在install-record.json及tools内；2024.5预实验对驱动12.7提示回退，正式Nsight使用2024.6.2；直接CUPTI使用匹配CUDA12.6的12.6.80。没有更新驱动、重启、修改系统PATH/注册表/计数器权限或WSL。CPU sampling和context-switch tracing关闭；未采GPU性能计数器。Compute Sanitizer未运行。

## 有条件的理想上限

假设suffix耗时可完全消除，其他GPU/主机时间、频率、数据语义和同步成本完全不变，用 `1/(1-f)` 作Amdahl推算。该假设不是已实现优化，也不是下一次运行必然可达到的加速。

| 窗口 | 仅kernel分母：消除suffix | loop wall分母：消除suffix | loop wall分母：消除全部窄层 |
|---|---:|---:|---:|
| 1024 | 1.531× | 1.450× | 1.504× |
| 20000 | 1.533× | 1.450× | 1.503× |
| 90000 | 1.534× | 1.452× | 1.505× |

长尾已由设备起止时间量化，耗时不能由节点数量解释。未使用DRAM/SM性能计数器，不能把kernel duration直接归因为显存带宽瓶颈。

## 重现与交付

`scripts/build.cmd` 构建独立采样二进制，`scripts/build_injection.cmd` 构建CUPTI DLL。`scripts/run_experiment.py` 支持 baseline / ranges / cupti，每次唯一label，完整命令见各runs/<label>/command.json。CUPTI DLL遵循NVIDIA官方cupti_trace_injection样例，以cuProfilerStart退出/Stop进入控制活动采集。所有采样仅处于三个256周期窗口，启停在step计时外。原引擎、运算及oracle文件逐字节未变，hash证明在source-proof.json；main和CMake diff在logs。

主要原始结果：`captures/nsight-v2-three-windows.{1,2,3}.nsys-rep` 与SQLite、`activities/cupti-full100K/`、`analysis-cupti/`、`analysis-nsight/`、`runs/`、`profile-summary.json`。可下载归档不包含数百MB的工具安装包，包含来源、校验、重现脚本、代码、命令、完整验收结果和原始活动数据。

