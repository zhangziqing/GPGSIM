已完成旧版 **Nanhu 的 CoreMark 100K 周期配对测试**：Verilator 为 **811.708 cycles/s**，RTeAAL NU 为 **225.608 cycles/s**。RTeAAL 吞吐为 Verilator 的 **27.79%**，相同周期数的求值用时为 **3.598 倍**。本轮各测一次，没有作为重复测量中位数或新的参数搜索排名。

**设计、镜像与计时。**

- Nanhu 提交 `0a68ebc13b3d96a78d8663679393ecbea5b8fb73`，单核 DefaultConfig，保留 L1/L2/L3。使用已有时钟/外设实验适配；本次没有修改 RTL 或 tensor。版本依据见 [版本选择](../../nanhu/version-selection.json)。
- 两边使用同一个旧 Nanhu checkout 自带的 `XiangShan-nanhu-legacy/ready-to-run/coremark-2-iteration.bin`，18,984 字节，SHA-256：`1156255cce0d01cffbf7232fa4a168a00defc41e0c8ea6096a3186ca30a48519`。home 下新版 XiangShan 的同名镜像不同，未混用。
- 每边复位 64 周期，然后精确执行并测量 **100,000 个非复位主周期**，共 100,064 周期。计时包含启动和 cache 等待，排除加载、复位、逐周期 trace 格式化及参考比较。Verilator 一周期包含时钟低/高两次求值；RTeAAL 一周期执行一次完整 tensor。
- RTeAAL 使用既有最优参数 `nu-aligned-all-s1`、`-O3 -march=native`，2,096,953 个操作、1,036 层。evaluator header 与已接受版本逐字节一致。五个热数组共 36 MiB，在加载结束及运行结束均测得 `AnonHugePages: 36864 kB`。
- 主机 node007，AMD EPYC 7773X；最终两边绑定 **CPU 110**，依次执行，使用共享 benchmark 文件锁。SMT sibling 为 238。测试前 CPU/sibling 忙碌比例为 9.74%/8.29%；运行期间 sibling 分别为 18.14% 和 7.33%。这是共享主机的单次测量，不能声称无系统干扰或给出稳定误差范围。

| 仿真器 | 100K 求值时间 | 吞吐 cycles/s | 仿真循环墙钟时间 | 进程墙钟时间 |
| --- | ---: | ---: | ---: | ---: |
| Verilator，单线程 | 123.197 s | 811.708 | 124.415 s | 125.1 s |
| RTeAAL NU，36 MiB 大页 | 443.247 s | 225.608 | 445.615 s | 449.2 s |

循环墙钟包含复位及 trace/核对；进程墙钟还包含加载，外部监控以约 1 秒轮询，因此只列一位小数。RTeAAL 的 tensor 加载为 2.797 秒。[最终完整数据](coremark-thp-final-comparison.json)、[精确命令与镜像哈希：Verilator](coremark-thp-final-verilator.command.json)、[RTeAAL](coremark-thp-final-rteaal.command.json)。

**正确性核对。**

两边 100,000 行逐周期 trace **逐字节一致**。每行包含该周期的全部退休事件、32 个 GPR、32 个 FPR、18 个 CSR、异常、trap 和 UART。独立解析 trace 后再次核对结果摘要；先前 CPU 31 的完整对照与最终 CPU 110 对照共四份完整 trace 也全部一致，SHA-256 为 `aac4bdad014633cadab8d50497e1555c60e05272eb091e0c3445c32237e6c3ae`。

| 检查项 | 两边一致的结果 |
| --- | ---: |
| 非复位观测周期 | 100,000 |
| 退休 commit 事件 | 167,375 |
| 融合指令的额外退休计数 | 4,523 |
| 退休指令总数，包含融合 | 171,898 |
| 压缩指令 commit 事件 | 74,948 |
| skip commit 事件 | 74 |
| 异常 / trap 周期 | 0 / 0 |
| 首次退休 / 首次 RAM 退休主周期 | 630 / 8,321 |
| 最后退休 PC | `0x80001c84` |

UART 为 `Running CoreMark for 2 iterations`。**100K 截止时 CoreMark 尚未结束，没有输出最终 CRC 或分数**；成功状态表示固定周期前缀的跨仿真器核对通过。原独立 RV64I 小循环 oracle 不支持这个镜像，本次没有使用它，也没有独立 ISA oracle 或完整设计形式等价结论。[独立核验结果](validation.json)、[核验脚本](analyze.py)。

**大页与运行器调整的记录。**

初次固定周期运行器在第 9,829 主周期发现 UART 差异，其余观测一致。RTL 的 UART valid 表示时钟沿发生的总线事务；本轮初版 Verilator 入口在上升沿后读取组合输出，晚于 DPI 对应的采样点。新入口改为在 clock-low 求值后保存 UART，再在上升沿触发 DPI，保持同一沿采样。10K 诊断和两轮完整 100K 均通过；初次与修正后 Verilator 的全部非 UART 观测也逐行一致。修正只在新的宿主入口，已有模型 archive 未改变。[相位诊断](phase-probe-validation.json)、[可复现的入口生成脚本](build-edge.py)。

CPU 31 上原加载顺序在加载结束时大页覆盖为 0；该完整对照测得 Verilator 776.570、RTeAAL 171.375 cycles/s，吞吐比为 22.07%。它未在结束时读取大页覆盖，不能保证整个测量期间覆盖一直为 0。[该轮原始数据](coremark-edge-comparison.json)。先建议大页再初始化，以及对尚未初始化的对齐 payload 执行 `MADV_DONTNEED` 后重新触页，CPU 31 的预检仍仅得到 26 MiB；这些预检被 36 MiB 检查拒绝，没有计入性能结果。最终在另一 NUMA 节点获得完整覆盖，具体低层分配失败原因未确定。

CPU 71 的测试前负载为 100%，其中止于部分 RTeAAL 前缀，保留在 `coremark-thp-node1-*`；未用作最终比较。依据 [核心负载扫描](node1-cpu-scan.json) 改选 CPU 110，然后重新顺序测量两边。因为 CPU 和加载方式同时变化，**不能把 CPU 31 与 CPU 110 的全部差异归因于大页**。未改动全局 THP 或 NUMA 策略。新增加载逻辑仅在本实验构建，[源码及命令清单](thp-prefault-build-manifest.json)。

原 tensor/runner 清单 20 个文件、最优候选清单 4 个文件及 Verilator archive 在测量前后均核对一致；四份本实验构建清单全部通过哈希核验。所有新实现和运行结果位于本目录，未覆盖旧参数排名、旧 profile 或原 XiangShan 工作区。

**复核与重跑。**

在工作区根目录复核本轮四份完整 trace：

```bash
python3 results/experimental/nanhu-coremark-100k/analyze.py
```

使用现有封存二进制重跑，选择空闲且可满足 36 MiB 大页检查的核心，并使用新的 prefix 保留原样本：

```bash
python3 results/experimental/nanhu-coremark-100k/run-thp-prefault.py \
  --prefix coremark-repeat-1 --preflight --cpu 110
```

驱动器会先做 1,024 周期预检，再各执行 100K 周期；完整结果由驱动器在线逐周期及文件哈希核对。`analyze.py` 默认复核本报告封存的 `coremark-edge` / `coremark-thp-final` 两轮。全部文件的大小和校验和见 [产物清单](artifact-manifest.json)。
