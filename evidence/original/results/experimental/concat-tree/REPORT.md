# Ordered binary concat balancing

The variadic concat implementation has been reverted. Current compilation
balances eligible concat trees using the original binary opcode 17 and the
original 29-opcode tensor/runtime contract. It preserves operand order and
operator count. All historical variadic artifacts remain under
[`../concat-operands/`](../concat-operands/REPORT.md).

## Semantics and implementation

Concat is associative, but not commutative. For fields `a`, `b`, and `c` of
known unsigned widths, `(a ++ b) ++ c` and `a ++ (b ++ c)` produce the same
ordered bit string. A binary node continues to compute
`(uint64_t(left) << right_width) | uint64_t(right)`.

`BalanceConcatOps` runs after extraction, constant propagation and lookup
compression, before layering. It flattens maximal eligible trees in field
order and constructs a midpoint binary tree. The original node IDs and output
names are reused; all shift immediates and intermediate widths are recomputed.
The number of operations remains unchanged. Inputs with equal arrival depths
need `ceil(log2(N))` concat levels rather than up to `N-1` serial levels.
Seven or eight fields require three binary concat levels.

Each scalar tree requires known positive widths, exact result width sums,
matching right-width shift immediates, unique producers, and at most 64 result
bits. Shared, aliased, repeated, protected and lookup-referenced intermediates
are boundaries. Wider values retain the existing multiword representation.
Both recorded reverse edges and actual operand users are checked. Trees are
processed in dependency order; a replacement is accepted only if its actual
root arrival depth strictly decreases. Thus a late input cannot be made slower
merely to obtain a visually balanced tree.

Disable the late transform with `-Drteaal.balanceConcat=false`. It also respects
the existing associative-optimization option. The earlier FIRRTL
`BalanceAssociativeOps` already supports ordered concat; this late pass handles
eligible structures remaining after extraction. No cycle boundary, reset,
register update, memory access contract, runtime loop, or opcode is added.

Accepted simulator source: `274d8f75c13f690546dfb64f4254fcbc0c1ef7a3`.
Only `BalanceConcatOps.scala`, its tests and its compiler hook differ from the
accepted pre-variadic simulator `84973ad`. Full experiments compile these
accepted compiler sources into an isolated overlay, excluding concurrent
uncommitted PLRU/projection changes. Per-run provenance records input, classpath,
source hashes, commands, affinity and Java version.

## Local verification

- The current workspace Scala suite passes **93 tests in 15 suites**. This
  includes four committed binary-tree tests and separate pending PLRU tests;
  those pending changes are excluded from the full experiment compiler overlay.
- Unit tests cover left and right trees, odd/even field counts, 64 one-bit
  inputs, unequal widths, 64-bit boundaries, field-order bit-string oracles,
  shared/protected/aliased/lookup intermediates, repeated producers, unknown
  widths, invalid shifts, late inputs and repeated execution.
- RU, OU, NU and PSU each execute both minimal variants with ASan/UBSan:
  **8 executions**, **7,999 observations over 1,000 cycles each**, all passing.
  LeakSanitizer is disabled because it cannot operate in the sandbox.
- The minimal graph retains **148 operations**. The observed 64-field path
  changes **64 to 7 nodes**, including its output assignment. The repeated-field
  path changes **7 to 3**; other observed paths do not regress. Only original
  opcodes 0 through 28 occur.
- Restored portable CPU CTest: **4/4 passed**. The ASan/UBSan large-layer
  check passes **290,000 operations / 350,000 outputs / all 29 opcodes**.

See [minimal-checks.json](minimal-checks.json). Logs and binaries are retained
under ignored `runtime/` paths.

## Full Nanhu validation

Both variants retain FTQ normalization, CSR reduction, priority mux balancing
and early FIRRTL balancing; only late binary concat balancing differs. The
compiler overlays and lowered FIRRTL are byte-identical. Lowered FIRRTL SHA-256:
`af0192d4fd48a57eb1fd1077bad47979724da0977f131bc65a4ad67e715af712`.

| Final tensor metric | Late balancing off | Late balancing on |
| --- | ---: | ---: |
| Layers / longest value path | 126 | 126 |
| Operations | 2,222,558 | 2,222,558 |
| Binary concat operations | 177,560 | 177,560 |
| Data slots | 490,183 | 490,089 |
| Lookup operations / entries | 4,391 / 198,676 | 4,391 / 198,676 |
| Named outputs with depth >50 | 12,890 | 12,890 |
| ITLB PLRU result path | 126 | 126 |
| LoadUnit performance result path | 70 | 69 |
| LoadQueue writebacked result path | 94 | 94 |
| Store-buffer drain result path | 78 | 78 |

The pass actually rebalances **2,496 trees / 5,975 nodes**, with at most
**34 inputs** in an accepted tree. Across **151,461 named outputs**, **901**
paths improve by exactly one node, **150,560** are unchanged and **none**
become longer. Full lookup size inventories match, including all 189 tables
with 128 entries. The final layout passes operand bounds, scatter consistency,
write conflict and memory-write placeholder checks.

This is a small local improvement, not a reduction of the current full-graph
maximum. A mean fusion input count near 7.3 does not imply a seven-deep chain:
a binary concat tree can already be balanced. The earlier FIRRTL pass already
supports concat. On the ITLB critical path there are **56 concat nodes**, but
the maximum consecutive concat run is **5**. They are interleaved with **25
slice nodes and 33 mux nodes**, as well as other operations. Concat
reassociation alone cannot cross those dependencies. Both variants retain
the same critical-path opcode sequence.

One full Java compilation per variant took **16:41.62 off / 16:49.88 on**;
maximum resident memory was **58.3 / 97.1 GiB**. These concurrent builds record
compilation cost, not an isolated performance or memory attribution experiment.
Preprocessing and correctness runs are measured separately in their logs.

The disabled graph matches the retained priority-mux baseline byte-for-byte
for operations, arguments, scatter, layering and lookup data. The compiler
randomizes initial nonconstant signal values; all differences in the initial
value files lie in input/state slots that the retained runner zeros before
reset. Constants and the runtime-normalized initial values match.

See [graph-checks.json](graph-checks.json). Both complete **100,000-cycle**
RU reference runs pass: their traces are byte-identical to the retained
Verilator reference, with **171,898 retired instructions / 167,375 commit
events** each. Final GPR, FPR and CSR values match that reference; named memory
mappings and contents match between variants. Full-buffer fingerprints are
not compared across layouts. These are correctness runs with fixed CPU affinity
and concurrent execution, not performance samples.

Consolidated compiler/RU evidence: [checks.json](checks.json), including source
hashes, reference hashes, baseline comparison and compilation cost. GPU results
and their separate acceptance records follow below. The retired variadic result
of 96 layers is not a result of this binary pass.

## GPU measurements (2026-11-14)

The accepted binary pass shows **no throughput gain** in this experiment.
Three full runs per variant use one frozen binary per GPU, ordered
`off, on, on, off, off, on`. Every run executes 64 reset cycles followed by
100,000 measured non-reset cycles. CUDA Graph, device 0, CPU affinity 2,
threads 128, tail threshold 256 and direct memory writes are fixed. The
original kernel loop-unrolling setting is retained. Both layouts still have
126 layers, 2,222,558 operations and **141 kernel executions per cycle**.

| GPU | Off samples (cycles/s) | On samples (cycles/s) | Off median | On median | On / off change |
| --- | --- | --- | ---: | ---: | ---: |
| RTX PRO 6000 Blackwell | 2942.708, 2942.204, 2939.105 | 2931.108, 2930.850, 2928.467 | **2942.204** | **2930.850** | **-0.386%** |
| RTX 2060 | 771.351, 772.544, 773.208 | 772.163, 771.444, 773.152 | **772.544** | **772.163** | **-0.049%** |

Reported throughput is `100000 / evaluation_seconds`: non-reset evaluation
wall time including dispatch, synchronization and compact observation copies.
Loading, reset, trace formatting, reference comparisons and final fingerprints
are excluded. CUDA-event device time is reported separately below. The Windows
desktop remained running; pre-run checks found no unexpected applications.
Blackwell had no other compute process before each run. Samples support a
small measured Blackwell slowdown and overlapping RTX 2060 ranges; three
pairs do not establish a general ranking. Different hosts and toolchains
prevent attributing their absolute throughput difference solely to the GPUs.

| Cost / resource metric | RTX PRO 6000 Blackwell | RTX 2060 |
| --- | ---: | ---: |
| Configure/build/test wall seconds | 10.926 | 47.610 |
| Median evaluation seconds, off / on | 33.988 / 34.120 | 129.443 / 129.506 |
| Median CUDA-event device seconds, off / on | 33.333 / 33.468 | 118.937 / 118.969 |
| Allocated device bytes, off / on | 1,156,375,500 / 1,156,375,428 | 1,156,375,500 / 1,156,375,428 |

Build/test wall time includes configuration and CTest; it is separate from
Scala compilation and simulation. Device allocation is the runner's reported
allocation, not a peak-memory measurement. Blackwell uses Linux, GCC 13.3.0,
NVCC 13.2.78, driver 580.178.04 and `sm_120`; RTX 2060 uses native Windows,
MSVC 19.29.30037, NVCC 12.6.85, driver 565.90 and `sm_75`. Both GPU builds use
an empty `CMAKE_BUILD_TYPE`, the retained C++17/CUDA settings, and no added fast-math or
no-unroll option. Exact caches, flags, build commands, binary hashes, GPU UUIDs
and pre/postflight device state are retained in the evidence archives.

### Correctness and provenance

The frozen input archive is 45,408,522 bytes, SHA-256
`a53577553e1d88c3b978a52805de37f282f905fa3c0834380be7a4182f00dea8`.
It uses accepted simulator `274d8f75c13f690546dfb64f4254fcbc0c1ef7a3`,
workspace packaging revision `77c31fbb1b086109f986bab331c5b96f33876da1`,
and the retained Nanhu RTL revision
`0a68ebc13b3d96a78d8663679393ecbea5b8fb73`. Concurrent PLRU/projection
compiler edits are excluded. Program SHA-256 is
`1156255cce0d01cffbf7232fa4a168a00defc41e0c8ea6096a3186ca30a48519`;
the 100K reference trace SHA-256 is
`aac4bdad014633cadab8d50497e1555c60e05272eb091e0c3445c32237e6c3ae`.

- **45/45 CTests in CUDA-enabled builds** pass across the two throughput builds
  and the separate Blackwell profiling build. Four 1,024-cycle smoke runs also pass.
- All **16 full GPU runs** pass strict online comparison of cycle, commit,
  GPR/FPR/CSR, exception, trap and UART observations. Their entire saved traces
  match the Verilator reference byte-for-byte; only Windows CRLF line endings
  are normalized to LF. Each run has 167,375 commit events and 171,898 retired
  instructions including 4,523 fused instructions, with no exception or trap.
- The portable CPU build passes **4/4 CTests**. Independently executed references
  for both layouts pass the same full 100K trace comparison. Every full GPU
  run's final data, memory64 and memory8 fingerprints match its corresponding
  CPU layout. The two layouts' full data fingerprints are not compared with
  each other. CPU references use clang++ 19, `-O3 -DNDEBUG -march=native`,
  affinity 12/13 and concurrent execution; their timings are correctness-run
  metadata, not CPU performance samples.
- Local audits recompute transfer/archive/file hashes, verify model and source
  identity, exact Windows adaptations, binary hashes, build settings, run order,
  raw outputs, traces and CPU fingerprints. Remote raw results retain the status
  `trace_validated_CPU_fingerprint_audit_required`; final acceptance is recorded
  separately in [rtx6000-checks.json](rtx6000-checks.json) and
  [rtx2060-checks.json](rtx2060-checks.json).

CPU and CUDA share scalar opcode definitions; their independent execution and
storage paths are cross-checked by the final fingerprints. The retained
Verilator trace supplies the independent RTL observation reference, and opcode
CTests also compare against the original OU arithmetic implementation.

The initial portable CPU attempts encountered NFS `ENOSPC` and incomplete
trace files. They were rejected and preserved separately; complete reruns use
local scratch storage. SHA-verified large artifacts remain under ignored
`runtime/` paths, with symlinks to `/tmp/rteaal-binary-concat-20261114/` where
needed. These scratch artifacts are not part of Git. The fixed 100K prefix
does **not** establish complete CoreMark execution, CRC or score.

### Blackwell CUPTI windows

A separate instrumented build executes one control and one CUPTI 100K run per
variant. These four runs are excluded from throughput medians. Each variant
records windows `[1024,1280)`, `[20000,20256)` and `[90000,90256)`, in non-reset
cycles. Each of the six windows contains **36,096 kernel executions and 256 D2H copies**
with **zero dropped records**. Independent analysis reproduces every recorded
schedule ordinal and category count.

| Mean kernel microseconds/cycle, off / on | Early | Middle | Late |
| --- | ---: | ---: | ---: |
| Compute | 187.756 / 189.530 | 189.168 / 190.952 | 188.530 / 190.393 |
| Commit | 83.256 / 83.661 | 83.607 / 83.895 | 83.425 / 83.733 |
| Fused suffix | 59.755 / 59.098 | 59.689 / 59.048 | 59.639 / 58.997 |
| All kernels | 332.779 / 334.302 | 334.474 / 335.907 | 333.606 / 335.134 |

CUPTI values sum kernel `end-start` durations and exclude host work, copies
and gaps. The shorter suffix is outweighed by measured compute and commit
increases in these windows; this does not identify a hardware cause. Hardware
counters and occupancy were **not run**, and RTX 2060 CUPTI was **not run** for
this binary-tree pair.

## Reproduction

From the workspace root, after building the Scala sources and with the retained
Nanhu input, chain-index overlay and reference artifacts available:

```sh
python3 results/experimental/concat-tree/check_minimal.py
python3 results/experimental/concat-tree/compile.py --mode off
python3 results/experimental/concat-tree/compile.py --mode on
python3 results/experimental/concat-tree/prepare.py --mode off
python3 results/experimental/concat-tree/prepare.py --mode on
python3 results/experimental/concat-tree/prepare.py --compare
python3 results/experimental/concat-tree/run_coremark.py --mode off
python3 results/experimental/concat-tree/run_coremark.py --mode on
python3 results/experimental/concat-tree/run_coremark.py --collect
```

For the GPU extension, export the retained accepted tensors and create a new
frozen package, then generate independent CPU references:

```sh
python3 results/experimental/concat-tree/export_gpu.py --mode off
python3 results/experimental/concat-tree/export_gpu.py --mode on
python3 results/experimental/concat-tree/package_gpu.py
python3 results/experimental/concat-tree/run_gpu_oracle.py --build
python3 results/experimental/concat-tree/run_gpu_oracle.py --mode off
python3 results/experimental/concat-tree/run_gpu_oracle.py --mode on
python3 results/experimental/concat-tree/run_gpu_oracle.py --collect
```

On each GPU host, retain the archive as `<root>/input.tar.gz`, extract it into
`<root>/frozen/`, and copy `frozen/support/` to `<root>/support/`. Use the retained
host toolchains and the Windows adapter's recorded toolchain paths. From that
root, run `taskset -c 2 python3 support/run_gpu.py --profile` on Blackwell, or
`python support/run_gpu.py` on RTX 2060. The runner checks occupancy before
build tests and each execution, preserving other workloads. Transfer the current
`collect_gpu.py` as `support/collect_binary_gpu.py`, then run
`python support/collect_binary_gpu.py <root>` after the postflight file exists.
Copy the receipt and its hash-checked archive into local
`runtime/rtx6000/` or `runtime/rtx2060/`, respectively, and run:

```sh
python3 results/experimental/concat-tree/audit_gpu.py --gpu rtx6000
python3 results/experimental/concat-tree/audit_gpu.py --gpu rtx2060
```

Collection can precede local CPU completion; final audit requires both complete
CPU references. `--artifacts-only` verifies preserved GPU evidence while leaving
CPU fingerprint acceptance pending. Export, package and run commands refuse
existing output directories to preserve measurements.

Compilation refuses to replace existing variant directories. The minimal test
disables early FIRRTL balancing to expose long concat chains; the full Nanhu
comparison keeps that existing optimization enabled. The retained CoreMark
check covers a fixed 100,000-cycle reference prefix, not workload completion or
its final CRC. Local simulator commits have not been pushed upstream.
