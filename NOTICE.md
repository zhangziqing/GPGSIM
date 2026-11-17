# Source notices and license boundaries

- Compiler and general kernel implementation are referenced through the
  `RTeAAL-Sim` submodule. Its original project identity, authorship and citation
  remain in that repository. The Nanhu NU wrapper changes the source function's
  16-bit counters to 32-bit counters; the source hash is checked before generation.
- The XiangShan submodule references unchanged Nanhu processor RTL at the exact
  commit in `sources.lock.json`. Its own Mulan PSL v2 and dependency notices apply.
  The runtime's copy of that license is in
  `runtime/cuda/licenses/XiangShan-MulanPSL2.txt`.
- `runtime/cuda/vendor/json.hpp` is nlohmann JSON 3.12.0. Its embedded notices
  and `runtime/cuda/licenses/nlohmann-json-MIT.txt` are preserved.
- `runtime/cuda/tests/reference_ou.h` adapts the pinned upstream OU evaluator
  for a standalone independent semantic check. Runtime provenance is in
  `runtime/cuda/NOTICE.md`.
- Per-file source identities and path adaptations are recorded in
  `provenance/imports.json`. Original source comments and existing copyright
  notices are retained. This file does not relicense those components or assign
  a new blanket license to the research artifact.
- CoreMark workload binaries, generated RTL, model packages and reference traces
  are not source files in this Git repository. Their source identities, notices
  and permitted distribution must accompany any future release assets.
