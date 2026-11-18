# GPGSIM framework and infrastructure migration plan

Updated November 18, 2026. The user selected **option 2**: maintain an independent
repository by selecting necessary files from the research workspace. The
repository is now `zhangziqing/GPGSIM`, a simulation framework and supporting
infrastructure built on RTeAAL. Nanhu is its current design integration and
validation case. The earlier name, `RTeAAL-Nanhu`, described the initial import;
Nanhu performance reproduction is one framework use case.

The current workspace is the source pool. Its existing local checkout remains
at `RTeAAL-Nanhu/`; the logical repository name below is GPGSIM.
[ARCHITECTURE.md](ARCHITECTURE.md) describes component responsibilities and the
boundary between current integration support and future generalization.

## Repository ownership

| Repository | Maintained content |
| --- | --- |
| `zhangziqing/RTeAAL-Sim` | FIRRTL compiler, general graph transformations, fast-accesses source, upstream CPU kernels and compiler tests |
| `zhangziqing/XiangShan` | Processor RTL and any actual RTL modifications; pinned Nanhu dependencies |
| `zhangziqing/GPGSIM` | Simulation runtimes and hosts, build/run infrastructure, design integrations, workload recipes, checks, measurement workflows and provenance |

GPGSIM references both source repositories by exact Git
submodule commit. Compiler or RTL source is not duplicated in it. The initial
pins are simulator `9e702ac16e0035592f9285d840629aba159be6bd` and Nanhu
`0a68ebc13b3d96a78d8663679393ecbea5b8fb73`; both were confirmed accessible
through the user's GitHub forks on November 17, 2026. These references do not
include the current workspace's uncommitted compiler edits.

The processor source is unchanged in the selected experiment. No new RTL
patch is inferred from the presence of host or compiler adapters.

## Selected layout

```text
GPGSIM/
├── RTeAAL-Sim/                  # Pinned compiler submodule
├── XiangShan-nanhu-legacy/      # Pinned processor submodule
├── sources.lock.json           # Commits, tool versions, workload and input hashes
├── configs/                    # Observation interfaces, CSR schema, explicit passes
├── workloads/                  # Small RV64I sources and linker script
├── runtime/
│   ├── common/                 # CoreMark prefix observer
│   ├── cpu/                    # RU/NU/THP hosts, source build and kernel checks
│   ├── verilator/              # Rising-edge reference and idle JTAG policy
│   └── cuda/                   # CUDA backend, independent CPU controls and notices
├── tools/                      # Acquisition, elaboration, compilation, run entry points
├── fixtures/nanhu/             # Small semantic control inputs
├── evidence/original/          # Immutable selected historical records
├── provenance/                 # Per-file source hashes and migration inventory
├── docs/                       # Framework boundaries, integration status and remaining work
└── build/                      # Ignored models, generated RTL, binaries and new runs
```

Generic compiler changes stay in the compiler repository. GPGSIM owns runtime
execution and its supporting infrastructure. Existing Nanhu-specific observers,
host devices, model inputs and settings retain their current locations and
contracts. Broader design support requires explicit interfaces and validation;
this positioning update does not generalize the runtime implementation.

## File selection and import status

[publication-files.csv](../provenance/publication-files.csv) retains the original
392-file, approximately 8.5 MB source inventory. Its source hashes are unchanged.
`destination_repo` assigns current ownership, including the GPGSIM rename,
and `stage` distinguishes imported files, existing compiler dependencies,
replacement entry points and pending ports. Repeated target names can identify
several old scripts replaced by one new source-build entry point; they are not
multiple file copies.

The first import selects CPU and Verilator hosts, the CUDA implementation and
its tests/licenses/JSON header, required observation configuration, small
workload sources, semantic fixtures and historical evidence. The per-file
import record is in `provenance/imports.json`. Path adaptations
are recorded separately from the original source hashes.

The inventory is a source pool and migration backlog, not an assertion that all
392 files are necessary for one result or have already been ported. In particular,
legacy graph-optimization, GSIM, AVX-512 and profiling drivers remain pending.
Do not copy the entire `results/` tree or the outer workspace history.

Toucan, unrelated Kunminghu work, dispatch/account settings, downloads, caches,
large source exports, executable files, compiled libraries, JARs, full traces,
tensors and checkpoints are excluded from ordinary source Git. Existing local
files and generated artifacts are preserved.

## Reproduction identities and boundaries

- Single-core Nanhu `DefaultConfig`, real L1/L2/L3; exact dependency revisions
  are retained in the original source manifest and pinned submodule trees.
- JDK 11, Mill 0.11.7, Scala 2.12.13/FIRRTL 1.5.1 for elaboration; simulator
  Scala 2.13.12/FIRRTL 1.5.6 and sbt 1.9.6.
- `ready-to-run` revision `55df55381ce26dab14211e0fed4165661035ec54`, 18,984-byte
  CoreMark image, SHA-256
  `1156255cce0d01cffbf7232fa4a168a00defc41e0c8ea6096a3186ca30a48519`.
- Fixed 256 MiB RAM, synchronous flash, idle JTAG, rejected SD and out-of-range
  access are part of the simulation contract.
- CoreMark measurements cover **100,000 non-reset cycles after 64 reset cycles**,
  not a completed benchmark, final CRC or CoreMark score. Historical reference
  trace SHA-256:
  `aac4bdad014633cadab8d50497e1555c60e05272eb091e0c3445c32237e6c3ae`.
- Graph layouts have separate whole-data fingerprints. A newly compiled graph
  requires its own independent CPU interpretation; architectural trace equality
  does not make data-slot fingerprints comparable across layouts.
- Historical reports and manifests keep their original paths, commands and
  hashes. New builds write new identities; never relabel historical measurements
  as having used newly integrated source.

## Code Modification Plan

1. Maintain `README.md`, `docs/ARCHITECTURE.md`, this migration plan and the
   publication inventory around GPGSIM's framework and infrastructure ownership.
   Preserve source hashes, dependency pins and Nanhu validation boundaries.
   Check links, Git metadata, inventory fields and ignored artifact paths.
2. Build the CPU hosts and preprocessing from pinned upstream source using CMake;
   generate the measured wide-counter NU wrapper directly from `kernel_NU.cc`.
   Rebuild RV64I workloads and run existing independent RU/NU kernel controls.
   Preserve the measured THP policies and fail when required coverage is missing.
3. Add explicit source-build entry points for JDK 11/sbt, the Mill classpath,
   elaboration, tensor compilation, Verilator reference construction and checked
   CoreMark-prefix execution. Use isolated build outputs; require hashes and
   explicit CPU affinity. These entry points require full source-flow validation
   before declaring fresh-clone reproduction accepted.
4. Maintain CPU/CUDA runtime builds, semantic controls and dependency notices.
   Run the controls applicable to each source revision. CUDA compilation,
   real-device controls, portable model export and full-prefix execution have
   separate acceptance records; historical test counts are revision-specific.
5. Port reported comparisons one at a time: original CPU baseline, GPU graph and
   runtime variants, repaired PLRU, then GSIM/AVX-512 if their results are published.
   Resolve transitive imports, old class overlays, source pins, graph settings and
   model-package dependencies for each. Preserve defective PLRU and reverted
   variadic-concat measurements as historical evidence.
6. Validate in a fresh checkout, check path independence, submodule availability,
   ignore behavior, notices and imported hashes. Record compilation/setup/execution
   separately. Publish source only with explicit current verification status;
   full reproducibility requires regeneration or hashed release assets and a
   checked full-prefix result from the documented configuration.
7. Before adding further designs, define model generation, host-device and
   observation interfaces around the current runtime and Nanhu integration.
   Keep compiler and RTL ownership separate. Validate each new integration
   against its own reference; defer directory moves and API refactoring until
   those requirements are established.

## Initial implementation status

The independent public repository is now named `zhangziqing/GPGSIM`.
The selected import contains 202 source/evidence records with original and
imported hashes; the larger inventory remains a backlog, not a wholesale copy.

The CPU CMake build rebuilds RU/NU/CoreMark hosts, FESVR and preprocessing from
pinned source. Its two independent kernel CTests pass; the CUDA backend's eight
CPU CTests also pass. Both RV64I workload binaries were rebuilt. Relocated tensor
layout/liveness checks match the original reports, and a source-built CPU host
matches the original Verilator trace over 1,024 non-reset cycles using the old
frozen model. A separate fresh local checkout builds both runtime projects and
passes all ten CTests with an explicitly supplied installed yaml-cpp dependency.
These checks do not build fresh recursive RTL dependencies or regenerate Nanhu.

`main` contains the checked import and runtime helpers. `reproduction-flow`
isolates the new compiler-build, elaboration, model-compilation and Verilator
source-build entry points, which have syntax/CLI checks but no complete Nanhu
acceptance. No new performance result or complete CoreMark run is claimed.

## Remaining acceptance work

The framework still needs explicit interfaces for additional designs and a
portable model-generation flow. The steps below describe the remaining Nanhu
integration acceptance, rather than defining the repository's whole scope.

The compiler's prior small-test integration does not establish a newly generated
full Nanhu result. Full elaboration, source compiler build, tensor export,
CPU/Verilator 100K comparison, portable CUDA export and GPU measurements must be
tracked individually. An unavailable check is `not run`, not a pass. Keep newly
added, unverified full-flow entry points separate from accepted source imports.

Historical absolute throughput requires comparable hardware and measurement
settings. The repaired projection-only flat comparison has one timed sample per
variant; it does not complete the repaired original-kernel A/B/C three-round
matrix. Neither repository creation nor importing a passing report reproduces
that report's performance measurement.
