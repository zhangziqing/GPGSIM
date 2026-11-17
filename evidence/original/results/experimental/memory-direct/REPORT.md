# Direct CUDA Memory Commit

Date: 2026-11-13

Direct memory commit passed the fixed Nanhu CoreMark 100K-cycle checks. Three alternating pairs measured **2,290.326 cycles/s** for direct stores versus **2,323.590 cycles/s** for candidate arbitration: direct throughput was **1.43% lower**. Removing the remaining candidate checks did not improve throughput in this configuration. This is a prefix experiment; it does not complete CoreMark or verify its CRC.

Follow-up profiling localized the regression to the commit kernels. An isolated control that only disables memory-loop unrolling recovered throughput to **2,351.359 cycles/s**, **2.66% above** the original direct version. The control patch remains separate from the accepted CUDA source.

## Implementation

CUDA source revision: `d4bb4fe2905d4e1aa7d28cc94c754a703d94755c`.

`--direct-memory-writes` selects a separate template instantiation of [commit](../nanhu-cuda-2060/src/engine_cuda.cu). Each active staged write stores directly into its word or byte pool. Both the full-scan and candidate-list conflict checks are compiled out of this instantiation. No candidate plan is allocated. Results record `direct_memory_writes` so the checked runner can confirm the requested mode.

The compute/commit layer boundary, scatter and small-layer barriers are preserved. Fused small layers already commit memory sequentially without conflict scanning. The existing ordered full-scan and candidate modes remain available as references, with their original conflicting-write tests. The new mode is explicit and is not the default.

Direct parallel stores assume that active writes in one layer address different locations in each backing pool. The earlier source audit did not prove this premise for the entire Nanhu design. The passing fixed-prefix runs establish agreement for these executions, without proving general absence of conflicts. Equal-value concurrent writes would also be insufficient to establish that premise.

## Local checks

CPU CMake build passed with GCC 13.3.0, C++17, `NANHU_ENABLE_CUDA=OFF`, `CMAKE_BUILD_TYPE=Release`. CTest: **4/4 passed**. Five invalid CLI combinations were rejected as expected. Python syntax and Git whitespace checks passed.

[memory_direct_test.cpp](../nanhu-cuda-2060/tests/memory_direct_test.cpp) exercises 14 and 1,282 disjoint writers over 24 cycles. An independent expected-memory calculation checks reset-dependent and toggling enables, masks, byte truncation, same numeric addresses in different pools, rotating addresses, reads before and after commit, and final data/memory fingerprints. CPU/CUDA snapshot comparison is included for GPU execution. Registered CUDA variants cover Graph with fusion, Graph without fusion and stream execution; all passed on corvus01.

The source/tool package contains 25 independently hash-checked files and the retained CPU oracle. It excludes executable model arrays, which the remote runner copied from the accepted mux-index-on package. Generated files are under ignored `runtime/`.

## GPU correctness and measurements

Execution used `/home/zhangziqing/memory-direct-20261113` on corvus01. Source and evidence were transferred through Paseo relay. The accepted model and earlier experiments were preserved. Only three CPU fingerprints were added to the copied model's expectation metadata; all executable arrays and other metadata match the retained mux-index-on export.

GPU: NVIDIA RTX PRO 6000 Blackwell Workstation Edition, UUID `GPU-67983fd2-eb55-ece7-a1ec-40165d7759fb`, driver 580.178.04. CUDA 13.2.78, sm_120, GCC 13.3.0, CMake 3.28.3; CPU affinity **2**, CUDA Graph, **threads=128**, **tail_threshold=256**. Both modes share the executable and model. The CMake build type is empty, matching the previous candidate experiment. The GPU was idle at preflight.

CUDA compilation passed and CTest passed **15/15**, including the existing conflicting-write controls for ordered arbitration and all three direct-write tests. CTest took 2.32 s; compilation wall time was not separately instrumented. Both modes passed a 1,024-cycle smoke check. All six full runs passed online cycle/commit/GPR/FPR/CSR/exception/trap/UART comparison, complete trace comparison and all data/mem64/mem8 fingerprints from the retained CPU oracle.

Each full run retired **171,898** instructions including fused instructions, with **167,375** commit events and no exception cycles. The trace SHA256 is `aac4bdad014633cadab8d50497e1555c60e05272eb091e0c3445c32237e6c3ae`. Both modes retain **2,218,090 operations, 205 logical layers, 144 kernel launches per cycle and 135 fused layers**.

| Round and order | Direct cycles/s | Candidates cycles/s |
| --- | ---: | ---: |
| 1: direct, candidates | 2,290.325593 | 2,324.084469 |
| 2: candidates, direct | 2,290.565965 | 2,323.589972 |
| 3: direct, candidates | 2,290.019007 | 2,321.583330 |
| Median | **2,290.325593** | **2,323.589972** |

| Median cost | Direct | Candidates |
| --- | ---: | ---: |
| 100K nonreset evaluation | 43.661914 s | 43.036853 s |
| CUDA-event interval | 42.667225 s | 42.164404 s |
| Model loading and initialization | 0.391894 s | 0.488201 s |
| Candidate-plan device bytes | 0 | 22,036 |

Evaluation includes dispatch, synchronization and compact observation copies, excluding model loading, reset, trace formatting/comparison and final fingerprints. CUDA-event intervals are separate measurements and may contain device gaps. The absence of candidate-plan construction reduces startup cost, without improving steady-state throughput here.

An additional static check of the measured binary's PTX found that `commit<true>` never loads the `View` fields for candidate offsets/later-writer arrays; `commit<false>` does. The source's `if constexpr` also removes the full-scan path from the direct instance. Both instantiated commit kernels use **40 registers**, with zero stack and local-memory allocation. The follow-up below investigates the throughput decrease with kernel timings and a controlled compilation change.

## Follow-up profiling and loop-unrolling control

Profiling used the same corvus01 GPU, CUDA 13.2.78, sm_120, CPU 2, Graph schedule, 128 threads, threshold 256 and empty CMake build type as the measurements above. The source was copied into `profile-20261113/` under the original remote experiment directory. Host callbacks enabled CUPTI collection in three windows; all six device kernels have identical SASS instruction encodings in the original and profiling builds. Two unprofiled controls reproduced the regression: **2,290.762 cycles/s direct** and **2,323.040 cycles/s candidates**.

Four full runs collected CUPTI timelines, two per mode with reversed order. Each window covers 256 nonreset cycles, with 36,864 kernels and 256 observation copies; all windows have zero dropped records. The following values sum the commit kernels and divide by 256, in microseconds per simulated cycle. Direct and candidate columns average their two profiles; the final column comes from one separate no-unroll profile.

| Nonreset cycle window, end exclusive | Direct commit | Candidate commit | Direct, unrolling disabled |
| --- | ---: | ---: | ---: |
| 1,024 to 1,280 | 84.205 | 78.661 | **77.426** |
| 20,000 to 20,256 | 84.431 | 82.221 | **77.746** |
| 90,000 to 90,256 | 84.343 | 80.478 | **77.581** |

Compute remains approximately **192 us/cycle**, suffix **99.8 us/cycle**, and gaps between kernels within a cycle **14.1 us/cycle**. Their direct/candidate differences are small compared with the commit difference. Kernel times exclude copies, host work and gaps. CUPTI adds overhead to the full run, so profiled whole-run throughput is not used for the performance comparison.

The generated direct memory loop was automatically unrolled after arbitration was compiled out. Its repeated write bodies include runtime trip-count and remainder handling. Static disassembly shows the following changes; instruction counts describe the compiled kernel, not instructions dynamically executed by each thread.

| Commit template | Static SASS instructions | Reciprocal instructions | Registers | Stack/local bytes |
| --- | ---: | ---: | ---: | ---: |
| Ordered/candidate, original | 320 | 2 | 40 | 0/0 |
| Direct, original | 408 | 3 | 40 | 0/0 |
| Direct, unrolling disabled | **248** | 2 | 40 | 0/0 |

The isolated [no-unroll.patch](no-unroll.patch) adds only `#pragma unroll 1` immediately before the memory-write loop. NVIDIA documents this directive as disabling loop unrolling in the [CUDA C++ Programming Guide](https://docs.nvidia.com/cuda/archive/12.8.1/cuda-c-programming-guide/index.html#pragma-unroll). Build flags match, and only the direct commit kernel changes SASS; the other five kernels, including ordered commit, remain identical. Scatter, memory stores, enables and conflict assumptions are unchanged.

Three alternating unprofiled pairs compare the original profiling executable against the one-pragma variant:

| Round and order | Original direct cycles/s | Direct, unrolling disabled cycles/s |
| --- | ---: | ---: |
| 1: original, no-unroll | 2,290.440637 | 2,351.358522 |
| 2: no-unroll, original | 2,290.206205 | 2,352.575516 |
| 3: original, no-unroll | 2,290.360954 | 2,350.638892 |
| Median | **2,290.360954** | **2,351.358522** |

Disabling unrolling improves median throughput by **2.663%** and reduces sampled commit time by approximately **8%** in all three windows. It is also **1.195% above the earlier candidate median**, although that comparison uses a separate set of runs. The single-change control supports compiler loop generation as the cause of the observed regression. Hardware counters are unavailable: Nsight Compute returned **`ERR_NVGPUCTRPERM`**. No bandwidth, cache-miss or stall-counter explanation is established.

Both new CUDA builds passed **15/15 CTests**. All **13 additional full 100K runs** across the profiling and unrolling experiments passed online reference comparison, full trace comparison and all three CPU state fingerprints. These checks preserve the original fixed-prefix scope and do not prove whole-design conflict freedom. Compilation logs are retained; build wall time was not separately instrumented. The accepted runtime source has not been changed by this diagnostic control.

## Evidence and reproduction

[checks.json](checks.json) was generated by the local independent audit. It verified **70 archived files**, raw results and traces for all six full GPU runs and both smoke runs, source hashes, requested mode flags, identical model metadata, the CPU oracle and all 15 CTests. The five relay parts and the complete 9,436,771-byte evidence archive were hash checked; archive SHA256 is `0f603d256f80dbb354512e830f7606b665eaec47de2ffea3990c80a50a1c885b`. The pipeline exited with status 0.

Use the existing Paseo relay to transfer a new source package into a fresh directory on corvus01. The configured CUDA 13.2 toolkit and sm_120 GPU are required. Preserve the accepted model and earlier experiments.

```bash
python3 results/experimental/memory-direct/direct_experiment.py pack \
  --output results/experimental/memory-direct/runtime/new-inputs.tar.gz
```

After transfer and extraction on the GPU host, record GPU/toolchain preflight information as `$DIRECT_ROOT/preflight.json` and run:

```bash
taskset -c 2 python3 -u "$DIRECT_ROOT/direct_experiment.py" run \
  --root "$DIRECT_ROOT" \
  --model-package /home/zhangziqing/mux-index-20261113/on
```

The runner checks all transferred hashes, reuses the existing model/oracle preparation, builds CUDA, runs all 15 CTests, then checks a 1,024-cycle smoke prefix in each mode. Three alternating direct/candidate pairs each run 100K nonreset cycles after 64 reset cycles, using CUDA Graph, 128 threads and threshold 256. Each full run must pass online Verilator observation comparison, complete trace hash comparison and all three retained CPU state fingerprints. A mismatch stops the run and preserves its output.

The runner collects source manifests, raw traces, raw results, build/test logs and model metadata into a hashed archive. After downloading its receipt and parts through the relay:

```bash
python3 results/experimental/memory-direct/direct_experiment.py audit \
  --root results/experimental/memory-direct/runtime
```

Full CoreMark completion/CRC, formal whole-design equivalence and Compute Sanitizer are **not run**. Hardware-counter collection was attempted in the follow-up and denied as described above.

Follow-up [profile-checks.json](profile-checks.json) verifies **69 files and six full runs**, with raw CUPTI timelines, controls, flags, traces and fingerprints. Its 14,237,227-byte archive SHA256 is `2eee79e55ec6570e6b02fbfd66af327bad2d42a94ce7d6c15b5af3751f23c4f1`. [unroll-checks.json](unroll-checks.json) verifies **57 files and seven full runs**, the exact one-pragma source difference, matching flags, CTests, traces, fingerprints and recomputed timing windows. Its 12,157,118-byte archive SHA256 is `cb7d3b28b59263e5bfa73261d70031be12be9115a79bc1f13533157c6a8d1e67`. Relay parts and both complete archives were hash checked before local auditing. Raw evidence remains in ignored `runtime/profile/` and `runtime/unroll/`.

Transfer [profile_direct.py](profile_direct.py), [probe_unroll.py](probe_unroll.py), [inspect_unroll.py](inspect_unroll.py), [no-unroll.patch](no-unroll.patch) and the existing mux-index profiling support into the authorized remote experiment. Record preflight information as `$DIRECT_PROFILE/preflight.json`. Use fresh work directories for each run:

```bash
/usr/local/cuda-13.2/bin/cuobjdump --dump-sass \
  "$DIRECT_ROOT/build/nanhu_gpu" > "$DIRECT_PROFILE/sass.txt"
/usr/local/cuda-13.2/bin/cuobjdump --dump-resource-usage \
  "$DIRECT_ROOT/build/nanhu_gpu" > "$DIRECT_PROFILE/resources.txt"
if /usr/local/cuda-13.2/bin/ncu --set basic --launch-count 1 \
  --kernel-name regex:commit "$DIRECT_ROOT/build/nanhu_gpu" --self-test \
  > "$DIRECT_PROFILE/ncu-probe.log" 2>&1; then
  printf '0\n' > "$DIRECT_PROFILE/ncu-probe.exit"
else
  printf '%s\n' "$?" > "$DIRECT_PROFILE/ncu-probe.exit"
fi
taskset -c 2 python3 "$DIRECT_PROFILE/profile_direct.py" run \
  --root "$DIRECT_ROOT" --work "$DIRECT_PROFILE"
taskset -c 2 python3 "$DIRECT_ROOT/probe_unroll.py" \
  --root "$DIRECT_ROOT" --profile "$DIRECT_PROFILE" --work "$UNROLL_WORK"
python3 "$DIRECT_ROOT/inspect_unroll.py" collect \
  --root "$DIRECT_ROOT" --profile "$DIRECT_PROFILE" --work "$UNROLL_WORK"
```

After downloading the receipts and archives through the relay, audit into directories that do not yet contain an extracted `evidence/` directory:

```bash
python3 results/experimental/memory-direct/profile_direct.py audit \
  --work results/experimental/memory-direct/runtime/profile
python3 results/experimental/memory-direct/inspect_unroll.py audit \
  --work results/experimental/memory-direct/runtime/unroll
```
