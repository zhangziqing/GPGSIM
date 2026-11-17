# Flat runtime descriptor experiment

## Outcome and scope

Strategy 1 is implemented as the opt-in `--flat-descriptors` path in
`../nanhu-cuda-2060/`. It copies fixed scalar operands into a runtime descriptor
to remove one dependent operand-pool lookup. Default execution still uses the
original representation. The implementation plan is in [PLAN.md](PLAN.md).

Host functional checks and CUDA `sm_75` compilation pass. The initial build
host had no exposed NVIDIA device, so it could not run the GPU checks. A later
run on an RTX PRO 6000 Blackwell is recorded below; its measured result is
specific to the frozen graph and 100K-cycle prefix.

## Representation and execution contract

The serialized `Op` remains four uint32 fields / 16 bytes. Existing little-endian
format-1 exports, source mappings, operation ordering and operand pools are
unchanged. After model validation, `build_flat_plan()` builds a five-field,
20-byte `FlatOp`: `out`, `code`, `x`, `y`, `z`.

| Opcodes | Runtime operand fields |
| --- | --- |
| 0–20 and 24 | Copy the original one, two or three argument words; zero unused fields |
| 21–23 and 25–28 | Original argument offset, output-word count, zero |

Argument words include width encodings, shift immediates, masks and jump offsets;
they are not all signal indices. All indices remain uint32. Arithmetic, widths,
signedness, truncation, defined large shifts and error behavior use the shared
evaluator. A compile-time fixed-operand specialization prunes unreachable
variadic and memory cases without duplicating their arithmetic implementation.

CUDA selects legacy or flat template instantiations when dispatching or capturing
the graph. Compute, fused small layers, ordered memory arbitration and direct
memory commit all use the selected descriptor. Only that descriptor array is
allocated on the device; the original operand pool remains for fallback nodes.
The CPU validation backend uses the same choice and removes its legacy descriptor
pointer in flat mode.

Each original layer still completes all reads and output/memory-write staging
before commit and scatter. Layer order, reused signal slots, reset, cycle
boundaries, memory latency, aliased-write priority and direct-commit applicability
remain unchanged. MUX embeds all three indices but still reads its condition and
only the selected candidate value. This change does not introduce shared-memory
residency, eager MUX candidate loads or a graph transformation.

## Frozen graph and cost

Validation uses the accepted binary-concat **on** graph in
`../concat-tree/runtime/portable/gpu/on/data`, rather than replacing the portable
runner's original 1,036-layer default data.

- Model SHA-256:
  `46bfece3ef8cbbf71d2efd91a8886070554db7b606a5b40d7e4c90eac17b4be2`.
- 2,222,558 nodes, 126 layers, 490,089 data slots.
- 2,177,119 nodes inline their operands: **97.96%**.
- Legacy descriptors: 35,560,928 bytes; flat descriptors: 44,451,160 bytes.
  Additional device descriptor storage is 8,890,232 bytes (**8.48 MiB**, +25%).
- The full argument pool is retained. Host setup temporarily owns both model
  descriptors and the flat plan; conversion time is recorded separately as
  `flat_plan_seconds`, within `tensor_load_seconds`.

The larger descriptor footprint and 20-byte stride can offset the shorter load
dependency. Inline coverage is a node count, not a measured fraction of GPU time.

## Verification evidence

| Check | Result |
| --- | --- |
| Release CPU CTest | 8/8 pass |
| ASan/UBSan CPU CTest | 8/8 pass; leak detection disabled |
| CUDA 12.6.85 `sm_75` compilation, GCC 13.3.0 | Pass, no compiler warnings |
| CPU CTests from the CUDA build | 8/8 pass |
| Legacy/flat 4,096-cycle full-size CPU runs | Both trace comparisons pass; all three final-state fingerprints agree |
| Flat 100K CPU run against Verilator and accepted legacy CPU state | Pass: exact 100,000-line trace and all three final-state fingerprints |
| Blackwell CUDA build, architecture `sm_120` | Pass, see GPU evidence below |
| CUDA-build CTest | 30/30 pass, including 22 GPU tests |
| Compute Sanitizer | 9/9 short memcheck, racecheck and synccheck runs report zero errors |
| Alternating GPU 100K comparison | 6/6 runs pass exact trace and three accepted CPU state fingerprints |

The opcode fixture compares all 29 supported opcodes with the existing independent
OU oracle over more than 18,000 vectors and four cycles. The descriptor test
also compares 29 opcodes over 64 states each, requires inline evaluation to work
with both the original descriptor and operand-pool pointers null, checks fallback
multiword/memory behavior, and covers width, division, large-shift and bounds
errors. Scheduling tests cover staging/scatter, reset and aliased mem64/mem8
writes; direct-memory fixtures cover disjoint writes.

The 4,096-cycle runs retire only three instructions and are smoke checks, not
workload completion or performance evidence. The full 100K prefix includes
CoreMark execution but does not finish the benchmark or validate its final CRC.
The full run records 167,375 commit events and 171,898 retired instructions
including fused instructions, with UART `Running CoreMark for 2 iterations\r\n`
and no exception/trap. The 100K trace SHA-256 is
`aac4bdad014633cadab8d50497e1555c60e05272eb091e0c3445c32237e6c3ae`.

All three final-state fingerprints equal the accepted legacy CPU result for the
same frozen graph:

| State | Fingerprint |
| --- | ---: |
| Full data slots | 6218498432779941060 |
| mem64 | 15503565812696885107 |
| mem8 | 656100811014868721 |

The frozen model does not contain these expectations in `expected_100k`, so the
run wrapper correctly leaves `full_100k_state_fingerprints_checked=false`.
The separate `collect_checks.py` audit verifies the legacy result's model hash,
reference trace and accepted status, then compares these three fingerprints and
sets `full_100k_fingerprints_compared_with_legacy_cpu=true` in `checks.json`.
The full-run binary is byte-identical to the final CPU Release build (SHA-256
`024c707213f4e0a9b5c41eb946c1bc956caff147b990170b51d5c2a793235091`).
Its descriptor conversion takes 0.049770523 seconds on this host; this is setup
cost and does not establish GPU conversion cost or a speedup.

CUDA resource use for the two current instantiations:

| Kernel | Legacy registers | Flat registers |
| --- | ---: | ---: |
| Compute | 60 | 60 |
| Fused small layers | 63 | 63 |
| Ordered commit | 22 | 22 |
| Direct commit | 20 | 20 |

All ten emitted entry points report zero stack bytes and zero spill loads/stores;
PTX contains no local-memory loads/stores. Inspection of the fixed AND path shows
two uint64 data loads using descriptor indices, eliminating the two uint32
operand-pool loads in the legacy path. This verifies the intended dependency
removal in this compiler output; it does not measure cache behavior or speed.

## Blackwell GPU Validation

The committed source at `0a6d7cc93a67823f26753741a309ed215ca5ab84` was built
unchanged for an NVIDIA RTX PRO 6000 Blackwell Workstation Edition (`sm_120`),
driver 580.178.04 and CUDA 13.2.78. The fixed binary-concat ON model hash and
accepted CPU oracle matched the frozen input above. The 100,000-line reference
trace SHA-256 is
`aac4bdad014633cadab8d50497e1555c60e05272eb091e0c3445c32237e6c3ae`.

All six GPU executions used CUDA Graph, 128 threads, fusion threshold 256,
ordered memory commit, device 0 and CPU affinity 0. The three pairs ran in
AB/BA/AB order. Every sample performed 100,000 cycles, passed the online
reference comparison and matched all three state fingerprints recorded above.
The CTest suite passed 30/30 (8 CPU, 22 GPU). Separate short Compute Sanitizer
runs covered flat layer execution, fused execution and the direct-memory
fixture under memcheck, racecheck and synccheck; all 9 exited successfully with
zero errors/hazards.

| Pair | Legacy cycles/s | Flat cycles/s | Flat / legacy |
| --- | ---: | ---: | ---: |
| 1 (AB) | 1875.009 | 1931.203 | 1.029970 |
| 2 (BA) | 1874.296 | 1930.607 | 1.030043 |
| 3 (AB) | 1874.202 | 1927.471 | 1.028422 |

The median throughput was 1874.296 cycles/s for legacy and 1930.607 cycles/s
for flat descriptors, a **3.004%** ratio-of-medians improvement for this run.
The median of the three per-pair ratios was 1.029970. Flat-plan conversion took
12.5 ms and was included in tensor loading/setup, outside the reported
steady-state evaluation interval. Device allocation increased from
1,156,375,428 to 1,165,265,660 bytes, matching the additional 8,890,232-byte
descriptor array. Reported compute, fusion and commit kernels used 40 registers
in both modes with zero stack and spill bytes; gather used 20 registers and
reset used 10. CMake configuration took 0.799 s and the CUDA build took
3.484 s. Nsight Compute counters were not collected.

This is a three-pair result on one graph and a fixed 100K prefix, not evidence
of a general speedup or a completed CoreMark run. The prefix does not validate
the final CoreMark CRC. Device event time is included in evaluation time and
should not be added to it. See
[`runtime/blackwell-rtxpro6000-20261115/`](runtime/blackwell-rtxpro6000-20261115/)
for the retrieved report, six result/command receipts, CTest and Sanitizer logs,
input/build hashes and SM120 resource data. Large traces, input data and
binaries remain on the GPU host.

## Optimization replay (2026-11-15)

**Status: independent source, run-receipt and raw-monitoring audit passed.**
The archive's size/hash and all 316 manifest entries match. The earlier HTTP 502
transfer failures were resolved through runtime approval. The earlier six-run
comparison above remains unchanged. Large traces remain on the GPU host;
local review checks their hash receipts and the source that compares complete
traces. [optimization-rerun-checks.json](optimization-rerun-checks.json) records
checks reproduced by `python3 results/experimental/flat-descriptors/audit_optimization_rerun.py`.

The replay preserves the accepted binary-concat ON model and CPU oracle above.
Its recorded configuration fixes CUDA 13.2.78 / `sm_120`, device 0, CUDA Graph,
128 threads, threshold 256, CPU affinity 2 and an empty `CMAKE_BUILD_TYPE`.
The two builds use identical `-lineinfo --ptxas-options=-v` flags. The patched
build adds only `#pragma unroll 1` to the parallel memory-commit loop; the control
retains its original loop. Candidate arbitration and direct commit are separate,
mutually exclusive settings. FTQ normalization, CSR reduction, priority MUX and
binary-concat balancing are retained in the frozen graph. Corrected PLRU work
is excluded because its GPU performance remains unverified.

| Loop / memory commit | Legacy cycles/s | Flat cycles/s | Flat / legacy change |
| --- | ---: | ---: | ---: |
| No-unroll / full scan | 1794.139 | 1846.170 | +2.900% |
| No-unroll / candidates | 2886.242 | 3143.617 | +8.917% |
| No-unroll / direct | 2906.753 | 3174.644 | +9.216% |
| Original loop / direct | 2878.113 | 3139.856 | +9.094% |

Each cell is one independent 100K sample. These are neither medians nor a
statistical ranking. Within this reported matrix, candidates improves legacy
and flat throughput over full scan by 60.87% and 70.28%; no-unroll improves
direct throughput over the original loop by 0.995% and 1.108%. This supports
missing memory-commit settings as the main explanation for the previous large
legacy deficit. Cross-experiment absolute comparisons also differ in CPU
affinity and build configuration and cannot isolate the cause. The historical
binary-concat ON result used direct commit with the original loop and measured
2930.850 cycles/s as its three-run median; compare it to the new original-loop
legacy sample of 2878.113, rather than equating it with the full-scan baseline.

All eight samples' receipts pass the full 100K trace hash and the three accepted
CPU fingerprints; CUDA CTest passed 30/30. A separate, untimed alias diagnostic
detected the known conflicting
fixture with error 7 and found no active same-pool/same-address collision,
including equal-value writes, in the large layers parallelized by direct
commit during reset and the complete 100K prefix. That diagnostic does not
establish conflict freedom for other executions. No full CoreMark CRC was run.

The idle policy requires ten consecutive observations, about one second
apart, with 0% GPU utilization and no compute process before each sample, then
process/utilization monitoring throughout execution. The raw logs pass all idle
gates and contain no external compute PID for the accepted samples. One-second
sampling cannot exclude activity between observations. A discarded initial CTest
attempt and its logs remain preserved; the subsequent 30/30 retry is audited
separately and no timing sample comes from the discarded attempt.

Local checks for the one-line source patch are independently available: CPU
Release CTest 8/8 passed, CUDA 12.6.85 `sm_75` compilation passed without compiler
warnings/errors, and all 30 emitted kernel entries across three binaries report
zero stack/spill bytes. All 17 prepared source hashes still match. Patched
`engine_cuda.cu` SHA-256 is
`6c39fad7ed9b9311aed901e4ecc5b94980bec748f82d75a68e217935a03b80ad`;
its base at `0a6d7cc93a67823f26753741a309ed215ca5ab84` is
`6edf7d848611ecb6a4f7b284a8c07e761d4ee7b1783ffcf48e6b32cd4e885384`.

The retrieved evidence archive is 864,993 bytes, SHA-256
`e1c63e880e918439dc18db700df147730986f2f3caa2a57ffedbab2629c7e0ac`, at
`/home/zhangziqing/flat-descriptors-20261115T040104Z/optimization-rerun-20261115T080342Z/optimization-rerun-evidence.tar.gz`
on `my-laptop` (`srv_Mmu5pxKa0bxY`). It includes source, diffs,
replay/monitor scripts, commands, logs and a manifest; large traces remain on
that host. The structured result is
[optimization-rerun-results.json](optimization-rerun-results.json). Local preparation evidence
is under `runtime/optimized-blackwell-20261115/`.
The earlier uncommitted snapshot patch remains preserved in that runtime
directory. No push was made.

## Reproduction

From the workspace root, configure and test a CPU validation build:

```bash
cmake -S results/experimental/nanhu-cuda-2060 -B /tmp/rteaal-flat-unit-cpu \
  -DNANHU_ENABLE_CUDA=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/rteaal-flat-unit-cpu -j4
ctest --test-dir /tmp/rteaal-flat-unit-cpu --output-on-failure
taskset -c 15 python3 results/experimental/nanhu-cuda-2060/run.py \
  --binary /tmp/rteaal-flat-unit-cpu/nanhu_gpu --cpu --flat-descriptors \
  --data results/experimental/concat-tree/runtime/portable/gpu/on/data \
  --cycles 100000 --label flat-descriptors-flat-100k
```

Choose an available affinity CPU and a new label if preserving an earlier run.
The GPU build uses CUDA 12.6.85, `-DCMAKE_CUDA_ARCHITECTURES=75` and
`-DCMAKE_CUDA_FLAGS='-lineinfo --ptxas-options=-v'`, with the same Release source.
ASan/UBSan uses a separate Debug CPU build with
`-O1 -fsanitize=address,undefined -fno-omit-frame-pointer` and
`ASAN_OPTIONS=detect_leaks=0`.

On a GPU host, after compiling and making the same frozen data and accepted CPU
reference available, run three alternating pairs:

```bash
python3 results/experimental/flat-descriptors/run_pairs.py \
  --build /tmp/rteaal-flat-cuda75 \
  --data results/experimental/concat-tree/runtime/portable/gpu/on/data \
  --oracle-result results/experimental/concat-tree/runtime/portable/gpu/on/runs/binary-concat-cpu-100k/result.json \
  --cpu-affinity 15 --label flat-pairs
```

This defaults to CUDA Graph, 128 threads, fusion threshold 256, ordered memory
commit, three pairs in AB/BA/AB order and 100K cycles per sample. Use the same
explicit `--direct-memory-writes` or `--memory-candidates` selection for both
representations when testing either mode. It runs CTest first, verifies model and
binary hashes and all three final-state fingerprints for every sample, and only
then reports throughput medians and their ratio. It records build configuration,
affinity, GPU/driver metadata, conversion/loading cost, device time and allocated
bytes. `--dry-run` only checks prerequisites and prints commands; its successful
local check is not a GPU test. Use Compute Sanitizer separately from timed runs.

Local logs, PTX, cubin, copied full-run binary and pair dry-run receipt live in
`runtime/`. `collect_checks.py` audits these artifacts and writes the compact
[checks.json](checks.json), including input, source, binary, trace and log hashes:

```bash
python3 results/experimental/flat-descriptors/collect_checks.py
```

Large datasets and generated binaries remain local.
