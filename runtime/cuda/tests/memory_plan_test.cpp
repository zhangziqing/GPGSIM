#include "memory_plan.h"
#include <algorithm>
#include <iostream>
#include <random>
#include <stdexcept>

using namespace nanhu;
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
uint32_t add(Model& m, uint32_t kind, uint32_t base, uint32_t mask, uint32_t enable = 1) {
    const uint32_t address = m.initial.size();
    m.initial.insert(m.initial.end(), {0, 1});
    m.ops.push_back({uint32_t(m.args.size()), m.memwrites++, 0, 28});
    m.args.insert(m.args.end(), {enable, 1, kind, base, mask, address, address + 1});
    return m.memwrites - 1;
}
bool edge(const MemoryWritePlan& p, uint32_t a, uint32_t b) {
    return std::find(p.later.begin() + p.offsets[a], p.later.begin() + p.offsets[a + 1], b)
        != p.later.begin() + p.offsets[a + 1];
}
Model initial_model() {
    Model m; m.initial = {0, 1, 0, 0}; m.reset_slot = 0; m.size64 = m.size8 = 128;
    return m;
}
void static_boundaries() {
    Model m = initial_model();
    // Scatter makes slot3 mutable, even when its only writer is in another layer.
    m.ops.push_back({0, 0, 1, 20}); m.args = {1}; m.scatter = {3}; m.max_outputs = 1;
    m.layers.push_back({0, 1, 1, 1});
    add(m, 0, 10, 7);                 //0: inclusive range10..17
    add(m, 0, 17, 3);                 //1: overlaps at17, different base
    add(m, 0, 18, 0);                 //2: no overlap with0
    add(m, 1, 10, 7);                 //3: different pool
    add(m, 2, 10, 7);                 //4: same byte pool as kind1
    add(m, 0, 10, 7, 2);              //5: immutable zero enable
    add(m, 0, 10, 7, 3);              //6: initial zero but scatter-written
    add(m, 0, 10, 7, m.reset_slot);   //7: externally mutable reset
    add(m, 0, UINT32_MAX, 7);          //8: uint64 address interval
    add(m, 0, UINT32_MAX - 1, 7);      //9: uint64 overlap
    add(m, 0, 10, 7); m.args[m.ops.back().arg + 1] = 2; //10: dead mask
    m.layers.push_back({1, uint32_t(m.ops.size()), 0, 1});
    const uint32_t next = m.ops.size(); add(m, 0, 10, 7); //11: different layer
    m.layers.push_back({next, next + 1, 0, next});
    m.validate();
    const auto p = build_memory_write_plan(m), again = build_memory_write_plan(m);
    require(p.offsets == again.offsets && p.later == again.later, "Non-deterministic plan");
    require(p.offsets.size() == m.memwrites + 1 && p.inactive_writers == 2, "Dead writer proof");
    require(p.full_scan_pairs == 55, "Full scan pair accounting");
    require(edge(p, 0, 1) && !edge(p, 0, 2), "Inclusive/different-base interval boundary");
    require(!edge(p, 0, 3) && edge(p, 3, 4), "Backing pool normalization");
    require(!edge(p, 0, 5) && !edge(p, 0, 10) && edge(p, 0, 6) && edge(p, 0, 7), "Mutable zero handling");
    require(edge(p, 8, 9) && !edge(p, 0, 11), "Overflow/layer boundary");
    m = initial_model(); m.ops = {{0, 0, 1, 20}}; m.args = {1};
    m.scatter = {3}; m.max_outputs = 1; m.layers = {{0, 1, 1, 1}}; m.validate();
    const auto empty = build_memory_write_plan(m);
    require(empty.offsets == std::vector<uint32_t>{0} && empty.later.empty(), "Empty plan");
}
void randomized_equivalence() {
    std::mt19937_64 rng(2813);
    for (unsigned trial = 0; trial < 64; ++trial) {
        Model m = initial_model();
        for (uint32_t n : {80u, 31u}) {
            const uint32_t begin = m.ops.size();
            for (uint32_t j = 0; j < n; ++j) {
                const uint32_t w = add(m, rng() % 3, rng() % 64, rng() % 32,
                    j % 17 == 0 ? 2 : j % 19 == 0 ? m.reset_slot : 1);
                if (w % 13 == 0) m.args[m.ops.back().arg + 1] = 2;
            }
            m.layers.push_back({begin, uint32_t(m.ops.size()), 0, begin});
        }
        m.validate(); const auto p = build_memory_write_plan(m);
        auto data = m.initial; std::vector<Write> writes(m.memwrites);
        std::vector<uint64_t> expected64(m.size64), actual64(m.size64);
        std::vector<uint8_t> expected8(m.size8), actual8(m.size8);
        Error error{};
        View v{}; v.ops = m.ops.data(); v.args = m.args.data(); v.data = data.data();
        v.writes = writes.data(); v.error = &error; v.size64 = m.size64; v.size8 = m.size8;
        for (uint32_t cycle = 0; cycle < 32; ++cycle) {
            data[0] = cycle & 1; data[1] = cycle % 5 != 0;
            for (const Op o : m.ops) {
                const auto* a = m.args.data() + o.arg;
                data[a[5]] = rng(); data[a[6]] = cycle % 2 ? 1 : rng();
            }
            for (const Layer l : m.layers) {
                for (uint32_t i = l.begin; i < l.end; ++i) evaluate(v, i);
                require(!error.code, "Unexpected evaluator error");
                v.mem64 = expected64.data(); v.mem8 = expected8.data(); commit_memory(v, l);
                for (uint32_t i = l.begin; i < l.end; ++i) {
                    const uint32_t w = m.ops[i].out;
                    const bool final = is_final_memory_write_candidate(v, w, p.offsets.data(), p.later.data());
                    require(final == is_final_memory_write(v, l, i), "Candidate/full scan decision differs");
                }
                // Reverse application order exposes any unsuppressed aliases,
                // including equal-value writers (whose flags are checked above).
                for (uint32_t i = l.end; i-- > l.begin;) {
                    const uint32_t w = m.ops[i].out;
                    if (!is_final_memory_write_candidate(v, w, p.offsets.data(), p.later.data())) continue;
                    if (writes[w].kind == 0) actual64[writes[w].address] = writes[w].value;
                    else actual8[writes[w].address] = uint8_t(writes[w].value);
                }
                require(actual64 == expected64 && actual8 == expected8, "Ordered memory state differs");
            }
        }
    }
}
int main() try {
    static_boundaries(); randomized_equivalence();
    std::cout << "PASS: static candidate boundaries; 64 models x 32 cycles vs ordered memory commit\n";
} catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
