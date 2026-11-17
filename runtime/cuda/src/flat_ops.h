#pragma once
#include "engine.h"

namespace nanhu {
HD inline unsigned flat_arity(uint32_t code) {
    if (code <= 2 || (code >= 17 && code <= 19) || code == 24) return 3;
    if (code >= 3 && code <= 15) return 2;
    if (code == 16 || code == 20) return 1;
    return 0;
}
struct FlatPlan {
    std::vector<FlatOp> ops;
    uint64_t inlined_nodes = 0;
    double build_seconds = 0;
};
// Requires a validated model. Leaves the serialized model and operand pool intact.
FlatPlan build_flat_plan(const Model& model);

// Scalar fields allow constant-index operand reads to become register values.
// Only opcodes with flat_arity(code) > 0 may use this accessor.
struct InlineOperands {
    uint32_t x, y, z;
    HD uint32_t operator[](uint32_t i) const { return i == 0 ? x : i == 1 ? y : z; }
};
HD inline void evaluate_flat(const View& v, uint32_t node) {
    const FlatOp f = v.flat_ops[node];
    if (flat_arity(f.code)) {
        evaluate_operands<true>(v, node, Op{0, f.out, 1, f.code}, InlineOperands{f.x, f.y, f.z});
    } else {
        evaluate_operands(v, node, Op{f.x, f.out, f.y, f.code}, v.args + f.x);
    }
}
} // namespace nanhu
