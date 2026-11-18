#include "op_groups.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>

using namespace nanhu;
void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
struct Fixture {
    Model model;
    uint32_t outputs = 0;
    void add(uint32_t code, uint32_t count) {
        for (uint32_t k = 0; k < count; ++k) {
            const uint32_t x = 1 + (count - k) % 31, y = 1 + k % 31;
            std::vector<uint32_t> args;
            args.reserve(7);
            if (code <= 2) args = {code == 2 ? 2u : 129u, x, y};
            else if (code == 3) args = {x, 7};
            else if (code <= 15) args = {x, y};
            else if (code == 16 || code == 20) args = {x};
            else if (code == 17) args = {x, y, k % 64};
            else if (code == 18) args = {x, k % 64, y};
            else if (code == 19) args = {x, y, 1};
            else if (code == 21) args = {0, 0, 63, x};
            else if (code == 22 || code == 23) args = {2, x, y};
            else if (code == 24) args = {x, 3, 0};
            else if (code <= 27) args = {1, x, y, 64, 1};
            else args = {1, 1, 0, 0, 63, x, y};
            model.ops.push_back({uint32_t(model.args.size()), code == 28 ? model.memwrites++ : outputs,
                                 code == 28 ? 0u : 1u, code});
            model.args.insert(model.args.end(), args.begin(), args.end());
            if (code != 28) {
                model.scatter.push_back(uint32_t(model.initial.size()));
                model.initial.push_back(0);
                ++outputs;
            }
        }
    }
    void layer(uint32_t begin, uint32_t memory_begin) {
        model.layers.push_back({begin, uint32_t(model.ops.size()), outputs, memory_begin});
        model.max_outputs = std::max(model.max_outputs, outputs);
        outputs = 0;
    }
    Fixture() {
        model.initial.resize(64);
        for (uint32_t i = 0; i < 64; ++i) model.initial[i] = i;
        model.size64 = model.size8 = 128;
        model.jump = {1, 2, 3, 4};
        add(13, 257); add(14, 255); add(13, 1); add(20, 256);
        for (uint32_t code = 0; code <= 20; ++code) add(code, 3);
        add(24, 257);
        layer(0, uint32_t(model.ops.size()));
        const uint32_t begin = model.ops.size();
        for (uint32_t code : {21u, 22u, 23u, 25u, 26u, 27u}) add(code, 2);
        const uint32_t memory_begin = model.ops.size();
        add(28, 2); layer(begin, memory_begin);
        layer(uint32_t(model.ops.size()), uint32_t(model.ops.size()));
        model.validate();
    }
};
int main() try {
    Fixture fixture;
    const Model& m = fixture.model;
    const auto saved_ops = m.ops;
    const auto saved_args = m.args;
    const OpGroupPlan plan = build_op_groups(m, 256);
    uint64_t nodes = 0, work = 0;
    for (uint32_t layer = 0; layer < m.layers.size(); ++layer) {
        uint32_t cursor = m.layers[layer].begin;
        for (uint32_t s = 0; s < plan.layer_seg_count.at(layer); ++s) {
            const GroupSeg seg = plan.segs.at(plan.layer_seg_begin.at(layer) + s);
            require(seg.begin == cursor, "group coverage has a gap");
            cursor += seg.count;
            const uint32_t stride = op_group_stride(seg.code);
            std::vector<uint32_t> offsets;
            for (uint32_t w = 0; w < plan.layer_work_count.at(layer); ++w) {
                const BlockWork item = plan.work.at(plan.layer_work_begin.at(layer) + w);
                if (item.seg == s) offsets.push_back(item.offset);
            }
            require(offsets.size() == (seg.count + 255) / 256, "block coverage differs");
            for (uint32_t k = 0; k < offsets.size(); ++k) require(offsets[k] == k * 256, "block slice differs");
            work += offsets.size();
            if (!stride) continue;
            nodes += seg.count;
            for (uint32_t j = 0; j < seg.count; ++j) {
                const uint32_t node = seg.begin + j;
                const Op op = m.ops[node];
                require(op.code == seg.code && op.out == seg.first_out + j, "original node/output order differs");
                const uint32_t* a = m.args.data() + op.arg;
                for (uint32_t q = 0; q < stride; ++q)
                    require(plan.vals.at(seg.data_off + j * stride + q) == a[q],
                            "original operand order differs");
            }
        }
        require(cursor == m.layers[layer].end, "group coverage differs at layer end");
    }
    require(nodes == plan.nodes && work == plan.work.size(), "plan totals differ");
    require(std::memcmp(saved_ops.data(), m.ops.data(), saved_ops.size() * sizeof(Op)) == 0 && saved_args == m.args,
            "plan changed the serialized model");
    bool threw = false;
    try { build_op_groups(m, 0); } catch (const std::runtime_error&) { threw = true; }
    require(threw, "zero block size accepted");
    std::cout << "PASS: grouped tuples in original order, original node IDs, 255/256/257 boundaries, generic and empty layers\n";
    return 0;
} catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
