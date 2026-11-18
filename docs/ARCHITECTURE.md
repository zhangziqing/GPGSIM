# GPGSIM architecture and responsibilities

GPGSIM maintains a simulation framework based on RTeAAL and the infrastructure
needed to build, execute, validate and measure it. Nanhu is the current integrated
design and research workload. General framework responsibilities and the scope
verified by that integration must be distinguished.

## Repository responsibilities

| Repository | Responsibility |
| --- | --- |
| `zhangziqing/GPGSIM` | CPU/CUDA runtimes, simulation hosts, build/run tooling, design integration, workload recipes, checks, measurement workflows and provenance |
| `zhangziqing/RTeAAL-Sim` | FIRRTL compiler, tensor extraction, general graph transformations, upstream CPU kernels, preprocessing and compiler tests |
| `zhangziqing/XiangShan` | Processor RTL and its source dependencies; the pinned Nanhu integration |

Compiler changes belong in RTeAAL-Sim. RTL changes belong in the corresponding
design repository. GPGSIM connects these sources to host execution and records
which source, configuration and model were actually checked.

## Components in this repository

| Area | Current content and boundary |
| --- | --- |
| `runtime/cpu/` | CPU simulation hosts and source builds over pinned RTeAAL kernels; Nanhu layout, workload and observation interfaces |
| `runtime/cuda/` | Tensor-graph execution, CUDA scheduling and memory handling, independent CPU controls; the executable and model replay currently use Nanhu interfaces |
| `runtime/common/`, `runtime/verilator/` | CoreMark observations and the Nanhu reference harness, including device and reset policies |
| `tools/` | Source verification, workload construction, kernel wrapper generation, layout/liveness checks and checked execution |
| `configs/`, `workloads/`, `fixtures/nanhu/` | Current Nanhu integration settings, small RV64I workload sources and semantic controls |
| `sources.lock.json`, `.gitmodules` | Dependency commits, tool versions, kernel identities and Nanhu workload/reference hashes |
| `provenance/`, `evidence/original/` | Import identities, selected historical evidence and validation records |
| `build/` | Ignored generated models, binaries and new run output |

This layout describes existing files. Shared infrastructure and Nanhu-specific
code still coexist in some directories. Additional designs need explicit model,
device and observation interfaces; changing the project name does not establish
those interfaces or their correctness.

## Simulation flow and infrastructure

1. Select design and compiler revisions and record tool versions and inputs.
2. Elaborate RTL, lower FIRRTL and extract a tensor graph with RTeAAL-Sim.
3. Preprocess the graph, memory metadata and observation interfaces into a
   model for the selected runtime.
4. Build CPU/CUDA hosts and a suitable independent reference.
5. Run semantic controls and compare the requested simulation observations.
6. Measure compilation, setup, execution and resource use separately, retaining
   commands, source/model hashes, hardware settings and result provenance.

GPGSIM's infrastructure includes source locking, source builds, workload recipes,
model checks, reference comparison and measurement records. The source-flow
entry points for full elaboration, compilation and Verilator construction are
currently isolated on `reproduction-flow` pending end-to-end validation. A
portable CUDA model exporter remains unfinished. See [STATUS.md](STATUS.md) for
checks actually completed and [MIGRATION.md](MIGRATION.md) for remaining work.

## Nanhu integration and future designs

The present model and measurement contract is documented in
[MODEL_CONTRACT.md](MODEL_CONTRACT.md). It includes reset, RAM, flash, JTAG,
observation and cycle-prefix policies. These are properties of the Nanhu
integration, not universal defaults for new designs. Current CoreMark evidence
covers a bounded prefix, not benchmark completion or a final score.

A further design integration must identify its RTL revision and top-level
configuration, model generation recipe, device and clock/reset semantics,
observation interface, workloads and an independent reference. CPU and CUDA
acceptance must be recorded for the generated model before publishing performance
claims. General asynchronous or multiple-clock support is not established by
the present Nanhu checks.

The next organization steps are to make those integration boundaries explicit
and validate portable model generation.
