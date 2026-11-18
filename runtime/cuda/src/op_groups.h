#pragma once
#include "engine.h"
#include <vector>

namespace nanhu {
// A contiguous opcode run. Node and operand order match the validated model;
// scalar output slots are sequential from first_out.
struct GroupSeg {
    uint32_t code, begin, count, data_off, first_out;
};
struct BlockWork { uint32_t seg, offset; };
static_assert(sizeof(GroupSeg) == 20 && sizeof(BlockWork) == 8, "group plan layout");
struct OpGroupPlan {
    std::vector<GroupSeg> segs;
    std::vector<BlockWork> work;
    std::vector<uint32_t> vals;
    std::vector<uint32_t> layer_work_begin, layer_work_count;
    std::vector<uint32_t> layer_seg_begin, layer_seg_count;
    uint64_t nodes = 0;
    double build_seconds = 0;
    uint64_t device_bytes() const {
        return segs.size() * sizeof(GroupSeg) + work.size() * sizeof(BlockWork) +
            vals.size() * sizeof(uint32_t);
    }
};
HD constexpr uint32_t op_group_stride(uint32_t code) {
    return code <= 2 || code == 17 || code == 18 || code == 19 || code == 24 ? 3u :
        code >= 3 && code <= 15 ? 2u : code == 16 || code == 20 ? 1u : 0u;
}
// Requires a validated model. All nodes and operand tuples retain their original
// order. threads matches the CUDA block.
OpGroupPlan build_op_groups(const Model& model, uint32_t threads);
} // namespace nanhu
