#include "engine.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace nanhu;

// Independent expected values check old-state reads, enable/mask gating, pool
// separation and byte truncation while writes span multiple CUDA blocks.
void exercise(bool cpu, uint32_t count) {
    Model m;
    // reset, one, zero, rotating address, toggled enable, toggled mask, value
    m.initial = {0, 1, 0, 0, 1, 0, 0x11223344556677F0ULL};
    m.size64 = m.size8 = uint64_t(count) * 4;
    auto slot = [&](uint64_t value) {
        m.initial.push_back(value); return uint32_t(m.initial.size() - 1);
    };
    uint32_t outputs = 0;
    auto add = [&](uint32_t code, std::initializer_list<uint32_t> args, uint32_t dest = 0) {
        m.ops.push_back({uint32_t(m.args.size()), code == 28 ? m.memwrites++ : outputs,
                         code == 28 ? 0u : 1u, code});
        m.args.insert(m.args.end(), args);
        if (code != 28) { m.scatter.push_back(dest); ++outputs; }
    };
    std::vector<uint32_t> before, after;
    for (uint32_t i = 0; i < count; ++i) {
        for (uint32_t kind : {0u, 1u}) {
            before.push_back(slot(0));
            add(21, {kind, 4 * i, 3, 3}, before.back());
        }
    }
    const uint32_t mem_begin = m.ops.size();
    const uint32_t enables[] = {1, 2, 0, 4};
    for (uint32_t i = 0; i < count; ++i) {
        // Nonzero kinds share the byte pool; the same numeric address in the
        // word pool is independent. All ranges within each pool are disjoint.
        for (uint32_t kind : {0u, i % 2 ? 2u : 1u})
            add(28, {enables[i % 4], i % 3 ? 1u : 5u, kind, 4 * i, 3, 3, 6});
    }
    m.layers.push_back({0, uint32_t(m.ops.size()), outputs, mem_begin});
    m.max_outputs = outputs;
    // Small read layers observe committed memory, including across fusion.
    for (uint32_t i = 0; i < count; ++i) {
        const uint32_t begin = m.ops.size(); outputs = 0;
        for (uint32_t kind : {0u, 1u}) {
            after.push_back(slot(0));
            add(21, {kind, 4 * i, 3, 3}, after.back());
        }
        const uint32_t end = m.ops.size();
        m.layers.push_back({begin, end, outputs, end});
    }
    const uint32_t begin = m.ops.size(); outputs = 0;
    add(0, {128, 3, 1}, 3); add(15, {4, 1}, 4);
    add(15, {5, 1}, 5); add(0, {128, 6, 1}, 6);
    const uint32_t end = m.ops.size();
    m.layers.push_back({begin, end, outputs, end});
    for (uint32_t i = 0; i < m.initial.size(); ++i) m.observe.push_back(i);
    m.validate();
    auto oracle = make_cpu(m);
    auto tested = cpu ? make_cpu(m) : make_cuda(m);
    std::vector<uint64_t> words(m.size64);
    std::vector<uint8_t> bytes(m.size8);
    for (uint32_t cycle = 0; cycle < 24; ++cycle) {
        const bool reset = cycle < 5;
        const auto reference = oracle->step(reset);
        const auto actual = tested->step(reset);
        if (reference.snapshot != actual.snapshot)
            throw std::runtime_error("CPU/CUDA snapshot differs at cycle " + std::to_string(cycle));
        for (uint32_t i = 0; i < count; ++i) {
            const uint64_t address = 4 * i + (cycle & 3);
            const uint64_t old_word = words[address], old_byte = bytes[address];
            const bool enabled = i % 4 == 0 || (i % 4 == 2 && reset) || (i % 4 == 3 && cycle % 2 == 0);
            const bool masked = i % 3 != 0 || cycle % 2 != 0;
            if (enabled && masked) {
                words[address] = m.initial[6] + cycle;
                bytes[address] = static_cast<uint8_t>(words[address]);
            }
            if (actual.snapshot[before[2 * i]] != old_word || actual.snapshot[before[2 * i + 1]] != old_byte ||
                actual.snapshot[after[2 * i]] != words[address] || actual.snapshot[after[2 * i + 1]] != bytes[address])
                throw std::runtime_error("Independent memory oracle differs at cycle " + std::to_string(cycle));
        }
    }
    const auto expected = oracle->fingerprints(), actual = tested->fingerprints();
    if (expected.data != actual.data || expected.memory64 != actual.memory64 || expected.memory8 != actual.memory8 ||
        actual.memory64 != fingerprint(words) || actual.memory8 != fingerprint(bytes))
        throw std::runtime_error("Direct commit final state differs");
    const auto info = tested->info();
    if (!cpu && (info.memory_candidate_pairs || !info.memory_plan_bytes ||
                 !info.fused_layers || info.backend != "CUDA Graph"))
        throw std::runtime_error("Disjoint writers were not committed directly with the expected fusion");
    std::cout << "PASS: " << count * 2 << " disjoint writers, 24 cycles, independent memory oracle; " << info.backend << '\n';
}

int main(int argc, char** argv) try {
    bool cpu = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--cpu") cpu = true;
        else throw std::runtime_error("Unknown memory_direct_test argument: " + arg);
    }
    exercise(cpu, 7); exercise(cpu, 641);
    return 0;
} catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
