#pragma once
#include <cstdint>
#include <climits>

#ifdef __CUDACC__
#define HD __host__ __device__
#else
#define HD
#endif

namespace nanhu {
struct Op { uint32_t arg, out, words, code; };
// Runtime-only layout: fixed scalar operands, or {arg offset, words, 0}.
struct FlatOp { uint32_t out, code, x, y, z; };
struct Layer { uint32_t begin, end, outputs, mem_begin; };
struct Error { uint32_t code = 0, node = 0; uint64_t detail = 0; };
struct Write { uint64_t address, value; uint32_t kind, active; };
static_assert(sizeof(Op) == 16 && sizeof(Layer) == 16, "portable descriptor layout");
static_assert(sizeof(FlatOp) == 20, "flat runtime descriptor layout");
struct View {
    const Op* ops;
    const uint32_t *args, *scatter, *jump;
    uint64_t *data, *out, *mem64;
    uint8_t* mem8;
    Write* writes;
    Error* error;
    uint64_t size64, size8;
    const uint32_t *memory_offsets = nullptr, *memory_later = nullptr;
    const FlatOp* flat_ops = nullptr;
};

HD inline void fail(Error* e, uint32_t code, uint32_t node, uint64_t detail) {
#ifdef __CUDA_ARCH__
    if (atomicCAS(&e->code, 0u, code) == 0) { e->node = node; e->detail = detail; }
#else
    if (!e->code) { e->code = code; e->node = node; e->detail = detail; }
#endif
}
HD inline uint64_t shl(uint64_t v, uint64_t s) { return s < 64 ? v << s : 0; }
HD inline uint64_t shr(uint64_t v, uint64_t s) { return s < 64 ? v >> s : 0; }
HD inline uint64_t sar(uint64_t v, uint64_t s) {
    const uint64_t sign = (v >> 63) ? UINT64_MAX : 0;
    if (s >= 64) return sign;
    if (!s) return v;
    return (v >> s) | (sign << (64 - s));
}
HD inline uint64_t extend(uint64_t v, unsigned bits, bool sign) {
    if (bits >= 64) return v;
    const uint64_t mask = (uint64_t(1) << bits) - 1;
    v &= mask;
    return sign && (v & (uint64_t(1) << (bits - 1))) ? v | ~mask : v;
}

// No in-place data writes here. Memory writes are staged until every read of
// this layer has completed. Ordered commit preserves opcode priority; optional
// direct CUDA commit requires disjoint active writes in each backing pool.
// Fixed removes unreachable variadic/memory cases from the inline specialization.
template<bool Fixed = false, class Operands>
HD inline void evaluate_operands(const View& v, uint32_t node, const Op o, const Operands& a) {
    uint64_t r = 0;
    switch (o.code) {
    case 0: case 1: {
        const unsigned bw = a[0] >> 1;
        if (!bw || bw > 64) { fail(v.error, 1, node, bw); return; }
        const uint64_t x = extend(v.data[a[1]], bw, a[0] & 1);
        const uint64_t y = extend(v.data[a[2]], bw, a[0] & 1);
        r = o.code == 0 ? x + y : x - y; break;
    }
    case 2: {
        const uint64_t x = v.data[a[1]], y = v.data[a[2]];
        if (a[0] <= 1) r = x * y; // Signed low64 product is identical modulo2^64.
        else if (!y) { fail(v.error, 2, node, 0); return; }
        else if (a[0] == 2) r = x / y;
        else if (a[0] == 3) r = x == (uint64_t(1) << 63) && y == UINT64_MAX ? x :
            static_cast<uint64_t>(static_cast<int64_t>(x) / static_cast<int64_t>(y));
        else if (a[0] == 4) r = x % y;
        else { fail(v.error, 3, node, a[0]); return; }
        break;
    }
    case 3:
        if (a[1] >= 64) { fail(v.error, 1, node, a[1]); return; }
        r = v.data[a[0]];
        if (a[1] < 63 && ((r >> a[1]) & 1)) r |= UINT64_MAX << (a[1] + 1);
        break;
    case 4: r = v.data[a[0]] < v.data[a[1]]; break;
    case 5: r = (v.data[a[0]] ^ (uint64_t(1) << 63)) < (v.data[a[1]] ^ (uint64_t(1) << 63)); break;
    case 6: r = v.data[a[0]] <= v.data[a[1]]; break;
    case 7: r = (v.data[a[0]] ^ (uint64_t(1) << 63)) <= (v.data[a[1]] ^ (uint64_t(1) << 63)); break;
    case 8: r = v.data[a[0]] == v.data[a[1]]; break;
    case 9: r = v.data[a[0]] != v.data[a[1]]; break;
    case 10: r = shl(v.data[a[0]], v.data[a[1]]); break;
    case 11: r = shr(v.data[a[0]], v.data[a[1]]); break;
    case 12: r = sar(v.data[a[0]], v.data[a[1]]); break;
    case 13: r = v.data[a[0]] & v.data[a[1]]; break;
    case 14: r = v.data[a[0]] | v.data[a[1]]; break;
    case 15: r = v.data[a[0]] ^ v.data[a[1]]; break;
    case 16: {
        uint64_t x = v.data[a[0]];
        x ^= x >> 32; x ^= x >> 16; x ^= x >> 8;
        x ^= x >> 4; x ^= x >> 2; x ^= x >> 1; r = x & 1; break;
    }
    case 17: r = shl(v.data[a[0]], a[2]) | v.data[a[1]]; break;
    case 18: r = shr(v.data[a[0]], a[1]) & v.data[a[2]]; break;
    case 19: r = v.data[v.data[a[0]] ? a[1] : a[2]]; break;
    case 20: r = v.data[a[0]]; break;
    case 21: {
        if constexpr (Fixed) { fail(v.error, 6, node, o.code); return; }
        else {
        const uint64_t addr = uint64_t(a[1]) + (v.data[a[3]] & a[2]);
        if (addr >= (a[0] == 0 ? v.size64 : v.size8)) { fail(v.error, 4, node, addr); return; }
        r = a[0] == 0 ? v.mem64[addr] : v.mem8[addr]; break;
        }
    }
    case 22: case 23:
        if constexpr (Fixed) { fail(v.error, 6, node, o.code); return; }
        else {
        for (uint32_t j = 0; j < a[0]; ++j)
            r = o.code == 22 ? r | v.data[a[1 + j]] : r ^ v.data[a[1 + j]];
        break;
        }
    case 24: r = v.data[v.jump[uint64_t(a[2]) + (v.data[a[0]] & a[1])]]; break;
    case 25: case 26: case 27: {
        if constexpr (Fixed) { fail(v.error, 6, node, o.code); return; }
        else {
        const uint64_t shift = v.data[a[1]] & v.data[a[2]];
        const uint64_t ws = shift / 64;
        const unsigned bs = shift & 63;
        if (o.code == 27 && !a[0]) { fail(v.error, 5, node, 0); return; }
        const uint64_t fill = o.code == 27 && (v.data[a[4 + a[0] - 1]] >> 63) ? UINT64_MAX : 0;
        for (uint32_t w = 0; w < o.words; ++w) {
            uint64_t low = 0, high = 0;
            if (o.code == 25) {
                if (w >= ws) {
                    const uint64_t src = w - ws;
                    low = src < a[0] ? v.data[a[4 + src]] : 0;
                    high = src > 0 && src - 1 < a[0] ? v.data[a[4 + src - 1]] : 0;
                }
                r = bs ? (low << bs) | (high >> (64 - bs)) : low;
            } else {
                const uint64_t src = uint64_t(w) + ws;
                low = src < a[0] ? v.data[a[4 + src]] : fill;
                high = src + 1 < a[0] ? v.data[a[4 + src + 1]] : fill;
                r = bs ? (low >> bs) | (high << (64 - bs)) : low;
            }
            v.out[o.out + w] = r;
        }
        return;
        }
    }
    case 28: {
        if constexpr (Fixed) { fail(v.error, 6, node, o.code); return; }
        else {
        Write write{};
        write.active = v.data[a[0]] != 0 && v.data[a[1]] != 0;
        write.kind = a[2];
        if (write.active) {
            write.address = uint64_t(a[3]) + (v.data[a[5]] & a[4]);
            write.value = v.data[a[6]];
            if (write.address >= (write.kind == 0 ? v.size64 : v.size8)) {
                fail(v.error, 4, node, write.address); write.active = 0;
            }
        }
        v.writes[o.out] = write;
        return;
        }
    }
    default: fail(v.error, 6, node, o.code); return;
    }
    v.out[o.out] = r;
}

HD inline void evaluate(const View& v, uint32_t node) {
    const Op o = v.ops[node];
    evaluate_operands(v, node, o, v.args + o.arg);
}

template<bool Flat = false>
HD inline uint32_t node_output(const View& v, uint32_t node) {
    if constexpr (Flat) return v.flat_ops[node].out;
    else return v.ops[node].out;
}
template<bool Flat = false>
HD inline void commit_memory(const View& v, Layer l) {
    for (uint32_t i = l.mem_begin; i < l.end; ++i) {
        const Write w = v.writes[node_output<Flat>(v, i)];
        if (w.active) {
            if (w.kind == 0) v.mem64[w.address] = w.value;
            else v.mem8[w.address] = static_cast<uint8_t>(w.value);
        }
    }
}
template<bool Flat = false>
HD inline bool is_final_memory_write(const View& v, Layer l, uint32_t index) {
    const Write w = v.writes[node_output<Flat>(v, index)];
    if (!w.active) return false;
    for (uint32_t later_index = index + 1; later_index < l.end; ++later_index) {
        const Write later = v.writes[node_output<Flat>(v, later_index)];
        if (later.active && later.address == w.address && ((later.kind == 0) == (w.kind == 0)))
            return false;
    }
    return true;
}
HD inline bool is_final_memory_write_candidate(const View& v, uint32_t writer,
        const uint32_t* offsets, const uint32_t* later_writers) {
    const Write w = v.writes[writer];
    if (!w.active) return false;
    for (uint32_t p = offsets[writer]; p < offsets[writer + 1]; ++p) {
        const Write later = v.writes[later_writers[p]];
        if (later.active && later.address == w.address && ((later.kind == 0) == (w.kind == 0)))
            return false;
    }
    return true;
}
} // namespace nanhu
