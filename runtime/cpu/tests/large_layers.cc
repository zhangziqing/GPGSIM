#define JSONPATH NANHU_TENSOR_ROOT "/json/"
#define TXTPATH NANHU_TENSOR_ROOT "/txt/"
#define YAMLPATH NANHU_TENSOR_ROOT "/yaml/"
#define main rteaal_rocket_main
#include "kernel_RU.cc"
#undef main
#include "nanhu_tensor_layout.h"

int main() {
    std::vector<uint8_t> types;
    std::vector<uint32_t> arguments;
    size_t words = 0;
    const auto append = [&](unsigned type, std::initializer_list<uint32_t> params, unsigned outputs = 1) {
        types.push_back(type);
        arguments.insert(arguments.end(), params);
        words += outputs;
    };
    for (unsigned repeat = 0; repeat < 10000; ++repeat) {
        for (unsigned op = 0; op <= 28; ++op) {
            if (op <= 1) append(op, {128, 3, 4});
            else if (op == 2) append(op, {repeat % 5, 3, 4});
            else if (op == 3) append(op, {3, 7});
            else if (op >= 4 && op <= 15) append(op, {3, 4});
            else if (op == 16 || op == 20) append(op, {3});
            else if (op == 17) append(op, {3, 4, 7});
            else if (op == 18) append(op, {3, 2, 5});
            else if (op == 19) append(op, {4, 3, 6});
            else if (op == 21) append(op, {0, 0, 15, 4});
            else if (op == 22 || op == 23) append(op, {3, 3, 4, 6});
            else if (op == 24) append(op, {4, 3, 0});
            else if (op >= 25 && op <= 27) append(op, {3, 4, 5, 130, 3, 6, 7}, 3);
            else if (op == 28) append(op, {4, 4, 0, 0, 15, 4, 3});
        }
    }
    std::vector<uint64_t> inputs(256, 0x1357), a(words, 0x99), b(a), memory_a(32), memory_b(32);
    std::vector<uint8_t> memory8_a(32), memory8_b(32);
    inputs[2] = 7; inputs[3] = 0x81000000000000feULL; inputs[4] = 1;
    inputs[5] = ~uint64_t(0); inputs[6] = 0x0302010007080504ULL; inputs[7] = 2;
    std::vector<uint32_t> jump{3, 4, 6, 7};
    std::vector<uint64_t> scratch1, scratch2;
    const uint32_t* rp = arguments.data();
    size_t offset = 0;
    uint16_t local = 0;
    // Original RU calling convention in safe-size chunks.
    for (auto type : types) {
        if (local >= 50000) { offset += local; local = 0; }
        local += op_ss_3(type, local, rp, inputs.data(), a.data() + offset,
                         memory_a.data(), memory8_a.data(), jump.data(), scratch1, scratch2);
    }
    if (offset + local != words || rp != arguments.data() + arguments.size())
        throw std::runtime_error("Reference dimensions differ");
    rp = arguments.data(); offset = 0;
    for (auto type : types)
        offset += op_ss_3(type, 0, rp, inputs.data(), b.data() + offset,
                         memory_b.data(), memory8_b.data(), jump.data(), scratch1, scratch2);
    if (offset != words || rp != arguments.data() + arguments.size() || a != b ||
        memory_a != memory_b || memory8_a != memory8_b)
        throw std::runtime_error("Large-layer calling convention differs from original RU");
    auto sizes = nanhu_layer_output_sizes(types, arguments, {uint32_t(types.size())}, words, words);
    if (sizes != std::vector<size_t>{words}) throw std::runtime_error("Decoded output size differs");
    std::cout << "PASS: " << types.size() << " operations, " << words
              << " outputs; all 29 opcodes; uint16 boundary crossed; wide shifts and memory match original RU\n";
}
