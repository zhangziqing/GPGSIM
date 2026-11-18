#include <cuda_runtime.h>
#include "engine.h"
#include "flat_ops.h"
#include "memory_plan.h"
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace nanhu {
constexpr unsigned threads_per_block = 256, small_layer_threshold = 128;
static void check(cudaError_t e, const char* call) {
    if (e != cudaSuccess) throw std::runtime_error(std::string(call) + ": " + cudaGetErrorString(e));
}
#define CU(call) check((call), #call)
template<class T> struct Device {
    T* p = nullptr; size_t n = 0;
    Device() = default;
    explicit Device(size_t count) : n(count) { if (n) CU(cudaMalloc(reinterpret_cast<void**>(&p), n * sizeof(T))); }
    ~Device() { if (p) cudaFree(p); }
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    void copy(const std::vector<T>& x) { if (x.size() != n) throw std::runtime_error("Device size"); if (n) CU(cudaMemcpy(p, x.data(), n * sizeof(T), cudaMemcpyHostToDevice)); }
    std::vector<T> download() const { std::vector<T> x(n); if (n) CU(cudaMemcpy(x.data(), p, n * sizeof(T), cudaMemcpyDeviceToHost)); return x; }
};
__global__ void set_reset(uint64_t* data, uint32_t slot, bool reset) { if (threadIdx.x == 0) data[slot] = reset; }
__global__ void compute(View v, Layer l) {
    for (uint32_t i = l.begin + blockIdx.x * blockDim.x + threadIdx.x; i < l.end; i += blockDim.x * gridDim.x) {
        evaluate_flat(v, i);
    }
}
__global__ void commit(View v, Layer l, uint32_t scatter_offset) {
    const uint64_t thread = uint64_t(blockIdx.x) * blockDim.x + threadIdx.x;
    const uint64_t stride = uint64_t(gridDim.x) * blockDim.x;
    commit_memory_parallel(v, l, thread, stride);
    for (uint64_t j = thread; j < l.outputs; j += stride)
        v.data[v.scatter[scatter_offset + j]] = v.out[j];
}
// One block owns all of these layers. Every barrier is reached by every thread.
// Global data/memory writes become visible before the next layer's reads.
__global__ void small_layers(View v, const Layer* layers, uint32_t begin, uint32_t end, uint32_t scatter_offset) {
    for (uint32_t k = begin; k < end; ++k) {
        const Layer l = layers[k];
        for (uint32_t i = l.begin + threadIdx.x; i < l.end; i += blockDim.x) {
            evaluate_flat(v, i);
        }
        __syncthreads();
        if (threadIdx.x == 0) commit_memory<true>(v, l);
        for (uint32_t j = threadIdx.x; j < l.outputs; j += blockDim.x)
            v.data[v.scatter[scatter_offset + j]] = v.out[j];
        __syncthreads();
        scatter_offset += l.outputs;
    }
}
__global__ void gather(const uint64_t* data, const uint32_t* slots, uint32_t n, const Error* error, uint64_t* result) {
    for (uint32_t i = blockIdx.x * blockDim.x + threadIdx.x; i < n; i += blockDim.x * gridDim.x) result[i] = data[slots[i]];
    if (blockIdx.x == 0 && threadIdx.x == 0) {
        result[n] = error->code; result[n + 1] = error->node; result[n + 2] = error->detail;
    }
}

class CUDA final : public Engine {
    const Model& m;
    Info details;
    MemoryWritePlan plan;
    Device<FlatOp> flat_ops;
    Device<Layer> layers;
    Device<uint32_t> args, scatter, jump, observe;
    Device<uint32_t> memory_offsets, memory_later;
    Device<uint64_t> data, out, mem64, result;
    Device<uint8_t> mem8;
    Device<Write> writes;
    Device<Error> error;
    cudaStream_t stream = nullptr;
    cudaEvent_t start = nullptr, stop = nullptr;
    cudaGraphExec_t graphs[2]{};
    uint64_t* pinned = nullptr;
    std::vector<uint64_t> snapshot;
    View view{};

    static bool small(Layer l) { return l.end - l.begin <= small_layer_threshold && l.outputs <= small_layer_threshold; }
    static unsigned blocks(uint32_t n) { return std::max(1u, std::min(65535u, (n + threads_per_block - 1) / threads_per_block)); }
    void enqueue(bool reset) {
        set_reset<<<1, 1, 0, stream>>>(data.p, m.reset_slot, reset);
        unsigned kernels = 2, fused = 0; uint32_t offset = 0;
        for (uint32_t i = 0; i < m.layers.size();) {
            const Layer l = m.layers[i];
            if (small(l)) {
                const uint32_t begin = i, old_offset = offset;
                do { offset += m.layers[i].outputs; ++i; } while (i < m.layers.size() && small(m.layers[i]));
                small_layers<<<1, threads_per_block, 0, stream>>>(view, layers.p, begin, i, old_offset);
                ++kernels; fused += i - begin;
            } else {
                compute<<<blocks(l.end - l.begin), threads_per_block, 0, stream>>>(view, l);
                commit<<<blocks(l.outputs), threads_per_block, 0, stream>>>(view, l, offset);
                kernels += 2; offset += l.outputs; ++i;
            }
        }
        gather<<<blocks(m.observe.size()), threads_per_block, 0, stream>>>(data.p, observe.p, m.observe.size(), error.p, result.p);
        CU(cudaGetLastError());
        details.kernels_per_cycle = kernels; details.fused_layers = fused;
    }
    void capture(bool reset) {
        cudaGraph_t graph = nullptr;
        CU(cudaStreamBeginCapture(stream, cudaStreamCaptureModeThreadLocal));
        enqueue(reset);
        CU(cudaStreamEndCapture(stream, &graph));
        cudaError_t e = cudaGraphInstantiate(&graphs[reset], graph, nullptr, nullptr, 0);
        cudaGraphDestroy(graph);
        CU(e);
        CU(cudaGraphUpload(graphs[reset], stream));
        CU(cudaStreamSynchronize(stream));
    }
public:
    explicit CUDA(const Model& model, int device) : m(model),
        plan(build_memory_write_plan(m)),
        flat_ops(m.ops.size()),
        layers(m.layers.size()), args(m.args.size()), scatter(m.scatter.size()),
        jump(m.jump.size()), observe(m.observe.size()), memory_offsets(plan.offsets.size()), memory_later(plan.later.size()),
        data(m.initial.size()), out(m.max_outputs),
        mem64(m.size64), result(m.observe.size() + 3), mem8(m.size8), writes(m.memwrites), error(1), snapshot(m.observe.size()) {
        cudaDeviceProp prop{}; CU(cudaGetDeviceProperties(&prop, device));
        details.backend = "CUDA Graph"; details.device = prop.name;
        details.threads_per_block = threads_per_block; details.small_layer_threshold = small_layer_threshold;
        CU(cudaStreamCreateWithFlags(&stream, cudaStreamNonBlocking));
        CU(cudaEventCreate(&start)); CU(cudaEventCreate(&stop));
        CU(cudaMallocHost(reinterpret_cast<void**>(&pinned), (snapshot.size() + 3) * sizeof(uint64_t)));
        const FlatPlan flat = build_flat_plan(m);
        flat_ops.copy(flat.ops);
        details.flat_inlined_nodes = flat.inlined_nodes; details.flat_plan_seconds = flat.build_seconds;
        details.descriptor_bytes = flat_ops.n * sizeof(FlatOp);
        layers.copy(m.layers); args.copy(m.args); scatter.copy(m.scatter);
        jump.copy(m.jump); observe.copy(m.observe); data.copy(m.initial);
        memory_offsets.copy(plan.offsets); memory_later.copy(plan.later);
        details.memory_candidate_pairs = plan.later.size(); details.memory_full_scan_pairs = plan.full_scan_pairs;
        details.memory_inactive_writers = plan.inactive_writers; details.memory_plan_seconds = plan.build_seconds;
        details.memory_plan_bytes = (plan.offsets.size() + plan.later.size()) * sizeof(uint32_t);
        CU(cudaMemset(mem64.p, 0, m.size64 * sizeof(uint64_t)));
        CU(cudaMemset(mem8.p, 0, m.size8)); CU(cudaMemset(error.p, 0, sizeof(Error)));
        for (auto base : m.ram_bases) if (!m.program.empty()) CU(cudaMemcpy(mem64.p + base, m.program.data(), m.program.size(), cudaMemcpyHostToDevice));
        // cudaMemset and pageable H2D copies may still be pending on the default
        // stream. Our non-blocking simulation stream must not race initialization.
        CU(cudaDeviceSynchronize());
        view = {nullptr, args.p, scatter.p, jump.p, data.p, out.p, mem64.p, mem8.p, writes.p, error.p, m.size64, m.size8};
        view.memory_offsets = memory_offsets.p; view.memory_later = memory_later.p;
        view.flat_ops = flat_ops.p;
        details.allocated_bytes = details.descriptor_bytes + m.layers.size() * sizeof(Layer) +
            (m.args.size() + m.scatter.size() + m.jump.size() + m.observe.size()) * sizeof(uint32_t) +
            (m.initial.size() + m.max_outputs + m.size64 + snapshot.size() + 3) * sizeof(uint64_t) +
            m.size8 + m.memwrites * sizeof(Write) + sizeof(Error) + details.memory_plan_bytes;
        capture(true); capture(false);
    }
    ~CUDA() override {
        for (auto g : graphs) if (g) cudaGraphExecDestroy(g);
        if (start) cudaEventDestroy(start); if (stop) cudaEventDestroy(stop);
        if (pinned) cudaFreeHost(pinned); if (stream) cudaStreamDestroy(stream);
    }
    Step step(bool reset) override {
        CU(cudaEventRecord(start, stream));
        CU(cudaGraphLaunch(graphs[reset], stream));
        CU(cudaEventRecord(stop, stream));
        CU(cudaMemcpyAsync(pinned, result.p, (snapshot.size() + 3) * sizeof(uint64_t), cudaMemcpyDeviceToHost, stream));
        CU(cudaStreamSynchronize(stream));
        if (pinned[snapshot.size()]) throw std::runtime_error("GPU evaluator error " +
            std::to_string(pinned[snapshot.size()]) + " node " + std::to_string(pinned[snapshot.size() + 1]) +
            " detail " + std::to_string(pinned[snapshot.size() + 2]));
        std::copy(pinned, pinned + snapshot.size(), snapshot.begin());
        float ms = 0; CU(cudaEventElapsedTime(&ms, start, stop));
        return {snapshot, ms / 1000.0};
    }
    Fingerprints fingerprints() override {
        return {fingerprint(data.download()), fingerprint(mem64.download()), fingerprint(mem8.download())};
    }
    Info info() const override { return details; }
};
std::unique_ptr<Engine> make_cuda(const Model& model, int device) {
    CU(cudaSetDevice(device));
    cudaDeviceProp p{}; CU(cudaGetDeviceProperties(&p, device));
    if (p.major < 7 || (p.major == 7 && p.minor < 5)) throw std::runtime_error("This build targets compute capability 7.5+ (RTX 2060)");
    size_t free = 0, total = 0; CU(cudaMemGetInfo(&free, &total));
    const uint64_t need = model.size64 * 8 + model.size8 +
        model.ops.size() * sizeof(FlatOp) +
        (model.args.size() + model.scatter.size() + model.jump.size()) * 4 + model.initial.size() * 8 +
        model.max_outputs * 8 + model.memwrites * sizeof(Write) + 64 * 1024 * 1024;
    if (free < need) throw std::runtime_error("Not enough free VRAM: need approximately " + std::to_string(need) + " bytes");
    return std::make_unique<CUDA>(model, device);
}
} // namespace nanhu
