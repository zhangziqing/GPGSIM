# Nanhu Priority Mux Balancing

Date: 2026-11-14 (UTC). With CSR reduction enabled in both variants, priority
mux balancing reduces the final executable graph from **140 to 126 layers**
and adds **4,531 operations (0.204%)**. CLZ and LFST dependency paths shorten
after extraction. Both local 100K-cycle runs match the complete Verilator
reference trace. The remaining longest path is the unchanged ITLB PLRU update.
On RTX PRO 6000 Blackwell, the graph change improves three-round median
throughput by **16.38%** with the original direct-commit kernel. Combining it
with the isolated no-unroll kernel control improves throughput by **17.05%**.
All 16 full GPU runs match the reference trace and independent CPU state.
The same frozen experiment on RTX 2060 improves throughput **7.00%** with
graph balancing; no-unroll provides no additional measured benefit there.
Its 16 full GPU runs also pass independent trace and CPU-state checks.

## Transformation and scope

`BalancePriorityMuxChains` follows `NormalizeMuxIndex` and
`ReduceConstantMuxChains`, before `BalanceAssociativeOps`, in the Nanhu
compilation path. Disable it independently with
`-Drteaal.balancePriorityMuxChains=false`. The CSR optimization remains enabled
in both variants. The compiler transformation leaves Chisel, upstream FIRRTL,
tensor encodings and runtime kernels unchanged. The GPU experiment separately
controls a one-line kernel compilation change described below.

A segment records `(hit, value)`. Merging a high-priority segment A and a
low-priority segment B produces `hitA OR hitB` and
`mux(hitA, valueA, valueB)`. This composes ordered first-hit selection; only
parenthesization changes. The original default is the last always-hit
candidate. A segment's value when it has no hit is irrelevant, and the full
chain always has a hit through its default. Generated hit summaries are named
and shared, rather than expanded into every consumer.

The pass requires at least eight false-branch muxes, UInt<1> conditions and
identical known positive UInt widths for all candidates, defaults and muxes.
It handles named nodes, aliases and inline output expressions. A shared or
protected intermediate, unsupported type, malformed width or residual
conditional scope leaves the maximal chain unchanged. Original condition and
data expressions are retained. Dense same-selector equality runs are kept
for existing lookup extraction, including runs inside a mixed priority chain.
A rewrite is accepted only when its combinational arrival-depth estimate
improves; repeated execution preserves the balanced result.

The exact real CLZ_8 contract is highest input bit first on UInt<107>, yielding
`106 - bit_index`, with zero and bit 0 returning 106. The LFST lane 5 contract
is bypass lanes 4, 3, 2, 1, 0 first, then table indices 127..1, with table 0
as the default.
The LFST table selector is two-dimensional: UInt<5> row and UInt<2> column.
Earlier LFST lane 0 lookup extraction is tracked separately from lane 5.

## Verification

The new suite and three related pass suites pass 33 tests. The full command
`bash ../tools/sbt test` passes 70 tests in 11 suites. The sandbox blocked sbt's
local IPC socket, so the unchanged launcher ran through the runtime's approved
execution path. Simulator commits are local on `priority-mux-balance`:
`91bd70b` for the implementation and `84973ad` for tests.

Independent ordered first-hit models check exhaustive small hit masks,
multiple simultaneous hits, dynamic candidates, nonzero defaults, odd/even
lengths, UInt widths 1/7/63/64/65/127/128/129, shared/protected aliases, unknown
and signed types, lookup preservation, idempotence and StatementGraph/Renamer
compatibility. The original prepared-FIRRTL conditions and data definitions
are checked unchanged.

Actual prepared FIRRTL checks, per variant:

| Cone | Oracle cases | Selection depth off/on | Local operation depth off/on |
| --- | ---: | ---: | ---: |
| CLZ_8 | 5,780 | 106 / 7 | 107 / 8 |
| LFST lane 5 | 8,192 | 132 / 8 | 134 / 10 |

The CLZ cases include zero, all ones, all 107 single bits and all 5,671 bit pairs.
LFST covers 128 indices x 32 bypass masks x 2 dynamic data sets. Its state data,
precomputed row/column and bypass-hit conditions are treated as independent
interfaces; these checks establish local selection semantics rather than a
new proof of clocks or state transitions.

Fresh off/on compilation and enabled pre-clock replay each run the unchanged
upstream RU evaluator against 6,036 CLZ cases, 8,192 LFST cases and 8,192 dense
lookup cases. The executable minimal graph changes 619 to 811 operations and
134 to 11 layers. CLZ output depth is 108 to 10 and LFST output depth 134 to 11;
the dense lookup stays at depth 2 with one 16-entry opcode 24 table. Maximum
new OR fan-in is 6. Replay matches the enabled graph's operations and layers.
The final script also reexecuted the three hash-matched binaries and verified
that its regenerated fixture and oracle source match the original runs.

## Full tensor structure

![Layer widths, dependency paths and static CUDA schedule counts](runtime/tail-analysis/priority-mux-structure.png)

The three-panel figure compares the pass off/on graphs. Its CUDA panel shows
static counts; measured GPU timings are reported separately below. Vector exports
are available as `runtime/tail-analysis/priority-mux-structure.svg` and `.pdf`;
`plot-checks.json` records the input hashes and plotted values.

Full variants retain separate artifacts in `runtime/off` and `runtime/on`.
The only optimization setting changed is `rteaal.balancePriorityMuxChains`.
Both use the retained pre-clock input SHA256
`4eca6ecb46a01e8fc14a14e7d285ba123dbf70dd9d291d3c3217c7c09886ed0e`,
`-O1 --normalize-mux-index`, the existing chain-index override, identical
preserved interfaces and skipped register elision. Class-load logs verify the
new compiler/pass overlay. The pass reports 112 rewritten module chains,
2,896 muxes, longest 132, and 1,394 conservative lookup skips. The disabled
variant's prepared FIRRTL and all nine checked tensor JSON streams are
byte-identical to the retained CSR-enabled 140-layer baseline; see
[baseline-checks.json](baseline-checks.json).

These are final preprocessed executable counts and actual operand-value path
lengths. Register inputs are leaves; register outputs include their update
and commit operations. The analysis versions reused data slots at each layer's
scatter. It excludes memory-array dependencies and register anti-overwrite
scheduling edges, and does not model gate delay.

| Metric | Pass off | Pass on |
| --- | ---: | ---: |
| Executable operations | 2,218,027 | 2,222,558 |
| Scheduled layers / longest operand-value path | 140 | 126 |
| Longest path excluding assignments | 138 | 123 |
| CLZ UInt<107> result-register path, all four execution units | 140 | 41 |
| Smaller CLZ result-register path, all four execution units | 78 | 36 |
| LFST lane 5 receiving register path, fpDq/intDq/lsDq | 140 | 24 |
| LFST maximum table-value-to-result selection path | 133 | 9 |
| ROB `writebacked` memory-writer maximum path | 13 | 13 |
| ROB `store_data_writebacked` memory-writer maximum path | 13 | 13 |
| OR-chain operations (opcode 22) | 29,766 | 30,336 |
| Largest global OR-chain fan-in | 63 | 63 |
| Lookup tables (opcode 24) | 4,403 | 4,391 |
| Total lookup entries | 198,808 | 198,676 |

The CLZ mux spine through assignments falls from 108 to 9 for the larger CLZ
cones, and 50 to 8 for the smaller cones. The LFST receiving-register spine
falls from 133 to 9. Thus tensor extraction retains the shortened selection
structure. Maximum OR fan-in in the complete optimized target cones is 6 for
CLZ and 5 for LFST, including upstream logic.

The actual ROB memory writers are the 23 `writebacked` ports and 9
`store_data_writebacked` ports, all scheduled no later than layer 13. They do
not account for the former 140-layer tail. The LFST receiving registers named
`r_5_cf_waitForRobIdx_value` hold ROB indices and are separate from those ROB
memory writes. Grouped argument/scatter checks, memory-write placeholder
liveness and absence of simultaneous graph-slot writes pass in both variants.
Full target and layout evidence is in [graph-checks.json](graph-checks.json).

The remaining 126-node critical path ends at
`l_soc.core_with_l2.core.frontend.io_ptw_tlb_itlb.normal_refill_idx_state_reg`.
It has 56 concat, 25 slice-related, 33 mux, 4 OR, 4 AND, 1 XOR and 3 assignment
operations. It comes from six ordered guarded PLRU updates: TLB.scala:212
calls `re.access`, and Replacement.scala:22 implements `touch_ways.foldLeft`.
Recursive left/right subtree selection in Replacement.scala:196-208 carries
the preceding state through each update. The initial hit/index logic reaches
layer 24, the six updates occupy layers 25-122, and final valid/clock/reset
handling reaches layer 126.

The complete prepared `TLB` module (1,529 lines) is byte-identical off/on,
including all 676 `normal_refill_idx` node definitions. Its first five update
results each feed two subtree slices and a fallback; the outer guarded update
chain has only six muxes. The pass correctly retains this shared structure
below its threshold of eight. It is outside this change's matched selection
chains; no PLRU or RTL change was made. See
[critical-path-checks.json](critical-path-checks.json).

The exhaustive [inventory of paths deeper than 50 operations](LONG_PATHS.md)
covers 13,105 endpoint operations across 56 normalized module families,
including the remaining LoadQueue, store-buffer, SourceD, age-matrix and FTQ
paths. Shared prefixes are retained in the complete dependency DAG.

Lookup totals are not assumed invariant. The histogram changes only for
4-entry tables (190 to 193), 8-entry tables (2,206 to 2,192) and 32-entry tables
(117 to 116). The single 1,024-entry FTQ lookup and all 189 128-entry tables
survive. The independent lookup audit maps all 112 rewritten roots in 38
modules back to their original ordered chains. All 177 examined same-selector
equality-run prefixes fail the lookup density threshold after resolving
aliases and checking casts and explicit Boolean identities. No missed dense
lookup opportunity is found in these rewritten chains.

Some small-table changes occur in byte-identical source modules: the named
AXI4RAM state selector has one 4-entry table off and none on; two ICacheMissEntry
state selectors change from 16 to 13 and 20 to 14 8-entry tables. These changes
cannot be direct rewrites of those source chains. This supports downstream
extraction partitioning or sharing as a cause, without assigning every table
delta to a specific internal heuristic. The audit is structural for this
exact compilation and does not prove arbitrary Boolean equivalence. See
[lookup-checks.json](lookup-checks.json) and `inspect_lookups.py`.

The existing CUDA Graph scheduler, threads=128 and tail_threshold=256, gives:

| Static per-cycle schedule | Pass off | Pass on |
| --- | ---: | ---: |
| Kernel launches including reset and gather | 144 | 141 |
| Wide-layer compute/commit pairs | 70 | 69 |
| Fused small-layer runs | 2 | 1 |
| Fused logical layers | 70 | 57 |
| Block barriers across all fused runs | 140 | 114 |
| Final suffix logical layers | 43 | 57 |
| Final suffix block barriers | 86 | 114 |
| Final suffix operations | 918 | 1,468 |

The final suffix grows from layers 98-140 to layers 70-126 because formerly
separate small regions merge. Whole-cycle barrier count falls by 26, while
barriers within the final suffix increase by 28. Layer reduction therefore
does not imply an equally large launch reduction or a measured suffix speedup.
These are static scheduler counts, not observed GPU synchronization timings.

Java compilation cost was 17:06.78 wall time / 96,939,208 KiB peak RSS off and
15:40.73 / 61,546,264 KiB on. Both compiles ran concurrently with CPUs 0-11
allowed on node007; these are resource records, not controlled compiler
performance samples. The compiler's intermediate on count, 2,223,369 unique
operations, precedes tensor preprocessing. It must not replace the final
executable total of 2,222,558 above.

## Full workload and timing boundary

The fixed program is `coremark-2-iteration.bin`, SHA256
`1156255cce0d01cffbf7232fa4a168a00defc41e0c8ea6096a3186ca30a48519`.
The 100K-cycle Verilator reference SHA256 is
`aac4bdad014633cadab8d50497e1555c60e05272eb091e0c3445c32237e6c3ae`.
The correctness runner checks the complete trace, named architectural state
and off/on named memory contents. Both variants pass all 100,000 observed
cycles, retire 171,898 instructions including fusion, record 167,375 commit
events and report zero exception cycles. Complete trace hashes match the
reference; named GPR/FPR/CSR state matches Verilator, and named memory mappings
and final contents match between variants. Whole data-buffer fingerprints
are not compared across graphs because intermediate slot allocation changes.

Runs were concurrent on node007, pinned to CPUs 12/off and 13/on, using the
unchanged RU evaluator and Clang 19.1.1. Evaluation cost was 506.238 / 539.438
seconds and whole-run cost 513.442 / 546.666 seconds. These are single
correctness-run costs, not paired performance samples. CoreMark completion
and CRC are outside this fixed-prefix validation. Machine-readable evidence,
source hashes, full test-report hashes and verification boundaries are in
[checks.json](checks.json).

## Blackwell GPU measurement and kernel control

After the user reported that the GPU was free, the checked packages were
transferred directly through Paseo relay into the independent
`/home/zhangziqing/priority-mux-20261113-latest` experiment on corvus01.
The frozen source graph revision is `84973ad`; concurrent concat-fusion
development is excluded from these measurements. Original inputs and earlier
experiments are preserved.

![Audited graph, throughput and CUDA activity comparison](runtime/tail-analysis/priority-mux-gpu.png)

The device is RTX PRO 6000 Blackwell Workstation Edition, UUID
`GPU-67983fd2-eb55-ece7-a1ec-40165d7759fb`, driver 580.178.04. Both kernels use
CUDA 13.2.78, sm_120, GNU C++ 13.3, empty CMake build type, CPU 2 affinity,
CUDA Graph, 128 threads, threshold 256 and direct memory writes. Direct commit
assumes disjoint active memory destinations; successful prefix comparisons
do not establish conflict freedom for arbitrary workloads.

The original kernel already uses `commit<true>` to compile out conflict scans.
The [no-unroll control](../memory-direct/no-unroll.patch) only adds
`#pragma unroll 1` before its memory-write loop. It remains an isolated
experiment copy. On this build, direct commit changes from **408 to 248 static
instructions**, retaining 40 registers and zero stack/local allocation.
The other five kernels have identical instruction encodings. The profiling
build also retains all six no-unroll kernel encodings.

Three rounds use orders A/B/C/D, D/C/B/A, A/B/C/D, where A/B are original
kernel off/on and C/D are no-unroll off/on. Each sample executes 64 reset and
100,000 nonreset cycles. Throughput is `100000 / evaluation_seconds`, the sum
of measured nonreset `step` calls; program startup, trace processing and
final hashing are outside that interval. CUDA event time is retained separately.

| Graph / direct kernel | Round 1 cycles/s | Round 2 | Round 3 | Median | Median evaluation seconds |
| --- | ---: | ---: | ---: | ---: | ---: |
| 140 layers / original | 2,540.716 | 2,524.939 | 2,525.709 | **2,525.709** | 39.593 |
| 126 layers / original | 2,942.041 | 2,939.409 | 2,938.618 | **2,939.409** | 34.020 |
| 140 layers / no-unroll | 2,579.953 | 2,581.040 | 2,580.003 | **2,580.003** | 38.760 |
| 126 layers / no-unroll | 2,956.454 | 2,957.095 | 2,956.148 | **2,956.454** | 33.824 |

Graph balancing improves throughput **16.380%** with the original kernel and
**14.591%** with no-unroll. No-unroll improves the 140-layer graph **2.150%**
and the 126-layer graph **0.580%**. Their combined measured improvement is
**17.054%**; improvements are not assumed additive. These are three-sample
medians for this device and fixed prefix, without a broader workload ranking.

Explicit device arrays occupy 1,156,236,752 bytes off and 1,156,375,500 bytes
on, an increase of 138,748 bytes. Configure plus build costs, measured on
CPU 2, are 8.653 seconds original and 8.515 seconds no-unroll, excluding CTest.

### Separate activity profiles

Each no-unroll graph has an unprofiled 100K control and a separate CUPTI 100K
run. Controls measure 2,580.619 / 2,957.242 cycles/s off/on, close to the
corresponding trial medians. Profiled throughput is excluded from those medians.
Each graph has three complete 256-cycle windows beginning at nonreset cycles
1,024, 20,000 and 90,000. All six windows have zero dropped records, exactly
144 / 141 kernels per cycle and 256 observation copies per window. Kernel
names, launch order, graph node identities and direct-commit specialization
are checked against each exported schedule.

The following are means across the three windows, in microseconds per
simulated cycle. Durations sum kernel end-start intervals and exclude copies,
host work and gaps; kernel gaps are listed separately.

| Activity | 140 layers | 126 layers |
| --- | ---: | ---: |
| Wide-layer compute | 191.082 | 188.407 |
| Wide-layer commit | 77.438 | 76.630 |
| Earlier fused small-layer run | 48.994 | 0 |
| Final fused suffix | 58.828 | 59.698 |
| All fused small-layer runs | **107.822** | **59.698** |
| Reset and gather | 2.000 | 2.012 |
| All kernels | 378.342 | 326.747 |
| Gaps between kernels within a cycle | 14.048 | 13.759 |

The entire fused-small-layer cost falls **44.63%**. The final suffix itself
increases **1.48%**, consistent with its changed layer interval and membership.
Thus the whole-cycle improvement is not evidence that the final suffix alone
became faster. Kernel times and the unprofiled throughput independently show
that the added operators did not outweigh the scheduling benefit here. No
hardware-counter conclusion about bandwidth, caches or stalls is established.

### Independent correctness and evidence

Both measurement builds and the profiling build pass 15 CTests each. Four
1,024-cycle smokes and all **16 full GPU 100K runs** pass full reference trace
comparison and architectural checks. The local audit also compares all three
final data/memory fingerprints to independent portable CPU references for
the corresponding graph. The optimized graph gets a fresh full CPU run and
4/4 CPU CTests. The disabled graph reuses the previously checked CSR-enabled
CPU oracle only after checking every model field except the design label,
every model asset and every runtime source file for exact identity.

The complete evidence archive contains 203 hash-verified files, including
raw full traces, result output, commands, source copies, build flags, CTests,
disassembly and CUPTI timelines. Its SHA256 is
`6f0a96bd5dcac562923b596f19fdc256d03a37f3de87a2ee35c714681c798437`.
All 14 relay chunks and the complete archive are verified locally.
[gpu-checks.json](gpu-checks.json) records independent audit results, all
samples and recomputed windows; [gpu-plot-checks.json](gpu-plot-checks.json)
records plotted values and input/output hashes. Large evidence remains in
ignored `runtime/latest-gpu/`. Full CoreMark completion and CRC remain outside
the fixed-prefix scope.

## RTX 2060 native Windows measurement

![Two-host throughput and separate CUDA activity profiles](runtime/tail-analysis/priority-mux-two-gpus.png)

The same 70-entry frozen input package was transferred through Paseo into
`G:\FileSpace\priority-mux-rtx2060-20261114`. Its SHA256 is
`86f171d0f97b2f377bf2fa1d2fd0793bad5a0ffde39fcc5b22ac51879e3ae241`.
Both graphs and reference inputs are byte-identical to the Blackwell package.
The device is RTX 2060 6 GB, UUID
`GPU-8e4316ac-74f4-4233-0e1c-be24b5560068`, driver 565.90. Native Windows
CUDA 12.6.85, sm_75 and MSVC 19.29.30037 are used because WSL was unavailable.
The build type is empty, with identical CUDA flags across original, no-unroll
and profile builds. All runs use CPU 2 affinity, CUDA Graph, direct memory
writes, 128 threads, threshold 256, 64 reset and 100,000 nonreset cycles.
Host OS and compiler differences prevent attributing cross-host speed ratios
solely to GPU hardware.

The native adapters preserve the unchanged reference-oracle body, adding
only an MSVC `__builtin_expect` macro before its include. Runners retain raw
Windows CRLF traces and compare exact bytes after CRLF-to-LF normalization.
The only device-source change between the two timing kernels is the same
one-line no-unroll patch used on Blackwell. No system settings or desktop
applications were changed. [Native workflow and source copies](rtx2060/README.md)
record these adaptations separately from the frozen simulator.

### Three alternating rounds

The order and timing boundary match the Blackwell experiment. Every sample
is retained, including the slower third original/on sample.

| Graph / direct kernel | Round 1 cycles/s | Round 2 | Round 3 | Median | Median evaluation seconds |
| --- | ---: | ---: | ---: | ---: | ---: |
| 140 layers / original | 699.419 | 696.066 | 697.948 | **697.948** | 143.277 |
| 126 layers / original | 746.817 | 749.115 | 729.350 | **746.817** | 133.902 |
| 140 layers / no-unroll | 695.160 | 697.891 | 696.430 | **696.430** | 143.589 |
| 126 layers / no-unroll | 746.129 | 746.677 | 749.055 | **746.677** | 133.927 |

Graph balancing improves the original/no-unroll kernel medians **7.002% /
7.215%**. No-unroll changes throughput **-0.217%** on 140 layers and
**-0.019%** on 126 layers, within the observed sample variation. Its combined
result with graph balancing is **+6.982%** over original/off. These samples
support the graph benefit on this host and do not demonstrate a no-unroll
benefit. They do not establish a general kernel regression.

Explicit device arrays remain 1,156,236,752 / 1,156,375,500 bytes off/on.
Configure plus build takes 45.372 seconds original and 41.964 seconds
no-unroll; the separate profile build takes 42.747 seconds. These are single
build costs, not repeated compiler-performance samples.

The 500 telemetry samples span builds, idle intervals, tests and execution:
49-66 degrees C, 621-1,835 MiB device memory and 1-83% utilization. Desktop
applications remained active, so the GPU was not exclusively reserved.
The reported 300-1,890 MHz SM-clock range includes idle periods; it is not
a per-trial active-clock range. All raw telemetry and sample bounds remain
in the evidence archive.

### Separate activity profiles

Each graph has an unprofiled 100K control and an independent profiled 100K
run. Controls measure 698.470 / 748.088 cycles/s off/on; profiled runs measure
687.396 / 739.623. The aggregate changes are -1.585% / -1.132%; one pair does
not isolate instrumentation from environmental variation. Traced evaluation
also includes the three end-window Stop/flush calls. Profiled throughput is
excluded from the timing medians.

All six 256-cycle windows have zero drops, exact graph-node identities and
launch order, 36,864 / 36,096 kernels off/on and 256 exact-size observation
copies. The following means use the same three window positions as Blackwell.

| Activity, microseconds per simulated cycle | 140 layers | 126 layers |
| --- | ---: | ---: |
| Wide-layer compute | 749.449 | 764.632 |
| Wide-layer commit | 275.420 | 256.675 |
| Earlier fused small-layer run | 87.628 | 0 |
| Final fused suffix | 112.848 | 98.070 |
| All fused small-layer runs | **200.476** | **98.070** |
| Reset and gather | 3.970 | 3.678 |
| All kernels | 1,229.315 | 1,123.055 |
| Signed gaps between kernels within a cycle | 114.124 | 79.859 |

All fused-small-layer time decreases **51.08%**, and the final suffix itself
decreases **13.10%** on this host. Compute time increases in these samples;
the improvement does not require every category to become faster. This
differs from Blackwell, where the final suffix grows slightly while all fused
runs become faster overall.

Windows WDDM timestamps contain 213 / 583 negative adjacent-kernel gaps
across the off/on windows. They are preserved, with minima between -832 and
-1,696 ns. Kernel end-start sums are not disjoint device busy-time unions.
Intercycle intervals include copies, host observation/reference processing,
dispatch and waits; separate host API waits were not measured. No bandwidth,
occupancy or stall-counter conclusion follows from these activity timings.

### Independent audit and evidence

All three CUDA builds pass 15 CTests each; four 1,024-cycle smokes and all
16 full 100K runs pass. The local audit checks every raw trace after exact
newline normalization, all reference architectural fields and all three
independent CPU data/memory fingerprints for the corresponding graph.
It reconstructs the frozen source patches, verifies native adaptations,
checks executable hashes and compiler controls, and recomputes all six CSV
windows. Counts, identities and integer timestamps compare exactly. Python
3.9/3.12 floating summation differs by one ULP in three derived scalars;
the comparison allows relative 1e-14 and absolute 1e-12 rounding only.

The first collector attempt rejected `src/main.cpp.orig`, a patch-tool
backup. Its 14,229 bytes are identical to frozen `src/main.cpp`, SHA256
`538880bc8e41a0b677b5fe330c62ad95576773872c4691f27a542adcf7b6b4eb`;
it has no Ninja compilation reference. The backup and initial failure logs
are preserved. The collector was narrowly corrected and GPU samples were
not repeated.

The 91,976,187-byte archive has **406 verified files** and **44 verified
relay parts**, with SHA256
`efa5ecf0060645ce0942b153208647db2f02ec6978eb99c45289f3d4c71a6bd6`.
[rtx2060-checks.json](rtx2060-checks.json) records the primary audit;
[two-gpu-plot-checks.json](two-gpu-plot-checks.json) records all plotted values
and image hashes. Large evidence stays in ignored `runtime/rtx2060/`.
Compute Sanitizer, SASS/resource dumps and hardware-counter profiles were
not run on RTX 2060. All results retain the fixed-prefix scope.

## Concurrent concat progress

The separate [concat experiment](../concat-operands/REPORT.md) now has a
96-layer / 2,106,434-operation graph, compared with its 126-layer control.
Its consolidated evidence records 77 Scala tests, both complete CPU 100K
reference traces and matching named memory. The primary agent independently
checked the ten cited log/report hashes and both raw full-trace hashes.
Concat CUDA compilation, execution and timing remain **not run**. Opcode 29
requires its updated runtime; this graph is not included in either frozen
140/126 GPU matrix above.

## Reproduction

From the simulator, run the Scala suites with the workspace sbt launcher:

```bash
bash ../tools/sbt 'testOnly essent.BalancePriorityMuxChainsTest essent.NormalizeMuxIndexTest essent.ReduceConstantMuxChainsTest essent.BalanceAssociativeOpsTest'
bash ../tools/sbt test
```

From the workspace, use fresh output directories for:

```bash
python3 results/experimental/priority-mux/check_minimal.py
python3 results/experimental/priority-mux/compile.py --mode off
python3 results/experimental/priority-mux/compile.py --mode on
python3 results/experimental/priority-mux/inspect_firrtl.py
python3 results/experimental/priority-mux/prepare.py
python3 results/experimental/priority-mux/inspect_lookups.py
python3 results/experimental/priority-mux/run_coremark.py --mode off --cpu 12
python3 results/experimental/priority-mux/run_coremark.py --mode on --cpu 13
python3 results/experimental/priority-mux/run_coremark.py --collect
python3 results/experimental/priority-mux/export_gpu.py
python3 results/experimental/priority-mux/collect_checks.py
python3 results/experimental/priority-mux/plot_structure.py
python3 results/experimental/priority-mux/package_latest_gpu.py
```

The plot command requires Matplotlib and NumPy in the selected Python environment.
The off/on correctness commands can run concurrently on the distinct cores
shown above; `--collect` requires both to finish. `collect_checks.py` also
checks disabled output against the retained `constant-mux/runtime/on` baseline.
Use `check_minimal.py --verify-existing` to reexecute retained minimal binaries
and check their fixture/oracle hashes without recompilation. Compilation,
workload and export scripts refuse to overwrite existing run results.
In the new remote experiment, run `run_latest_gpu.py --gpu-root PATH` under
CPU 2 affinity, followed by `profile_latest_gpu.py --gpu-root PATH` and
`collect_latest_gpu.py --gpu-root PATH`. These entry points use the recorded
Blackwell UUID and sm_120 configuration. After retrieving and verifying the
archive and its receipt, run `audit_latest_gpu.py` and `plot_latest_gpu.py`
locally. The native Windows RTX 2060 adapters are recorded separately.
After retrieving the RTX 2060 receipt/archive into `runtime/rtx2060/`, run
`python3 results/experimental/priority-mux/audit_rtx2060.py`, then
`python3 results/experimental/priority-mux/plot_gpu_comparison.py`. The latter
exports PNG, SVG and PDF from both audited data sets.

Large FIRRTL, tensors, binaries and traces remain ignored. Required
local jars, source RTL export, reference trace and C++ dependencies are
retained inputs, not distributed artifacts. The upstream submodule remote is
not established to contain these local simulator commits.
