# Projection-only measurement with flat descriptors

The corrected normalized-v5 projection model improves measured throughput by
**7.350%** over its projection-disabled control in this comparison. Each variant
has one independent 100K timed sample, run in A then B order. Update composition
is disabled in both. These results supersede the pending status of this specific
replay, while the repaired original-runtime A/B/C matrix remains unmeasured.

| Metric | A: projection OFF | B: projection ON |
| --- | ---: | ---: |
| Cycles/s | 3,191.975 | 3,426.573 |
| Evaluation seconds / 100K | 31.328566 | 29.183680 |
| CUDA device seconds / 100K | 29.981983 | 27.604066 |
| Full graph layers | 126 | 94 |
| Nodes | 2,222,558 | 2,221,744 |
| Kernels/cycle | 141 | 129 |
| Load/setup seconds | 0.533288 | 0.554373 |
| Flat conversion seconds, within setup | 0.026739 | 0.026680 |
| Allocated device bytes | 1,165,265,660 | 1,165,161,436 |

Both use the same CUDA binary, flat descriptors, direct memory commit, parallel
commit-loop `#pragma unroll 1`, CUDA Graph, 128 threads, threshold 256, CPU 2 and
device 0. GPU: RTX PRO 6000 Blackwell, UUID
`GPU-67983fd2-eb55-ece7-a1ec-40165d7759fb`; driver 580.178.04, CUDA 13.2.78,
`sm_120`, empty `CMAKE_BUILD_TYPE`, `-lineinfo --ptxas-options=-v`.

The runtime source matches workspace commit `9c84c90` byte for byte. Its engine
SHA256 is `6c39fad7ed9b9311aed901e4ecc5b94980bec748f82d75a68e217935a03b80ad`.
Runtime configuration/build take 0.995458/2.182763 seconds, recorded separately
from execution. The compute, commit and fused-layer kernels use 40 registers,
zero stack and zero spills in this build. Evaluation includes non-reset
dispatch, synchronization and compact observation copy. CUDA device time is
nested within evaluation; neither setup nor trace formatting is added to it.

The independent local audit passes all 55 original input manifest entries and
205 returned evidence files. CTest passes 30/30, both 1024-cycle GPU smoke checks
pass, and separate candidate-mode reset-plus-100K alias diagnostics pass for
both variants. A known-conflict fixture triggers the expected error 7/exit 1.
Those diagnostics justify direct commit for these timed prefixes; fused small
layers retain ordered commits. The diagnostic binary differs only by the alias
failure hook and is excluded from timing.

Both timed runs match the full 100K reference trace and their own independent
CPU fingerprints. The memory64/memory8 fingerprints agree across variants;
full-data fingerprints differ because the layouts differ. Both retire 171,898
instructions including fused instructions, with 167,375 commit events and the
expected UART prefix. No full CoreMark completion or final CRC is established.

Raw monitoring confirms at least ten consecutive observations about one second
apart with zero utilization and no compute PID before every attempt. Timed A/B
have 35/33 running observations, with no external compute PID detected. This
sampling cannot exclude activity between observations. Single samples provide
a descriptive result, with no statistically established ranking. Reduced graph
depth and kernel count are measured structural changes; this run does not
attribute the speedup to individual kernels.

[Results](projection-only-flat-results.json),
[independent checks](projection-only-flat-checks.json), and
[preparation/provenance](projection-only-flat-preparation.json) retain exact
inputs, commands, timings, hashes and limitations. Replay the local audit with
`python3 results/experimental/plru-projection/audit_projection_only_flat.py`.
The 765,076-byte evidence archive is retained under
`runtime/projection-only-flat-20261115T084928Z/`, SHA256
`147292d42b887f6143b3888a338136106a0f4ceea2f98dccf1603e726b095be2`.
Large GPU traces and executable binaries remain on corvus01; the local audit
checks their receipts and the retained source that rehashes and verifies them
before accepting each run. The original compiler evidence is rehashed rather
than rerun; no Scala tests are run. No push is performed.
