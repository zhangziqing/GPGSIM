// Single master-cycle runner for the experimental pinned Nanhu adapter.
// Uses the upstream RU operation evaluator. No FESVR/SimDTM timing is included.
#define JSONPATH NANHU_TENSOR_ROOT "/json/"
#define TXTPATH NANHU_TENSOR_ROOT "/txt/"
#define YAMLPATH NANHU_TENSOR_ROOT "/yaml/"
#include "kernel.h"
#include "coremark_common.h"
#include "nanhu_tensor_layout.h"
#include <algorithm>
#include <iomanip>
#include <set>


#include <sys/mman.h>
#include <unistd.h>
#include <cerrno>
template<class T> void nanhu_huge_pages(const char* name,const std::vector<T>& data) {
    const uintptr_t page=uintptr_t(sysconf(_SC_PAGESIZE));
    const uintptr_t begin=(reinterpret_cast<uintptr_t>(data.data())+page-1)&~(page-1);
    const uintptr_t end=(reinterpret_cast<uintptr_t>(data.data())+data.size()*sizeof(T))&~(page-1);
    if(end<=begin || end-begin<2*1024*1024)return;
    int advised=madvise(reinterpret_cast<void*>(begin),end-begin,MADV_HUGEPAGE);
    int collapsed=madvise(reinterpret_cast<void*>(begin),end-begin,MADV_COLLAPSE);
    int error=collapsed ? errno : 0;
    std::cerr<<"THP "<<name<<" bytes="<<(end-begin)<<" advise="<<advised<<" collapse="<<collapsed<<" errno="<<error<<'\n';
}


// Owns an ordinary aligned allocation; THP advice is local to this process.
template<class T> class AlignedSignals {
    T* values_=nullptr;size_t size_=0,bytes_=0;
public:
    explicit AlignedSignals(std::vector<T> original):size_(original.size()) {
        constexpr size_t huge=2*1024*1024;
        bytes_=(size_*sizeof(T)+huge-1)&~(huge-1);
        void* allocated=nullptr;
        if(posix_memalign(&allocated,huge,bytes_))throw std::bad_alloc();
        values_=static_cast<T*>(allocated);
        int advised=madvise(values_,bytes_,MADV_HUGEPAGE);
        std::memset(values_,0,bytes_);
        std::copy(original.begin(),original.end(),values_);
        int collapsed=madvise(values_,bytes_,MADV_COLLAPSE);
        std::cerr<<"Aligned signals bytes="<<bytes_<<" advise="<<advised<<" collapse="<<collapsed<<" errno="<<(collapsed?errno:0)<<'\n';
    }
    ~AlignedSignals(){std::free(values_);}
    AlignedSignals(const AlignedSignals&)=delete;
    AlignedSignals& operator=(const AlignedSignals&)=delete;
    size_t size()const{return size_;}
    T* data(){return values_;}
    const T* data()const{return values_;}
    T& operator[](size_t i){return values_[i];}
    const T& operator[](size_t i)const{return values_[i];}
    T& at(size_t i){if(i>=size_)throw std::out_of_range("signal index");return values_[i];}
    const T& at(size_t i)const{if(i>=size_)throw std::out_of_range("signal index");return values_[i];}
    const T* begin()const{return values_;}
    const T* end()const{return values_+size_;}
};

using Json = nlohmann::json;

uint64_t anon_hugepages_kb() {
    std::ifstream smaps("/proc/self/smaps_rollup");
    std::string key, line;
    while (std::getline(smaps, line)) {
        if (line.rfind("AnonHugePages:", 0) == 0) {
            std::istringstream values(line);
            uint64_t kb = 0; values >> key >> kb; return kb;
        }
    }
    throw std::runtime_error("Cannot read AnonHugePages");
}


template<unsigned N,class Input,class Output> inline void tuned_scatter(const uint32_t*& sp,
    Input& in, const Output& out, size_t count) {
    size_t j=0;
    for (; j+N<=count; j+=N) {
        #pragma clang loop unroll(full)
        for (unsigned k=0;k<N;++k) in[*sp++] = out[j+k];
    }
    for (;j<count;++j) in[*sp++] = out[j];
}

template<class T> uint64_t fingerprint(const std::vector<T>& data) {
    uint64_t h=14695981039346656037ULL;
    for(auto v:data) {h^=uint64_t(v);h*=1099511628211ULL;} return h;
}
template<class Input> inline uint64_t live_fingerprint(const Input& data, const std::set<uint32_t>& dead) {
    uint64_t h=14695981039346656037ULL;
    for(size_t i=0;i<data.size();++i) if(!dead.count(i)) {h^=data[i];h*=1099511628211ULL;} return h;
}

Json read_json(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("Missing " + path);
    Json value; f >> value; return value;
}
struct Event {
    std::string module, prefix;
    std::map<std::string, size_t> ports;
    template<class Input> uint64_t get(const Input& data, const std::string& name) const {
        auto pin = ports.find(name);
        if (pin == ports.end()) throw std::runtime_error("Missing observed pin " + prefix + name);
        return data.at(pin->second);
    }
};

int main(int argc, char** argv) try {
    if (argc < 6) throw std::runtime_error("Usage: rteaal_nanhu DESIGN PROGRAM.BIN PRESERVED_FILE MAX_CYCLES [TRACE]");
    const std::string design = argv[1], program_path = argv[2], preserved = argv[3];
    const uint64_t nonreset_cycles = std::stoull(argv[4]);
    if(!nonreset_cycles)throw std::runtime_error("Cycle count must be positive");
    std::ifstream program_file(program_path, std::ios::binary);
    if (!program_file) throw std::runtime_error("Missing program");
    std::vector<uint8_t> program((std::istreambuf_iterator<char>(program_file)), {});
    auto load_start = std::chrono::steady_clock::now();
    AlignedSignals<uint64_t> in(load_txt_file("layerIn", design, loadSizeTFromJson(JSONPATH + design + "_S_shape.json")));
    AlignedSignals<uint32_t> r(loadVectorFromJson<uint32_t>(JSONPATH + design + "_rptr.json"));
    AlignedSignals<uint32_t> s(loadVectorFromJson<uint32_t>(JSONPATH + design + "_sptr.json"));
    auto n = loadVectorFromJson<uint8_t>(JSONPATH + design + "_unswizzled_nptr.json");
    auto lengths = loadVectorFromJson<uint32_t>(JSONPATH + design + "_unswizzled_ss_lens_ptr.json");
    auto inputs = parse_json_var_indices(JSONPATH "InputRegMap_" + design + ".json");
    auto outputs = parse_json_dtm_indices(JSONPATH "dtmSignal_" + design + ".json");
    AlignedSignals<uint32_t> jt(load_mux_file("muxJT", design));
    const size_t max_s = loadSizeTFromJson(JSONPATH + design + "_max_s.json");
#ifndef NANHU_LARGE_LAYERS
    // Do not silently truncate the upstream evaluator's 16-bit output counter.
    if (max_s > 65535) throw std::runtime_error("Upstream RU counter is 16-bit; layer width " + std::to_string(max_s));
    if (std::any_of(lengths.begin(), lengths.end(), [](auto x) { return x > 65535; }))
        throw std::runtime_error("Upstream RU operation count is 16-bit");
#endif
    std::vector<size_t> expected_outputs;
    auto layout_meta=read_json(NANHU_LAYOUT_FILE);
    for(auto l:layout_meta.at("layers")) expected_outputs.push_back(l.at("outputs").get<size_t>());
    const auto bounds=loadVectorFromJson<uint32_t>(JSONPATH + design + "_fptr.json");
    for(auto index:s) if(index>=in.size()) throw std::runtime_error("Scatter index exceeds data array");
    auto sizes = read_json(NANHU_METADATA_ROOT "/memory_sizes_" + design + ".json");
    AlignedSignals<uint64_t> out{std::vector<uint64_t>(max_s)};std::vector<uint64_t> scratch1,scratch2;
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
    Event trap, arch, fparch, csr;
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
        else if (event.module == "DifftestArchFpRegState") fparch = event;
        else if (event.module == "DifftestCSRState") csr = event;
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
    
    
    {std::ifstream smaps("/proc/self/smaps_rollup");std::string line;
     while(std::getline(smaps,line))if(line.rfind("AnonHugePages:",0)==0)std::cerr<<line<<'\n';}
    const uint64_t hugepages_after_load = anon_hugepages_kb();
    if (hugepages_after_load < 36 * 1024)
        throw std::runtime_error("Full 36 MiB THP coverage required for this sample");
    auto load_end = std::chrono::steady_clock::now();
    std::cerr << "Loaded " << n.size() << " operations, " << lengths.size() << " layers; "
              << ram_copies << " RAM banks; load "
              << std::chrono::duration<double>(load_end - load_start).count() << " s\n";

    CoremarkObserver observer(argv[5], argc > 6 ? argv[6] : "");
    auto csr_names=read_json(COREMARK_DIR "/csr-schema.json");
    std::vector<std::string> csr_ports=csr_names.get<std::vector<std::string>>();
    constexpr uint64_t reset_cycles=64;
    double simulation_seconds=0;
    auto run_start=std::chrono::steady_clock::now();
    for(uint64_t cycle=0;cycle<nonreset_cycles+reset_cycles;++cycle){
        in.at(reset_index)=cycle<reset_cycles;
        auto eval_start=std::chrono::steady_clock::now();
        const uint32_t *rp = r.data(), *sp = s.data();
        const uint32_t *fp = bounds.data();
        size_t layer = 0;
        for (auto length : lengths) {
            size_t count = op_ss_for_loop(rp, fp, in.data(), out.data(),
                memory64.data(), memory8.data(), jt.data(), scratch1, scratch2);
            if (count != expected_outputs.at(layer++))
                throw std::runtime_error("RU output count differs from decoded layer size");
            tuned_scatter<1>(sp,in,out,count);
        }

        if(cycle>=reset_cycles)simulation_seconds+=std::chrono::duration<double>(std::chrono::steady_clock::now()-eval_start).count();
        if (fp != bounds.data() + bounds.size() || rp != r.data() + r.size() || sp != s.data() + s.size())
            throw std::runtime_error("Tensor pointers do not match the model dimensions");
        for (auto& e : ram_events) if (e.get(in, "en")) {
            if (e.get(in, "rIdx") >= (1ULL << 25) ||
                (e.get(in, "wen") && e.get(in, "wIdx") >= (1ULL << 25)))
                throw std::runtime_error("Access exceeds modeled 256 MiB RAM");
        }

        if(cycle<reset_cycles)continue;
        for(auto& e:sd_events)if(e.get(in,"ren")||e.get(in,"setAddr"))throw std::runtime_error("Unexpected SD access");
        CoremarkState state;
        for(auto& e:commits)if(e.get(in,"io_valid")){
            state.commits.push_back({unsigned(e.get(in,"io_index")),unsigned(e.get(in,"io_special")),
                unsigned(e.get(in,"io_skip")),unsigned(e.get(in,"io_isRVC")),unsigned(e.get(in,"io_fpwen")),
                unsigned(e.get(in,"io_rfwen")),unsigned(e.get(in,"io_wdest")),unsigned(e.get(in,"io_wpdest")),
                e.get(in,"io_pc"),uint32_t(e.get(in,"io_instr"))});
        }
        for(unsigned i=0;i<32;++i){
            state.gpr[i]=arch.get(in,"io_gpr_"+std::to_string(i));
            state.fpr[i]=fparch.get(in,"io_fpr_"+std::to_string(i));
        }
        for(auto& port:csr_ports)state.csr.push_back(csr.get(in,port));
        for(auto& e:exception_events)state.exception={e.get(in,"io_intrNO"),e.get(in,"io_cause"),e.get(in,"io_exceptionPC"),e.get(in,"io_exceptionInst")};
        state.trap=trap.get(in,"io_valid");
        if(state.trap){state.trap_pc=trap.get(in,"io_pc");state.trap_code=trap.get(in,"io_code");}
        observer.observe(cycle,std::move(state),in.at(outputs.at("io_uart_out_valid")),in.at(outputs.at("io_uart_out_ch")));
        if((cycle+1-reset_cycles)%10000==0)std::cerr<<"cycles "<<cycle+1-reset_cycles<<" retired "<<observer.retired<<" eval_seconds "<<simulation_seconds<<" pc 0x"<<std::hex<<observer.last_pc<<std::dec<<'\n';
    }
    auto run_end=std::chrono::steady_clock::now();
    observer.finish();
    Json result=observer.result("RTeAAL-NU",nonreset_cycles,simulation_seconds,std::chrono::duration<double>(run_end-run_start).count());
    result["candidate"]="nu-aligned-all-s1";
    result["allocation_policy"]="MADV_HUGEPAGE before initialization; MADV_COLLAPSE after copy";
    result["anon_hugepages_kb_after_load"]=hugepages_after_load;
    result["anon_hugepages_kb_after_run"]=anon_hugepages_kb();
    result["program"]=program_path;
    result["tensor_load_seconds"]=std::chrono::duration<double>(load_end-load_start).count();
    std::cout<<result.dump(2)<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
