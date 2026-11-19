# RTX 3090 Nanhu 69-layer checked-prefix measurement

## Result

On `server8x3090`, one RTX 3090 executed the published
`plru-C-decoder-mux1h-v1` model with the grouped CUDA runtime. All three
100,000-cycle samples passed online reference checks, complete trace byte
comparison, architectural endpoints and all three model state fingerprints.

| Sample | Evaluation seconds | Evaluation cycles/s | CUDA-event cycles/s | Checked-loop cycles/s |
| --- | ---: | ---: | ---: | ---: |
| 1 | 39.606888 | 2524.81 | 2631.00 | 2281.91 |
| 2 | 39.422344 | 2536.63 | 2641.72 | 2290.53 |
| 3 | 39.542540 | 2528.92 | 2637.81 | 2283.87 |
| Median | **39.542540** | **2528.92** | **2637.81** | **2283.87** |

The full observed evaluation-rate range is 0.4674% of its median. These were
three separate processes, preceded by a checked 1,024-cycle GPU smoke run.
The measurement was performed on November 19, 2026 UTC. Exact sample values
and aggregate resource measurements are in [summary.json](summary.json).

Evaluation time sums non-reset `Engine::step` wall time: dispatch, graph
execution, synchronization and compact observation copy. It excludes loading,
reset, trace formatting/comparison and final fingerprints. CUDA events bracket
the graph, excluding the observation copy and host checks. Checked-loop time
includes 64 reset cycles and per-cycle observation/trace/reference work but
excludes loading, final trace flush and final fingerprints; its numerator is
still 100,000 non-reset cycles. The wrapper's process interval has median
47.176106 seconds and also includes loading and final fingerprints, but not
initial input hashing/reference preparation. These rates are not interchangeable.

## Model and runtime

- Measured repository checkout: `c078d5066ef195bb087c1cbb069abbe5d301417c`, `cuda-op-groups` branch.
- Runtime source: `7d3de7ac6277fa5049240bb4225d404aee47136b`, unchanged in the measured checkout.
- Kernel/layout/arbitration changes: `b9b4fa0`; original-order opcode grouping: `7d3de7a`.
- Binary SHA-256: `40575939369737344e4c2eca5e60abd8c758ef1ea1081074559e18611e9ff170`.
- Model manifest SHA-256: `feac8519b4a0ea0fb5d42364f3959469796b4b75da3d43840a8730b1a5e9e55e`.
- Graph: 69 executable layers, 2,192,220 operations, 483,208 data slots.
- Runtime: flat descriptors, original-order opcode groups, no operand sorting,
  CUDA Graphs, 256-thread blocks, small-layer threshold 128, candidate memory arbitration.
- Executed schedule: 137 kernels/cycle, 2 fused small layers, 1,034 opcode segments,
  2,148,690 fixed-arity grouped nodes.
- Inputs: 64 reset cycles followed by 100,000 observed non-reset cycles;
  pinned 18,984-byte upstream CoreMark image and published 100K reference.

Graph optimizations, cached compiler overlay, RTL revision, licenses and input
hashes are documented in the [ready-to-run package](../../ready-to-run/README.md).
This was not a fresh compilation from the repository's compiler submodule pin.
The server downloaded the model/reference through GitHub Git. Its raw-content
hostname lookup failed, so the same pinned workload blob was fetched through
GitHub Git and its SHA-256 verified. No GitHub Release was used.

## Environment and costs

- One NVIDIA GeForce RTX 3090, 24,576 MiB, selected as physical GPU 0.
  No multi-GPU execution was used.
- AMD EPYC 7302; wrapper and simulation descendants pinned to logical CPU 2.
- Linux 5.15, glibc 2.35, GCC 11.4.0, CMake 3.22.1,
  CUDA/NVCC 12.9.86, driver 595.91.07.
- Release `sm_86`, `-O3 -DNDEBUG`, `-j4`: configure 5.806 s, build 18.865 s
  for the runner and controls. These costs are separate from simulation time.
- Median tensor/model load and runtime-plan setup: 1.195838 s. Original graph
  compilation/export cost was not measured because this is a frozen package.
- Runtime-accounted device arrays: 1,183,383,672 bytes. One-second GPU memory
  samples peaked at 1,401 MiB including driver/context overhead.
- Sampled SM clocks: 1695--1950 MHz, not locked by this experiment. Maximum
  sampled temperature 53 C, power 199.58 W; median sampled GPU utilization 87%.
- Each selected-GPU check was idle before invocation; no competing compute
  process was observed at one-second intervals. This does not prove continuous
  isolation from other users. Host RSS was not measured.

## Validation and reproduction

The source build passed **9/9 CTests** on this card, including `gpu_opcodes`,
`gpu_graph_tail` and `gpu_direct_memory`. Each full-prefix sample retired
171,898 instructions including fusion, with 167,375 commit events, zero
exception cycles and no trap. Every actual 100K trace has 36,458,271 bytes and
SHA-256 `aac4bdad014633cadab8d50497e1555c60e05272eb091e0c3445c32237e6c3ae`.

Actual traces returned from the server were independently decompressed and
compared byte-for-byte with the repository's reference. The local audit also
checked binary/model/source identities, all expected final fields, cycle
counts, timing denominators, window sums, medians, device identity and CTests.
Raw server records remain local; this public report contains only the
necessary measurement summary, not internal paths, hardware identifiers or
process-level telemetry. Actual traces are not duplicated in Git because
all three equal the reference already published in `ready-to-run/`.

For a fresh checkout, follow the ready-to-run build/input instructions. Select
an idle GPU and an allowed CPU, run CTest and the 1,024-cycle smoke, then run
three separate 100K processes with fresh labels, for example from the repo root:

```sh
CUDA_VISIBLE_DEVICES=0 taskset -c 2 python3 runtime/cuda/run.py --binary build/cuda/nanhu_gpu --data build/ready-to-run/data --cycles 100000 --label nanhu-69l-100k-1
CUDA_VISIBLE_DEVICES=0 taskset -c 2 python3 runtime/cuda/run.py --binary build/cuda/nanhu_gpu --data build/ready-to-run/data --cycles 100000 --label nanhu-69l-100k-2
CUDA_VISIBLE_DEVICES=0 taskset -c 2 python3 runtime/cuda/run.py --binary build/cuda/nanhu_gpu --data build/ready-to-run/data --cycles 100000 --label nanhu-69l-100k-3
```

This establishes a single-GPU frozen-model prefix result, not completed
CoreMark, final CRC, a CoreMark score, eight-GPU scaling, fresh compiler-to-RTL
reproduction or an opcode-grouping speedup against an ungrouped build. Those
comparisons were not run here.
