#include "op_groups.h"
#include <chrono>
#include <stdexcept>

namespace nanhu {
OpGroupPlan build_op_groups(const Model& model, uint32_t threads) {
    if (!threads) throw std::runtime_error("op groups require a nonzero block size");
    const auto start = std::chrono::steady_clock::now();
    OpGroupPlan plan;
    for (const Layer layer : model.layers) {
        const uint32_t work_begin = uint32_t(plan.work.size());
        const uint32_t seg_begin = uint32_t(plan.segs.size());
        for (uint32_t i = layer.begin; i < layer.end;) {
            const uint32_t code = model.ops[i].code;
            uint32_t end = i + 1;
            while (end < layer.end && model.ops[end].code == code) ++end;
            const uint32_t stride = op_group_stride(code);
            GroupSeg segment{code, i, end - i, uint32_t(plan.vals.size()), model.ops[i].out};
            if (stride) {
                for (uint32_t node = i; node < end; ++node) {
                    const auto begin = model.args.begin() + model.ops[node].arg;
                    plan.vals.insert(plan.vals.end(), begin, begin + stride);
                }
                plan.nodes += end - i;
            }
            const uint32_t local = uint32_t(plan.segs.size()) - seg_begin;
            plan.segs.push_back(segment);
            for (uint64_t offset = 0; offset < segment.count; offset += threads)
                plan.work.push_back({local, uint32_t(offset)});
            i = end;
        }
        plan.layer_work_begin.push_back(work_begin);
        plan.layer_work_count.push_back(uint32_t(plan.work.size()) - work_begin);
        plan.layer_seg_begin.push_back(seg_begin);
        plan.layer_seg_count.push_back(uint32_t(plan.segs.size()) - seg_begin);
    }
    plan.build_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    return plan;
}
} // namespace nanhu
