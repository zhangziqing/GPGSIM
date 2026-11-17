// Single master-cycle runner for the experimental pinned Nanhu adapter.
// Uses the upstream RU operation evaluator. No FESVR/SimDTM timing is included.
#define JSONPATH NANHU_TENSOR_ROOT "/json/"
#define TXTPATH NANHU_TENSOR_ROOT "/txt/"
#define YAMLPATH NANHU_TENSOR_ROOT "/yaml/"
#define main rteaal_rocket_main
#include "kernel_RU.cc"
#undef main
#include "rv64_loop_oracle.h"
#include "nanhu_tensor_layout.h"
#include <algorithm>
#include <iomanip>
#include <set>

using Json = nlohmann::json;
Json read_json(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("Missing " + path);
    Json value; f >> value; return value;
}
struct Event {
    std::string module, prefix;
    std::map<std::string, size_t> ports;
    uint64_t get(const std::vector<uint64_t>& data, const std::string& name) const {
        auto pin = ports.find(name);
        if (pin == ports.end()) throw std::runtime_error("Missing observed pin " + prefix + name);
        return data.at(pin->second);
    }
};

int main(int argc, char** argv) try {
    if (argc < 5) throw std::runtime_error("Usage: rteaal_nanhu DESIGN PROGRAM.BIN PRESERVED_FILE MAX_CYCLES [TRACE]");
    const std::string design = argv[1], program_path = argv[2], preserved = argv[3];
    const uint64_t max_cycles = std::stoull(argv[4]);
    std::ifstream program_file(program_path, std::ios::binary);
    if (!program_file) throw std::runtime_error("Missing program");
    std::vector<uint8_t> program((std::istreambuf_iterator<char>(program_file)), {});
    LoopOracle oracle(program);
    std::ofstream trace;
    if (argc > 5) trace.open(argv[5]);
    auto load_start = std::chrono::steady_clock::now();
    auto in = load_txt_file("layerIn", design, loadSizeTFromJson(JSONPATH + design + "_S_shape.json"));
    auto r = loadVectorFromJson<uint32_t>(JSONPATH + design + "_unswizzled_rptr.json");
    auto s = loadVectorFromJson<uint32_t>(JSONPATH + design + "_unswizzled_sptr.json");
    auto n = loadVectorFromJson<uint8_t>(JSONPATH + design + "_unswizzled_nptr.json");
    auto lengths = loadVectorFromJson<uint32_t>(JSONPATH + design + "_unswizzled_ss_lens_ptr.json");
    auto inputs = parse_json_var_indices(JSONPATH "InputRegMap_" + design + ".json");
    auto outputs = parse_json_dtm_indices(JSONPATH "dtmSignal_" + design + ".json");
    auto jt = load_mux_file("muxJT", design);
    const size_t max_s = loadSizeTFromJson(JSONPATH + design + "_max_s.json");
#ifndef NANHU_LARGE_LAYERS
    // Do not silently truncate the upstream evaluator's 16-bit output counter.
    if (max_s > 65535) throw std::runtime_error("Upstream RU counter is 16-bit; layer width " + std::to_string(max_s));
    if (std::any_of(lengths.begin(), lengths.end(), [](auto x) { return x > 65535; }))
        throw std::runtime_error("Upstream RU operation count is 16-bit");
#endif
    const auto expected_outputs = nanhu_layer_output_sizes(n, r, lengths, max_s, s.size());
    auto sizes = read_json(NANHU_METADATA_ROOT "/memory_sizes_" + design + ".json");
    std::vector<uint64_t> out(max_s), scratch1, scratch2;
    std::vector<uint64_t> memory64(std::max<size_t>(1, sizes["size64"].get<size_t>()));
    std::vector<uint8_t> memory8(std::max<size_t>(1, sizes["size8"].get<size_t>()));
    auto memory_map = read_json(NANHU_METADATA_ROOT "/accum_length_dict_" + design + ".json");
    size_t ram_copies = 0;
    for (auto& [name, index] : memory_map.items()) {
        if (name.size() >= 4 && name.substr(name.size() - 4) == ".mem") {
            // Require a RAMHelper interface at this prefix, not any memory named mem.
            auto interfaces = read_json(preserved + ".interfaces.json");
            bool is_ram = false;
            for (auto& j : interfaces)
                if (j["module"].get<std::string>().find("RAMHelper") == 0 &&
                    j["prefix"].get<std::string>() + "mem" == name) is_ram = true;
            if (!is_ram) continue;
            const size_t base = index.get<size_t>();
            if (base + (program.size() + 7) / 8 > memory64.size())
                throw std::runtime_error("RAM image exceeds tensor allocation");
            std::memcpy(memory64.data() + base, program.data(), program.size());
            ++ram_copies;
        }
    }
    if (!ram_copies) throw std::runtime_error("No modeled RAMHelper memories found");
    std::vector<Event> commits, ram_events, sd_events, exception_events;
    Event trap, arch;
    for (auto& j : read_json(preserved + ".interfaces.json")) {
        Event event{j["module"], j["prefix"], {}};
        for (auto& port : j["inputs"]) {
            auto key = port.get<std::string>();
            auto it = outputs.find(event.prefix + key);
            if (it != outputs.end()) event.ports[key] = it->second;
        }
        if (event.module == "DifftestInstrCommit") commits.push_back(event);
        else if (event.module == "DifftestTrapEvent") trap = event;
        else if (event.module == "DifftestArchIntRegState") arch = event;
        else if (event.module.find("RAMHelper") == 0) ram_events.push_back(event);
        else if (event.module.find("SDHelper") == 0) sd_events.push_back(event);
        else if (event.module == "DifftestArchEvent") exception_events.push_back(event);
    }
    if (commits.empty() || trap.ports.empty() || arch.ports.empty())
        throw std::runtime_error("Missing retirement/trap/register observation");
    // Runtime inputs and uninitialized architectural state are deterministic.
    // Constants are stored separately in layerIn and are left intact.
    for (auto& [name, indices] : inputs) for (auto index : indices) in.at(index) = 0;
    auto reset_index = inputs.at("reset").at(0);
    auto load_end = std::chrono::steady_clock::now();
    std::cerr << "Loaded " << n.size() << " operations, " << lengths.size() << " layers; "
              << ram_copies << " RAM banks; load "
              << std::chrono::duration<double>(load_end - load_start).count() << " s\n";
    constexpr uint64_t reset_cycles = 64;
    uint64_t retired = 0, first_commit_cycle = 0, first_commit_instructions = 0, measured_cycles = 0;
    double simulation_seconds = 0;
    bool saw_trap = false;
    uint64_t trap_code = ~uint64_t(0), trap_pc = 0;
    std::chrono::steady_clock::time_point measured_start;
    std::set<unsigned> defined_registers{0};
    for (uint64_t cycle = 0; cycle < max_cycles; ++cycle) {
        in.at(reset_index) = cycle < reset_cycles;
        const auto eval_start = std::chrono::steady_clock::now();
        const uint32_t *rp = r.data(), *sp = s.data();
        const uint8_t *np = n.data();
        size_t layer = 0;
        for (auto length : lengths) {
#ifdef NANHU_LARGE_LAYERS
            size_t count = 0;
            for (uint32_t j = 0; j < length; ++j)
                count += op_ss_3(*np++, 0, rp, in.data(), out.data() + count,
                    memory64.data(), memory8.data(), jt.data(), scratch1, scratch2);
#else
            uint16_t count = 0;
            for (uint32_t j = 0; j < length; ++j)
                count += op_ss_3(*np++, count, rp, in.data(), out.data(),
                    memory64.data(), memory8.data(), jt.data(), scratch1, scratch2);
#endif
            if (count != expected_outputs.at(layer++))
                throw std::runtime_error("RU output count differs from decoded layer size");
            for (uint32_t j = 0; j < count; ++j) in.at(*sp++) = out.at(j);
        }
        if (retired) simulation_seconds +=
            std::chrono::duration<double>(std::chrono::steady_clock::now() - eval_start).count();
        if (np != n.data() + n.size() || rp != r.data() + r.size() || sp != s.data() + s.size())
            throw std::runtime_error("Tensor pointers do not match the model dimensions");
        if (cycle <= reset_cycles) continue;
        for (auto& e : ram_events) if (e.get(in, "en")) {
            if (e.get(in, "rIdx") >= (1ULL << 25) ||
                (e.get(in, "wen") && e.get(in, "wIdx") >= (1ULL << 25)))
                throw std::runtime_error("Access exceeds modeled 256 MiB RAM");
        }
        for (auto& e : sd_events) if (e.get(in, "ren") || e.get(in, "setAddr"))
            throw std::runtime_error("Checked program unexpectedly accesses SD");
        for (auto& e : exception_events) if (e.get(in, "io_intrNO") || e.get(in, "io_cause"))
            throw std::runtime_error("Checked program raises an unexpected architectural exception");
        std::vector<const Event*> active;
        for (auto& e : commits) if (e.get(in, "io_valid")) active.push_back(&e);
        std::sort(active.begin(), active.end(), [&](auto a, auto b) {
            return a->get(in, "io_index") < b->get(in, "io_index");
        });
        for (auto* event : active) {
            auto& e = *event;
            if (oracle.trapped) break;
            auto pc = e.get(in, "io_pc"), insn = e.get(in, "io_instr");
            if (!retired) {
                first_commit_cycle = cycle;
                measured_start = std::chrono::steady_clock::now();
                std::cerr << "First commit cycle " << cycle << " pc 0x" << std::hex << pc << std::dec << '\n';
            }
            if (pc != oracle.pc || insn != oracle.instruction()) {
                std::ostringstream msg;
                msg << "Commit mismatch cycle " << cycle << " pc=0x" << std::hex << pc
                    << " expected=0x" << oracle.pc << " instruction=0x" << insn
                    << " expected=0x" << oracle.instruction();
                throw std::runtime_error(msg.str());
            }
            if (e.get(in, "io_skip") || e.get(in, "io_isRVC") || e.get(in, "io_fpwen"))
                throw std::runtime_error("Unexpected skipped/compressed/floating commit");
            const unsigned fused = e.get(in, "io_special");
            if (fused > 1) throw std::runtime_error("Unexpected fusion encoding");
            if (trace.is_open()) trace << cycle << ' ' << std::hex << pc << ' ' << insn << std::dec << '\n';
            oracle.step();
            ++retired;
            const unsigned rd = (insn >> 7) & 31;
            if ((insn & 127) != 0x23 && (insn & 127) != 0x63 && (insn & 127) != 0x0f && (insn & 127) != 0x6b)
                defined_registers.insert(rd);
            if (fused) { oracle.step(); ++retired; }
        }
        if (retired && cycle == first_commit_cycle) first_commit_instructions = retired;
        if (!active.empty()) for (auto reg : defined_registers) {
            if (arch.get(in, "io_gpr_" + std::to_string(reg)) != oracle.reg[reg]) {
                std::ostringstream msg;
                msg << "Register mismatch cycle " << cycle << " x" << reg << " observed=0x"
                    << std::hex << arch.get(in, "io_gpr_" + std::to_string(reg))
                    << " expected=0x" << oracle.reg[reg];
                throw std::runtime_error(msg.str());
            }
        }
        if (trap.get(in, "io_valid")) {
            saw_trap = true; trap_code = trap.get(in, "io_code"); trap_pc = trap.get(in, "io_pc");
        }
        if (saw_trap && oracle.trapped) {
            const auto end = std::chrono::steady_clock::now();
            const double seconds = std::chrono::duration<double>(end - measured_start).count();
            measured_cycles = cycle - first_commit_cycle;
            Json result{{"status", "passed"}, {"design", design}, {"program", program_path},
                {"total_cycles", cycle + 1}, {"retired_instructions", retired},
                {"first_commit_cycle", first_commit_cycle}, {"measured_cycles", measured_cycles},
                {"checked_run_seconds", seconds}, {"simulation_seconds", simulation_seconds},
                {"cycles_per_second", measured_cycles / simulation_seconds},
                {"checked_cycles_per_second", measured_cycles / seconds},
                {"instructions_per_second", (retired-first_commit_instructions) / simulation_seconds}, {"trap_code", trap_code},
                {"trap_pc", trap_pc}, {"loop_count_observed", oracle.reg[7]},
                {"tensor_load_seconds", std::chrono::duration<double>(load_end - load_start).count()},
                {"correctness", "RV64I commit stream and defined integer registers checked"},
                {"timing", "evaluation of cycles after first commit, excluding oracle, trace, loading and reset; includes timer overhead"}};
            if (trap_code || oracle.trap_code || !oracle.reg[7] || trap_pc != oracle.pc - 4)
                throw std::runtime_error("Program reported failure or loop did not execute");
            std::cout << result.dump(2) << '\n';
            return 0;
        }
        if (cycle % 1000 == 0) std::cerr << "cycle " << cycle << ", retired " << retired << '\n';
    }
    throw std::runtime_error("Maximum cycles reached without checked success trap");
} catch (const std::exception& e) {
    std::cerr << "FAIL: " << e.what() << '\n'; return 1;
}
