node035 上，本轮完整 100K 复测的最高吞吐中位数为 **322.258 cycles/s，配置 `avx2-control`**。共筛选 20 个配置，最终对 4 个配置各做两次完整 100K 周期运行，第二轮按相反顺序执行。

最快的显式 AVX-512 修改是 `hybrid-scatter8`，中位数 **303.271 cycles/s**，相对同机 AVX2 对照变化 **-5.89%**。默认自动向量化 AVX-512 为 **219.434 cycles/s**；AVX2 对照为 **322.258 cycles/s**。这里的最优限定于本次候选和固定工作负载，不构成所有 AVX-512 实现的性能上限。

**100K 完整复测。** 以下中位数只使用同一组两轮确认样本；初始基线和短筛选不混入统计。

| 配置 | 第 1 轮 cycles/s | 第 2 轮 cycles/s | 中位数 | 相对 AVX2 | 100K 求值时间范围 |
| --- | ---: | ---: | ---: | ---: | ---: |
| `avx2-control` | 321.082 | 323.434 | **322.258** | +0.00% | 309.183–311.446 s |
| `hybrid-scatter8` | 303.443 | 303.099 | **303.271** | -5.89% | 329.551–329.925 s |
| `soa-hot8` | 263.213 | 262.148 | **262.680** | -18.49% | 379.921–381.465 s |
| `native-control` | 220.984 | 217.884 | **219.434** | -31.91% | 452.522–458.960 s |

最快显式 AVX-512 修改的两次样本均低于两次 AVX2 对照；本轮未测得 AVX-512 超越 AVX2。

原始数据见 [validation.json](validation.json)、[各次命令和输出](runs/)、[筛选选择记录](selection.json)。

**全部 20 个配置的 16K 筛选。** 每次精确运行 16,384 个非复位主周期；AVX2/default 两个对照各测两次，其余各一次。按这些短窗口的中位数选出两个最快显式修改进入完整复测。短窗口包含启动阶段，不能把这张表称为所有候选的 100K 排名。

| 配置 | 修改内容 | 样本数 | 16K cycles/s | 相对同窗口 AVX2 |
| --- | --- | ---: | ---: | ---: |
| `avx2-control` | 原 NU，禁用 AVX-512，偏好 256 位 | 2 | 320.834 | +0.00% |
| `hybrid-scatter8` | 其余代码 AVX2，仅 scatter 启用 AVX-512 | 1 | 303.553 | -5.39% |
| `soa-hot8` | SoA 热点逻辑/mux/cat/bits/assign，原 scatter | 1 | 267.249 | -16.70% |
| `combo8-soa256` | SoA combo，其余自动向量化偏好 256 位 | 1 | 260.926 | -18.67% |
| `combo8-soa` | combo，加载后将索引转为每 8 操作的 SoA | 1 | 260.409 | -18.83% |
| `hybrid-combo8-soa` | 其余代码 AVX2，SoA 热点组及 scatter 启用 AVX-512 | 1 | 257.340 | -19.79% |
| `hybrid-mux8` | 其余代码 AVX2，仅 mux 组启用 AVX-512 | 1 | 247.843 | -22.75% |
| `combo8-permute` | combo，向量 permute 拆分 AoS 索引 | 1 | 244.279 | -23.86% |
| `auto256` | AVX-512 可用，自动向量化偏好 256 位 | 1 | 220.865 | -31.16% |
| `native-control` | 原 NU，znver4 默认自动向量化 | 2 | 219.739 | -31.51% |
| `mux-select8` | 先选择索引，再 gather 所选分支 | 1 | 218.666 | -31.84% |
| `auto512` | 自动向量化偏好 512 位 | 1 | 218.301 | -31.96% |
| `scatter8x2` | 一次处理两组 8 lane scatter | 1 | 215.698 | -32.77% |
| `scatter8` | 8 × 64 位 scatter | 1 | 212.956 | -33.62% |
| `mux-dual8` | 两侧均 gather，再 blend 数值 | 1 | 209.428 | -34.72% |
| `mux-masked8` | 两个互补 mask 的 gather | 1 | 208.430 | -35.04% |
| `scatter4` | 4 × 64 位 AVX-512VL scatter | 1 | 205.937 | -35.81% |
| `logic8` | and/or/xor/assign 显式 8 lane | 1 | 195.893 | -38.94% |
| `broad8` | 比较、逻辑、cat/bits/assign 显式 8 lane | 1 | 180.458 | -43.75% |
| `combo8` | broad + mux + scatter，AoS 标量打包索引 | 1 | 179.247 | -44.13% |

**实际指令与实现边界。**

所有版本均从已接受的 `nu-aligned-all-s1/kernel.h` 派生，保留原标量尾部及未修改的运算。基础编译使用本地 Clang 19.1.1、`-O3 -static -march=znver4 -mno-avx512bf16`。AVX2 对照额外使用 `-mno-avx512f -mprefer-vector-width=256`；默认/auto512 允许 AVX-512。这次没有把 node007 上的 `-march=native` 二进制直接当作 node035 的最佳对照。

| 配置 | ZMM 操作数指令位置 | gather 位置 | scatter 位置 |
| --- | ---: | ---: | ---: |
| `avx2-control` | 0 | 0 | 0 |
| `native-control` | 539 | 58 | 0 |
| `auto256` | 0 | 58 | 0 |
| `auto512` | 539 | 58 | 0 |
| `hybrid-scatter8` | 2 | 0 | 1 |
| `soa-hot8` | 569 | 71 | 0 |

这里统计 main 和显式 group/scatter helper 的静态指令位置，不是动态执行次数。hybrid 版本的 AVX-512 位于带 target 属性的独立函数，main 仍为 AVX2；反汇编已覆盖这些 helper，不能只看 main 而误判为没有 AVX-512。[完整指令清单](evaluator-instruction-inventory.json)、[生成脚本](inspect_instructions.py)。

默认版本和 auto256 都有 58 处 gather，即使后者没有 ZMM 操作数，仍使用 AVX-512VL gather。因此只设 `-mprefer-vector-width=256` 没有消除这些间接读取。显式方案分别尝试减少 mux 的分支读取、改变 AoS 索引打包方式、加载时改成 SoA，以及仅对局部函数启用 AVX-512。这些对照反映各实现整体的性能；本轮没有新做 PMU 门控测量，不能将全部差距单独归因于 gather、频率变化或某一个微架构瓶颈。此前 node007 的 profile 只能作为选择热点的依据。

固定同一组合的操作数和 scatter 后，AoS 打包、向量 permute、SoA 的 16K 筛选分别为 179.247、244.279、260.409 cycles/s；SoA 相对 AoS 打包提升 45.28%。所以本轮索引布局优化确有收益，但不能把相对某个较慢向量版本的收益当成相对 AVX2 的收益。

SoA 只在进程加载后重排自有 rptr 副本，每 8 个操作打包成一个 block；加载时逐块逆向核对原操作数。转换在求值计时外，加载时间另有记录。原 tensor 文件不变。向量 scatter 使用前已验证目的地址不越界，4/8/16 连续 lane 的全部批次均无重复地址，保持层内先计算、再提交的语义。cat/bits 的静态移位已验证在 0–63 范围，动态移位沿用原实现。

**固定条件与正确性。**

- 共享主机 node035，AMD EPYC 9684X，Linux 3.10.0-1127.el7.x86_64；CPU 9、NUMA node0，SMT sibling 201。全部测速顺序执行并持有同一个文件锁；sibling 实测忙碌比例范围为 0.0000%–0.0128%。
- 原单核 Nanhu DefaultConfig，提交 `0a68ebc13b3d96a78d8663679393ecbea5b8fb73`，保留真实 L1/L2/L3 及既有时钟/外设实验适配。模型含 2,096,953 个运算、1,036 层、461,324 个数据槽。本轮没有修改 RTL。
- 同一旧 checkout 的 `ready-to-run/coremark-2-iteration.bin`，SHA-256 `1156255cce0d01cffbf7232fa4a168a00defc41e0c8ea6096a3186ca30a48519`。
- 每次先复位 64 周期，再执行要求的非复位周期数。100K 样本共 100,064 周期，计时累计 evaluator 的全部层和 scatter，含启动/cache 等待，排除加载、复位、trace 格式化、参考比较和最终指纹计算。
- 全部版本使用相同的本地 THP 建议与加载方法；全部样本在加载后及结束时实测均为 **0 kB AnonHugePages**。旧内核拒绝 MADV_COLLAPSE，errno 22。没有修改全局 THP/NUMA 设置；不能与 node007 的 36 MiB 大页结果直接作 AVX-512 因果比较。
- 20 个配置均通过两类独立算子检查：全部 29 个 opcode 与未修改 OU 对照；另对 13 个向量目标 opcode 做 4 seed、全 64 位随机数据和 13 种长度检查，含 0/1/7/8/9/15/16/17/63/64/65/4099/65541，覆盖向量尾部与 65,535 边界。scatter 另测 0–258 个打乱且互异的目的地址。[检查汇总](extended-check-results.json)。
- 共 **32 次** CoreMark 前缀运行全部通过在线逐周期核对及 trace 文件逐字节对比；其中 **10 次**为完整 100K。等长样本的最终数据槽、64 位/8 位存储器指纹及全部寄存器/CSR 一致。
- 每份 100K trace 与既有同源 Verilator 参考一致，SHA-256 `aac4bdad014633cadab8d50497e1555c60e05272eb091e0c3445c32237e6c3ae`，退休 171,898 条指令（含融合），异常/trap 均为 0。100K 时 CoreMark 尚未结束，未得到最终 CRC 或分数；本轮没有独立 ISA oracle。
- 原 tensor/runner 清单 20 个文件、已接受最优构建清单 4 个文件及全部新构建清单均经哈希核验。实验位于本目录，旧搜索排名独立保留。

**初始 100K 基线。** 它们用于最初判断方向，不混入上述两轮确认中位数。

| 配置 | cycles/s | 求值秒数 |
| --- | ---: | ---: |
| `avx2-control` | 324.811 | 307.871 |
| `native-control` | 217.730 | 459.284 |

**复核与复现。** 在共享工作区根目录执行；AVX-512 二进制只在支持的 node035 上运行。

```bash
python3 results/experimental/nanhu-avx512-node035/analyze.py
python3 results/experimental/nanhu-avx512-node035/inspect_instructions.py
```

本地重建使用 `build_candidates.py --jobs 3`。每个候选的 `build/<name>/manifest.json` 保存完整编译命令与二进制哈希；`extended-check-builds.json` 保存额外随机检查的编译命令。重跑使用新 prefix，以免覆盖已有样本：

```bash
ssh -F /nfs/home/zhangziqing/.ssh/config node035 \
  'python3 /nfs/home/zhangziqing/RTeAAL/results/experimental/nanhu-avx512-node035/experiment.py full \
    --prefix repeat-100k --cpu 9 --rounds 2 --names avx2-control native-control hybrid-scatter8 soa-hot8'
```

应先检查 CPU 9/201 的实时负载；脚本保存运行前后实际负载、大页覆盖、精确命令和镜像/二进制哈希。本次封存文件的大小与 SHA-256 见 [artifact-manifest.json](artifact-manifest.json)。
