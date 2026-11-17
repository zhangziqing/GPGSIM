# GSIM performance on the pinned Nanhu revision

Three validated CoreMark 100K prefixes on Nanhu commit `0a68ebc13b3d` achieved
**1,781.611 cycles/s median**, with a range of **1,771.453–1,808.044 cycles/s**.
All three full traces are byte-identical to the existing Verilator reference.
The experiment ran on node007, AMD EPYC 7773X, pinned to CPU 82, on 2026-11-14.

## Results

| Run | Evaluation seconds | Cycles/s | Loop wall seconds | Process wall seconds | Maximum RSS, MiB |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1 | 56.450845 | 1,771.453 | 57.854637 | 58.067759 | 302.0 |
| 2 | 56.128980 | 1,781.611 | 57.548227 | 57.766149 | 302.0 |
| 3 | 55.308386 | 1,808.044 | 56.697028 | 56.920668 | 302.0 |

The sample span is 2.054% of the median; three runs do not establish a confidence
interval. The median including reset, trace formatting, and online comparison
is **1,737.673 cycles/s**. CPU 82 and SMT sibling 210 were initially 3.50% and
3.02% busy. During the samples the sibling was 9.29%, 8.97%, and 7.70% busy.
There is no new paired Verilator measurement on CPU 82, so these samples do not
establish a controlled speedup over the historical Verilator measurements.

| Run | Whole-process userspace IPC | Branch miss % | L1I misses / 1K instructions |
| --- | ---: | ---: | ---: |
| 1 | 1.185983 | 33.1777 | 2.62793 |
| 2 | 1.187656 | 33.1902 | 2.63886 |
| 3 | 1.206419 | 33.2205 | 2.62520 |

All runs observe 167,375 commit events and 171,898 retired instructions including
4,523 fused instructions, 74,948 compressed events, and 74 skipped events.
First retirement is at master cycle 630, first RAM retirement at 8,321, and the
last retired PC is `0x80001c84`. There are no exceptions or traps. UART is
`Running CoreMark for 2 iterations\r\n`.

[All samples and raw counter values](coremark-100k-20261114-result.json) and
[independent audit](coremark-100k-20261114-audit.json) preserve the measurements
and checks. The 1,024-cycle preflight also passed. Failed diagnostic runs are
excluded from these statistics.

## Workload and acceptance

The design is single-core Nanhu DefaultConfig, including L1/L2/L3 caches, at
`0a68ebc13b3d96a78d8663679393ecbea5b8fb73`. The checkout is an export; the
revision comes from [version-selection.json](../../nanhu/version-selection.json).
This experiment does not change the original RTL or the RTeAAL tensor.

Each sample asserts reset for 64 master cycles and measures exactly 100,000
non-reset master cycles of `ready-to-run/coremark-2-iteration.bin`. The image is
18,984 bytes, SHA-256
`1156255cce0d01cffbf7232fa4a168a00defc41e0c8ea6096a3186ca30a48519`.
The reference is the existing cycle trace documented in the
[CoreMark report](../nanhu-coremark-100k/REPORT_zh.md), SHA-256
`aac4bdad014633cadab8d50497e1555c60e05272eb091e0c3445c32237e6c3ae`.

Acceptance requires an online, unshifted, byte-for-byte comparison of every
cycle's retirement events, GPRs, FPRs, CSRs, exception fields, trap, and UART.
The driver also hashes the complete traces. `audit.py` independently parses
each accepted trace with the existing reference parser and checks build/source
hashes and the recorded summaries. This is a fixed CoreMark prefix: the guest
does not finish CoreMark, and no final CoreMark CRC or score is established.

## Input and execution contract

The accepted generation route starts at the same-revision, sealed
`results/nanhu/SimTop.preclock.lo.fir`, SHA-256
`4eca6ecb46a01e8fc14a14e7d285ba123dbf70dd9d291d3c3217c7c09886ed0e`.
It precedes the RTeAAL clock-removal pass. See
[input-manifest.json](input-manifest.json) for the exact adapted input hash.

- The adapter restores 993 scalar memories to 3,705 explicit readers and 1,868
  writers. The source memories all have asynchronous reads, one-cycle edge
  writes, undefined read-under-write behavior, and no readwriters. The control
  covers enabled reads, masked writes, signed bit patterns, and same-address
  accesses against a separate software recurrence.
- Main-clock ports and cast aliases remain clocks. Original clock gates retain
  their TE/E/CK/Q interfaces and map to GSIM's built-in `ClockGate`.
- All 17,226 registers with a real reset use `AsyncReset`. The adapter retains
  that reset and explicitly holds each next-state input at its reset value
  while reset is asserted. The host refreshes GSIM scheduling with
  `activateAll()` at external reset transitions. No trace cycle is shifted,
  dropped, or masked to obtain acceptance.
- Each `step()` exposes state before the modeled rising edge and computes the
  following state. Callbacks and UART are observed at that boundary. External
  RAM reads use the pre-edge image; pending writes are published after the
  step, with bounds and overlapping-write checks. RAM is 256 MiB.
- Flash holds its synchronous boot-ROM result between enabled reads. Unexpected
  SD access fails the run. Difftest callbacks retain port declaration order;
  output references for RAM/SD are interleaved with input arguments as required
  by the generated interface.
- The C++ model and memory use deterministic zero initialization. JTAG is idle
  at TCK=0, TMS=1, TDI=0, TRSTn=0, matching the reference. The complementary
  JTAG clock is constant. Active JTAG behavior and general multi-clock behavior
  are outside the validation scope.

[Adapter controls](adapter-checks.json) contain three passing 2,048-cycle
checks for gated synchronous-reset recurrence, scalar memory, and asynchronous
reset with a two-stage release pipeline. Reset pulses and gate disable are
included. The unguarded asynchronous-reset negative control fails at cycle 1
because its reset pipeline advances while reset remains asserted.

## Timing and provenance

GSIM is `master-2026-10-29-23c4553`, built 2026-10-30; the compiler is clang++
19.1.1, C++17, `-O3`, without PGO or LTO. C++ compilation uses 16 workers.
Generation uses supernode size 15, `cpp-max-size-KB=8192`, and `__DOT__` hierarchy
separators. The runtime executes one model thread.

The evaluation timer covers model evaluation and publishing external RAM
writes, including boot and cache stalls. It excludes loading, reset, trace
formatting, and reference comparison. Loop wall time includes reset and the
observer; process wall time also includes initialization and perf/time wrappers.
Hardware counters cover whole-process userspace, so their IPC and cache-miss
rates have a broader scope than the evaluation timer. RSS is whole-process
maximum RSS. Samples share one pinned CPU and the repository benchmark lock.
The CPU and its SMT sibling utilization are recorded for each sample; this is
a shared host and the lock does not reserve the physical core.

Build measurements start from an existing Low FIRRTL checkpoint. They exclude
Chisel elaboration, FIRRTL lowering into that checkpoint, input adaptation,
and failed experiments. GNU time reports the largest individual child RSS for
parallel compilation; it does not measure aggregate peak compiler memory.
Full commands and generated-source hashes are in
[build-manifest.json](build-manifest.json). Large generated sources, binaries,
logs, and traces remain under ignored `build/`.

Successful GSIM generation took **313.53 s** wall time, peaking at
**27,357,416 KiB (26.090 GiB) RSS**, and emitted 230 C++ files. C++ compilation
and linking took **262.653 s**; the complete build driver took **272.17 s**,
including source hashing and manifest preparation. Its largest child RSS was
1,005,092 KiB; concurrent aggregate peak memory was not measured.
[Generation/build measurements](generation.json) identify the successful logs.
The manifest records the workspace and simulator-submodule heads at build time
and hashes the actual source files, including experiment files committed after
the build. The executable SHA-256 is
`fb97d8cece8fcc824aa4abdc0296c75714d768484ed76a1a935d919fdcb72cb6`.

## Diagnostic history

The raw CHIRRTL route (`prepare.py --raw`) reaches GSIM's `splitArray.cpp:115`
assertion and is not the measured route. Its partial-connect adapter and
diagnostic artifacts are retained. The default uses the sealed Low checkpoint.

The initial Low-input binary advanced the complete first 10,000-cycle trace by
one cycle. Every observation matched only after that diagnostic shift. A
separate reset control demonstrated incorrect next-state progression during
reset. The explicit reset guards fix the recurrence without changing the
comparison. Initial failed builds and traces are excluded from results.

One regeneration attempt lacked its output directory. GSIM terminated with
SIGSEGV when emitting C++; the same condition reproduced on a small control.
The directory was created before retrying. This failure is retained separately
and excluded from the successful build cost.

## Reproduction

Run from the workspace root; the pinned input checkpoint and reference trace
must already exist locally.

```bash
python3 results/experimental/gsim-nanhu/check_controls.py
python3 results/experimental/gsim-nanhu/prepare.py --low
mkdir -p results/experimental/gsim-nanhu/build/model
gsim --supernode-max-size=15 --cpp-max-size-KB=8192 \
  --sep-mod=__DOT__ --sep-aggr=__DOT__ \
  --dir=results/experimental/gsim-nanhu/build/model \
  results/experimental/gsim-nanhu/build/SimTop.fir
python3 results/experimental/gsim-nanhu/build.py --jobs 16
python3 results/experimental/gsim-nanhu/run.py --prefix repeat-unique
python3 results/experimental/gsim-nanhu/audit.py --prefix repeat-unique
```

Use a new prefix to retain previous samples. `run.py` chooses an idle allowed
CPU by a two-second CPU/sibling scan; `--cpu N` pins an explicit CPU instead.
To audit the accepted samples on this host, use `--prefix coremark-100k-20261114`.
