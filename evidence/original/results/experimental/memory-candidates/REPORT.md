# Static Memory Writer Candidates

Date: 2026-11-13

The optional candidate-list path passed the fixed 100K-cycle CoreMark checks and improved median GPU throughput from **1,483.981 to 2,323.590 cycles/s (1.565782x, +56.58%)**. Three alternating pairs used the same tensor model, executable, GPU, launch schedule and settings. This is a prefix measurement; CoreMark completion and CRC were not tested.

## Implementation and semantic boundary

[memory_plan.cpp](../nanhu-cuda-2060/src/memory_plan.cpp) builds CSR lists once during loading. Each writer retains only later writers in its layer whose inclusive `[uint64(base), uint64(base)+address_mask]` intervals overlap in the same backing pool. `kind == 0` selects the word pool; every nonzero kind selects the byte pool. The interval test is conservative for non-contiguous masks and handles sums beyond UINT32_MAX.

An enable or mask is permanently zero only when its initial value is zero, no scatter targets its slot, and it is not the externally mutable reset slot. This proof depends on the current runtime contract: only reset is supplied externally to the data slots. Other runtime inputs would have to be included in the mutable set before reusing this exclusion.

`--memory-candidates` enables the new CUDA path; the default retains the full scan. Runtime checks still require the later writer to be active and to address the same location. Only the last active writer stores, including when values are equal. The CPU oracle and fused small-layer sequential commit are unchanged. No architectural mutual-exclusion assumptions, signal names or special layer numbers are used.

| Static property | Value |
| --- | ---: |
| Memory write records | 5,428 |
| Proven permanently inactive | 752 |
| Remaining potentially active writers | 4,676 |
| All same-layer ordered-priority pairs before filtering | 1,344,905 |
| Candidate pairs retained | 80 |
| Additional device arrays | 22,036 bytes |
| Median candidate-plan construction | 93.145 ms |

The pair counts describe possible checks, not dynamic active counts or observed same-address collisions. Layers 5 and 7 have no surviving candidate pairs. Layer 1 retains 46 pairs, including equal-value writes, under the original priority rule.

The measured CUDA sources are workspace revision `3aad1ab43cc7860b1faba2268bf3c321c1b06ee3`, following planner revision `47ea0c2`. The simulator remains `d863ac5461d335c9813e1c658d7570f8d21c75af`; no Scala or compiler change was needed. The source manifest records every transferred source hash.

## Correctness and isolation

Execution used `/home/zhangziqing/memory-candidates-20261113` on **corvus01**, with sources and evidence transferred through **Paseo relay**. The original mux-index packages, binaries, reports and traces were preserved. All executable model arrays are identical to the accepted mux-index-on export; only the copied model's `expected_100k` metadata adds three fingerprints from the retained independent CPU run.

Local CPU checks passed **3/3**. CUDA builds passed **11/11 CTests**, covering both arbitration modes with opcode, Graph, unfused and stream paths. The planner test checks interval/layer boundaries, distinct pools including kind 2, immutable and mutable zero slots, empty plans, deterministic output, and 64 randomized models over 32 cycles against sequential memory commit and full-scan winner flags.

Each mode passed a 1,024-cycle smoke check and a dedicated 100K-cycle correctness run before timing. All six timing runs and four profile/control runs also passed online cycle/commit/GPR/FPR/CSR/exception/trap/UART comparison, full trace byte comparison, and the CPU oracle's complete data/mem64/mem8 fingerprints. These 12 full runs each retired 171,898 instructions including fused instructions, with 167,375 commit events and no exception cycles. The checked trace SHA256 is `aac4bdad014633cadab8d50497e1555c60e05272eb091e0c3445c32237e6c3ae`.

Both modes retain **2,218,090 operations, 205 logical layers, 144 kernel launches per cycle and 135 fused logical layers**. Existing graph dependencies, layout, scatter, ports and register anti-overwrite constraints are preserved by using the same hashed arrays. This experiment does not claim that graph scatter uniqueness proves dynamic memory-address uniqueness.

## GPU performance

GPU: NVIDIA RTX PRO 6000 Blackwell Workstation Edition, UUID `GPU-67983fd2-eb55-ece7-a1ec-40165d7759fb`, 97,887 MiB, driver 580.178.04. CUDA 13.2.78, sm_120, GCC 13.3.0, CMake 3.28.3; CPU affinity **2**, CUDA Graph, **threads=128**, **tail_threshold=256**. Both throughput and profiling builds use the empty CMake build type and matching CXX/CUDA flags.

| Round | Full scan cycles/s | Candidates cycles/s |
| --- | ---: | ---: |
| 1 (scan, candidates) | 1,483.921468 | 2,323.589507 |
| 2 (candidates, scan) | 1,484.015794 | 2,323.765129 |
| 3 (scan, candidates) | 1,483.980675 | 2,322.641423 |
| Median | **1,483.980675** | **2,323.589507** |

| Median cost | Full scan | Candidates |
| --- | ---: | ---: |
| 100K nonreset evaluation | 67.386322 s | 43.036862 s |
| CUDA-event interval | 66.547154 s | 42.181639 s |
| Model loading and initialization | 0.402369 s | 0.494389 s |
| Candidate construction within loading | 0 s | 0.093145 s |

Evaluation time includes dispatch, synchronization and compact observation copy; it excludes loading, reset, trace formatting/comparison and final fingerprinting. CUDA-event intervals are reported separately and include device gaps. Explicit device-array allocation changes from 1,156,240,756 to 1,156,262,792 bytes, excluding context and Graph overhead. The measured fresh trial build and tests took 8.570 s; this is separate from loading and simulation.

## CUPTI follow-up

Separate full runs sampled the same three 256-cycle nonreset windows used previously. Each window contains 36,864 kernels and 256 D2H copies of 4,144 bytes with zero dropped records. The table reports kernel end-start durations, excluding gaps, host work and copies.

| Nonreset cycles [begin,end) | Scan commit us/cycle | Candidates commit us/cycle | Reduction | Scan / candidates suffix us/cycle |
| --- | ---: | ---: | ---: | ---: |
| [1024,1280) | 92.619 | 78.754 | 14.97% | 99.846 / 99.837 |
| [20000,20256) | 430.328 | 82.300 | 80.88% | 99.843 / 99.838 |
| [90000,90256) | 315.130 | 80.556 | 74.44% | 99.845 / 99.840 |

Aggregate compute remains approximately 192 us/cycle. The large middle/late commit variation falls to a 78.8–82.3 us range across the sampled windows. With only arbitration changed, this supports the later-writer scan as the main cause of the previous excess commit cost; it does not establish that active writes actually collided. Dynamic enables can change scanning work even when no writes alias.

The profiling build's unprofiled controls measured 1,484.096 and 2,323.962 cycles/s. CUPTI runs measured 1,359.953 and 2,141.564 cycles/s; instrumented throughput is excluded from the three-pair result. No hardware-counter claims are made.

## Evidence and reproduction

[checks.json](checks.json) is generated by the independent [evidence.py](evidence.py) audit. It checks the downloaded archive manifest, raw traces, raw results versus summaries, CPU fingerprints, source/model hashes, matching build flags, CTest logs and complete CUPTI windows. Raw generated assets remain under ignored `runtime/`; the remote experiment is retained.

The audit passed **116 archived files and 12 full GPU runs**. The two relay parts and complete 4,181,686-byte archive were hash checked; archive SHA256 is `9bb6ab77896fc7d9b17096c60f99d65bdb15106b4e08201454e6d1f8f9d9e361`.

Package sources and the retained CPU oracle locally, using a fresh output path:

```bash
python3 results/experimental/memory-candidates/prepare_inputs.py \
  --output results/experimental/memory-candidates/runtime/new-inputs.tar.gz
```

Transfer that archive through the authorized relay. On the GPU host, extract it into a fresh `MEMORY_ROOT`. `MODEL_PACKAGE` must identify the retained, checked mux-index-on package. Keep its data intact and use the installed toolkit:

```bash
export PATH=/usr/local/cuda-13.2/bin:$PATH
python3 "$MEMORY_ROOT/experiment.py" prepare --root "$MEMORY_ROOT" \
  --model-package "$MODEL_PACKAGE" --cpu-oracle "$MEMORY_ROOT/cpu-oracle-input.json"
taskset -c 2 python3 -u "$MEMORY_ROOT/experiment.py" trials --root "$MEMORY_ROOT"
taskset -c 2 python3 -u "$MEMORY_ROOT/experiment.py" profile --root "$MEMORY_ROOT"
python3 "$MEMORY_ROOT/evidence.py" collect --root "$MEMORY_ROOT"
```

The trials runner requires both full correctness runs before its six alternating measurements. The profile runner requires passing trials. Each run refuses to overwrite its label or output directory. The collector hashes every raw trace and stores identical copies as archive hard links. After downloading the receipt and parts, verify every part and the archive, then run locally:

```bash
python3 results/experimental/memory-candidates/evidence.py audit
```

Defaults use `runtime/{receipt.json,evidence.tar.gz}` and the retained mux-index-on data and CPU oracle. Python with `tarfile` data-filter support is required for the audit. CUDA/device prerequisites and the original reference assets are required to reproduce GPU execution; generated arrays and unpublished simulator commits are not supplied by a fresh public clone.

Compute Sanitizer, formal whole-design equivalence, complete CoreMark CRC and hardware performance counters: **not run**. The earlier counter probe returned ERR_NVGPUCTRPERM; driver settings were not changed. Scala tests were not rerun because this change is confined to the experimental CUDA runtime and tools.
