# FTQ Mux Index Normalization

Date: 2026-11-13

Status: compiler, minimal RU pipeline, full 100K-cycle correctness, and GPU checks passed. Three alternating GPU pairs measured **737.100 → 1,486.200 cycles/s**, a **2.01628×** ratio of medians. The final suffix fell from approximately **788 to 100 µs/cycle** in independent CUPTI windows. These results cover the fixed CoreMark 100K-cycle prefix; CoreMark has not finished at that boundary, so this is not a completed CoreMark score or CRC validation.

Compilation and CPU validation ran on `node007.bosccluster.com`. GPU execution ran on **`corvus01`**, shown as `my-laptop` in Paseo, under `/home/zhangziqing/mux-index-20261113`. Packages and evidence used **Paseo relay**, after the configured SSH connection failed. SSH was not used for this transfer.

## Change

`NormalizeMuxIndex` is available behind `--normalize-mux-index` and remains disabled by default. It folds dense, mutually exclusive two-dimensional equality selectors into a shared concatenated index and scalar comparisons, allowing the existing mux-table extraction and opcode 24 path to consume the lookup. It introduces no runtime opcode. Reference counting now walks flattened FIRRTL statements so equality nodes observed outside the mux chain remain present.

The 64-by-16 test checks all 1,024 indices against an independent array lookup with two 8-bit state patterns and a default branch. Other cases cover 1+1, 2+3 and 4+1-bit indices, 1/8/17-bit output data, arbitrary defaults, a complete domain with an unreachable default, swapped equality operands, sparse and repeated conditions, shared comparisons, repeated execution, FIRRTL type and width checks, and StatementGraph/Renamer construction.

This first version recognizes chains represented by `DefNode` muxes with known UInt selector widths whose sum is at most 16. It requires distinct cases covering the whole index domain or all but one entry. It preserves mux ordering and the default expression. A mux inlined directly into a FIRRTL output `Connect` can leave a shorter node chain and is skipped; the minimal fixture uses an internal lookup followed by an observable XOR, matching the internal-node form in FTQ.

The measured compiler revision is `12f0e00fadda98a5d0b343a868332c941e7c572a`. Revision `d863ac5461d335c9813e1c658d7570f8d21c75af` only extends tests; its compiler and kernel sources are identical to the measured revision. The workspace base was `39e9d76`. These simulator commits are local; the upstream submodule URL is not established to contain them.

## Nanhu Structure

Both variants used `results/nanhu/SimTop.preclock.lo.fir`, the same preserved-signal list, the chain-index override jar, and an overlay containing the current compiler entry points. Class-load logs confirm `EssentCompiler` and `ArgsParser` came from `compiler-overlay.jar`.

| Metric | Off | On |
| --- | ---: | ---: |
| Tensor operations | 2,220,712 | 2,218,090 |
| Layers | 1,036 | 205 |
| Opcode 24 mux-table operations | 4,295 | 4,403 |
| Mux-table entries | 196,792 | 198,808 |
| 1,024-entry opcode 24 tables | 0 | 1 |
| NormalizeMuxIndex chains / muxes | 0 / 0 | 2 / 1,150 |
| Compile wall time | 27:34.23 | 18:06.65 |
| Peak RSS | 80,948,172 KiB | 55,000,032 KiB |

Tensor preprocessing and static-layout validation passed. The graph has unique scatter destinations within each layer, memwrite placeholders are overwritten before a subsequent read, and the named memory mappings match between variants. Scatter uniqueness does not establish that dynamic memory addresses cannot alias. The layer count is measured output; it was not assumed in advance.

The generated header records the FTQ UInt<10> index at `l_soc$core_with_l2$core$frontend$ftq$__rteaal_mux_index`, concatenating `commPtr_value` and `can_commit_cfi_bits`. A second UInt<7> index belongs to LFST. The header is emitted before tensor extraction; elimination of the FTQ-sized serial chain is established by the final opcode-24 table, final graph depth, and exhaustive minimal execution. The final Nanhu graph has 205 layers, not 10.

## Correctness

`bash ../tools/sbt test` passed all **54 tests in 9 suites**. Java 21.0.13.0.3, sbt 1.9.6, Scala 2.13.12 and FIRRTL 1.5.6 were used.

`check_minimal.py` compiled the internal 64×16 lookup with normalization off, on, and on through pre-clock replay, then preprocessed and executed each with the upstream RU evaluator. Each variant checked all 1,024 indices with three independent data patterns, including the default and shared comparison probes: **9,216 successful RU checks** in total. Off produced 2,129 operations / 1,026 layers / no tables; both enabled paths produced **7 operations / 3 layers / one opcode-24 table**. These counts include the observable XOR and probes. The first output-connected fixture was retained in `runtime/minimal-attempt1`; it correctly executed but did not match this pass's node-chain requirement.

Each variant ran the fixed 100,000-cycle CoreMark prefix against the existing Verilator trace. Both online cycle checks and complete trace byte comparisons passed; the shared trace SHA256 is `aac4bdad014633cadab8d50497e1555c60e05272eb091e0c3445c32237e6c3ae`. Both variants retired 171,898 instructions including fused instructions, recorded 167,375 commits, and saw zero exception cycles with trap code 0. Final named architectural state matched the Verilator result, and named memory mappings and final contents matched between off and on. Whole data-buffer fingerprints are not compared across variants because slot allocation differs.

Independent portable CPU exports also completed both 100K runs. All six throughput trials and four profiling/control runs matched the corresponding CPU export's complete data/mem64/mem8 fingerprints, the reference trace, and named architectural state. Both CUDA packages passed **6/6 CTests** and a 1,024-cycle GPU smoke check before the paired trials. The profiling build separately passed 6/6 CTests.

Graph controls checked **600 register writers** against all old-state readers and the required anti-overwrite dependencies. Memory dependency sets matched the original construction for **600 writers with Nanhu disabled and another 600 enabled**. These controls and the finite prefix are verification evidence, not a proof of equivalence for every workload.

## Scheduling and Performance

Both packages used the same **NVIDIA RTX PRO 6000 Blackwell Workstation Edition**, UUID `GPU-67983fd2-eb55-ece7-a1ec-40165d7759fb`, 97,887 MiB, driver 580.178.04, CUDA 13.2.78, `sm_120`, GCC 13.3.0 and CMake 3.28.3. CPU affinity was fixed to CPU **2**. Runs used CUDA Graph, `threads=128`, `tail_threshold=256`, and 100,000 measured cycles after reset. The execution implementation retained parallel memory commit.

The paired order was off/on, on/off, off/on. Throughput samples were:

| Round | Off cycles/s | On cycles/s |
| --- | ---: | ---: |
| 1 | 737.099678 | 1,486.200199 |
| 2 | 737.123931 | 1,486.665177 |
| 3 | 736.738250 | 1,484.954663 |
| Median | **737.099678** | **1,486.200199** |

The median ratio is **2.016281×**, or **101.6281% greater throughput**. Independent correctness acceptance and measured performance improvement both passed.

| Timing or schedule metric | Off | On |
| --- | ---: | ---: |
| Median evaluation seconds | 135.666862 | 67.285686 |
| Median device-kernel seconds | 134.784501 | 66.431673 |
| Median final 10K-cycle evaluation window seconds | 14.069269 | 7.229853 |
| Kernel launches per cycle | 144 | 144 |
| Fused logical layers per cycle | 966 | 135 |
| Final suffix logical layers / nodes | 939 / 2,955 | 108 / 1,038 |

The final 10K evaluation window includes all evaluation work in those cycles; it is not the suffix kernel duration. Launch count remained 144 because the scheduler already fused the long tail. Normalization reduced serial work inside the suffix kernel.

Separate CUPTI activity runs sampled three 256-cycle nonreset windows:

| Cycles [begin, end) | Off suffix µs/cycle | On suffix µs/cycle | Reduction |
| --- | ---: | ---: | ---: |
| [1,024, 1,280) | 787.867973 | 99.832598 | 87.3288% |
| [20,000, 20,256) | 787.674219 | 99.828496 | 87.3262% |
| [90,000, 90,256) | 787.822797 | 99.838281 | 87.3273% |

Each window contained **36,864 kernels**, **256 D2H copies of 4,144 bytes**, and **zero dropped records**. The six windows cover 221,184 kernels and 1,536 copies. Suffix share of kernel activity was 55.54%–70.75% off and 12.92%–22.98% on. Durations are CUPTI kernel end minus start, excluding device gaps, host work and copies; the six unprofiled trials establish throughput.

The profiling binary preserved identical CUDA/CPU engine, operation and observer sources, adding only optional sampling-window controls. It used CMake `Release`, while throughput builds used the CMake default build type. Its single unprofiled control runs differed from the throughput medians by −0.06736% off and +0.04610% on. This does not isolate build flags, instrumentation or clock drift. The recorder synchronizes before stop, uses ordinary activity flushes, and checks buffer/context/global drop counts, following the [NVIDIA CUPTI Activity API](https://docs.nvidia.com/cupti/api/group__CUPTI__ACTIVITY__API.html).

### Optimized graph follow-up

A requested follow-up on 2026-11-13 profiled the enabled graph again, under the same GPU, CPU 2, CUDA Graph, threads=128 and threshold=256. Its CMake build type was empty, matching the throughput experiment; the recorded CXX/CUDA flags and defines match exactly. The engine and operation sources also match. Earlier profiling artifacts were retained, and this run used `profiling-followup-20261113a` and labels `mux-followup-20261113a-{control,cupti}`.

The new unprofiled control completed 100K cycles at **1,486.839351 cycles/s**, **+0.04301%** from the original enabled median. The instrumented run completed at 1,363.482910 cycles/s; its throughput includes profiling overhead and does not replace the six original throughput samples. Both full traces and named final state passed; each complete data/mem64/mem8 fingerprint independently matched the corresponding portable CPU export. CUDA CTest passed 6/6.

| Nonreset cycle window | Compute µs/cycle (kernel share) | Commit µs/cycle (kernel share) | Other fused layers µs/cycle | Suffix µs/cycle (kernel share) |
| --- | ---: | ---: | ---: | ---: |
| [1,024, 1,280) | 191.855 (44.20%) | 91.713 (21.13%) | 48.727 | 99.764 (22.98%) |
| [20,000, 20,256) | 192.185 (24.91%) | **429.016 (55.60%)** | 48.619 | 99.760 (12.93%) |
| [90,000, 90,256) | 191.941 (29.25%) | **313.955 (47.84%)** | 48.609 | 99.761 (15.20%) |

The remaining middle/late hotspot is **commit**, whose time changes with the workload phase. Early in the run, aggregate compute is largest and the suffix remains the largest single kernel. Compute and suffix timing are stable across these windows. There is no single measured percentage for the whole 100K trajectory; each percentage above uses the kernel activity in its own sampled window.

| Selected launch / zero-based logical layer | Outputs | Static memory-write nodes | At 20K µs/cycle | At 90K µs/cycle |
| --- | ---: | ---: | ---: | ---: |
| Commit ordinal 12 / layer 5 | 137,352 | 1,249 | **152.910** | **112.911** |
| Commit ordinal 16 / layer 7 | 83,499 | 720 | 97.287 | 72.615 |
| Commit ordinal 4 / layer 1 | 115,033 | 524 | 55.377 | 25.542 |
| Suffix ordinal 142 / layers [97, 205) | 1,038 | 0 | 99.760 | 99.761 |

The commit kernel combines memory-write arbitration and scatter. In the preserved `src/ops.h`, `is_final_memory_write` scans later write records for each active writer to retain last-writer priority; this is a candidate for the phase-dependent cost. The static writer counts are not active-writer counts. These timings do not separate arbitration from scatter or establish that either alone causes the measured cost. The next focused experiment would split those two costs before changing arbitration or layout, while preserving same-cycle write priority and register anti-overwrite dependencies.

Each new sampling window again contains 36,864 kernels, 256 D2H copies of 4,144 bytes, and zero dropped records: **110,592 kernel records and 768 copies** total. These are kernel end-start durations, excluding gaps, host work and copies. Nsight Compute 2026.1.1 was attempted on a separate self-test and returned **`ERR_NVGPUCTRPERM`**; the driver reports `RmProfilingAdminOnly: 1`. Hardware counters, occupancy and bandwidth saturation were therefore **not measured**. No driver permission settings were changed.

`inspect_profile.py` independently checked **34 archived evidence files**, both actual 100K traces, raw versus summarized results, CPU final fingerprints, identical model/schedule/engine sources, build flags, CTest and raw CUPTI windows. The archive is 4,326,263 bytes with SHA256 `a6bb2d20ac1aa605b89453ae672149e00822ca992325dfce9a29c1e8068341c4`; all three relay parts and the complete archive were hash checked. Full category timings and the five slowest launches per window are in [profile-breakdown.json](profile-breakdown.json). This follow-up is additional evidence, separate from the original ten GPU runs.

The subsequent [static memory candidate experiment](../memory-candidates/REPORT.md) preserves this optimized graph and its last-writer rule. Three new alternating pairs measured 1,483.981 to 2,323.590 cycles/s (1.565782x), with all 100K checks passing. Middle-window commit fell from 430.328 to 82.300 us/cycle while compute and suffix stayed stable. These new results are separate from the compiler-normalization results above.

The CPU correctness runner reported 213.67 cycles/s off and 206.66 cycles/s on in one run per variant. These single CPU samples are recorded for provenance and are not a controlled speed comparison.

`audit_gpu.py` independently verified all **10 actual 100K trace files**, raw output versus runner JSON, corresponding CPU state fingerprints, named state, input/model/binary provenance, alternating run order, medians, CTest records, CUPTI scheduling and drop counts, and Scala/graph controls. The downloaded archive contains 91 manifest-checked evidence files plus its manifest: 17,853,205 bytes, SHA256 `01addc2d797789fccc063a848657425c01d35bd602208ae6d24d50bad2618184`. Relay chunks and the complete archive were hash checked after download. Machine-readable results are in [checks.json](checks.json) and [gpu-checks.json](gpu-checks.json).

Compute Sanitizer, formal whole-design equivalence, and complete CoreMark CRC: **not run** in this experiment. The initial paired experiment did not collect hardware counters; the follow-up counter probe was denied as recorded above. The measurements do not establish bandwidth saturation or a specific hardware resource bottleneck.

## Reproduction

Prerequisites include the retained Nanhu pre-clock FIRRTL, preserved signal list, baseline jar, chain-index and FIRRTL-access override jars, original CoreMark image and Verilator prefix, local C++/yaml-cpp/FESVR dependencies, and the isolated CUDA implementation under `nanhu-cuda-2060`. These generated inputs are intentionally absent from Git. Use a new experiment runtime directory when repeating package preparation; it refuses to overwrite an existing GPU package.

On the compilation host:

```bash
bash results/experimental/mux-index/compile.sh --mode both
python3 results/experimental/mux-index/check_minimal.py
bash results/experimental/mux-index/check_graph_contracts.sh
python3 results/experimental/mux-index/prepare_experiment.py
python3 results/experimental/mux-index/run_coremark.py
```

`compile.sh` runs `bash ../tools/sbt test` and builds the overlay ahead of the old Compiler bundled in chain-index. The minimal check uses current compiled classes. `prepare_experiment.py` recomputes layouts, port maps, arguments, scatter and schedules for both new graphs; `run_coremark.py` writes the portable correctness gate after both CPU checks pass. To recreate the additional portable CPU oracle used by the audit, configure each package with `-DNANHU_ENABLE_CUDA=OFF`, build it, then run its `run.py --cycles 100000 --label cpu-export-100k`.

Copy the isolated `runtime/gpu/{off,on}` packages and `correctness-gate.json`, plus `run_gpu_trials.py`, `profile_gpu.py`, `collect_gpu_evidence.py` and `profiling/`, to one GPU directory. Preserve a `transfer-manifest.json` mapping each transferred relative input path to its byte size and SHA256. On the GPU host, with that directory denoted by `GPU_ROOT`:

```bash
set -o pipefail
taskset -c 2 python3 run_gpu_trials.py --gpu-root "$GPU_ROOT" --cuda-architecture 120 2>&1 | tee "$GPU_ROOT/gpu-run.log"
taskset -c 2 python3 profile_gpu.py --gpu-root "$GPU_ROOT" --cuda-root /usr/local/cuda-13.2 --cuda-architecture 120 2>&1 | tee "$GPU_ROOT/profile-run.log"
python3 collect_gpu_evidence.py --gpu-root "$GPU_ROOT"
```

The collector creates a new `evidence-capture/` directory and 2 MiB parts with receipts; it selects the same 91 paths as the original capture. After transferring and hash checking the archive and receipt into local `runtime/`, run `python3 results/experimental/mux-index/audit_gpu.py`. The original capture is preserved independently from this collector's new output directory.

The follow-up used a copy of the updated `profile_gpu.py` named `profile_gpu_followup.py`. Its actual profiling command was:

```bash
taskset -c 2 python3 profile_gpu_followup.py --gpu-root "$GPU_ROOT" --mode on \
  --work-dir "$GPU_ROOT/profiling-followup-20261113a" \
  --run-prefix mux-followup-20261113a --build-type ''
```

`--work-dir` and `--run-prefix` must be fresh for another run. `inspect_profile.py collect --gpu-root "$GPU_ROOT" --work-dir "$GPU_ROOT/profiling-followup-20261113a"` captures this follow-up's result, build, profiler-tool/probe and preflight records and splits its evidence archive for transfer. Locally, `python3 results/experimental/mux-index/inspect_profile.py inspect` verifies the downloaded `runtime/profile-followup-evidence.tar.gz` against `runtime/profile-followup-receipt.json` and regenerates `profile-breakdown.json`.

Generated FIRRTL, tensor data, binaries, logs, layouts, raw traces, and downloaded evidence remain under the ignored `runtime/` or `logs/` directories. The compiler source and tests are committed in `RTeAAL-Sim`; this report, summaries and experiment sources belong to the workspace repository. Original tensors, jars, reports and RTL exports were retained.
