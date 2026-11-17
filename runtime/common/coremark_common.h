#pragma once
#include "json.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

struct CoremarkCommit {
    unsigned index, special, skip, compressed, floating, rfwen, wdest, wpdest;
    uint64_t pc;
    uint32_t instruction;
};
struct CoremarkState {
    std::vector<CoremarkCommit> commits;
    std::array<uint64_t, 32> gpr{}, fpr{};
    std::vector<uint64_t> csr;
    std::array<uint64_t, 4> exception{};
    bool trap = false;
    uint64_t trap_code = 0, trap_pc = 0;
};
extern CoremarkState coremark_state;

class CoremarkObserver {
    std::ofstream trace_;
    std::ifstream reference_;
    bool compare_;
public:
    uint64_t retired = 0, commit_events = 0, fused = 0, compressed = 0, skipped = 0;
    uint64_t first_commit = ~uint64_t(0), last_pc = 0, first_ram_commit = ~uint64_t(0);
    uint64_t checked_cycles = 0, exception_cycles = 0;
    bool saw_trap = false;
    uint64_t trap_pc = 0, trap_code = 0;
    std::string uart;
    CoremarkState final_state;
    CoremarkObserver(const std::string& trace, const std::string& reference = "")
        : trace_(trace), compare_(!reference.empty()) {
        if (!trace_) throw std::runtime_error("Cannot open trace " + trace);
        if (compare_) {
            reference_.open(reference);
            if (!reference_) throw std::runtime_error("Cannot open reference " + reference);
        }
    }
    void observe(uint64_t cycle, CoremarkState state, bool uart_valid, unsigned uart_ch) {
        std::sort(state.commits.begin(), state.commits.end(), [](auto a, auto b) { return a.index < b.index; });
        std::ostringstream row;
        row << std::hex << cycle << " C " << state.commits.size();
        for (auto e : state.commits) {
            if (e.special > 1) throw std::runtime_error("Unknown Nanhu fusion flag");
            if (first_commit == ~uint64_t(0)) first_commit = cycle;
            if (first_ram_commit == ~uint64_t(0) && e.pc >= 0x80000000) first_ram_commit = cycle;
            ++commit_events;
            retired += 1 + e.special;
            fused += e.special;
            compressed += e.compressed;
            skipped += e.skip;
            last_pc = e.pc;
            row << ' ' << e.index << ' ' << e.pc << ' ' << e.instruction << ' ' << e.special
                << ' ' << e.skip << ' ' << e.compressed << ' ' << e.floating << ' ' << e.rfwen
                << ' ' << e.wdest << ' ' << e.wpdest;
        }
        row << " G";
        for (auto v : state.gpr) row << ' ' << v;
        row << " F";
        for (auto v : state.fpr) row << ' ' << v;
        row << " S";
        for (auto v : state.csr) row << ' ' << v;
        row << " E";
        for (auto v : state.exception) row << ' ' << v;
        row << " T " << state.trap;
        if (state.trap) row << ' ' << state.trap_pc << ' ' << state.trap_code;
        row << " U " << uart_valid;
        if (uart_valid) { row << ' ' << uart_ch; uart.push_back(char(uart_ch)); }
        auto text = row.str();
        trace_ << text << '\n';
        if (compare_) {
            std::string expected;
            if (!std::getline(reference_, expected) || expected != text) {
                std::cerr << "Trace mismatch at master cycle " << cycle << "\nexpected: "
                          << expected << "\nobserved: " << text << '\n';
                throw std::runtime_error("Cycle/commit/GPR/FPR/CSR/exception/trap/UART trace mismatch");
            }
        }
        exception_cycles += state.exception[0] != 0 || state.exception[1] != 0;
        if (state.trap) {
            saw_trap = true; trap_pc = state.trap_pc; trap_code = state.trap_code;
            if (trap_code) throw std::runtime_error("Guest reports unsuccessful trap");
        }
        final_state = std::move(state);
        ++checked_cycles;
    }
    void finish() {
        trace_.flush();
        if (!trace_) throw std::runtime_error("Trace write failure");
        std::string extra;
        if (compare_ && std::getline(reference_, extra)) throw std::runtime_error("Reference has extra cycles");
    }
    nlohmann::json result(const std::string& simulator, uint64_t requested_cycles,
                          double evaluation_seconds, double run_seconds) const {
        return {{"status", "cycle_limit_reached"}, {"simulator", simulator},
            {"requested_nonreset_cycles", requested_cycles}, {"reset_cycles", 64},
            {"total_master_cycles", requested_cycles + 64}, {"measured_cycles", requested_cycles},
            {"observed_cycles", checked_cycles}, {"evaluation_seconds", evaluation_seconds},
            {"cycles_per_second", requested_cycles / evaluation_seconds},
            {"simulation_loop_wall_seconds", run_seconds},
            {"loop_cycles_per_second", requested_cycles / run_seconds},
            {"retired_instructions_including_fused", retired}, {"commit_events", commit_events},
            {"fused_extra_instructions", fused}, {"compressed_commit_events", compressed},
            {"skipped_commit_events", skipped}, {"first_commit_master_cycle", first_commit},
            {"first_ram_commit_master_cycle", first_ram_commit}, {"last_retired_pc", last_pc},
            {"guest_ipc_including_boot", double(retired) / requested_cycles},
            {"saw_trap", saw_trap}, {"trap_pc", trap_pc}, {"trap_code", trap_code},
            {"exception_cycles", exception_cycles}, {"uart", uart},
            {"final_gpr", final_state.gpr}, {"final_fpr", final_state.fpr},
            {"final_csr", final_state.csr}, {"online_reference_comparison", compare_},
            {"timing", "100000 non-reset master cycles for full run; evaluation only, includes boot/cache stalls; excludes loading/reset/trace/reference comparison"},
            {"validation_scope", "Cross-simulator cycle/commit/integer and FP registers/CSRs/exceptions/trap/UART; no independent ISA oracle for CoreMark"}};
    }
};
