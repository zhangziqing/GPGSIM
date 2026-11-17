# Nanhu Boolean Constant Mux Reduction

Date: 2026-11-13. Status: compiler, exhaustive equivalence, full tensor
structure, 100K-cycle RU/CPU/GPU correctness, paired GPU timing and CUPTI
activity profiling passed. With memory write arbitration disabled in both
variants, CSR reduction improves median GPU throughput by **10.42%**.

The Nanhu compiler now converts matching long Boolean constant mux chains
into equality reductions before `BalanceAssociativeOps`. The real CSR chain
falls from 193 serial muxes to eight FIRRTL OR levels. The complete executable
tensor graph falls from 205 to 140 layers. See `GRAPH_ANALYSIS.md` for the
remaining FMAC priority-encoder and ROB-index paths.

## Implementation and scope

Simulator revision: `03e16868fa3d73f4df6e957a369621576fd818aa`.

`ReduceConstantMuxChains` requires at least eight muxes with the same
known-width unsigned selector, equality-to-literal conditions, identical
`UInt<1>` hit constants and the complementary `UInt<1>` default. Hit=1 becomes
an OR reduction; hit=0 becomes its inversion. Original comparisons, including
their operand order and literal widths, are retained. Duplicate and
out-of-domain comparison values preserve their existing equality semantics.

Only maximal false-branch chains are rewritten. Unsupported forms, short
chains, conditional scopes, protected nodes and shared intermediate muxes or
aliases are retained. Named and inline output-connected chains are supported.
The pass runs only for Nanhu with associative balancing enabled, after
`NormalizeMuxIndex` and before `BalanceAssociativeOps`. Set
`-Drteaal.reduceConstantMuxChains=false` for the A/B control. Chisel, upstream
FIRRTL, tensor encodings and runtime implementations were not changed.

The existing jump-table density check is not the solution used here:
4096/193 exceeds its `<4` threshold. No 4096-entry CSR jump table is added.
The rewrite is combinational within the existing cycle boundary. Full-system
validation uses the pinned Nanhu clock/helper adapter and its previously
documented semantic boundaries in [RESULTS_zh.md](../../../RESULTS_zh.md);
it is not a proof of unrestricted RTL equivalence.

## Verification

`bash ../tools/sbt test` passed **61 tests in 10 suites**, including
`NormalizeMuxIndexTest` and `BalanceAssociativeOpsTest`. New tests cover both
Boolean polarities, threshold boundaries, unsupported and shared patterns,
aliases, swapped comparisons, constant widths, duplicate and out-of-domain
values, a 193-case chain, repeated execution, the disable property, FIRRTL
type/width checks, and StatementGraph/Renamer compatibility. Widths 3–5 are
checked exhaustively against the original mux evaluation.

`inspect_firrtl.py` checks the actual prepared CSR outputs against the same
193-address set for every 12-bit address, including the default: **4096 per
variant**. Comparisons remain identical. FIRRTL mux depth changes 193→0 and
OR depth 0→8.

`check_minimal.py` executes the actual CSR address set through fresh/off,
fresh/on and pre-clock replay/on compilation, preprocessing and upstream RU
evaluation: **12,288 successful address checks**. The minimal graph changes
387 operations/195 layers to 324 operations/11 layers; replay has the same
enabled structure.

Full tensor validation checks grouped arguments/scatter/bounds against the
unswizzled stream, operand bounds, unique per-layer scatter destinations,
placeholder liveness and named RAM mappings. Independent controls pass for
600 memory writers in each mode and 600 register writers, including old-state
anti-overwrite ordering. The exported portable runtime also passes all four
CPU CTest controls. These checks do not prove arbitrary dynamic RAM alias
freedom or general multiclock support.

## Full compilation provenance

Both builds use `results/nanhu/SimTop.preclock.lo.fir`, SHA256
`4eca6ecb46a01e8fc14a14e7d285ba123dbf70dd9d291d3c3217c7c09886ed0e`,
with `-O1 --normalize-mux-index`, Nanhu mode, pre-clock replay, skipped register
elision, identical preserved interfaces and the existing chain-index override.
Class-load logs confirm the compiler and new pass load from the hashed overlay.

Host: `node007.bosccluster.com`; OpenJDK 21.0.13.0.3, Scala 2.13.12, FIRRTL
1.5.6 and sbt 1.9.6. Each Java process uses `-XX:ActiveProcessorCount=12`,
`-Xmx256g`, `-Xss64m` and CPUs 0–11. They ran concurrently on those CPUs;
the following are recorded costs, not a controlled compilation speedup claim.

| Compilation cost | Off | On |
| --- | ---: | ---: |
| Java wall time | 16:52.87 | 17:25.15 |
| Peak Java RSS, KiB | 57,241,108 | 100,284,784 |
| Executable operations | 2,218,090 | 2,218,027 |
| Tensor layers | 205 | 140 |

Full commands, dependency hashes, class origins, resource measurements and
isolated tensors are retained under `runtime/{off,on}/`. Existing accepted
Nanhu artifacts were not overwritten. The simulator commit is local; the
upstream submodule remote is not established to contain it.

## Workload and GPU measurement

The fixed image is `coremark-2-iteration.bin`, SHA256
`1156255cce0d01cffbf7232fa4a168a00defc41e0c8ea6096a3186ca30a48519`.
The retained 100K-cycle Verilator trace has SHA256
`aac4bdad014633cadab8d50497e1555c60e05272eb091e0c3445c32237e6c3ae`.
`run_coremark.py` builds the existing RU host runner against each isolated
tensor set and checks the trace online, complete trace bytes, named
architectural state and off/on named memory contents. Whole data-buffer
fingerprints are not compared because slot allocation changes.

Both variants passed online reference comparison and complete trace byte
comparison. Final named GPR/FPR/CSR state matches Verilator; named memory
mapping and final named memory contents agree between variants. Each reports
171,898 retired instructions including fused instructions, 167,375 commit
events and zero exception cycles. These runs cover the fixed 100K-cycle
prefix; CoreMark completion and CRC are not established by this prefix.

The host runs were sequential on CPU 12, with the same Clang 19.1.1 `-O3
-march=native` runner and deterministic initialization:

| Single RU host run | Off | On |
| --- | ---: | ---: |
| Evaluation seconds | 470.5534 | 482.2314 |
| Cycles/second | 212.5157 | 207.3693 |

This pair shows no CPU speedup. It is not a repeated timing experiment and
does not predict GPU performance. Evaluation time excludes model loading,
reference comparison and final memory hashing. Whole-process timings and
source/binary hashes are retained in the per-variant runtime metadata.

GPU measurements ran on `corvus01`, RTX PRO 6000 Blackwell Workstation
Edition, UUID `GPU-67983fd2-eb55-ece7-a1ec-40165d7759fb`, driver 580.178.04,
CUDA 13.2.78, GCC 13.3.0 and CMake 3.28.3. The host thread was pinned to CPU
2; device 0 used CUDA Graph, 128 threads and tail threshold 256. Preflight
required an idle GPU. The measured packages were frozen at workspace revision
`598231ea14841ede6a02126d9f5cfcd1835d55b3` and simulator revision
`03e16868fa3d73f4df6e957a369621576fd818aa`. Later priority-mux compiler work
is outside this measurement.

**Off/on switches only `ReduceConstantMuxChains`.** FTQ mux-index normalization
and associative balancing remain enabled in both. Off has 205 layers and on
has 140. Runtime sources are identical between them. For the requested primary
comparison, both use `--direct-memory-writes`: `commit<true>` compiles out
later-writer arbitration and candidate checks; inactive-write enable checks
remain. The separate `no-unroll.patch` was not applied. Direct writes assume
disjoint active memory destinations; this workload check does not prove that
assumption for arbitrary inputs.

All three GPU builds (off, on and the shared profiling build) passed 15 CTest
controls each. They use the same empty CMake build type, C++17, CUDA `sm_120`
and `--expt-relaxed-constexpr`, without explicit `-O3`/Release flags. Exact
flags and binary hashes are archived. This preserves the existing measurement
configuration rather than establishing a best-build performance result.

Three unprofiled pairs ran in order off/on, on/off, off/on:

| Direct-write throughput, cycles/s | CSR off | CSR on |
| --- | ---: | ---: |
| Round 1 | 2,289.897 | 2,528.726 |
| Round 2 | 2,290.119 | 2,526.781 |
| Round 3 | 2,290.049 | 2,529.156 |
| Median | **2,290.049** | **2,528.726** |
| Median evaluation seconds, 100K cycles | 43.667184 | 39.545609 |
| Median CUDA event seconds | 42.703159 | 38.586232 |

The ratio of medians is **1.104223**, a **10.4223% throughput increase** and
9.44% less evaluation time. Evaluation includes device dispatch, synchronization
and compact observation copies, excluding loading, reset, trace/reference work
and final hashing. CUDA event intervals include gaps between kernels and are
not a sum of pure kernel durations.

The initial run retained write arbitration, before the user's clarification.
Its three-round medians were 1,480.241 / 1,567.802 cycles/s (**+5.9153%**).
Those runs and profiles remain as ordered-write controls; they are separate
from the direct-write result above.

## GPU activity profile

Each variant ran an additional unprofiled control and a separate CUPTI activity
run after the timing pairs. Windows are non-reset cycles `[1024,1280)`,
`[20000,20256)` and `[90000,90256)`. Each window contains 36,864 kernels and
256 D2H copies of 4,144 bytes, with zero dropped activity records. Across both
write modes, 12 windows were independently reconstructed from the raw CSVs.
Hardware performance counters were **not run**.

The following direct-write durations average the three equal 256-cycle windows:

| Mean kernel time, microseconds/cycle | CSR off | CSR on |
| --- | ---: | ---: |
| Compute, 70 launches | 192.257 | 191.337 |
| Commit, 70 launches | 84.394 | 84.378 |
| Earlier fused run, one launch | 48.790 | 49.154 |
| Final suffix, one launch | **99.829** | **58.877** |
| Reset and observation gather | 2.015 | 2.004 |
| Sum of kernel durations | 427.285 | 385.751 |

The suffix shortens by **41.02%**, saving 40.952 microseconds/cycle. The
unprofiled evaluation saves 41.216 microseconds/cycle. This supports the
shortened suffix as the main observed source of improvement. Launch count
stays at **144 per cycle**; the suffix executes 43 instead of 108 logical
layers, with 130 fewer barriers. The measurements do not isolate barrier
cost from the other changes within that kernel.

On the enabled graph, compute accounts for 49.60% of measured kernel time,
commit 21.87%, the earlier fused run 12.74% and the final suffix 15.26%.
The suffix remains the longest individual launch, followed by the earlier
fused run. The mean sum of gaps between kernels within each cycle is about
14.06 microseconds in both variants. These activity durations exclude those
gaps, copies and host work; they do not establish a bandwidth or compute limit.

The unprofiled profiling-binary controls measured 2,290.411 / 2,525.935
cycles/s, +0.0158% / -0.1104% relative to the trial medians. CUPTI runs measured
2,131.505 / 2,360.990 cycles/s, so their whole-run throughput is not substituted
for the unprofiled performance result.

## GPU correctness and evidence

All **20 full GPU 100K-cycle runs** (10 ordered, 10 direct) and four 1,024-cycle
smoke runs passed online reference checks. Every full trace is byte-identical
to the retained Verilator trace, with the same retirement and exception counts
reported above. Two independent portable CPU runs also completed 100K cycles
using ordered memory semantics. Every full GPU run matches its corresponding CPU
oracle's complete data, 64-bit memory and 8-bit memory fingerprints. Cross-graph
data fingerprints differ because slot allocation changes; both memory
fingerprints match across graphs.

The local audits verify input and evidence hashes, raw results, run commands,
build flags, CTests, timing medians, static schedules, CUPTI launch/copy counts,
and the direct `commit<true>` specialization in every sampled commit symbol.
See [direct-write audit](gpu-direct-checks.json) and
[ordered-write audit](gpu-checks.json). The 166 hashed evidence files and
manifest are retained in ignored `runtime/gpu-evidence/`. The relay archive is
35,688,237 bytes, SHA256
`c724c155fadd1819a4046bc1ad9a34b43ec1e6f81558e1d52aa84c757e42b9fe`.
Direct-run GPU telemetry records 37–42 degrees C; full environment and resource
samples remain archived separately from execution timings. The scope remains
the fixed 100K prefix; full CoreMark completion/CRC is **not run**.

## Reproduction

After compiling the simulator classes with `tools/sbt`, run from the workspace:

```bash
python3 results/experimental/constant-mux/check_minimal.py
python3 results/experimental/constant-mux/compile.py --mode off
python3 results/experimental/constant-mux/compile.py --mode on
python3 results/experimental/constant-mux/inspect_firrtl.py
python3 results/experimental/constant-mux/prepare.py
python3 results/experimental/constant-mux/inspect_lzc.py
python3 results/experimental/constant-mux/run_coremark.py
python3 results/experimental/constant-mux/export_gpu.py
python3 results/experimental/constant-mux/package_gpu.py --archive /tmp/csr-gpu-input.tar.gz
```

Transfer and extract the archive into a fresh GPU directory, then run
`taskset -c 2 bash measure_gpu.sh /absolute/path/to/gpu-directory` for the
ordered controls and their profiles. Copy `run_direct_gpu.py` beside the
extracted `profile_gpu.py`, then run
`taskset -c 2 python3 run_direct_gpu.py --gpu-root /absolute/path/to/gpu-directory`
for direct-write pairs followed by their control/profile runs. The CUDA 13.2
path and `sm_120` setting in the wrapper describe the measured host.

After downloading the complete evidence archive and receipts into `runtime/`
and generating `runs/cpu-export-100k` for each portable graph with `run.py --cpu`,
run `audit_gpu.py`, then `audit_direct_gpu.py` locally. The recorded CPU oracle
commands are retained in `gpu-checks.json`. The audits inspect frozen artifacts
and their compiler provenance; they do not validate subsequent compiler edits.

Compilation and runner entry points refuse to overwrite existing run
destinations/results. Use a separate checkout/output area for a fresh
reproduction. Required cached dependencies, jars, prepared FIRRTL and reference
traces are recorded in the scripts and provenance; they are not distributed
as source files. Generated tensors, binaries, checkpoints and large traces
remain ignored.
