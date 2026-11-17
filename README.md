# RTeAAL Nanhu reproduction artifact

This repository selects the Nanhu simulation and performance-reproduction files
from the original research workspace. It owns the Nanhu hosts, CUDA backend,
workload recipes, configurations and selected evidence. The FIRRTL compiler is
maintained in the `RTeAAL-Sim` submodule; processor RTL is maintained in the
`XiangShan-nanhu-legacy` submodule, pointing to the author's XiangShan fork.

**Status:** initial source organization and runtime checks. This is not yet an
accepted fresh-clone reproduction of every reported performance result.
[Verification and remaining work](docs/STATUS.md) distinguishes the completed
checks from unrun full-design and hardware checks. The `reproduction-flow`
branch holds the source-flow entry points awaiting full end-to-end validation.

## Contents

```text
RTeAAL-Sim/                 Pinned compiler and general kernels (submodule)
XiangShan-nanhu-legacy/     Pinned Nanhu RTL (submodule)
sources.lock.json          Source commits, tool versions and workload hashes
configs/                   Observation interfaces, CSR schema and graph settings
workloads/                 Checked RV64I source and linker script
runtime/cpu/               RU/NU/THP hosts and source-build CMake project
runtime/verilator/         CoreMark reference host and idle JTAG harness
runtime/cuda/              CUDA backend, independent CPU controls and notices
fixtures/nanhu/            Small clock, memory and reset control inputs
provenance/                Source-file hashes and migration inventory
evidence/original/         Selected original reports, checks and compact results
tools/                     Source identity, workload and tensor-layout helpers
build/                     Ignored generated inputs, binaries and new runs
```

The exact dependencies are simulator
`9e702ac16e0035592f9285d840629aba159be6bd` and Nanhu
`0a68ebc13b3d96a78d8663679393ecbea5b8fb73`. See
[sources.lock.json](sources.lock.json) and [the model contract](docs/MODEL_CONTRACT.md).
The original workspace's uncommitted compiler edits are not included in these pins.

## Build and check the runtimes

On Linux, install a C++17 compiler, CMake 3.18+, Python 3, pthreads and yaml-cpp
headers/library. GNU/Clang host compilers are supported by the source wrappers.
Fetch the compiler and top-level RTL checkouts first:

```bash
git submodule update --init -- RTeAAL-Sim XiangShan-nanhu-legacy
python3 tools/verify_sources.py
cmake -S runtime/cpu -B build/cpu -DCMAKE_BUILD_TYPE=Release
cmake --build build/cpu -j4
ctest --test-dir build/cpu --output-on-failure
```

For a non-system yaml-cpp installation, supply
`-DCMAKE_PREFIX_PATH=/path/to/install`. CMake rebuilds FESVR and preprocessing
from source; it does not need an old `libfesvr.a` or a candidate runner binary.
The measured wide-counter NU evaluator is generated from the pinned upstream
`kernel_NU.cc`, with an independent OU comparison and a RU wide-layer check.
`-march=native` is enabled for the CPU host to match the measured candidate;
use `-DNANHU_NATIVE=OFF` for a portable diagnostic binary and record the change.

The CPU controls do not need Nanhu tensors. Running the processor additionally
requires generated model files under `build/model`, or a supplied
`-DNANHU_MODEL_ROOT=/path/to/model` at CMake configuration time. That tree has
`kernel/{json,txt,yaml}`, `utils/bin` memory metadata and `layout.json`.
Historical evidence is not a model package.

The CUDA backend also provides eight CPU-only controls:

```bash
cmake -S runtime/cuda -B build/cuda-cpu -DNANHU_ENABLE_CUDA=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build/cuda-cpu -j4
ctest --test-dir build/cuda-cpu --output-on-failure
```

For real CUDA execution, build with `NANHU_ENABLE_CUDA=ON` and explicitly select
`CMAKE_CUDA_ARCHITECTURES` for the device. [CUDA runtime instructions](runtime/cuda/README.md)
explain model requirements, flags and validation boundaries.

The RV64I workloads require a RISC-V GNU toolchain:

```bash
python3 tools/build_workloads.py
```

This creates two binaries and a new manifest under ignored `build/workloads/`.
The CoreMark image comes from the pinned `ready-to-run` submodule; the acquisition
identity and hash are in the lock file. No workload or generated model binary
is embedded in this repository.

## Evidence and publication scope

[The publication inventory](provenance/publication-files.csv) assigns a destination
and migration state to each candidate source file. [Import provenance](provenance/imports.json)
records original and imported hashes. Original reports keep their commands,
paths and identities: they are historical records, not measurements of this
new repository. Relative links inside them can refer to original-workspace
files that have not been ported.

CoreMark results in this artifact concern a **100,000-cycle non-reset prefix
after 64 reset cycles**. They do not establish a completed CoreMark run, final
CRC or score. [The migration plan](docs/MIGRATION.md) lists the remaining source
flow, CUDA export and comparison-specific work. Toucan and unrelated Kunminghu
experiments are excluded.

[Third-party source notices](NOTICE.md) preserve the provenance and license
boundaries of the imported code and pinned dependencies.
