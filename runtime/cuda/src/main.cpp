#include "engine.h"
#include "coremark_common.h"
#include <filesystem>
#include <map>
#include <random>
#include <cstring>

using Json = nlohmann::json;
using namespace nanhu;
namespace fs = std::filesystem;

template<class T> std::vector<T> binary(const fs::path& p) {
    std::ifstream in(p, std::ios::binary | std::ios::ate);
    if (!in) throw std::runtime_error("Missing " + p.string());
    const auto bytes = in.tellg();
    if (bytes < 0 || uint64_t(bytes) % sizeof(T)) throw std::runtime_error("Bad binary size " + p.string());
    std::vector<T> data(size_t(bytes) / sizeof(T));
    in.seekg(0); in.read(reinterpret_cast<char*>(data.data()), bytes);
    if (!in) throw std::runtime_error("Truncated " + p.string());
    return data;
}
Json read_json(const fs::path& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Missing " + path.string());
    Json j; in >> j; return j;
}
struct Event {
    std::string module;
    std::map<std::string, size_t> ports;
    uint64_t get(const std::vector<uint64_t>& x, const std::string& name) const { return x.at(ports.at(name)); }
};
Model load_model(const fs::path& dir, const Json& j) {
    const uint32_t endian = 1;
    if (*reinterpret_cast<const uint8_t*>(&endian) != 1 || j.at("byte_order") != "little" || j.at("format_version") != 1)
        throw std::runtime_error("Requires little-endian model format1");
    Model m;
    m.ops = binary<Op>(dir / "ops.bin"); m.layers = binary<Layer>(dir / "layers.bin");
    m.args = binary<uint32_t>(dir / "args.bin"); m.scatter = binary<uint32_t>(dir / "scatter.bin");
    m.jump = binary<uint32_t>(dir / "jump.bin"); m.observe = binary<uint32_t>(dir / "observe.bin");
    m.initial = binary<uint64_t>(dir / "initial.bin"); m.program = binary<uint8_t>(dir / "program.bin");
    m.size64 = j.at("memory").at("size64"); m.size8 = j.at("memory").at("size8");
    m.reset_slot = j.at("reset_slot"); m.max_outputs = j.at("max_outputs"); m.memwrites = j.at("memwrites");
    m.ram_bases = j.at("ram_bases").get<std::vector<uint64_t>>();
    if (m.ops.size() != j.at("nodes") || m.layers.size() != j.at("layers") || m.initial.size() != j.at("data_slots"))
        throw std::runtime_error("Descriptor counts differ from model.json");
    m.validate(); return m;
}

// Exercise the actual GPU launch/barrier paths against the shared CPU path.
// Independent opcode tests are in op_test; this checks staging/scatter/order.
int self_test(bool cpu, int device) {
    Model m; m.initial.resize(1024); m.reset_slot = 0; m.size64 = 1024; m.size8 = 1024;
    std::mt19937_64 rng(103);
    for (auto& x : m.initial) x = rng();
    m.initial[1] = 1; m.initial[2] = 17;
    uint32_t args = 0, out = 0;
    // A wide layer, two aliased writes (last writer wins), old-state reads.
    for (uint32_t i = 0; i < 257; ++i) {
        m.ops.push_back({args, out++, 1, 15}); m.args.insert(m.args.end(), {i % 200 + 3, (i * 7) % 200 + 3}); args += 2;
        m.scatter.push_back(300 + i);
    }
    for (uint32_t val : {3u, 4u}) {
        m.ops.push_back({args, m.memwrites++, 0, 28});
        m.args.insert(m.args.end(), {1, 1, 0, 0, 1023, 2, val}); args += 7;
    }
    const uint32_t mem8_first_slot = uint32_t(m.initial.size()); m.initial.push_back(0x35);
    const uint32_t mem8_last_slot = uint32_t(m.initial.size()); m.initial.push_back(0xA7);
    for (uint32_t value_slot : {mem8_first_slot, mem8_last_slot}) {
        m.ops.push_back({args, m.memwrites++, 0, 28});
        m.args.insert(m.args.end(), {1, 1, 1, 0, 1023, 2, value_slot}); args += 7;
    }
    m.layers.push_back({0, uint32_t(m.ops.size()), out, 257}); m.max_outputs = out;
    uint32_t previous = 300;
    for (uint32_t i = 0; i < 20; ++i) {
        const uint32_t begin = m.ops.size();
        m.ops.push_back({args, 0, 1, 19}); m.args.insert(m.args.end(), {1, previous, previous + 1}); args += 3;
        m.scatter.push_back(600 + i); previous = 600 + i;
        m.layers.push_back({begin, begin + 1, 1, begin + 1});
    }
    // Read address17 after the aliased writes have committed.
    uint32_t begin = m.ops.size(); m.ops.push_back({args, 0, 1, 21}); m.args.insert(m.args.end(), {0, 0, 1023, 2});
    args += 4;
    m.scatter.push_back(900); m.layers.push_back({begin, begin + 1, 1, begin + 1});
    begin = m.ops.size(); m.ops.push_back({args, 0, 1, 21}); m.args.insert(m.args.end(), {1, 0, 1023, 2});
    args += 4;
    m.scatter.push_back(901); m.layers.push_back({begin, begin + 1, 1, begin + 1});
    for (uint32_t i = 0; i < m.initial.size(); ++i) m.observe.push_back(i);
    m.validate();
    auto reference = make_cpu(m);
    auto test = cpu ? make_cpu(m) : make_cuda(m, device);
    for (uint32_t cycle = 0; cycle < 96; ++cycle) {
        auto expected = reference->step(cycle < 64);
        auto actual = test->step(cycle < 64);
        if (expected.snapshot != actual.snapshot) throw std::runtime_error("Self-test snapshot mismatch cycle " + std::to_string(cycle));
        if (actual.snapshot[900] != m.initial[4]) throw std::runtime_error("Aliased writes did not preserve last-writer order");
        if (actual.snapshot[901] != m.initial[mem8_last_slot])
            throw std::runtime_error("Aliased mem8 writes mismatch: expected " + std::to_string(m.initial[mem8_last_slot]) +
                                     " got " + std::to_string(actual.snapshot[901]));
    }
    const auto a = reference->fingerprints(), b = test->fingerprints();
    if (a.data != b.data || a.memory64 != b.memory64 || a.memory8 != b.memory8) throw std::runtime_error("Self-test final memory mismatch");
    const Info info = test->info();
    if (!cpu && (info.backend != "CUDA Graph" || info.kernels_per_cycle != 5 ||
                 info.fused_layers != m.layers.size() - 1))
        throw std::runtime_error("Optimized graph/fusion schedule differs");
    std::cout << Json{{"status", "passed"}, {"test", "96 cycles; 257-operation wide + 22 narrow layers; ordered aliased memory writes; 64 reset cycles; all state and memory"},
        {"backend", info.backend}, {"device", info.device}, {"kernels_per_cycle", info.kernels_per_cycle}, {"fused_layers", info.fused_layers},
        {"flat_inlined_nodes", info.flat_inlined_nodes},
        {"descriptor_bytes", info.descriptor_bytes}, {"flat_plan_seconds", info.flat_plan_seconds}}.dump(2) << '\n';
    return 0;
}

int main(int argc, char** argv) try {
    fs::path data_dir = "data";
    std::string trace = "gpu.trace", reference;
    uint64_t cycles = 100000; bool cpu = false, self = false;
    int device = 0;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        const auto next = [&]() -> std::string { if (++i >= argc) throw std::runtime_error("Missing value after " + a); return argv[i]; };
        if (a == "--data") data_dir = next(); else if (a == "--cycles") cycles = std::stoull(next());
        else if (a == "--trace") trace = next(); else if (a == "--reference") reference = next();
        else if (a == "--device") device = std::stoi(next());
        else if (a == "--cpu") cpu = true; else if (a == "--self-test") self = true;
        else if (a == "--help") {
            std::cout << "nanhu_gpu [--data data] [--cycles 100000] [--trace gpu.trace] [--reference prefix.trace]\n"
                " [--device 0] [--self-test] [--cpu]\n";
            return 0;
        } else throw std::runtime_error("Unknown argument " + a);
    }
    if (self) return self_test(cpu, device);
    if (!cycles || cycles > 100000) throw std::runtime_error("This packaged reference supports 1..100000 non-reset cycles");
    const auto load_start = std::chrono::steady_clock::now();
    Json meta = read_json(data_dir / "model.json");
    Model model = load_model(data_dir, meta);
    auto engine = cpu ? make_cpu(model) : make_cuda(model, device);
    const Info info = engine->info();
    std::cerr << info.backend << " device=" << info.device << " VRAMbytes=" << info.allocated_bytes
        << " kernels/cycle=" << info.kernels_per_cycle << " fused_small_layers=" << info.fused_layers << '\n';
    std::vector<Event> commits, ram, sd, exceptions;
    Event trap, arch, fp, csr;
    for (const auto& e : meta.at("events")) {
        Event event{e.at("module"), e.at("ports").get<std::map<std::string, size_t>>()};
        if (event.module == "DifftestInstrCommit") commits.push_back(event);
        else if (event.module == "DifftestTrapEvent") trap = event;
        else if (event.module == "DifftestArchIntRegState") arch = event;
        else if (event.module == "DifftestArchFpRegState") fp = event;
        else if (event.module == "DifftestCSRState") csr = event;
        else if (event.module == "DifftestArchEvent") exceptions.push_back(event);
        else if (event.module.find("RAMHelper") == 0) ram.push_back(event);
        else if (event.module.find("SDHelper") == 0) sd.push_back(event);
    }
    if (commits.empty() || trap.ports.empty() || arch.ports.empty() || fp.ports.empty() || csr.ports.empty())
        throw std::runtime_error("Required observation events missing");
    CoremarkObserver observer(trace, reference);
    const auto csr_ports = meta.at("csr_ports").get<std::vector<std::string>>();
    const size_t uart_valid = meta.at("uart_valid"), uart_ch = meta.at("uart_ch");
    const auto load_end = std::chrono::steady_clock::now();
    double evaluation_seconds = 0, gpu_seconds = 0;
    std::array<double, 10> windows{};
    const auto run_start = std::chrono::steady_clock::now();
    for (uint64_t cycle = 0; cycle < cycles + 64; ++cycle) {
        const auto start = std::chrono::steady_clock::now();
        const Step step = engine->step(cycle < 64);
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        if (cycle >= 64) { evaluation_seconds += seconds; gpu_seconds += step.device_seconds; windows[std::min<uint64_t>(9, (cycle - 64) / 10000)] += seconds; }
        const auto& in = step.snapshot;
        for (const auto& e : ram) if (e.get(in, "en") && (e.get(in, "rIdx") >= (1ULL << 25) ||
                (e.get(in, "wen") && e.get(in, "wIdx") >= (1ULL << 25)))) throw std::runtime_error("Modeled RAM exceeds256MiB");
        if (cycle < 64) continue;
        for (const auto& e : sd) if (e.get(in, "ren") || e.get(in, "setAddr")) throw std::runtime_error("Unexpected SD access");
        CoremarkState state;
        for (const auto& e : commits) if (e.get(in, "io_valid")) state.commits.push_back({
            unsigned(e.get(in, "io_index")), unsigned(e.get(in, "io_special")), unsigned(e.get(in, "io_skip")),
            unsigned(e.get(in, "io_isRVC")), unsigned(e.get(in, "io_fpwen")), unsigned(e.get(in, "io_rfwen")),
            unsigned(e.get(in, "io_wdest")), unsigned(e.get(in, "io_wpdest")), e.get(in, "io_pc"), uint32_t(e.get(in, "io_instr"))});
        for (unsigned i = 0; i < 32; ++i) { state.gpr[i] = arch.get(in, "io_gpr_" + std::to_string(i)); state.fpr[i] = fp.get(in, "io_fpr_" + std::to_string(i)); }
        for (const auto& name : csr_ports) state.csr.push_back(csr.get(in, name));
        for (const auto& e : exceptions) state.exception = {e.get(in, "io_intrNO"), e.get(in, "io_cause"), e.get(in, "io_exceptionPC"), e.get(in, "io_exceptionInst")};
        state.trap = trap.get(in, "io_valid");
        if (state.trap) { state.trap_pc = trap.get(in, "io_pc"); state.trap_code = trap.get(in, "io_code"); }
        observer.observe(cycle, std::move(state), in.at(uart_valid), unsigned(in.at(uart_ch)));
        if ((cycle + 1 - 64) % 10000 == 0) std::cerr << "cycles=" << cycle + 1 - 64 << " retired=" << observer.retired << " eval_seconds=" << evaluation_seconds << '\n';
    }
    const auto run_end = std::chrono::steady_clock::now();
    observer.finish();
    const auto fingerprints = engine->fingerprints();
    Json result = observer.result(info.backend, cycles, evaluation_seconds, std::chrono::duration<double>(run_end - run_start).count());
    result["backend"] = info.backend; result["device"] = info.device;
    result["kernel_device_seconds"] = gpu_seconds;
    result["kernel_device_cycles_per_second"] = gpu_seconds > 0 ? Json(cycles / gpu_seconds) : Json(nullptr);
    result["allocated_device_bytes"] = info.allocated_bytes; result["kernels_per_cycle"] = info.kernels_per_cycle;
    result["fused_small_layers"] = info.fused_layers; result["original_layers"] = model.layers.size();
    result["tail_threshold"] = info.small_layer_threshold; result["threads"] = info.threads_per_block;
    result["full_data_fingerprint"] = fingerprints.data;
    result["memory64_fingerprint"] = fingerprints.memory64; result["memory8_fingerprint"] = fingerprints.memory8;
    result["window_evaluation_seconds"] = windows;
    result["tensor_load_seconds"] = std::chrono::duration<double>(load_end - load_start).count();
    result["memory_candidate_pairs"] = info.memory_candidate_pairs;
    result["memory_full_scan_pairs"] = info.memory_full_scan_pairs;
    result["memory_inactive_writers"] = info.memory_inactive_writers;
    result["memory_plan_bytes"] = info.memory_plan_bytes;
    result["memory_plan_seconds"] = info.memory_plan_seconds;
    result["flat_inlined_nodes"] = info.flat_inlined_nodes;
    result["descriptor_bytes"] = info.descriptor_bytes;
    result["flat_plan_seconds"] = info.flat_plan_seconds;
    result["op_group_segments"] = info.op_group_segments;
    result["op_group_nodes"] = info.op_group_nodes;
    result["op_group_plan_bytes"] = info.op_group_plan_bytes;
    result["op_group_plan_seconds"] = info.op_group_plan_seconds;
    result["timing"] = "Non-reset evaluation wall time includes dispatch, synchronization and compact observation copy; excludes loading, reset, trace formatting, reference comparison and final fingerprints. kernel_device_seconds separately uses CUDA events.";
    result["validation_scope"] = "Nanhu CoreMark fixed prefix; optional online cycle/commit/GPR/FPR/CSR/exception/trap/UART reference; 100K cycles do not complete CoreMark.";
    std::cout << result.dump(2) << '\n';
    return 0;
} catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
