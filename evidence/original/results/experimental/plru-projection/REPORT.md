# PLRU projection: binary concat A/B/C

The six-bit host normalization defect is repaired. All 48 production Scala
sources compile, and fresh A/B/C models pass the complete 100K Verilator prefix
and exact raw byte-memory and named-state comparison with retained original A.
The 466 byte-memory and four hold-register differences disappear. Repaired ITLB
depth remains 126/73/39, with full graph depth 126/94/94. No Scala tests are run
under the user's instruction. The corrected projection-only flat-runtime replay
now passes independent audit: A/B reach 3,191.975/3,426.573 cycles/s (+7.350%),
with one independent 100K sample each. The repaired original-runtime A/B/C matrix
remains unmeasured. The pre-repair measurements remain historical evidence.

The historical binary experiment reduced the ITLB PLRU path from 126 layers in
A to 73 in B and 39 in C, with full depth 126/94/94 and Blackwell medians of
2,925.833 / 3,189.111 / 3,227.730 cycles/s. Although the prefix trace and declared
state agreed, the later raw-byte audit found a real normalization defect in B/C.
The historical artifacts and their original audits remain unchanged.

## Projection-only replay with flat descriptors (2026-11-15)

The follow-up isolates corrected slice projection: A disables projection and B
enables it; update composition is disabled in both. Both use the current flat
runtime with the one-line parallel-commit `#pragma unroll 1` patch. CUDA Graph,
128 threads, threshold 256, CPU 2, `sm_120`, an empty build type and identical
compiler flags are fixed. One independent 100K sample per variant completed in
A then B order. Separate reset-plus-100K candidate-mode collision diagnostics
for each variant and a known-conflict positive control pass; both timed runs
therefore use direct commit. Applicability is limited to these fixed prefixes.

Local preparation passes: all 30 evidence files from the repaired normalization
gate were rehashed, model assets and variant-specific CPU fingerprints agree,
and both frozen models pass exact 1024-cycle CPU flat traces with the current
runtime. Those short checks retire only three instructions and provide no GPU
performance evidence. The repaired full graphs contain 126/94 layers and have
141/129 statically reconstructed kernel executions per cycle.

The frozen A/B package contains model data, the reference trace, CPU verification
receipts, compiler provenance, and current runtime/build dependencies. It is
45,296,001 bytes, SHA256
`d8e156bb51ced9694a3cee27578c13bc7f2b15f8cc6d8bcf6d718dbf6293071b`;
all 55 manifest entries and the archive itself were independently verified.
It is retained under `runtime/projection-only-flat-20261115T084928Z/`.
The composition-enabled C model is excluded.

The user explicitly approved this exact 45.30 MB package transfer to the paired
my-laptop/corvus01 host and continued GPU measurement. After the earlier HTTP 502
failures, runtime approval succeeded: all 22 parts uploaded with valid receipts
and the designated agent completed the measurement. Whole-archive verification,
CTest 30/30, two GPU smoke checks, both full-prefix alias diagnostics and both
timed 100K trace/model-specific CPU fingerprint comparisons pass. The local
audit verifies 205 returned evidence files, original input identities, timing
arithmetic and raw monitoring. Both timed samples satisfy the consecutive-ten
idle gate; 35/33 running observations detect no external compute PID.
No approval bypass or alternate export was used. The retained runtime folder
contains upload receipts, 22 part hashes, authorization, commands and evidence.

| Corrected projection-only flat replay | A: OFF | B: ON |
| --- | ---: | ---: |
| Cycles/s, one sample each | 3,191.975 | 3,426.573 |
| Evaluation seconds / 100K | 31.328566 | 29.183680 |
| Kernels/cycle | 141 | 129 |

B/A is 1.073496 (+7.350%). Single samples do not establish statistical ranking.
Large GPU traces remain remote; local verification checks their hash receipts
and the retained comparison source. One-second monitoring cannot exclude
activity between observations. CoreMark and final CRC remain incomplete.
[The focused report](PROJECTION_ONLY_FLAT.md),
[results](projection-only-flat-results.json) and
[checks](projection-only-flat-checks.json) record this accepted replay.
[projection-only-flat-preparation.json](projection-only-flat-preparation.json)
records exact inputs, settings, destination and the completed transfer/audit.

## Normalization repair

The compiler now omits a reference truncation only when a bounded proof of its
actual producer semantics establishes normalized upper host bits. Unproven
narrow references retain explicit AND/slice masks, including the signed six-bit
predictor writers. The proof is local to a graph version, bounded to 128 visits
and 32 levels, and treats state, arithmetic, memory, and lookup as opaque.
Mux conditions also pass through the shared projection analysis so normalization
aliases do not split otherwise identical update prefixes.

[normalization-fix.patch](normalization-fix.patch) records the focused repair
against the historical frozen compiler: 46 added and six deleted lines across
four files. [normalization-source.json](normalization-source.json) identifies
the new production sources. The new snapshot manifest SHA256 is
`8d133413d59b4a0e38edda58464298dd1dcbfbeba3838d810063716b34c441e2`.
Large new artifacts use `/tmp/rteaal-plru-normalized-v5` after a previous
workspace compilation ran out of storage; failed attempts remain preserved.
[normalization-artifacts-v5.json](normalization-artifacts-v5.json) records the
retained workspace archives under `runtime/normalized-v5`: the exact GPU input
package and a 470,860,665-byte full local compilation/validation archive, SHA256
`0872bf04f4c28d1898ccc011ca63bd4901fbef8d3750978a32cf95b649d96a70`.
The archives preserve generated models, source snapshot, fixed inputs, raw dumps,
complete traces, runners, candidate reports and graph metadata. Both archive
copies were hash-checked, and all five historical audit/measurement files remain
unchanged. Git metadata is read-only, so these edits remain uncommitted.
The final gate requires exact raw byte-memory and named input/register equality
with retained A, full memory64 fingerprints, all 16 explicit AND63 writers,
layout checks, and complete reference traces. All three pass;
[normalization-checks-v5.json](normalization-checks-v5.json) records the full gate.
Every one of the 206,884 raw byte-memory elements and 150,978 named input/register
values agrees with retained A. Memory64 fingerprints, all 4,505 allocation map
entries, and named memory64 block hashes also agree. A additionally reproduces
all retained raw data slots byte for byte. These are fixed-prefix checks, not
exhaustive proof or full CoreMark CRC validation. No new CPU timing samples are
claimed; the dump-enabled executions are correctness runs.

[normalization-structure-v5.json](normalization-structure-v5.json) records the
passing final layout checks. All variants retain 4,391 lookup operators,
198,676 entries and 515 observed harness signals. Of 151,461 named outputs,
B/C each shorten 9,091, preserve 142,370, and lengthen none relative to A.
Full named paths and the bounded candidate reports remain at the recorded
artifact paths with SHA256 identities.

| Repaired graph / compilation | A | B | C |
| --- | ---: | ---: | ---: |
| ITLB normal-refill PLRU depth | 126 | 73 | 39 |
| DTLB super-refill PLRU depth | 59 | 55 | 31 |
| Full graph layers | 126 | 94 | 94 |
| LoadQueue `writebacked_0` depth | 94 | 94 | 94 |
| Operations | 2,222,558 | 2,221,744 | 2,220,671 |
| Data slots | 490,089 | 491,052 | 490,944 |
| Argument words | 5,121,670 | 5,098,656 | 5,094,665 |
| Compiler elapsed time | 13:58.06 | 18:58.29 | 14:07.39 |
| Compiler maximum RSS, KiB | 96,791,072 | 104,350,212 | 100,356,504 |

The three compiles run concurrently on separate 12-CPU affinities. The elapsed
times are observed resource costs, not isolated compiler speed comparisons.
Projection applies 15,949 rewrites, adding 25,048 and removing 25,862 operators
(net -814). Composition applies the two state candidates, adding 1,372 and
removing 2,445 (net -1,073). The ITLB bit tree costs -1,064 operators and depth
73 to 39; the word-mask alternative costs -1,061 and depth 41. Explicit
normalization changes operator costs relative to the historical faulty models;
only new validated models will supply repaired performance numbers.

The repaired portable package is 68,057,730 bytes, SHA256
`881d5745790d43124ad249ac9620ffbc3c792f0d7fe38f507095565d7e52e881`.
It retains the unchanged original CUDA runtime, adds independent NU CPU full-state
fingerprints, and targets only corvus01's Blackwell UUID
`GPU-67983fd2-eb55-ece7-a1ec-40165d7759fb`. Static reconstruction gives 141/129/129
kernel launches per cycle for A/B/C, rather than the historical 141/127/127;
B/C now have 63 wide compute/commit pairs. Their final suffix contains 1,733/1,627
nodes. Static structure does not establish execution speed.

[normalization-blackwell-block-v5.json](normalization-blackwell-block-v5.json)
records the actual automatic-review rejection before upload started. The reviewer
requires explicit approval to export this private package, containing models,
runtime sources, reference traces and verification metadata, to corvus01.
No alternative export or remote measurement was attempted after the rejection.
The next authorized GPU step is the same frozen package and the same three
correctness plus nine separate ABC/BCA/CAB measurement runs. New GPU performance
is **not run**. Historical GPU numbers below do not validate repaired models.

## Configuration and provenance

| Variant | Projection | Composition | Retained folder |
| --- | --- | --- | --- |
| A | Off | Off | `runtime/A-binary-v1` |
| B | On | Off | `runtime/B-binary-v1` |
| C | On | On | `runtime/C-binary-v1` |

The simulator base is `274d8f75c13f690546dfb64f4254fcbc0c1ef7a3` with the
experimental source overlay recorded in each provenance file. The source and
runtime snapshot manifest SHA256 is
`d85100878ed0e65039960f252f975484a69e66b6431979f0660af59f3df4f52d`.
Forty-eight frozen production Scala sources compile. Overlay probes and JVM
class-load logs confirm the new passes, compiler, exporter, and binary concat
class load from that same overlay in all three builds. No Scala tests were run
after the user's instruction. Test sources remain available but are not evidence
for this accepted binary workload run.

| Fixed input | SHA256 |
| --- | --- |
| `results/nanhu/SimTop.preclock.lo.fir` | `4eca6ecb46a01e8fc14a14e7d285ba123dbf70dd9d291d3c3217c7c09886ed0e` |
| `ready-to-run/coremark-2-iteration.bin` | `1156255cce0d01cffbf7232fa4a168a00defc41e0c8ea6096a3186ca30a48519` |
| Verilator 100K trace | `aac4bdad014633cadab8d50497e1555c60e05272eb091e0c3445c32237e6c3ae` |

All variants retain FTQ lookup normalization, CSR constant mux reduction,
priority mux balancing, and binary concat balancing. No opcode 29 is emitted.
The original 96-layer catn artifacts, unsuccessful integration attempts, and
earlier candidate reports remain preserved; none substitutes for this baseline.

## Graph and compilation results

| Metric | A | B | C |
| --- | ---: | ---: | ---: |
| ITLB normal-refill PLRU depth | 126 | 73 | 39 |
| DTLB super-refill PLRU depth | 59 | 55 | 31 |
| Full graph layers | 126 | 94 | 94 |
| LoadQueue `writebacked_0` depth | 94 | 94 | 94 |
| LoadUnit 1 perf register depth | 69 | 60 | 60 |
| Operations | 2,222,558 | 2,220,432 | 2,219,573 |
| Data slots | 490,089 | 491,448 | 491,338 |
| Argument words | 5,121,670 | 5,096,601 | 5,093,026 |
| Lookup operations | 4,391 | 4,391 | 4,391 |
| Lookup entries | 198,676 | 198,676 | 198,676 |
| Observed harness signals | 515 | 515 | 515 |
| Compiler elapsed time | 21:39.50 | 16:48.02 | 19:14.11 |
| Compiler maximum RSS, KiB | 101,534,972 | 57,667,852 | 101,823,672 |

A reproduces the independent retained `concat-tree/runtime/on` control,
including all named depths. Of 151,461 named outputs, B and C each shorten
10,625, preserve 140,836, and lengthen none. The complete named critical paths
remain in each `graph.json` and `named-depths.json`.

Projection applies 17,280 rewrites: 26,984 added operators and 29,110 removed,
net -2,126. Composition then applies two state candidates: 1,359 added and 2,218
removed, net -859. The ITLB bit tree contributes -840 operators and depth 73→39;
the DTLB bit tree contributes -19 and depth 55→31. The ITLB whole-word masked
form predicts depth 41 and net -838, so the bit tree wins. Six ordered accesses
are eligible without the existing priority mux pass's eight-entry threshold.

## Rewrite contract

`ProjectSlices.scala` uses a bounded cache keyed by signal, low bit, and width.
It projects through concat, nested slices, explicit masks, muxes, and supported
bitwise operators. Identical projected mux branches remove the condition
dependency. Arithmetic carries, dynamic shifts, memory, and lookup remain
boundaries. Rewrites operate only on known widths up to 64 bits; masks,
inversion, and truncation have explicit widths.

`ComposeMaskedUpdates.scala` proves every hold path reaches the exact old state
bit, merges mux path conditions, and rejects an enable or data input that
transitively depends on an earlier update. Immutable cycle inputs and old state
are explicit leaves. Ordered `(enable,data)` composition uses arrival depths
to select an interval tree and compares bit-tree and word-mask forms.

`SliceGraph.scala` and `SliceRewrite.scala` share expression information and
perform transactional backfill. Exact costs include retained shared producers,
reconstruction, and temporary nodes. Protected outputs, aliases, lookup
references, and register commits remain observable boundaries. Rewrites occur
before layering, with final binary concat balancing afterward. The experiment
uses per-candidate net growth 1,024 and total growth 8,192; candidate and traversal
limits are recorded in the reports. Neither new pass is enabled by default.

## Correctness and CPU measurement

[checks.json](checks.json) records the passing workload gate and hashes 45
evidence files. Layout checks cover operand/output bounds, same-layer writes,
memory placeholder liveness, dependencies, lookups, and observables. Each of
the three 100K traces matches the retained Verilator trace byte for byte; final
GPR/FPR/CSR agree. All 4,505 named memory map entries and six memory64 block
hashes agree across A/B/C and the retained accepted memory control. This original
gate did not compare memory8 contents. The memory control is a prior simulator
result, not an independent Verilator memory dump. The expanded byte-memory audit
is recorded separately and does not rewrite this immutable CPU gate.

Separate samples reuse the validated binaries and tensors on node007, CPU 95,
one host thread, 64 reset cycles followed by exactly 100,000 measured cycles.
The CPU policy is `schedutil/acpi-cpufreq`, 1.5–2.2 GHz. Host load is recorded.

| Separate CPU sample | A | B | C |
| --- | ---: | ---: | ---: |
| Evaluation seconds | 522.555297 | 531.061531 | 507.975543 |
| Cycles/s | 191.367307 | 188.302097 | 196.859871 |
| Throughput / A | 1.00000 | 0.98398 | 1.02870 |
| Tensor load seconds | 2.919497 | 3.145588 | 3.236889 |

[performance.json](performance.json) separately hashes ten measurement evidence
files. Every sample rechecks the complete trace, architectural state, and named
memory hashes. One sample per mode on a shared host does not establish a stable
CPU speedup. Compilation costs, load costs, and execution times are separate.

## GPU validation and measurement

The user selected Blackwell only for this run. Frozen A/B/C GPU models use the
unchanged original CUDA runtime and binary concat opcode 17. Static schedule
reconstruction reports 141/127/127 kernel launches per cycle for 126/94/94
logical layers; layer reduction alone is not a measured speedup.

| Static GPU schedule | A | B | C |
| --- | ---: | ---: | ---: |
| Wide compute kernels/cycle | 69 | 62 | 62 |
| Wide commit kernels/cycle | 69 | 62 | 62 |
| Fused suffix layers | 57 | 32 | 32 |
| Fused suffix nodes | 1,306 | 1,758 | 1,636 |
| Total kernels/cycle, including reset/gather | 141 | 127 | 127 |

Projection removes seven wide compute/commit pairs from the schedule. The suffix
has fewer dependency layers but more nodes than A. Composition further reduces
suffix nodes while preserving B's layer and launch counts. These are structural
facts; they do not attribute measured time to a kernel region.

The Blackwell input archive is 68,087,997 bytes, SHA256
`79d20c5e19d892f257a7a640e92435047e3885c167e4d775cdf8afba65d6c944`.
The selected device is RTX PRO 6000 Blackwell Workstation Edition,
UUID `GPU-67983fd2-eb55-ece7-a1ec-40165d7759fb`, on corvus01. The driver is
580.178.04; GPU architecture is sm_120. Execution is fixed to CPU 2, device 0,
CUDA Graph, 128 threads, tail threshold 256, and direct memory commit. Device
clocks remain automatic; preflight, postflight, and per-run occupancy are saved.

Fifteen existing C++/CUDA CTest cases, three 1024-cycle smoke comparisons, three
100K correctness checks, and nine separate 100K measurement runs pass. The three
measurement rounds use ABC/BCA/CAB order. Every full GPU trace matches Verilator
byte for byte, and final GPR/FPR/CSR match. Each model's complete data, memory64,
and memory8 fingerprints agree with CPU interpretations on both node007 and
corvus01. The CPU builds use CUDA disabled, four CPU CTest cases each, and
distinct compilers; their timings are validation costs. The remote CPU reference
jobs finish before GPU measurement. The final local audit verifies all
340 transferred evidence files, source/model/binary identities, schedules,
complete traces, architectural state, and independent CPU fingerprints.

| GPU measurement, median of three | A | B | C |
| --- | ---: | ---: | ---: |
| Cycles/s | 2,925.833 | 3,189.111 | 3,227.730 |
| Evaluation seconds / 100K | 34.178302 | 31.356700 | 30.981530 |
| Throughput / A | 1.00000 | 1.08998 | 1.10318 |
| Allocated device bytes | 1,156,375,428 | 1,156,244,192 | 1,156,211,832 |

[rtx6000-checks.json](rtx6000-checks.json) records the accepted GPU audit and links
the separately verified memory check. The immutable archive is 184,316,763 bytes,
SHA256 `0422934fe80fb23cba8fdf19a4d165feb485c30ca697fd27465721d3a845f5ee`.
The runtime build and CTest step take 11.040 seconds. A/B/C share one original
CUDA binary; compilation, model loading, and measured evaluation are separate.
The three throughput ranges are 2,924.570–2,929.222 / 3,189.007–3,189.957 /
3,227.116–3,231.598 cycles/s. The B→C comparison gains 1.21% with unchanged
global depth and launch count. This experiment does not include CUPTI attribution
of that gain to individual kernels.

[named-memory-checks.json](named-memory-checks.json) expands the original memory
gate. Local copies of the validated NU runners export final byte memory and data
after independent 100K trace checks. Both memory-pool fingerprints also reproduce
the portable CPU/GPU fingerprints for every variant. All 4,505 memory allocations
have identical names, bases, extents, and declared widths. All 206,884 elements of
the 2,343 byte memories agree at those declared widths. The entire memory64 pool
has the same fingerprint across A/B/C. The 150,978 named input/register values
also agree at their declared widths, using types recovered from the original
generated header.

The raw memory8 difference comprises 466 bytes in the 16 six-bit SC predictor
tables. Every changed byte has bit 5 set; B/C set extra host bits 6 and 7, while A
clears them. For example, A stores `0x3e` and B/C store `0xfe`; all store the same
six-bit pattern `111110`. Four six-bit hold registers have the corresponding
extra host bits. Normalizing each named byte value to its declared width gives
the same SHA256, `6a4cea62e99c57cefa769be4170dee767aea478da41af883de5043de1e53c84f`.
This establishes final declared-state equality for the prefix; it does not claim
raw byte-for-byte equality of host storage or equivalence for arbitrary future
workloads. Full traces and raw fingerprints remain retained, without modifying
the measured models, sources, or binaries.

The root cause is an incorrect normalization assumption in projection backfill,
confirmed by [width-loss-cause.json](width-loss-cause.json). `tpeConvExtract`
implements six-bit signed-to-unsigned conversion with `AND 63`. The projection
recognizer treats that mask as a slice, then the same-width `ref` case in
`SliceRewrite` bypasses it and emits an assignment. All 16 affected writers use
`AND 63` in A and `ASSIGN` in B/C. Width metadata describes the hardware bit
pattern but does not prove the 64-bit host slot has already cleared upper bits.
Signed values can remain sign-extended, and byte-memory commit truncates only
to eight bits. Thus signed -2 becomes `0x3e` with the six-bit mask and `0xfe`
without it. This is a compiler normalization defect; the successful fixed-prefix
checks do not establish that removing such masks is safe for every consumer.
The required correction is to preserve normalization unless it is proved from
the actual producer representation. This diagnosis left the historical measured artifacts and compiler source
unchanged at the time; the subsequent repair is documented above. Reproduce the static check with
`python3 results/experimental/plru-projection/inspect_width_loss.py`.

The diagnostic runners only add final dumps and memory fingerprints, and their
runs are excluded from performance samples. Automatic approval rejected an
additional diagnostic upload to Blackwell, so the completed check ran locally.

## Limits and retained work

All correctness claims concern this fixed Nanhu 100K prefix. They do not prove
complete CoreMark execution/CRC or exhaustive equivalence of every supported
rewrite. Requested Scala tests are not run under the later user instruction.
Runtime artifacts and failed attempts are preserved. Git metadata is mounted
read-only in this environment, so the source and experiment changes remain
uncommitted; the snapshot hashes identify the actual tested state.
