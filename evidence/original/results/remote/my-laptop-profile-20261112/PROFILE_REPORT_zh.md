# my-laptop 串行/并行 GPU profiling 最终报告

RTX PRO 6000 Blackwell / sm120，driver 580.178.04，CUDA 13.2.78。完成两版三个256周期窗口的 Nsight Systems 与独立 CUPTI kernel/memcpy 交叉核验；硬件计数器受权限限制，不能给出 DRAM/L2 带宽、SM/warp 活跃度、occupancy 或 stall 归因。

## 代码、设备与正确性

工具：Nsight Systems 2025.6.3.541、Nsight Compute 2026.1.1、Compute Sanitizer 2026.1.1、CUPTI API version130201（cuptiGetVersion返回0）；均复用/usr/local/cuda-13.2已有组件，无需下载安装。

原实现 `d01b26619d0c58889ffdd7bce0491f4539a6ef29`；benchmark结果 `7f3868116c0a2016551bd96126d4ad92f82536d9`。独立目录构建，仅main增加默认关闭的NANHU_PROFILE_WINDOWS控制，CMake增加CUDA头文件路径；CUDA引擎/CPU引擎及运算、写入、调度未修改。具体diff、构建cache、原始CTest日志及binary SHA-256见closing目录。Graph / threads128 / tail_threshold256保持一致。

历史默认沙箱中nvidia-smi失败、/dev/nvidia不可见；正式require_escalated后设备正常可见。这是执行环境设备可见性差异，不能据此推断宿主驱动故障。没有改驱动、重启、创建设备节点或更改系统计数器策略。benchmark-ready完成后才采样，所有任务顺序运行。

两版修改后binary各一次完全unset NANHU_PROFILE_WINDOWS、无profiler的100K run.py检查均通过在线完整轨迹和最终指纹；两版最终CUPTI窗口运行也均通过同样检查。所有结果：167375 commit events、171898 retired、无异常/trap，最终指纹10550482425419111625 /15503565812696885107 /656100811014868721。原始CTest两版各6/6通过；此前短self-test的memcheck/racecheck/synccheck六次exit0，本轮不重复Sanitizer。

|关闭tracing对照|cycles/s|原benchmark中位数|差异|
|---|---:|---:|---:|
|baseline|472.128855|472.412085|-0.0600%|
|parallel|732.240506|733.147993|-0.1238%|

每版仅一次，不把环境温度、频率和运行顺序漂移直接归因于capture hook。开采样的evaluation_seconds包含cudaProfilerStop/flush开销，不能当测速，也不能当纯256周期窗口扰动；完整计时与10K区间计时保留在原始result.json。

## 时间线事实（独立CUPTI零丢失活动为主）

窗口按非reset周期零起始：[1024,1280)、[20000,20256)、[90000,90256)。各周期140 kernels：ordinal0 reset；1–134为[0,67) compute/commit；135为29层[67,96)；136/137为层96；138为939层suffix[97,1036)；139 gather。静态layers.bin重新解码得到同一schedule。

|窗口起点|串行commit µs/cycle|并行commit µs/cycle|commit减少|串行suffix µs/cycle|并行suffix µs/cycle|串行/并行suffix占kernel|
|---:|---:|---:|---:|---:|---:|---:|
|1024|1019.370|84.421|91.72%|786.464|787.377|37.40% / 67.38%|
|20000|1019.547|345.734|66.09%|785.922|787.230|37.38% / 55.04%|
|90000|1020.229|237.537|76.72%|787.155|786.898|37.40% / 59.54%|


|窗口起点|串行commit占kernel|并行commit占kernel|
|---:|---:|---:|
|1024|48.48%|7.22%|
|20000|48.49%|24.17%|
|90000|48.47%|17.97%|

|版本/窗口|compute|29层融合|reset/gather|D2H|图内正空隙|图到D2H|D2H到下周期|
|---|---:|---:|---:|---:|---:|---:|---:|
|baseline/1024|244.725|50.253|0.836/1.085|0.415|13.045|4.726|67.787|
|baseline/20000|245.105|50.233|0.835/1.085|0.417|13.023|4.207|67.639|
|baseline/90000|245.232|50.300|0.836/1.085|0.415|13.093|5.484|67.691|
|parallel/1024|244.568|50.346|0.837/1.087|0.416|13.031|5.148|67.560|
|parallel/20000|245.103|50.352|0.836/1.086|0.416|13.044|5.108|67.888|
|parallel/90000|244.909|50.322|0.837/1.088|0.416|13.052|5.301|67.588|

上表时间单位均µs/cycle，周期间空隙以255个间隔为分母。kernel占比以该窗口所有kernel duration之和为分母；融合段包含在窄层合计，不能重复相加。host API/synchronize与GPU工作重叠，不能加到GPU时间；逐窗口host API时间、相邻活动重叠量和逐ordinal数据见activity-comparison.json。

|慢层commit|串行早/中/后 µs|并行早/中/后 µs|
|---|---:|---:|
|[1,2)|93.657 / 93.693 / 93.749|1.395 / 58.145 / 17.587|
|[5,6)|221.749 / 221.681 / 221.967|1.298 / 148.169 / 108.793|
|[7,8)|119.740 / 119.740 / 119.860|1.219 / 33.810 / 19.704|

事实：并行commit大幅下降且呈阶段差异，939层suffix绝对时间接近不变，其相对kernel占比上升。推断：剩余GPU时间更集中在suffix；没有硬件计数器时不能断言显存带宽饱和、访问延迟主导或Compute Bound。Nsight launch资源表可见层5 commit串行22/并行40 registers/thread、829 blocks×128 threads；suffix40 registers/thread、1 block×128 threads。它们是launch/编译资源数据，不是实际SM活跃率或stall指标。

## 完整性与工具限制

六个CUPTI窗口各35840 kernel、256次4144-byte D2H，合计215040 kernel/1536 copy；所有buffer、stop context与global查询DroppedRecords为0，flush flags始终0，无强制不完整记录。解析、非零正时间戳、逐ordinal名称/graphNodeId/stream、每周期140个不同节点与D2H大小断言全部通过。Nsight六SQLite也满足对应数量/顺序，映射与独立CUPTI一致；跨运行graphNodeId数值不要求相同。

Nsight所有原始DIAGNOSTIC_EVENT保留，包括“Not all CUDA events might have been collected”、NVTX/OSRT及线程调度缺失告警。独立CUPTI只启用kernel/memcpy，因此零丢失只证明这些已启用活动在独立运行中的完整性，不消除Nsight所有事件类型的警告。两工具不同时运行，时间差还受instrumentation/频率漂移影响；不能将二者差值解释为优化。

初版CUPTI迭代器未初始化产生INVALID_KIND，被判失败并排除；按官方API初始化record=nullptr后，1280周期短smoke通过，再完成两版最终窗口采样。失败日志与源代码均保留。此前graph-only、整段node及stop只收首窗口等历史实验不用于本表结论。

## 硬件计数器权限

已读取NVIDIA官方ERR_NVGPUCTRPERM说明和Nsight Compute Application Replay文档，来源/校验见official-docs.json。原用户态NCU返回ERR_NVGPUCTRPERM。正式审批下仅检查 `sudo -n -l /usr/local/cuda-13.2/bin/ncu`，exit1，stderr为 `sudo: a password is required`；没有现成免密授权，因此停止管理员NCU操作，未输入密码、配置sudo、授予capability或修改策略。

精确过滤计划记录在ncu-planned-not-executed.json：同阶段非reset1024的层5 commit（matching skip73989）与suffix（skip2177），application replay/strict、每目标一个调用。该计划未执行，不能声称取得代表性硬件计数器；历史失败NCU的默认kernel replay速度不作端到端结果。

## 复现与交付

profiling/activity.cpp是最小CUPTI记录器；profiling/reproduce.sh记录构建、unset控制、CUPTI环境与run.py命令；profiling/analyze.py重建逐节点、copy/gap与完整性断言。closing保留原始活动CSV、完整验收JSON/日志、原始CTest、CMakeCache、source/binary hashes；evidence/windows保留六份Nsight原始报告与SQLite。未上传工具安装包或重复模型/参考轨迹。所有采样进程已退出。

## 本地独立核验

父 Agent 使用 [audit_closing.py](audit_closing.py) 从固定结果提交 `c06dccc30d058014eab67e978fa677352809626d` 直接读取 Git 对象复算：125 项清单 SHA-256 全部一致；源码与记录哈希一致，原实现的 CUDA/CPU 引擎、模型和 run.py 未修改。四份完整 100K 记录（两版关闭采样、两版 CUPTI）均与原模型的周期数、配置、在线比较状态、参考哈希及最终状态一致。六窗口合计 215040 条 kernel、1536 条 D2H 的数目、名称、顺序、graph node 与静态调度吻合，丢失计数查询均为零；commit 减少 91.718%、66.089%、76.717%，与表格一致。

本地核验检查上传记录，未下载远端可执行文件重新计算其哈希，也未在本机重跑 GPU。完整 actual.trace 保留在远端；run.py 通过记录表明运行时完成了完整轨迹比较，本地无法对未上传的轨迹独立重算。先前的 Nsight 六 SQLite 已在 [audit_windows.py](audit_windows.py) 中独立复算，原始告警继续保留。

[NVIDIA CUPTI Activity API](https://docs.nvidia.com/cupti/api/group__CUPTI__ACTIVITY__API.html) 说明丢失计数查询会清零计数，默认 flush 不强制返回未完成记录；本轮检查了每次已记录的 buffer、停止点和 global 查询，记录器使用 flush flags=0。零丢失结论限定于本次独立 CUPTI 运行所启用的 kernel/memcpy，不能扩展为所有 Nsight 事件均无丢失。

剩余性能优化的直接目标是 939 层 suffix：当前 small_layers 仅启动一个 128-thread block，逐层执行并在每层两次同步。该结构和耗时集中说明单次仿真的并行度受到限制；具体访存延迟、带宽利用率和指令停顿仍需获准的硬件计数器采样。尚未实现或测量新的 suffix 优化。
