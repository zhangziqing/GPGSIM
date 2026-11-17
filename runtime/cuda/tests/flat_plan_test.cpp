#include "flat_ops.h"
#include <algorithm>
#include <iostream>
#include <random>
#include <stdexcept>

using namespace nanhu;

void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
Model fixture(uint32_t code, const std::vector<uint32_t>& operands) {
    Model m;
    m.initial = {0, UINT64_MAX, uint64_t(1) << 63, UINT64_MAX, 1, 0, 0, 0};
    m.size64 = m.size8 = 8;
    // Exercise nonzero argument offsets and descriptors ending at the pool end.
    m.args.assign(13, UINT32_MAX);
    m.args.insert(m.args.end(), operands.begin(), operands.end());
    const uint32_t words = code == 28 ? 0 : code >= 25 ? (operands[3] + 63) / 64 : 1;
    m.ops.push_back({13, 0, words, code});
    m.layers.push_back({0, 1, words, code == 28 ? 0u : 1u});
    for (uint32_t i = 0; i < words; ++i) {
        m.scatter.push_back(uint32_t(m.initial.size())); m.initial.push_back(0);
    }
    m.max_outputs = words; m.memwrites = code == 28;
    m.jump = {1, 2, 3, 4};
    m.validate();
    return m;
}
Error compare(const Model& m) {
    const auto saved_args = m.args;
    const FlatPlan plan = build_flat_plan(m);
    const Op o = m.ops[0]; const FlatOp f = plan.ops.at(0);
    const uint32_t* a = m.args.data() + o.arg;
    const unsigned n = flat_arity(o.code);
    require(m.args == saved_args && f.out == o.out && f.code == o.code, "Source or node identity changed");
    require(plan.inlined_nodes == (n ? 1u : 0u), "Incorrect inline count");
    require(f.x == (n ? a[0] : o.arg) && f.y == (n ? (n >= 2 ? a[1] : 0) : o.words) &&
            f.z == (n >= 3 ? a[2] : 0), "Incorrect descriptor fields");
    auto data = m.initial;
    std::vector<uint64_t> expected(m.max_outputs + 1, 0x12345678), actual = expected;
    std::vector<uint64_t> mem64(m.size64, 0x98765432);
    std::vector<uint8_t> mem8(m.size8, 0xFE);
    Write ew{0, 0, 0, 0}, aw = ew;
    Error ee{}, ae{};
    View v{};
    v.ops = m.ops.data(); v.args = m.args.data(); v.jump = m.jump.data();
    v.data = data.data(); v.out = expected.data(); v.mem64 = mem64.data(); v.mem8 = mem8.data();
    v.writes = &ew; v.error = &ee; v.size64 = m.size64; v.size8 = m.size8;
    evaluate(v, 0);
    v.ops = nullptr; v.flat_ops = plan.ops.data();
    // Fixed operations must work without any backing argument or legacy array.
    if (n) v.args = nullptr;
    v.out = actual.data(); v.writes = &aw; v.error = &ae;
    evaluate_flat(v, 0);
    require(expected == actual, "Flat output or guard word differs");
    require(ee.code == ae.code && ee.node == ae.node && ee.detail == ae.detail, "Flat error differs");
    require(ew.address == aw.address && ew.value == aw.value && ew.kind == aw.kind && ew.active == aw.active,
            "Flat memory staging differs");
    require(data == m.initial, "Evaluation changed source state");
    if (o.code == 28) {
        require(node_output<true>(v, 0) == o.out, "Flat writer index differs");
        require(is_final_memory_write<true>(v, m.layers[0], 0) == bool(aw.active), "Flat arbitration differs");
        commit_memory<true>(v, m.layers[0]);
        if (aw.active) require(aw.kind == 0 ? mem64[aw.address] == aw.value :
                mem8[aw.address] == uint8_t(aw.value), "Flat ordered commit differs");
    }
    return ae;
}
void random_equivalence() {
    std::mt19937_64 rng(3841);
    for (uint32_t code = 0; code <= 28; ++code) {
        std::vector<uint32_t> a; a.reserve(12);
        if (code <= 2) a = {code == 2 ? 0u : 128u, 1, 2};
        else if (code == 3) a = {1, 63};
        else if (code <= 15) a = {1, 2};
        else if (code == 16 || code == 20) a = {1};
        else if (code == 17) a = {1, 2, 63};
        else if (code == 18) a = {1, 64, 3};
        else if (code == 19) a = {4, 1, 2};
        else if (code == 21) a = {0, 0, 7, 5};
        else if (code <= 23) a = {3, 1, 2, 3};
        else if (code == 24) a = {5, 3, 0};
        else if (code <= 27) a = {3, 5, 3, 129, 1, 2, 3};
        else a = {4, 4, 0, 0, 7, 5, 1};
        Model m = fixture(code, a);
        for (uint32_t trial = 0; trial < 64; ++trial) {
            m.initial[1] = rng(); m.initial[2] = trial < 4 ? 63 + trial : rng();
            m.initial[3] = rng(); m.initial[4] = trial & 1; m.initial[5] = rng();
            if (code == 21 || code == 28) m.args[13 + (code == 21 ? 0 : 2)] = trial % 3;
            if (code == 17 || code == 18) m.args[13 + (code == 17 ? 2 : 1)] = trial < 4 ? 63 + trial : UINT32_MAX;
            compare(m);
        }
    }
}
void errors() {
    for (uint32_t code : {0u, 1u})
        for (uint32_t bw : {0u, 65u})
            require(compare(fixture(code, {2 * bw, 1, 2})).code == 1, "Width error missing");
    require(compare(fixture(3, {1, 64})).code == 1, "Sign bit error missing");
    Model div = fixture(2, {2, 1, 2}); div.initial[2] = 0;
    require(compare(div).code == 2, "Division error missing");
    require(compare(fixture(2, {5, 1, 2})).code == 3, "Arithmetic mode error missing");
    require(compare(fixture(21, {0, UINT32_MAX, 7, 5})).code == 4, "Read bounds error missing");
    require(compare(fixture(28, {4, 4, 0, UINT32_MAX, 7, 5, 1})).code == 4, "Write bounds error missing");
    require(compare(fixture(27, {0, 5, 3, 129})).code == 5, "Empty signed shift error missing");
    Model overflow = fixture(2, {3, 1, 2});
    overflow.initial[1] = uint64_t(1) << 63; overflow.initial[2] = UINT64_MAX;
    require(!compare(overflow).code, "Signed division overflow behavior changed");
}
int main() try {
    random_equivalence(); errors();
    std::cout << "PASS: 29 opcodes x 64 states; inline operands without args; fallback, errors and commit\n";
} catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
