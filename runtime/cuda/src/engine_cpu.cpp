#include "engine.h"
#include "flat_ops.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <unordered_set>

namespace nanhu {
void Model::validate() const {
    if (initial.empty() || layers.empty() || ops.empty() || !size64 || !size8 || reset_slot >= initial.size())
        throw std::runtime_error("Invalid model dimensions/reset slot");
    uint64_t nodes = 0, outputs = 0, maxout = 0, nwrites = 0;
    for (const Layer l : layers) {
        if (l.begin != nodes || l.end < l.begin || l.end > ops.size() || l.mem_begin < l.begin || l.mem_begin > l.end)
            throw std::runtime_error("Invalid layer range");
        std::unordered_set<uint32_t> destinations;
        uint32_t cursor = 0;
        for (uint32_t i = l.begin; i < l.end; ++i) {
            const Op o = ops[i];
            if (o.code > 28 || o.arg >= args.size() || (i >= l.mem_begin) != (o.code == 28))
                throw std::runtime_error("Invalid descriptor/opcode order");
            uint64_t span = 0;
            if (o.code <= 2 || (o.code >= 17 && o.code <= 19) || o.code == 24) span = 3;
            else if (o.code >= 3 && o.code <= 15) span = 2;
            else if (o.code == 16 || o.code == 20) span = 1;
            else if (o.code == 21) span = 4;
            else if (o.code == 22 || o.code == 23) span = uint64_t(args[o.arg]) + 1;
            else if (o.code >= 25 && o.code <= 27) span = uint64_t(args[o.arg]) + 4;
            else span = 7;
            if (uint64_t(o.arg) + span > args.size()) throw std::runtime_error("Truncated operands");
            const uint32_t* a = args.data() + o.arg;
            const auto check = [&](uint32_t slot) {
                if (slot >= initial.size()) throw std::runtime_error("Data operand out of range");
            };
            if (o.code <= 2) { check(a[1]); check(a[2]); }
            else if (o.code == 3 || o.code == 16 || o.code == 20) check(a[0]);
            else if (o.code <= 15 || o.code == 17) { check(a[0]); check(a[1]); }
            else if (o.code == 18) { check(a[0]); check(a[2]); }
            else if (o.code == 19) { check(a[0]); check(a[1]); check(a[2]); }
            else if (o.code == 21) check(a[3]);
            else if (o.code <= 23) for (uint32_t j = 1; j <= a[0]; ++j) check(a[j]);
            else if (o.code == 24) {
                check(a[0]);
                if (uint64_t(a[1]) + a[2] >= jump.size()) throw std::runtime_error("Jump table out of range");
            } else if (o.code <= 27) {
                check(a[1]); check(a[2]);
                for (uint32_t j = 0; j < a[0]; ++j) check(a[4 + j]);
            } else { check(a[0]); check(a[1]); check(a[5]); check(a[6]); }
            if (o.code == 28) {
                if (o.words || o.out != nwrites++) throw std::runtime_error("Memory write staging layout");
            } else {
                const uint64_t words = o.code >= 25 && o.code <= 27 ? (uint64_t(a[3]) + 63) / 64 : 1;
                if (!words || o.words != words || o.out != cursor) throw std::runtime_error("Output layout mismatch");
                cursor += o.words;
            }
        }
        if (cursor != l.outputs || outputs + l.outputs > scatter.size()) throw std::runtime_error("Scatter size mismatch");
        for (uint32_t j = 0; j < l.outputs; ++j) {
            const auto slot = scatter[outputs + j];
            if (slot >= initial.size() || !destinations.insert(slot).second)
                throw std::runtime_error("Overlapping/out-of-range scatter destinations");
        }
        outputs += l.outputs; nodes = l.end; maxout = std::max<uint64_t>(maxout, l.outputs);
    }
    if (nodes != ops.size() || outputs != scatter.size() || maxout != max_outputs || nwrites != memwrites)
        throw std::runtime_error("Model counts disagree");
    for (auto slot : observe) if (slot >= initial.size()) throw std::runtime_error("Observation slot out of range");
    for (auto slot : jump) if (slot >= initial.size()) throw std::runtime_error("Jump slot out of range");
    for (auto base : ram_bases) if (base > size64 || (program.size() + 7) / 8 > size64 - base)
        throw std::runtime_error("Program outside modeled RAM");
}

class CPU final : public Engine {
    const Model& model;
    FlatPlan flat;
    std::vector<uint64_t> data, out, mem64, snapshot;
    std::vector<uint8_t> mem8;
    std::vector<Write> writes;
    Error error{};
public:
    explicit CPU(const Model& m) : model(m), flat(build_flat_plan(m)), data(m.initial), out(m.max_outputs),
        mem64(m.size64), snapshot(m.observe.size()), mem8(m.size8), writes(m.memwrites) {
        for (auto base : m.ram_bases) std::memcpy(mem64.data() + base, m.program.data(), m.program.size());
    }
    Step step(bool reset) override {
        data[model.reset_slot] = reset;
        View v{nullptr, model.args.data(), model.scatter.data(), model.jump.data(),
            data.data(), out.data(), mem64.data(), mem8.data(), writes.data(), &error, model.size64, model.size8};
        v.flat_ops = flat.ops.data();
        size_t offset = 0;
        for (auto l : model.layers) {
            for (uint32_t i = l.begin; i < l.end; ++i) evaluate_flat(v, i);
            if (error.code) throw std::runtime_error("CPU evaluator error " + std::to_string(error.code) + " node " + std::to_string(error.node));
            commit_memory<true>(v, l);
            for (uint32_t j = 0; j < l.outputs; ++j) data[model.scatter[offset + j]] = out[j];
            offset += l.outputs;
        }
        for (size_t i = 0; i < model.observe.size(); ++i) snapshot[i] = data[model.observe[i]];
        return {snapshot, 0};
    }
    Fingerprints fingerprints() override { return {fingerprint(data), fingerprint(mem64), fingerprint(mem8)}; }
    Info info() const override {
        Info info{"CPU flat evaluator (validation)", "host"};
        info.flat_inlined_nodes = flat.inlined_nodes;
        info.descriptor_bytes = model.ops.size() * sizeof(FlatOp);
        info.flat_plan_seconds = flat.build_seconds;
        return info;
    }
};
std::unique_ptr<Engine> make_cpu(const Model& model) {
    return std::make_unique<CPU>(model);
}
#ifndef NANHU_HAS_CUDA
std::unique_ptr<Engine> make_cuda(const Model&, int) {
    throw std::runtime_error("CUDA backend not built; configure with -DNANHU_ENABLE_CUDA=ON and nvcc, or explicitly use --cpu for validation");
}
#endif
} // namespace nanhu
