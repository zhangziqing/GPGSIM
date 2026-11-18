#pragma once
#include "ops.h"
#include <memory>
#include <string>
#include <vector>

namespace nanhu {
struct Model {
    std::vector<Op> ops;
    std::vector<Layer> layers;
    std::vector<uint32_t> args, scatter, jump, observe;
    std::vector<uint64_t> initial, ram_bases;
    std::vector<uint8_t> program;
    uint64_t size64 = 0, size8 = 0;
    uint32_t reset_slot = 0, max_outputs = 0, memwrites = 0;
    void validate() const;
};
struct Fingerprints { uint64_t data, memory64, memory8; };
template<class T> uint64_t fingerprint(const std::vector<T>& x) {
    uint64_t h = 14695981039346656037ULL;
    for (T v : x) { h ^= uint64_t(v); h *= 1099511628211ULL; }
    return h;
}
struct Step { const std::vector<uint64_t>& snapshot; double device_seconds; };
struct Info {
    std::string backend, device;
    uint64_t allocated_bytes = 0;
    unsigned kernels_per_cycle = 0, fused_layers = 0;
    unsigned threads_per_block = 0, small_layer_threshold = 0;
    uint64_t memory_candidate_pairs = 0, memory_full_scan_pairs = 0, memory_plan_bytes = 0;
    uint32_t memory_inactive_writers = 0;
    double memory_plan_seconds = 0;
    uint64_t flat_inlined_nodes = 0, descriptor_bytes = 0;
    double flat_plan_seconds = 0;
    uint64_t op_group_segments = 0, op_group_nodes = 0, op_group_plan_bytes = 0;
    double op_group_plan_seconds = 0;
};
class Engine {
public:
    virtual ~Engine() = default;
    virtual Step step(bool reset) = 0;
    virtual Fingerprints fingerprints() = 0;
    virtual Info info() const = 0;
};
std::unique_ptr<Engine> make_cpu(const Model& model);
std::unique_ptr<Engine> make_cuda(const Model& model, int device = 0);
} // namespace nanhu
