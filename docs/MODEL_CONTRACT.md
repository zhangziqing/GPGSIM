# Nanhu simulation and measurement contract

The source design is single-core Nanhu `DefaultConfig` at
`0a68ebc13b3d96a78d8663679393ecbea5b8fb73`, with real L1/L2/L3. Original
module/instance hierarchy and source dependencies belong to XiangShan. FIRRTL
lowering and tensor graph transformations belong to RTeAAL-Sim. Tensor arrays,
CUDA descriptors and host buffers are execution representations; they do not
replace those source identities.

## Execution boundary

The Nanhu-specific compiler mode implements the selected simulation harness,
including modeled clock enables, RAM and flash and explicitly inactive JTAG.
General asynchronous or multiple-clock RTL is outside the acceptance boundary.
Compilation alone is not proof of full-device simulation correctness.

The hosts initialize 256 MiB RAM with the specified workload, model synchronous
flash, keep JTAG idle and reject SD requests and out-of-range RAM accesses.
Runs perform 64 reset cycles before the requested non-reset prefix. Operand
reads and combinational evaluation use the appropriate layer's state; state,
scatter and ordered memory updates retain the existing runtime rules. CUDA
direct memory writes additionally require their conflict-checked memory plan.
No new schedule or memory semantics are introduced by the repository import.

The source adapters retain width, signedness, truncation and reset semantics
through the compiler's operation encoding. The CPU source wrapper widens only
the NU evaluator counters from 16 to 32 bits to accommodate large layers;
its independent OU controls cover all 29 opcodes, wide shifts, memories and
counter boundaries. RU has a separate large-layer calling-convention control.

## Observations

`configs/preserved-signals.txt` and its interface metadata identify observed
ports and host devices. The CoreMark observer records commits, GPRs, FPRs,
CSRs, exceptions, trap and UART every non-reset cycle. The Verilator host
samples UART after clock-low evaluation for the upcoming rising edge, matching
the DPI observations. `configs/csr-schema.json` must match generated declarations.

The RV64I checked loop has a small independent ISA oracle. CoreMark uses a
cross-simulator trace and architectural-state comparison; it does not have
an independent ISA oracle in this artifact. A 100K prefix does not complete
CoreMark or validate its final CRC/score.

Whole-data fingerprints depend on slot allocation. Each graph layout needs
its own independent CPU reference; do not compare fingerprints across layouts.
Original trace and workload hashes are in `sources.lock.json`.

## Measurement boundary

Record compiler source, runtime source, workload, model hashes, all graph passes,
CPU/GPU hardware, toolchain, affinity, page policy, flags, cycle count, timing
scope and sample order. Compilation, setup and execution cost are separate.
CPU evaluation, CUDA device events, checked simulation-loop wall time and
whole-process wall time have different scopes and cannot be substituted.

The measured THP hosts require and record their original local huge-page policy;
a failed coverage requirement is not silently accepted. Historical reports
retain their original host settings and statistical limits. No new throughput
number is claimed by the import or the short migration smoke check.
