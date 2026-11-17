# PLRU slice projection and ordered update composition

This experiment uses the current binary concat encoding (opcode 17). The
historical 96-layer catn model is preserved elsewhere and is not the baseline.

| Variant | Slice projection | Update composition |
| --- | --- | --- |
| A | Off | Off |
| B | On | Off |
| C | On | On |

Both new compiler switches default to off: `--project-slices` and
`--compose-masked-updates`. All three variants retain the accepted FTQ lookup,
constant mux reduction, priority mux balancing, and binary concat balancing.

The historical CPU artifacts are in `runtime/A-binary-v1`, `runtime/B-binary-v1`,
and `runtime/C-binary-v1`. The frozen compiler, runtime, input, and helper sources
are in `runtime/binary-snapshot-v1`. Existing directories are never overwritten.
Large runtime files are intentionally not versioned.

Read [REPORT.md](REPORT.md) for findings and proof boundaries,
[checks.json](checks.json) for the CPU correctness gate,
[candidate-summary.json](candidate-summary.json) for bounded candidate evidence,
and [performance.json](performance.json) for separate CPU measurements.
The CPU gate is immutable evidence; its `gpu_execution` field describes that
CPU-only gate. GPU acceptance is recorded separately.
The completed Blackwell audit is [rtx6000-checks.json](rtx6000-checks.json), with
[named-memory-checks.json](named-memory-checks.json) documenting declared-width
state equality and the preserved differences in extra host storage bits.

## Repaired-model validation

The historical B/C artifacts contain a confirmed six-bit normalization defect.
Their original audits and measurements remain retained as historical evidence.
The focused compiler correction is [normalization-fix.patch](normalization-fix.patch).
Current source identity and validation status are recorded in
[normalization-source.json](normalization-source.json) and [REPORT.md](REPORT.md).
Fresh large artifacts are isolated under `/tmp/rteaal-plru-normalized-v5` because
an earlier compilation exhausted workspace storage. The repaired
[raw-state gate](normalization-checks-v5.json) passes all three real 100K runs.
Blackwell validation and timing are not run: automatic approval review blocked
the private payload upload to corvus01 pending explicit authorization, recorded
in [normalization-blackwell-block-v5.json](normalization-blackwell-block-v5.json).
Historical timing results do not establish repaired performance.
The exact input package and the full local compilation/validation archive are
preserved under `runtime/normalized-v5/`; their sizes and hashes are in
[normalization-artifacts-v5.json](normalization-artifacts-v5.json).

To reproduce from the current source, select a new work root; neither the root
nor its output labels may overwrite existing evidence. `freeze.py` accepts an
absolute snapshot path and compiles production sources only. On the recorded
node007 CPU allocation, use disjoint compile affinities and run one finish
worker per variant after starting compilation:

```bash
plru_work_root=/tmp/rteaal-plru-normalized-reproduction
python3 results/experimental/plru-projection/freeze.py --label "$plru_work_root/snapshot"
# Run these compilation commands concurrently in separate terminals.
taskset -c 0-11 python3 results/experimental/plru-projection/compile_normalized.py --mode A --work-root "$plru_work_root"
taskset -c 12-23 python3 results/experimental/plru-projection/compile_normalized.py --mode B --work-root "$plru_work_root"
taskset -c 24-35 python3 results/experimental/plru-projection/compile_normalized.py --mode C --work-root "$plru_work_root"
# Run one finish command per mode; each checks layout and the real 100K prefix.
python3 results/experimental/plru-projection/finish_normalized.py --mode A --work-root "$plru_work_root"
python3 results/experimental/plru-projection/finish_normalized.py --mode B --work-root "$plru_work_root"
python3 results/experimental/plru-projection/finish_normalized.py --mode C --work-root "$plru_work_root"
python3 results/experimental/plru-projection/check_normalization.py --audit --work-root "$plru_work_root"
python3 results/experimental/plru-projection/package_normalized_gpu.py --work-root "$plru_work_root"
```

The gate compares every raw byte-memory element and named input/register value
against retained original A, retains all 16 AND63 writer masks, and checks full
reference traces, architectural state, memory allocation metadata and memory64
fingerprints. Collector dumps are excluded from timing. The repaired packager
uses the retained unchanged CUDA runtime and supplies complete independent NU
CPU fingerprints for each model. Transfer only to the selected Blackwell host,
stage in a fresh remote directory, and run the same `run_gpu.py` driver below.
After collecting and verifying the evidence parts into the new work root's
`rtx6000/` folder, run:

```bash
python3 results/experimental/plru-projection/audit_normalized_gpu.py --work-root "$plru_work_root"
```

## Historical reproduction

Run from the workspace root. Choose new snapshot and output labels when
recompiling; the following labels describe the retained historical run. Production
Scala compilation is permitted. Scala tests were not run after the user stopped
Scala testing.

```bash
python3 results/experimental/plru-projection/freeze.py --label binary-snapshot-v1
python3 results/experimental/plru-projection/compile.py --mode A --label A-binary-v1 --snapshot results/experimental/plru-projection/runtime/binary-snapshot-v1
python3 results/experimental/plru-projection/compile.py --mode B --label B-binary-v1 --snapshot results/experimental/plru-projection/runtime/binary-snapshot-v1
python3 results/experimental/plru-projection/compile.py --mode C --label C-binary-v1 --snapshot results/experimental/plru-projection/runtime/binary-snapshot-v1
python3 results/experimental/plru-projection/prepare.py --mode A --label A-binary-v1
python3 results/experimental/plru-projection/prepare.py --mode B --label B-binary-v1
python3 results/experimental/plru-projection/prepare.py --mode C --label C-binary-v1
python3 results/experimental/plru-projection/run_coremark.py --mode A --label A-binary-v1 --cpu 95
python3 results/experimental/plru-projection/run_coremark.py --mode B --label B-binary-v1 --cpu 95
python3 results/experimental/plru-projection/run_coremark.py --mode C --label C-binary-v1 --cpu 95
python3 results/experimental/plru-projection/collect_checks.py --label-A A-binary-v1 --label-B B-binary-v1 --label-C C-binary-v1
```

`compile.py --analysis-only` records candidates without applying rewrites. The
full per-candidate dependencies, shared nodes, costs, depths, and refusal reasons
remain in each run's `slice-candidates.jsonl`; the compact summary truncates
dependency samples and retains their original counts. Overlapping dry-run costs
must not be added together.

GPU preparation uses the exact CPU-validated tensors and the unchanged portable
CUDA runtime from `../nanhu-cuda-2060`. Export checks use the frozen binary layout
decoder. A/B/C share one CUDA binary on the selected host.

```bash
python3 results/experimental/plru-projection/export_gpu.py --mode A
python3 results/experimental/plru-projection/export_gpu.py --mode B
python3 results/experimental/plru-projection/export_gpu.py --mode C
python3 results/experimental/plru-projection/run_gpu_oracle.py --build
python3 results/experimental/plru-projection/run_gpu_oracle.py --mode A
python3 results/experimental/plru-projection/run_gpu_oracle.py --mode B
python3 results/experimental/plru-projection/run_gpu_oracle.py --mode C
python3 results/experimental/plru-projection/run_gpu_oracle.py --collect
python3 results/experimental/plru-projection/package_gpu.py
```

The three portable CPU interpretations can run concurrently on CPUs 12/13/14;
their timings are validation costs, not performance samples. Packaging and GPU
compilation may overlap that work. The GPU driver waits for the separate
`cpu-fingerprints.json` before full GPU validation. The retained Blackwell run
also uses `run_host_oracles.py` on CPU cores 8/9/10 to generate these fingerprints
sooner, while the local interpretations continue. Both sets must agree in the
final audit. Copy this script to the remote `support/` directory and run it from
a separate process; it builds CPU-only Release sources under `oracle/` and writes
the gate atomically after all three complete.

Transfer `runtime/gpu-input.tar.gz` through the registered Paseo host route,
verify its SHA256, extract to a new `frozen/` directory, and copy `frozen/support/`
to `support/`. Deliver `runtime/gpu-cpu-oracles.json` atomically as
`cpu-fingerprints.json` in the experiment root. On the Blackwell host, with CUDA
13.2 on `PATH`, run:

```bash
taskset -c 2 python3 -u support/run_gpu.py
python3 support/collect_gpu.py /home/zhangziqing/plru-projection-20261114
```

The retained staging script and upload receipts are under `runtime/`. Download
the collector's receipt and all evidence parts, verify each part and the archive,
then run the local final-memory comparison before accepting GPU measurements:

```bash
python3 results/experimental/plru-projection/run_memory_diagnostic.py
python3 results/experimental/plru-projection/audit_memory_diagnostic.py
python3 results/experimental/plru-projection/audit_gpu.py
```

The diagnostic uses separate local copies of the validated NU runners on CPUs
90/91/92 and preserves their measured executables. It exports every byte-memory
element, verifies both memory-pool fingerprints against the portable CPU oracles,
and compares named values at their declared widths. Each allocation's name,
width, extent, and exact base must agree across A/B/C. These runs are excluded
from performance samples. `audit_gpu.py --provisional` checks the transferred
runtime evidence while leaving cross-model named-memory acceptance pending.
All output directories are created exclusively to preserve previous evidence.

GPU measurement uses device 0, CUDA Graph, 128 threads, threshold 256, direct
memory commit, the original kernel, and three rounds in ABC/BCA/CAB order.
Only Blackwell is in scope for this GPU run, as selected by the user.
