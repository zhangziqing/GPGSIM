#include "flat_ops.h"
#include <chrono>

namespace nanhu {
FlatPlan build_flat_plan(const Model& model) {
    const auto start = std::chrono::steady_clock::now();
    FlatPlan plan;
    plan.ops.reserve(model.ops.size());
    for (const Op o : model.ops) {
        FlatOp f{o.out, o.code, o.arg, o.words, 0};
        if (const unsigned n = flat_arity(o.code)) {
            const uint32_t* a = model.args.data() + o.arg;
            f.x = a[0]; f.y = n >= 2 ? a[1] : 0; f.z = n >= 3 ? a[2] : 0;
            ++plan.inlined_nodes;
        }
        plan.ops.push_back(f);
    }
    plan.build_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    return plan;
}
} // namespace nanhu
