# GPGSIM verification status and remaining work

GPGSIM is a simulation framework and supporting infrastructure built on RTeAAL.
The checks recorded here currently cover runtime components and the Nanhu
integration. They do not establish support for additional designs. See
[ARCHITECTURE.md](ARCHITECTURE.md) for responsibilities and integration boundaries.

Recorded November 17, 2026. Machine-readable evidence is in
[provenance/validation.json](../provenance/validation.json).

| Check | Result | Boundary |
| --- | --- | --- |
| Dependency source identities | Passed | Clean local checkouts at both exact pins; GitHub confirmed both commits exist in the author's forks |
| CPU source build | Passed | RU checked host, NU/CoreMark/THP variants, FESVR and preprocessing rebuilt from pinned C++ source |
| Existing RU/NU controls | 2/2 passed | All 29 opcodes, large counters, wide shifts and memory compared with independent existing evaluators |
| CUDA backend CPU controls | 8/8 passed | Schedule, opcode, memory, conflict-plan and flat-descriptor controls; GPU not exercised |
| Fresh local checkout | Passed | CPU and CUDA-CPU builds plus all 10 CTests in a separate temporary directory; existing installed yaml-cpp used |
| RV64I workloads | Rebuilt | Loop counts 1 and 1000; binary hashes recorded in local ignored build manifest |
| Tensor layout and liveness | Passed | Existing frozen model; semantic reports/input hashes match original checks after path relocation |
| CPU CoreMark short prefix | Passed | 1,024 observed non-reset cycles match the original Verilator trace byte-for-byte |
| Compiler build/full elaboration/new tensors | Not run | Source-flow entry points remain isolated on `reproduction-flow` |
| New Verilator source build and complete 100K comparison | Not run | Historical reports do not supply this acceptance |
| Portable CUDA model export, CUDA build and GPU replay | Not run | No NVCC/GPU on the source host; no model package is committed |
| Reported performance replication | Not run | No new timed experiment is claimed |

The 1,024-cycle smoke run uses the unchanged historical tensor files through
ignored local symlinks. Its new runner is rebuilt from source, and its complete
trace SHA-256 is
`3632de25749ade3041fc5901ddc4f08d85fdfc79fb021b00e141f15e4a2c79cc`.
This confirms a bounded host/path migration; it does not verify new compiler
output, the THP coverage requirement on another host, or a completed workload.

## Branch boundary

`main` holds the selected source baseline, the checked CMake builds and the
validated workload/layout/runtime helpers. `reproduction-flow` adds explicit
JDK/sbt, Mill/elaboration, tensor compilation and Verilator source-build entry
points. Those additions have only syntax/interface checks until the full
source flow is run. They must not be treated as accepted reproduction commands
on the strength of the main branch's runtime checks.

## Next acceptance units

1. Validate pinned recursive RTL dependencies and resolve historical nested
   submodule acquisition issues with explicit source manifests.
2. Run the JDK 11 compiler build and full pinned elaboration with a newly
   generated Mill classpath. Generate observations and compare with the pinned
   configuration before accepting the full processor flow.
3. Compile/preprocess tensors into a new output directory and validate an
   independent CPU model against a newly source-built Verilator reference.
   Progress from a small prefix to the full checked 100K prefix.
4. Add a portable CUDA exporter with per-graph fingerprints and a hashed asset
   recipe; run CUDA compilation, device controls and full-prefix checks on the
   selected GPU before timing.
5. Port each comparison's sources, settings and measurement driver separately.
   The inventory marks old CPU tuning, mux/concat/PLRU, runtime ablations,
   GSIM and AVX-512 drivers as pending. Their reports remain original evidence.

The old defective PLRU measurements and reverted variadic-concat results retain
historical status. The repaired projection-only flat comparison has one sample
per variant; it does not complete the repaired A/B/C three-round matrix.
