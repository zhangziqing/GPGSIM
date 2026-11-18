#pragma once
#include "engine.h"

namespace nanhu {
// CSR rows use global Write indices; entries retain later writers in opcode order.
struct MemoryWritePlan {
    std::vector<uint32_t> offsets, later;
    uint32_t inactive_writers = 0;
    uint64_t full_scan_pairs = 0;
    double build_seconds = 0;
};
// Requires a validated Model. Only reset_slot is externally mutable at runtime.
MemoryWritePlan build_memory_write_plan(const Model& model);

// Empty candidate rows store directly. Potential aliases retain last-writer
// priority, independent of CUDA thread order. Reads use the staged layer state.
HD inline void commit_memory_parallel(const View& v, Layer l, uint64_t thread, uint64_t stride) {
#ifdef __CUDACC__
#pragma unroll 1
#endif
    for (uint64_t i = uint64_t(l.mem_begin) + thread; i < l.end; i += stride) {
        const uint32_t writer = v.flat_ops[uint32_t(i)].out;
        if (!is_final_memory_write_candidate(v, writer, v.memory_offsets, v.memory_later)) continue;
        const Write w = v.writes[writer];
        if (w.kind == 0) v.mem64[w.address] = w.value;
        else v.mem8[w.address] = static_cast<uint8_t>(w.value);
    }
}
} // namespace nanhu
