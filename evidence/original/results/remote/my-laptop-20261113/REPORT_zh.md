# Nanhu CUDA 并行提交验证报告

- 固定实现：`portable-20261112-commit-parallel` / `d01b26619d0c58889ffdd7bce0491f4539a6ef29`
- GPU：NVIDIA RTX PRO 6000 Blackwell Workstation Edition，97,887 MiB
- 驱动：580.178.04；CUDA Toolkit：13.2.78；架构：`sm_120`
- 主机编译器：GNU C++ 13.3.0；CMake：3.28.3
- 参数：Graph、`threads=128`、`tail_threshold=256`

并行候选和串行基线均以相同模型、数据和参数构建。两版 CTest 的 6/6 项通过，1024 周期 CUDA Graph 在线参考烟测通过。两版各三次 100K 非 reset 周期运行均通过在线逐周期比较，提交事件 `167375`、退休指令 `171898`、异常/trap 为零，三项最终指纹均为：`10550482425419111625`、`15503565812696885107`、`656100811014868721`。

| 版本 | cycles/s 中位数（范围） | kernel cycles/s 中位数 | loop wall 中位数 | 相对串行 |
|---|---:|---:|---:|---:|
| 串行基线 | 472.412（472.212–472.564） | 474.597 | 212.174 s | 1.000x |
| 并行候选 | 733.148（732.813–733.253） | 737.689 | 136.835 s | 1.552x |

二进制 SHA-256：

- `build-baseline/nanhu_gpu`: `eabb859a619e6892d954744bb413668f064e254da089d5585f92631e59d5bf7a`
- `build-parallel/nanhu_gpu`: `a6f280dc2b9b394983be45e28547c24f73c945dab0595d8336a9e9aaa128bc9d`

每次运行的 `command.json`、`result.json`、stdout/stderr 日志和规范化轨迹 SHA-256 已保留在对应 `runs/` 目录；完整参考/实际轨迹未重复提交。原始 CMake 配置、CTest `LastTest.log`、构建缓存和执行权限模式已补在 `build-logs/`。Compute Sanitizer 二进制存在于 `/usr/local/cuda-13.2/bin/compute-sanitizer`，但不在当时默认 PATH；本次未执行 memcheck、racecheck、synccheck。100K 是 CoreMark 固定前缀，不是完整 CoreMark 分数。
