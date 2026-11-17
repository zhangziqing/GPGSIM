# RTX 2060 实测与独立复核

Nanhu CUDA 版已在 `LAPTOP-JTMR2H59` 的 RTX 2060 6 GB 上运行通过。取回原始结果后，本地复核通过：CTest 6/6、26 次 GPU 前缀运行中的 10 次完整 100K 校验、全部测速计算及三次复测中位数均与远端报告一致。另取回一份完整 GPU 实际轨迹，统一 CRLF/LF 后与本地 Verilator 参考逐字节相同。

## 性能结果

以下均为 CUDA Graph、同一程序和 100,000 个非 reset 周期。吞吐计入提交、同步及观察数据回传，排除加载、64 周期 reset、轨迹格式化/核对和最终指纹。

| 参数 | 三次吞吐 cycles/s | 中位数 | 100K 求值时间 |
| --- | --- | ---: | ---: |
| 默认：threads=256，tail_threshold=128 | 261.509、261.495、261.544 | 261.509 | 382.35–382.42 s |
| 本轮推荐：threads=128，tail_threshold=256 | 263.599、263.655、263.628 | **263.628** | **379.28–379.36 s** |

推荐配置相对默认的中位数提高 **0.810%**。含 reset 和轨迹核对的模拟循环实际耗时为 **386.47–386.66 s**，不能用 379 秒代表完整进程时间。

筛选范围为 Graph 的 12 组：threads∈{128,256,512}、tail_threshold∈{0,64,128,256}，每组 16,384 周期；另有三组 stream 对照。选出候选后完成 100K 确认，再对默认及推荐配置各做三次复测。threads=256、threshold=256 的单次 100K 也达到 263.578 cycles/s，与推荐配置很接近；没有对该组合做同样的三次复测，不能宣称 128 线程已被证明严格优于它，更不能称为全局最大性能。

与此前同工作负载的历史结果对照：

| 机器 / 实现 | cycles/s | RTX 2060 吞吐与其比值 |
| --- | ---: | ---: |
| node007 / RTeAAL NU | 225.608 | 1.169× |
| node035 / RTeAAL AVX2 | 322.258 | 0.818× |
| node007 / Verilator | 811.708 | 0.325× |

这些是跨机器的历史吞吐比较，样本数量和主机条件也不同。当前 GPU 实现尚未超过 node035 的 AVX2 结果，低约 18.2%。CPU 证据见 [CoreMark 配对实验](../../nanhu-coremark-100k/REPORT_zh.md) 和 [node035 AVX2 / AVX-512 实验](../../nanhu-avx512-node035/REPORT_zh.md)。

## 正确性和修改范围

- 设计是旧 Nanhu，提交 `0a68ebc13b3d96a78d8663679393ecbea5b8fb73`，单核 DefaultConfig，沿用此前缓存、时钟和外设实验适配。
- 镜像为该版本 `ready-to-run/coremark-2-iteration.bin`，SHA-256 为 `1156255cce0d01cffbf7232fa4a168a00defc41e0c8ea6096a3186ca30a48519`。
- 十次 100K 均启用在线逐周期参考核对；原始程序输出和结果 JSON 相符，最终 GPR/FPR/CSR 及三个状态指纹一致。退休指令 171,898，提交事件 167,375，异常/trap 为 0。
- 独立取回 `repeat-best-3/actual.trace`：原始 Windows 文件 36,558,271 字节；仅统一换行后为 36,458,271 字节、100,000 行，与本地既有 Verilator 文件逐字节相同。SHA-256：`aac4bdad014633cadab8d50497e1555c60e05272eb091e0c3445c32237e6c3ae`。其余完整运行依据原始日志、在线核对及最终指纹复核，没有重复取回全部实际轨迹。
- 最终数据槽指纹 `10550482425419111625`；64 位内存指纹 `15503565812696885107`；8 位内存指纹 `656100811014868721`。
- CUDA 内核、其余模拟器源码、独立算术参考头及 CMakeLists 与本地原版逐字节一致。远端候选仅添加测试用 MSVC `__builtin_expect` 宏，以及 runner 的 CRLF/LF 归一化；未放宽实际轨迹内容或最终指纹比较。
- 原始便携包和远程任务包的 SHA-256 均保持不变；候选改动及证据单独保存于本目录，未并入原封存源码。

100K 截止时 CoreMark 尚未结束，只有 `Running CoreMark for 2 iterations` 的 UART 输出；没有完整 CoreMark 分数或最终 CRC。

## 性能所支持的判断

同为 16K 前缀、128 线程时，关闭小层融合为 138.184 cycles/s，阈值 256 为 261.449 cycles/s，约 1.89×。推荐配置每周期 140 次 kernel 执行，关闭融合时为 2,074 次。Graph 相对相同参数的 stream 对照约快 2%，收益远小于融合。

因此小层调度方式对当前性能影响很大；但这些开关也改变并行粒度和同步方式，不能把全部差异单独归因于 launch 延迟。尚未做 profiler，不能断定显存带宽、运算吞吐或某个 kernel 的占比。当前逻辑依赖仍为 **1,036 层**，本轮没有实现之前讨论的 200 层图变换。

## 环境、复核和产物

Windows 10 build 19041，驱动 565.90；项目内 nvcc 12.6.85、cudart/CCCL 12.6.77，VS2019 MSVC 19.29，Ninja，目标 sm_75。Windows 原生执行，未依赖启动失败的 WSL。二进制 SHA-256 为 `7f08fd127477db8527d4a074064f781430247c0dfb26ed4efe595f13b90be25f`，本地已核对取回文件。

Compute Sanitizer 和 profiler 均未执行；上述正确性通过不代表这两项检查通过。

- [远端原报告](GPU_REPORT_zh.md)、[远端汇总](gpu-summary.json)
- [本地独立复核结果](audit.json)、[复核脚本](audit.py)
- [取回文件清单](retrieved-evidence.json)、[完整最佳样本](evidence/candidate/runs/repeat-best-3/result.json)
- [Windows runner 差异](evidence/logs/candidate-runpy.diff)、[MSVC 测试差异](evidence/logs/candidate-op-test.diff)、[CTest 日志](evidence/logs/ctest.log)

在当前工作区复核，不重新运行仿真：

```bash
python3 results/experimental/nanhu-cuda-2060/remote-results/audit.py
```

在远端已配置的 VS2019 x64 环境和项目内 CUDA PATH 下，以新 label 复现推荐配置：

```powershell
python candidate/run.py --binary candidate/build-win5/nanhu_gpu.exe --cycles 100000 --threads 128 --tail-threshold 256 --label repro-best
```
