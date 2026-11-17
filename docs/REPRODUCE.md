# Source-flow validation branch

These are explicit source-build entry points under development. They have not
completed a fresh source-to-Nanhu replay. The accepted import/runtime checks are
listed in [STATUS.md](STATUS.md); they do not accept this workflow. Use fresh
build directories and preserve any failed-stage logs.

## Prerequisites and source identity

Use Linux, JDK 11, Mill 0.11.7, CMake 3.18+, a C++17 compiler, yaml-cpp and
Verilator 5.x. See `sources.lock.json` for Scala/FIRRTL and workload versions.
The full Nanhu compiler flow historically used a 256 GiB JVM heap and the
elaborator 128 GiB; size the machine accordingly. Heap options are explicit.

```bash
python3 tools/verify_sources.py --fetch --workload
python3 tools/build_compiler.py --java /path/to/jdk11/bin/java --test
python3 tools/elaborate.py --java /path/to/jdk11/bin/java --mill /path/to/mill
```

The source helper initializes recursive submodules without forcing changes to
existing work. Historical nested dependencies may require additional pinned
acquisition work. The original dependency manifest is retained under
`evidence/original/results/xiangshan-nanhu-source.json`; it also records source
exports that originally needed pinned API-config/hardfloat archive fallbacks.
Recursive acquisition has not been validated for this new flow.

The compiler build verifies/downloads the pinned sbt launcher and builds both
FIRRTL fast-accesses overrides plus `essent.jar`. It does not consume old class
overlay JARs. The elaborator creates the Mill run classpath before launching
single-core `DefaultConfig` with the Scala 2.12 override.

## Tensor compilation and preprocessing

```bash
python3 tools/compile_model.py build/elaboration/SimTop.hi.fir --java /path/to/jdk11/bin/java
cmake -S runtime/cpu -B build/cpu -DCMAKE_BUILD_TYPE=Release
cmake --build build/cpu -j4
ctest --test-dir build/cpu --output-on-failure
build/cpu/preprocess_tensor xiangshan-nanhu-indexed
python3 tools/layout.py
python3 tools/check_liveness.py
```

`compile_model.py` creates an isolated output tree under `build/model/`, copies
the pinned memory parser and runs the unmodified integrated compiler with
`configs/integrated-current.json`. It writes commands, input/JAR hashes and the
exact compiler identity. The output tree is intentionally fresh: an existing
one is rejected. For preserved Low FIRRTL, use `--preclock` explicitly.

The source compiler has several graph optimizations integrated by default.
This configuration does not claim to reproduce the original 1,036-layer
baseline or any specific optimization comparison. Accept each newly generated
graph only with trace/state checks and an explicit result identity.

The graph checkpoint is generated as a local artifact. It is compiler-specific
and does not replace source acquisition or publishable configuration records.
The preprocessing binary's input/output root is chosen at CMake configuration,
so a custom compile output also needs a matching `NANHU_MODEL_ROOT`.

## Reference build and prefix comparison

```bash
python3 tools/build_verilator.py --java /path/to/jdk11/bin/java
python3 tools/run_coremark.py verilator --cpu CPU_ID --cycles 1024 --label source-reference-smoke
python3 tools/run_coremark.py cpu --cpu CPU_ID --cycles 1024 --label source-cpu-smoke --reference build/runs/source-reference-smoke/actual.trace
```

Replace `CPU_ID` with an allowed, recorded CPU on the execution host. The source
reference builder lowers the original High FIRRTL and rebuilds Verilator RTL
and runtime objects; it does not link an old model archive. It checks the DPI
callback declarations and CSR schema against generated interfaces before
compilation. The supplied harness retains the original rising-edge UART policy.

The default CPU run uses the measured THP-prefault host. Full 36 MiB huge-page
coverage remains required; failure is preserved. For a non-THP diagnostic run,
specify `--binary build/cpu/coremark` and record the difference. CPU acceptance
requires a reference argument. Full 100K traces must match the known reference
hash; prefixes are sliced to the requested length before comparison.

After smoke validation, repeat both commands with `--cycles 100000` and new
labels. Source builds, full traces and final architectural state must pass
before any throughput is accepted. Compilation/setup/execution have separate
records and timing scopes. These commands have **not been run end to end**.

## Unported comparisons and CUDA model export

The portable CUDA source build and existing controls are in `runtime/cuda/`.
The historical exporter depends on original tensor/layout manifests, an
independent CPU fingerprint and the complete reference trace. A new exporter
must make those inputs explicit and check them for each graph before publication;
it has not been implemented here.

The inventory retains candidates for CPU tuning, mux/concat/PLRU optimization,
direct-memory and flat-descriptor comparisons, GSIM and AVX-512. Their old
source pins, candidate generators, class overlays, absolute paths and archive
dependencies require comparison-specific ports. No historical performance
report is reclassified as a run of this branch.
