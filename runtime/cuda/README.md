# GPGSIM CUDA runtime: Nanhu integration

The CUDA runtime belongs to the GPGSIM simulation framework. Its current host,
model package and observation interfaces target Nanhu; broader design support
requires separate integration and validation. See the
[framework architecture](../../docs/ARCHITECTURE.md).

This directory contains the Nanhu CUDA backend, its independent CPU interpreter,
CMake controls and dependency notices. The selected frozen Nanhu model and
reference trace are available in [`ready-to-run/`](../../ready-to-run/README.md).

## Build and semantic controls

From the artifact root, without a GPU:

```bash
cmake -S runtime/cuda -B build/cuda-cpu -DNANHU_ENABLE_CUDA=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build/cuda-cpu -j4
ctest --test-dir build/cuda-cpu --output-on-failure
```

The CPU tests cover scheduling, opcodes, memory semantics, candidate
arbitration, flat descriptors and opcode grouping. CUDA builds require a
supported toolkit and the target architecture, for example `86` for an RTX 3090
or `75` for an RTX 2060:

```bash
cmake -S runtime/cuda -B build/cuda -DCMAKE_BUILD_TYPE=Release -DCMAKE_CUDA_ARCHITECTURES=86
cmake --build build/cuda -j4
ctest --test-dir build/cuda --output-on-failure
```

CPU controls do not establish GPU execution correctness. Run the CUDA controls
on the target device before checking model execution.

## Checked model execution

`run.py` requires a format-1 model directory containing `model.json`, all hashed
binary arrays, the workload and compressed 100K reference. Prepare the frozen
69-layer model using the ready-to-run instructions, then use explicit inputs
and a fresh label:

```bash
python3 runtime/cuda/run.py --binary build/cuda/nanhu_gpu --data /path/to/model --cycles 1024 --label gpu-smoke
python3 runtime/cuda/run.py --binary build/cuda/nanhu_gpu --data /path/to/model --cycles 100000 --label gpu-100k
```

The wrapper verifies package hashes, compares the complete requested trace and,
for 100K cycles, checks model-specific final reference fields. It fails on a
CUDA error instead of falling back to CPU. The CUDA runtime always uses flat
descriptors, CUDA Graphs, 256 threads and a small-layer threshold of 128. Large
layers use opcode groups that preserve node and operand order; fused small
layers use flat evaluation. Memory commits use a static arbitration plan to
preserve last-writer priority. Use `--device` to select the CUDA device or `--cpu`
for validation. Graph/compiler variants need distinct exported identities.

`cycles_per_second`, device-event throughput and simulation-loop wall throughput
cover different intervals; use the report's stated timing definition.
Historical GPU runs are recorded under `evidence/original/`; those results are
not new measurements of the imported source. See [the runtime notices](NOTICE.md).
