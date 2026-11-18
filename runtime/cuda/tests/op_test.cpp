#include "engine.h"
#include "reference_ou.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <random>
#include <stdexcept>

using namespace nanhu;

// Independent arithmetic oracle: the original OU implementation, not evaluate().
// Its undefined cases are excluded here and checked separately below.
struct Fixture {
    Model m;
    std::mt19937_64 rng{2060};
    uint32_t outputs = 0;
    uint32_t slot(uint64_t value) {
        m.initial.push_back(value);
        return uint32_t(m.initial.size() - 1);
    }
    uint64_t value(unsigned i) {
        static constexpr uint64_t edge[] = {0, 1, UINT64_MAX, 1ULL << 63,
            (1ULL << 63) - 1, 0xAAAAAAAAAAAAAAAAULL, 0x5555555555555555ULL, 63, 64, 65};
        return i < 20 ? edge[i % 10] : rng();
    }
    void add(uint32_t code, const std::vector<uint32_t>& a, uint32_t words = 1) {
        m.ops.push_back({uint32_t(m.args.size()), code == 28 ? m.memwrites++ : outputs,
                         code == 28 ? 0u : words, code});
        m.args.insert(m.args.end(), a.begin(), a.end());
        if (code != 28) for (uint32_t w = 0; w < words; ++w) {
            m.scatter.push_back(slot(0));
            ++outputs;
        }
    }
    Fixture() {
        m.initial = {0}; m.reset_slot = 0; m.size64 = 512; m.size8 = 512;
        m.ram_bases = {0};
        m.program.resize(m.size64 * sizeof(uint64_t));
        for (size_t i = 0; i < m.size64; ++i) {
            const uint64_t v = rng();
            std::memcpy(m.program.data() + i * sizeof(v), &v, sizeof(v));
        }
        uint32_t mem_begin = 0;
        for (uint32_t code = 0; code <= 28; ++code) {
            if (code == 28) mem_begin = m.ops.size();
            for (uint32_t i = 0; i < 512; ++i) {
                uint64_t x = value(i), y = value((i + 3) % 512);
                std::vector<uint32_t> a;
                uint32_t words = 1;
                if (code <= 1) {
                    static constexpr uint32_t widths[] = {1, 2, 7, 8, 16, 31, 32, 63, 64};
                    a = {2 * widths[i % 9] + ((i / 9) & 1), slot(x), slot(y)};
                } else if (code == 2) {
                    const uint32_t kind = i % 5;
                    if (kind == 1) { // Original signed multiplication must not overflow.
                        x = uint64_t(int64_t(int32_t(rng())));
                        y = uint64_t(int64_t(int32_t(rng())));
                    }
                    if (kind >= 2 && !y) y = 1;
                    if (kind == 3 && x == (1ULL << 63) && y == UINT64_MAX) y = 1;
                    a = {kind, slot(x), slot(y)};
                } else if (code == 3) {
                    static constexpr uint32_t bits[] = {0, 1, 7, 31, 62};
                    a = {slot(x), bits[i % 5]};
                } else if (code <= 15) {
                    if (code >= 10 && code <= 12) y = i % 64;
                    a = {slot(x), slot(y)};
                } else if (code == 16 || code == 20) a = {slot(x)};
                else if (code == 17) a = {slot(x), slot(y), i % 64};
                else if (code == 18) a = {slot(x), i % 64, slot(y)};
                else if (code == 19) a = {slot(i % 3 ? x | 1 : 0), slot(x), slot(y)};
                else if (code == 21) a = {i & 1, (i % 2) * 128, 255, slot(x)};
                else if (code == 22 || code == 23) {
                    const uint32_t n = code == 22 ? i % 9 : 1 + i % 8;
                    a.push_back(n);
                    for (uint32_t k = 0; k < n; ++k) a.push_back(slot(value((i + k) % 512)));
                } else if (code == 24) {
                    const uint32_t base = m.jump.size();
                    for (unsigned k = 0; k < 16; ++k) m.jump.push_back(slot(rng()));
                    a = {slot(x), 15, base};
                } else if (code <= 27) {
                    static constexpr uint32_t sizes[] = {0, 1, 3, 8};
                    static constexpr uint32_t widths[] = {1, 63, 64, 65, 127, 128, 191, 192, 255, 256, 511, 512, 513, 640};
                    static constexpr uint64_t shifts[] = {0, 1, 63, 64, 65, 127, 128, 191, 255, 511, 512, 575, 1023};
                    uint32_t n = sizes[i % 4];
                    if (code == 27 && !n) n = 1; // Original uses scratch.back().
                    const uint32_t bw = widths[(i / 4) % 14];
                    a = {n, slot(shifts[(i / 56) % 13]), slot(1023), bw};
                    for (uint32_t k = 0; k < n; ++k) a.push_back(slot(value((i + k) % 512)));
                    words = (bw + 63) / 64;
                } else {
                    a = {slot(i % 4 != 0), slot(i % 4 != 1), i & 1,
                         (i % 2) * 128, 255, slot(x), slot(y)};
                }
                add(code, a, words);
            }
        }
        // A Cartesian set of wide shifts/lengths/output widths, including every
        // cross-word shift boundary and both positive/negative signed inputs.
        // Append this as a second layer because memory writes finish layer one.
        m.layers.push_back({0, uint32_t(m.ops.size()), outputs, mem_begin});
        m.max_outputs = outputs;
        const uint32_t begin = m.ops.size(); outputs = 0;
        for (uint32_t code : {25u, 26u, 27u})
            for (uint32_t n : {0u, 1u, 3u, 8u}) {
                if (code == 27 && !n) continue;
                for (uint64_t shift : {0u, 1u, 63u, 64u, 65u, 127u, 128u, 511u, 512u, 1023u})
                    for (uint32_t bw : {1u, 64u, 65u, 127u, 192u, 512u, 513u, 640u}) {
                        std::vector<uint32_t> a{n, slot(shift), slot(1023), bw};
                        for (uint32_t k = 0; k < n; ++k) a.push_back(slot(rng()));
                        add(code, a, (bw + 63) / 64);
                    }
            }
        m.layers.push_back({begin, uint32_t(m.ops.size()), outputs, uint32_t(m.ops.size())});
        m.max_outputs = std::max(m.max_outputs, outputs);
        m.observe = m.scatter;
        m.validate();
    }
};

void boundaries() {
    const Op op{0, 0, 1, 10};
    std::array<uint64_t, 4> data{UINT64_MAX, 64, 0, 0}, out{};
    std::array<uint32_t, 4> args{0, 1, 2, 3};
    Error error{}; Write write{};
    View v{&op, args.data(), nullptr, nullptr, data.data(), out.data(), nullptr, nullptr, &write, &error, 0, 0};
    for (unsigned code : {10u, 11u, 12u}) {
        Op current = op; current.code = code; v.ops = &current;
        evaluate(v, 0);
        if (out[0] != (code == 12 ? UINT64_MAX : 0)) throw std::runtime_error("Shift >=64 boundary");
    }
    Op current = op; v.ops = &current;
    current.code = 3; args[1] = 63;
    evaluate(v, 0);
    if (out[0] != UINT64_MAX) throw std::runtime_error("asSInt bit63 boundary");
    current.code = 2; args = {3, 0, 1, 0}; data[0] = 1ULL << 63; data[1] = UINT64_MAX;
    evaluate(v, 0);
    if (out[0] != data[0]) throw std::runtime_error("Signed division overflow boundary");
    data[1] = 0; evaluate(v, 0);
    if (error.code != 2) throw std::runtime_error("Division by zero error missing");
    error = {}; current.code = 21; args = {0, 0, 3, 0}; data[0] = 7;
    evaluate(v, 0);
    if (error.code != 4) throw std::runtime_error("Out-of-bounds memory error missing");
}

void memory_write_order() {
    std::array<Op, 6> ops{};
    std::array<Write, 6> writes{{
        {17, 0x11, 0, 1}, {17, 0x22, 1, 1}, {17, 0x33, 0, 1},
        {17, 0x44, 0, 0}, {17, 0x55, 2, 1}, {18, 0x66, 0, 1}
    }};
    for (uint32_t i = 0; i < ops.size(); ++i) ops[i] = {0, i, 0, 28};
    std::array<uint64_t, 32> mem64{};
    std::array<uint8_t, 32> mem8{};
    View view{}; view.ops = ops.data(); view.writes = writes.data();
    view.mem64 = mem64.data(); view.mem8 = mem8.data();
    const Layer layer{0, uint32_t(ops.size()), 0, 0};
    const std::array<bool, 6> expected{false, false, true, false, true, true};
    for (uint32_t i = 0; i < ops.size(); ++i) {
        if (is_final_memory_write(view, layer, i) != expected[i])
            throw std::runtime_error("Parallel memory-write conflict resolution mismatch at " + std::to_string(i));
        if (is_final_memory_write(view, layer, i)) {
            const Write w = writes[ops[i].out];
            if (w.kind == 0) mem64[w.address] = w.value;
            else mem8[w.address] = static_cast<uint8_t>(w.value);
        }
    }
    if (mem64[17] != 0x33 || mem8[17] != 0x55 || mem64[18] != 0x66)
        throw std::runtime_error("Parallel memory-write final state differs from ordered commit");
}

void grouped_error_node() {
    Model m;
    m.initial = {0, 1, 0}; m.size64 = m.size8 = 1;
    for (uint32_t node = 0; node < 257; ++node) {
        m.ops.push_back({uint32_t(m.args.size()), node, 1, 2});
        m.args.insert(m.args.end(), {2, 1, node == 14 ? 2u : 1u});
        m.scatter.push_back(uint32_t(m.initial.size())); m.initial.push_back(0);
    }
    m.layers.push_back({0, 257, 257, 257}); m.max_outputs = 257;
    m.validate();
    auto engine = make_cuda(m);
    try { engine->step(false); }
    catch (const std::runtime_error& error) {
        if (std::string(error.what()).find("GPU evaluator error 2 node 14 detail 0") != std::string::npos) return;
        throw;
    }
    throw std::runtime_error("Grouped division-by-zero error missing");
}

int main(int argc, char** argv) try {
    bool cpu = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--cpu") cpu = true;
        else throw std::runtime_error("Unknown op_test argument: " + a);
    }
    boundaries(); memory_write_order();
    Fixture f; const Model& m = f.m;
    auto engine = cpu ? make_cpu(m) : make_cuda(m);
    if (!cpu && (engine->info().backend != "CUDA Graph" || !engine->info().op_group_nodes))
        throw std::runtime_error("Opcode test did not use CUDA Graph");
    auto data = m.initial;
    std::vector<uint64_t> output(m.max_outputs), mem64(m.size64), scratch1, scratch2;
    std::vector<uint8_t> mem8(m.size8);
    std::memcpy(mem64.data(), m.program.data(), m.program.size());
    for (unsigned cycle = 0; cycle < 4; ++cycle) {
        data[m.reset_slot] = cycle < 2;
        size_t offset = 0;
        for (Layer l : m.layers) {
            for (uint32_t i = l.begin; i < l.end; ++i) {
                const Op o = m.ops[i];
                const uint32_t* p = m.args.data() + o.arg;
                if (o.out > UINT16_MAX) throw std::runtime_error("Oracle counter too narrow");
                const auto words = op_ss_3(o.code, uint16_t(o.out), p, data.data(), output.data(), mem64.data(), mem8.data(), m.jump.data(), scratch1, scratch2);
                if ((o.code == 28 ? 0u : words) != o.words || p != m.args.data() + o.arg +
                    (o.code == 22 || o.code == 23 ? m.args[o.arg] + 1 : o.code >= 25 && o.code <= 27 ? m.args[o.arg] + 4 :
                    o.code == 28 ? 7 : o.code == 21 ? 4 : o.code <= 2 || (o.code >= 17 && o.code <= 19) || o.code == 24 ? 3 :
                    o.code == 16 || o.code == 20 ? 1 : 2))
                    throw std::runtime_error("Oracle descriptor consumption mismatch");
            }
            for (uint32_t j = 0; j < l.outputs; ++j) data[m.scatter[offset + j]] = output[j];
            offset += l.outputs;
        }
        const auto result = engine->step(cycle < 2);
        for (size_t j = 0; j < m.observe.size(); ++j) if (result.snapshot[j] != data[m.observe[j]])
            throw std::runtime_error("Opcode oracle mismatch cycle " + std::to_string(cycle) + " output " + std::to_string(j));
    }
    const auto fp = engine->fingerprints();
    if (fp.data != fingerprint(data) || fp.memory64 != fingerprint(mem64) || fp.memory8 != fingerprint(mem8))
        throw std::runtime_error("Opcode oracle final state or memory mismatch");
    if (!cpu) grouped_error_node();
    std::cout << "PASS: all 29 opcodes, " << m.ops.size() << " vectors x 4 cycles, original OU oracle; "
              << engine->info().backend << "; host boundary cases\n";
    return 0;
} catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
