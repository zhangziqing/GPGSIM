#define JSONPATH NANHU_TENSOR_ROOT "/json/"
#define TXTPATH NANHU_TENSOR_ROOT "/txt/"
#define YAMLPATH NANHU_TENSOR_ROOT "/yaml/"
#include NANHU_TUNED_HEADER
#include <array>
#include <random>

// Independent semantic check: unchanged OU calls vs wide-index grouped kernel,
// with all 29 opcodes, 64-bit boundary counts, unroll tails and wide shifts.
int main() {
    for (unsigned seed=0;seed<4;++seed) {
        std::mt19937_64 random(seed+17);
        std::vector<uint64_t> in(256), reference(600000,0x123), candidate(reference), ma(512),mb(ma);
        std::vector<uint8_t> m8a(512),m8b(m8a);
        for(auto& v:in)v=random();
        in[2]=7;in[3]=0x81000000000000feULL;in[4]=1;in[5]=~uint64_t(0);
        in[6]=0x0302010007080504ULL;in[7]=2;
        std::vector<uint32_t> jump{3,4,6,7},args,bounds;
        std::vector<uint64_t> sa,sb,sc,sd;
        uint32_t op_count=0;size_t outputs=0;
        for(unsigned op=0;op<29;++op) {
            const unsigned copies=5000+seed*3+(op%7);
            bounds.push_back(op_count);bounds.push_back(op_count+copies);op_count+=copies;
            for(unsigned k=0;k<copies;++k) {
                std::vector<uint32_t> one;
                if(op<=1)one={128,3,4};
                else if(op==2)one={k%5,3,4};
                else if(op==3)one={3,7};
                else if(op>=4 && op<=15)one={3,4};
                else if(op==16 || op==20)one={3};
                else if(op==17)one={3,4,7};
                else if(op==18)one={3,2,5};
                else if(op==19)one={4,3,6};
                else if(op==21)one={0,0,255,4};
                else if(op==22 || op==23)one={3,3,4,6};
                else if(op==24)one={4,3,0};
                else if(op>=25 && op<=27)one={3,4,5,130,3,6,7};
                else if(op==28)one={4,4,0,0,255,4,3};
                args.insert(args.end(),one.begin(),one.end());
                const uint32_t* rp=one.data();
                const auto n=op_ss_3(op,0,rp,in.data(),reference.data()+outputs,ma.data(),m8a.data(),jump.data(),sa,sb);
                if(op!=28)outputs+=n;
            }
        }
        const uint32_t *rp=args.data(),*fp=bounds.data();
        const size_t actual=op_ss_for_loop(rp,fp,in.data(),candidate.data(),mb.data(),m8b.data(),jump.data(),sc,sd);
        if(actual!=outputs || rp!=args.data()+args.size() || fp!=bounds.data()+bounds.size() ||
            !std::equal(candidate.begin(),candidate.begin()+outputs,reference.begin()) || ma!=mb || m8a!=m8b)
            throw std::runtime_error("Grouped kernel differs from unchanged OU evaluator");
        std::cout<<"PASS seed="<<seed<<" operations="<<op_count<<" output words="<<outputs<<'\n';
    }
}
