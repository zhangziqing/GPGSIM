# Ready-to-run Nanhu model

These files supply the frozen Nanhu model and independent reference trace used
for checked CoreMark prefix execution. They are ordinary Git files: clone the
`cuda-op-groups` branch of GPGSIM to download them. No Release is required.

| Property | Value |
| --- | --- |
| Model | `plru-C-decoder-mux1h-v1`, tensor format 1, little endian |
| Executable layers | **69** |
| Executable operations | **2,192,220** |
| Data slots | 483,208 |
| Validation interval | 64 reset cycles, then 100,000 non-reset cycles |
| Model manifest SHA-256 | `feac8519b4a0ea0fb5d42364f3959469796b4b75da3d43840a8730b1a5e9e55e` |
| Runtime implementation | `7d3de7ac6277fa5049240bb4225d404aee47136b` |

## Optimizations and provenance

The graph uses FTQ mux-index normalization, constant-mux reduction,
priority-mux balancing, binary concat balancing (opcode 17), PLRU slice
projection, masked-update composition and decoder/Mux1H table-read folding.
The optional long-logic chain, predicate and Boolean-table passes are disabled
in this model. The 69-layer count describes the exported executable graph;
it is not the number of CUDA kernel executions in a cycle.

The runtime uses flat descriptors, opcode grouping in original node and operand
order, CUDA Graphs, small-layer fusion, 256-thread blocks, threshold 128 and
candidate memory arbitration. Operand sorting is disabled. These runtime
choices do not change the graph's layer count.

This is a cached graph based on compiler revision
`457429623ba2f5df35c493a03f3d311dc2df6299` with a local compiler overlay,
not a new model generated from this repository's compiler submodule pin.
Compiler provenance SHA-256:
`79283f09890e7205ca53c9350e811a734888f774d7917488f0dff1950ab7940d`.
The model derives from XiangShan Nanhu revision
`0a68ebc13b3d96a78d8663679393ecbea5b8fb73`; the model archive includes its
Mulan PSL v2 notice. The runtime's original source notices remain in
[`runtime/cuda/NOTICE.md`](../runtime/cuda/NOTICE.md).

## Prepare the inputs

From the repository root, extract into a fresh directory:

```sh
(cd ready-to-run && sha256sum -c SHA256SUMS)
mkdir -p build/ready-to-run
tar -xzf ready-to-run/nanhu-plru-c-decoder-mux1h-69l-feac8519.tar.gz -C build/ready-to-run
cp ready-to-run/nanhu-coremark-100k-reference-aac4bdad.trace.gz build/ready-to-run/data/reference-100k.trace.gz
curl -fL 'https://raw.githubusercontent.com/OpenXiangShan/ready-to-run/55df55381ce26dab14211e0fed4165661035ec54/coremark-2-iteration.bin' -o build/ready-to-run/data/program.bin
```

The workload is the upstream 18,984-byte CoreMark image with SHA-256
`1156255cce0d01cffbf7232fa4a168a00defc41e0c8ea6096a3186ca30a48519`.
`runtime/cuda/run.py` verifies all model input sizes and SHA-256 values before
starting execution, including the workload downloaded above.

## Build and run on an RTX 3090

```sh
cmake -S runtime/cuda -B build/cuda -DNANHU_ENABLE_CUDA=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_CUDA_ARCHITECTURES=86
cmake --build build/cuda -j4
CUDA_VISIBLE_DEVICES=0 ctest --test-dir build/cuda --output-on-failure
CUDA_VISIBLE_DEVICES=0 taskset -c 2 python3 runtime/cuda/run.py --binary build/cuda/nanhu_gpu --data build/ready-to-run/data --cycles 1024 --label nanhu-69l-smoke
CUDA_VISIBLE_DEVICES=0 taskset -c 2 python3 runtime/cuda/run.py --binary build/cuda/nanhu_gpu --data build/ready-to-run/data --cycles 100000 --label nanhu-69l-100k-1
```

Select an idle GPU, an allowed CPU and fresh labels for subsequent runs.
Keep at least three complete timing samples and report their median.
`cycles_per_second` divides the non-reset cycle count by engine evaluation
wall time. CUDA-event and whole checked-loop rates use different intervals
and must be reported separately.

The separate compressed reference asset contains all 100,000 expected rows;
its decompressed SHA-256 is
`aac4bdad014633cadab8d50497e1555c60e05272eb091e0c3445c32237e6c3ae`.
Every requested row is checked online and byte-for-byte after execution.
Full-prefix runs also compare architectural endpoints and all three model
state fingerprints. This prefix does not establish completed CoreMark,
its final CRC or a CoreMark score.
