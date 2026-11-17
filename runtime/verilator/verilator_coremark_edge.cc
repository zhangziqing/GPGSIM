#include "VSimTop.h"
#include "VSimTop__Dpi.h"
#include "coremark_common.h"
#include <cstring>

static std::vector<uint64_t> memory(1ULL << 25);
extern "C" long long ram_read_helper(svBit en, long long index) {
    if (!en) return 0;
    if (uint64_t(index) >= memory.size()) throw std::runtime_error("RAM read out of bounds");
    return memory[index];
}
extern "C" void ram_write_helper(long long index, long long data, long long mask, svBit en) {
    if (!en) return;
    if (uint64_t(index) >= memory.size()) throw std::runtime_error("RAM write out of bounds");
    memory[index] = (memory[index] & ~uint64_t(mask)) | (uint64_t(data) & mask);
}
extern "C" void flash_read(int addr, long long* data) {
    const uint32_t index = uint32_t(addr) >> 3;
    *data = index == 0 ? 0x01f292930010029bULL : index == 1 ? 0x0000000000028067ULL : 0;
}
extern "C" void sd_read(int*) { throw std::runtime_error("Unexpected SD read"); }
extern "C" void sd_setaddr(int) { throw std::runtime_error("Unexpected SD address"); }
extern "C" void tl_log_write_helper(char, char, char, char, char, long long,
    long long, long long, long long, long long, long long, long long, long long, const char*) {}

int main(int argc, char** argv) try {
    if (argc < 4) throw std::runtime_error("Usage: verilator_coremark PROGRAM NONRESET_CYCLES TRACE [REFERENCE]");
    std::ifstream f(argv[1], std::ios::binary);
    if (!f) throw std::runtime_error("Missing program");
    std::vector<uint8_t> program((std::istreambuf_iterator<char>(f)), {});
    if (program.size() > memory.size() * 8) throw std::runtime_error("Program exceeds RAM");
    std::memcpy(memory.data(), program.data(), program.size());
    uint64_t cycles = std::stoull(argv[2]);
    if (!cycles) throw std::runtime_error("Cycle count must be positive");
    CoremarkObserver observer(argv[3], argc > 4 ? argv[4] : "");
    VSimTop dut;
    dut.clock = 0; dut.reset = 0;
    dut.io_logCtrl_log_begin = dut.io_logCtrl_log_end = dut.io_logCtrl_log_level = 0;
    dut.io_perfInfo_clean = dut.io_perfInfo_dump = 0; dut.io_uart_in_ch = 0;
    dut.eval();
    double seconds = 0;
    auto start = std::chrono::steady_clock::now();
    for (uint64_t cycle = 0; cycle < cycles + 64; ++cycle) {
        coremark_state.commits.clear(); coremark_state.trap = false; coremark_state.exception.fill(0);
        auto t = std::chrono::steady_clock::now();
        dut.clock = 0; dut.reset = cycle < 64; dut.eval();
        const bool uart_valid = dut.io_uart_out_valid;
        const unsigned uart_ch = dut.io_uart_out_ch;
        dut.clock = 1; dut.eval();
        if (cycle >= 64) {
            seconds += std::chrono::duration<double>(std::chrono::steady_clock::now() - t).count();
            observer.observe(cycle, coremark_state, uart_valid, uart_ch);
            if (uart_valid || dut.io_uart_out_valid) std::cerr << "UART phase cycle " << cycle << " before " << uart_valid << "/" << uart_ch << " after " << unsigned(dut.io_uart_out_valid) << "/" << unsigned(dut.io_uart_out_ch) << "\n";
        }
        if (cycle >= 64 && (cycle + 1 - 64) % 10000 == 0)
            std::cerr << "cycles " << cycle + 1 - 64 << " retired " << observer.retired
                      << " eval_seconds " << seconds << " pc 0x" << std::hex << observer.last_pc << std::dec << '\n';
    }
    auto end = std::chrono::steady_clock::now();
    observer.finish();
    auto result = observer.result("Verilator", cycles, seconds, std::chrono::duration<double>(end - start).count());
    result["program"] = argv[1]; result["verilator_threads"] = dut.threads();
    std::cout << result.dump(2) << '\n';
    return 0;
} catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
