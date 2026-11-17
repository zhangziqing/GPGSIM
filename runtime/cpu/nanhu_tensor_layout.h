#pragma once
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>

// Decode only operation spans and output counts before calling the unchanged
// RU evaluator. Wide operations still use its original uint16_t return value.
inline std::vector<size_t> nanhu_layer_output_sizes(
    const std::vector<uint8_t>& operations, const std::vector<uint32_t>& arguments,
    const std::vector<uint32_t>& lengths, size_t capacity, size_t scatter_size) {
    size_t op_index = 0, arg_index = 0, total_outputs = 0;
    std::vector<size_t> outputs;
    for (auto length : lengths) {
        size_t count = 0;
        for (uint32_t i = 0; i < length; ++i) {
            const unsigned op = operations.at(op_index++);
            size_t parameters = 0, words = 1;
            if (op <= 2 || (op >= 17 && op <= 19) || op == 24)
                parameters = 3;
            else if (op >= 3 && op <= 15) parameters = 2;
            else if (op == 16 || op == 20) parameters = 1;
            else if (op == 21) parameters = 4;
            else if (op == 22 || op == 23) parameters = 1 + arguments.at(arg_index);
            else if (op >= 25 && op <= 27) {
                parameters = 4 + arguments.at(arg_index);
                words = (size_t(arguments.at(arg_index + 3)) + 63) / 64;
                if (!words || words > 65535) throw std::runtime_error("RU wide-operation return count exceeds uint16_t");
            } else if (op == 28) parameters = 7;
            else throw std::runtime_error("Unknown RU opcode");
            if (parameters > arguments.size() - arg_index)
                throw std::runtime_error("Truncated RU operation arguments");
            arg_index += parameters;
            count += words;
            if (count > capacity) throw std::runtime_error("RU outputs exceed the allocated layer buffer");
        }
        outputs.push_back(count);
        total_outputs += count;
    }
    if (op_index != operations.size() || arg_index != arguments.size() || total_outputs != scatter_size)
        throw std::runtime_error("RU tensor dimensions do not match decoded operation spans");
    return outputs;
}
