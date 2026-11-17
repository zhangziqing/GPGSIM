#include "memory_plan.h"
#include <chrono>
#include <limits>
#include <stdexcept>

namespace nanhu {
MemoryWritePlan build_memory_write_plan(const Model& m) {
    const auto start = std::chrono::steady_clock::now();
    MemoryWritePlan plan;
    plan.offsets.reserve(size_t(m.memwrites) + 1);
    std::vector<bool> mutable_slot(m.initial.size());
    for (auto slot : m.scatter) mutable_slot[slot] = true;
    mutable_slot[m.reset_slot] = true;
    const auto zero = [&](uint32_t slot) { return !mutable_slot[slot] && m.initial[slot] == 0; };
    for (const Layer l : m.layers) {
        const uint64_t count = l.end - l.mem_begin;
        plan.full_scan_pairs += count * (count ? count - 1 : 0) / 2;
        for (uint32_t i = l.mem_begin; i < l.end; ++i) {
            plan.offsets.push_back(uint32_t(plan.later.size()));
            const uint32_t* a = m.args.data() + m.ops[i].arg;
            if (zero(a[0]) || zero(a[1])) { ++plan.inactive_writers; continue; }
            // Masked addresses are contained in this inclusive uint64 interval,
            // including non-contiguous masks and base+mask beyond UINT32_MAX.
            const uint64_t low = a[3], high = low + a[4];
            for (uint32_t j = i + 1; j < l.end; ++j) {
                const uint32_t* b = m.args.data() + m.ops[j].arg;
                if (zero(b[0]) || zero(b[1]) || ((a[2] == 0) != (b[2] == 0))) continue;
                if (low > uint64_t(b[3]) + b[4] || uint64_t(b[3]) > high) continue;
                if (plan.later.size() == std::numeric_limits<uint32_t>::max())
                    throw std::runtime_error("Memory candidate plan exceeds uint32 CSR capacity");
                plan.later.push_back(m.ops[j].out);
            }
        }
    }
    plan.offsets.push_back(uint32_t(plan.later.size()));
    plan.build_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    return plan;
}
} // namespace nanhu
