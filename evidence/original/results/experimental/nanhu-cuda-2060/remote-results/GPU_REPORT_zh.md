# Nanhu RTeAAL CUDA GPU 验证报告

## 结果

已在本机 NVIDIA GeForce RTX 2060（6 GB，UUID `GPU-8e4316ac-74f4-4233-0e1c-be24b5560068`）完成真实 CUDA 验证。CUDA 12.6.85、MSVC 19.29.30037、CMake 3.20 和 Ninja 构建了 `sm_75` 候选程序；CTest 6/6 通过，1024 周期烟测通过。完整 100K 周期逐周期参考轨迹及最终三项状态指纹均通过。

按已测范围，最佳配置为 CUDA Graph、`threads=128`、`tail_threshold=256`。交替三次 100K 的 `cycles_per_second` 为 263.5992、263.6548、263.6275，中位数 **263.6275 cycles/s**（最小 263.5992，最大 263.6548）。默认配置（Graph、256、128）三次为 261.5089、261.4946、261.5436，中位数 261.5089，最佳配置中位数高约 0.81%。这仅是所测参数范围内最佳，不代表全局最优。

## 硬件与工具链

- Windows 10 build 19041；RTX 2060，6144 MiB，驱动 565.90（驱动报告 CUDA 12.7）。
- 项目内 CUDA nvcc 12.6.85、cudart 12.6.77、CCCL 12.6.77；三个 NVIDIA 官方组件的 SHA-256 均按交接值核验。未执行系统安装器，也未修改系统 PATH。
- Visual Studio 2019 MSVC 19.29.30037、CMake 3.20.21032501-MSVC_2、Ninja、Python 3.9.2。MinGW GCC 8.1.0 存在但未用于 CUDA 构建。
- WSL2 Ubuntu 启动仍报 `Wsl/Service/0x8007273d`；原生 Windows 工具链可用，故未改动 WSL 或驱动。

## 构建与候选兼容改动

原始项目和收到的数据保持不变。候选副本位于 `candidate/`。MSVC 不识别测试 oracle 的 `__builtin_expect`，仅在候选 `tests/op_test.cpp`、包含 oracle 前为 MSVC 定义等价于直接求值的宏。Windows 将实际轨迹写为 CRLF，而参考文件为 LF；候选 `run.py` 对比前统一换行符，保留原始实际轨迹与参考轨迹文件。没有修改模拟器、算术 oracle 函数体、数据或预期值。候选构建日志、CTest 日志和失败/成功烟测日志均在 `logs/`；每次结果的 `command.json`、`result.json`、`result.out`、`actual.trace`、`reference.trace` 和 `run.log` 位于 `candidate/runs/<label>/`。

构建命令使用 VS2019 `vcvars64.bat` 初始化环境，并以 Ninja、项目内 CUDA 根目录及 `CMAKE_CUDA_ARCHITECTURES=75` 配置；二进制为 `candidate/build-win5/nanhu_gpu.exe`。CTest 共 6 项，CPU 调度/算子与 GPU 算子、Graph 尾层、Graph 分层及 stream 尾层检查全部通过。

## 正确性与性能

完整运行均为 64 reset + 100,000 非 reset 周期，167,375 个提交事件、171,898 条退休指令，无异常或 trap。参考轨迹 SHA-256 为 `aac4bdad014633cadab8d50497e1555c60e05272eb091e0c3445c32237e6c3ae`。最终指纹：

- `full_data_fingerprint`: `10550482425419111625`
- `memory64_fingerprint`: `15503565812696885107`
- `memory8_fingerprint`: `656100811014868721`

UART 为 `Running CoreMark for 2 iterations\r\n`；固定 100K 前缀尚未完成 CoreMark，不报告 CoreMark 分数或完整 CRC。最佳三次的 `kernel_device_cycles_per_second` 为 274.2538、274.2711、274.1468；`simulation_loop_wall_seconds` 为 386.66、386.57、386.47 秒；加载时间约 0.57 秒。100K 屏选中默认配置约 261.60 cycles/s，top-1（128/256）263.55，top-2（256/256）263.58。完整 12 组 16,384 周期筛选数据和三条 stream 对照列于 `gpu-summary.json`。

Graph 与 stream 的前缀对照约为默认 258.2 vs 252.2、top-1 261.4 vs 256.4、top-2 261.1 vs 257.6 cycles/s。无融合配置约 138 cycles/s，开启融合阈值 64/128/256 的配置约 257–261 cycles/s。结合每周期 kernel 启动数从约 2,074 降至 159（阈值更高时进一步减少），数据支持启动/调度开销显著；此为开关对照推断，不是 profiler 结论。温度采样约 62–64°C，功耗约 66–67 W；未调整电压或功耗限制。

## 未执行项目

Compute Sanitizer 未安装，memcheck、racecheck、synccheck 未执行；没有运行 profiler，因此未获得 kernel 级归因。未对模拟器源码做性能优化，结论限于参数筛选范围。

复现完整最佳结果：在 VS2019 x64 开发者环境中，将项目内 `toolchain/cuda-12.6/bin` 放入当前进程 PATH，并运行：

```powershell
python candidate/run.py --binary candidate/build-win5/nanhu_gpu.exe --cycles 100000 --threads 128 --tail-threshold 256 --label repro-best
```
