# GPGSIM CUDA runtime: Nanhu integration

The CUDA runtime belongs to the GPGSIM simulation framework. Its current host,
model package and observation interfaces target Nanhu; broader design support
requires separate integration and validation. See the
[framework architecture](../../docs/ARCHITECTURE.md).

This directory contains the Nanhu CUDA backend, its independent CPU interpreter,
CMake controls and dependency notices. It is a source import; generated `data/`
model packages and full traces are intentionally absent.

## Build and semantic controls

From the artifact root, without a GPU:

```bash
cmake -S runtime/cuda -B build/cuda-cpu -DNANHU_ENABLE_CUDA=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build/cuda-cpu -j4
ctest --test-dir build/cuda-cpu --output-on-failure
```

All eight CPU tests passed for this import. Real CUDA builds require a supported
CUDA toolkit and explicit target architecture, for example `75` for the historical
RTX 2060 configuration:

```bash
cmake -S runtime/cuda -B build/cuda -DCMAKE_BUILD_TYPE=Release -DCMAKE_CUDA_ARCHITECTURES=75
cmake --build build/cuda -j4
ctest --test-dir build/cuda --output-on-failure
```

CUDA compilation and execution for the imported repository have not been run
on this host. CPU controls do not establish GPU execution correctness.

## Checked model execution

`run.py` requires a format-1 model directory containing `model.json`, all hashed
binary arrays, the workload and compressed 100K reference. The migration does
not include such a package or claim to regenerate it yet. Once a verified model
package exists, use explicit inputs and a fresh label:

```bash
python3 runtime/cuda/run.py --binary build/cuda/nanhu_gpu --data /path/to/model --cycles 1024 --label gpu-smoke
python3 runtime/cuda/run.py --binary build/cuda/nanhu_gpu --data /path/to/model --cycles 100000 --label gpu-100k
```

The wrapper verifies package hashes, compares the complete requested trace and,
for 100K cycles, checks model-specific final reference fields. It fails on a
CUDA error instead of falling back to CPU. Memory-candidate commits and direct
writes are distinct modes; direct writes require a checked conflict plan.
Flat descriptors, CUDA Graph use, thread count and tail threshold remain explicit
runtime settings. Graph/compiler variants also need distinct exported identities.

`cycles_per_second`, device-event throughput and simulation-loop wall throughput
cover different intervals; use the report's stated timing definition.
Historical GPU runs are recorded under `evidence/original/`; those results are
not new measurements of the imported source. See [the runtime notices](NOTICE.md).
