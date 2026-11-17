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
} // namespace nanhu
