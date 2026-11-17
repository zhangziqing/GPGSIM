# Sources and provenance

- RTeAAL: https://github.com/TAC-UCB/RTeAAL-Sim . The CUDA backend implements the 29 operation encodings used by the existing Nanhu tensors. `tests/reference_ou.h` contains the unchanged `op_ss_3` function from the local upstream `kernel/kernel_OU.cc`, with only its include/macro wrapper adapted for a standalone test. The full original file SHA-256 is `eacc28eea85d7a67f09bee0ca976a7eacc9b18ee1513a51ab5b5570dee2f84a8`.
- XiangShan Nanhu: https://github.com/OpenXiangShan/XiangShan , commit `0a68ebc13b3d96a78d8663679393ecbea5b8fb73`. The exported data retains the existing single-core DefaultConfig experiment's clock/peripheral adaptations. See `licenses/XiangShan-MulanPSL2.txt` for the original XiangShan license and copyright notice.
- `data/program.bin` is the existing `ready-to-run/coremark-2-iteration.bin`, unchanged. SHA-256: `1156255cce0d01cffbf7232fa4a168a00defc41e0c8ea6096a3186ca30a48519`.
- `data/reference-100k.trace.gz` contains the existing Verilator 100K-cycle trace. Uncompressed SHA-256: `aac4bdad014633cadab8d50497e1555c60e05272eb091e0c3445c32237e6c3ae`.
- Tensor source manifest SHA-256: `b43b5d7d049d018becdd7c3df0eed2053ae97c8a56b2bc6890766a2a9ffb2615`. Export compares every grouped argument and scatter slot, verifies no duplicate scatter destinations within a layer, and checks source hashes before and after export.
- `src/coremark_common.h` is copied from the existing local CoreMark experiment. Its cycle trace schema is shared with the prior CPU and Verilator comparison.
- `vendor/json.hpp`: JSON for Modern C++ 3.12.0 by Niels Lohmann, https://github.com/nlohmann/json . Original embedded notices are preserved; MIT license text is in `licenses/nlohmann-json-MIT.txt`.

The portable archive contains exported assets, implementation and checks. It excludes the temporary CUDA toolkit, machine-specific binaries, original XiangShan source tree, and local run traces. The workspace-only `export.py` regenerates the assets using the original workspace; the destination computer does not need it.
