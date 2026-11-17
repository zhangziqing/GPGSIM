// Unchanged function extracted from upstream kernel_OU.cc; verification only.
// sourceSHA256 eacc28eea85d7a67f09bee0ca976a7eacc9b18ee1513a51ab5b5570dee2f84a8
#pragma once
#include <vector>
#include <cstdint>
#include <cstddef>
inline uint16_t op_ss_3(
    const uint8_t op_type,
    uint16_t counter,
    const uint32_t* __restrict& rptr,
    const uint64_t* __restrict databuf,
    uint64_t* __restrict output_buffer,
    uint64_t* __restrict memory_64,
    uint8_t* __restrict memory_8,
    const uint32_t* __restrict muxJT,
    std::vector<uint64_t>& scratch1,
    std::vector<uint64_t>& scratch2
) {
    #define LR(lhs, rhs) \
        uint32_t lhs = *rptr++;     \
        uint32_t rhs = *rptr++;

    switch (op_type) {
        case 0: 
            {
                uint64_t bwinfo = *rptr++;
                uint64_t bw = bwinfo >> 1;
                uint64_t shift = 64 - bw;
                uint8_t usint = bwinfo & 1;
            
                // uint64_t lhs = databuf[*rptr++];
                // uint64_t rhs = databuf[*rptr++];
                LR(l, r);
                uint64_t lhs = databuf[l];
                uint64_t rhs = databuf[r];
            
                if (bw == 64) {
                    output_buffer[counter] = lhs + rhs;
                } else {
                    uint64_t lhs_extended = usint
                        ? static_cast<uint64_t>(static_cast<int64_t>(lhs << shift) >> shift)
                        : (lhs << shift) >> shift;
            
                    uint64_t rhs_extended = usint
                        ? static_cast<uint64_t>(static_cast<int64_t>(rhs << shift) >> shift)
                        : (rhs << shift) >> shift;
            
                    output_buffer[counter] = lhs_extended + rhs_extended;
                }
            }
            return 1;
        case 1: 
            {
                uint64_t bwinfo = *rptr++;
                uint64_t bw = bwinfo >> 1;
                uint64_t shift = 64 - bw;
                uint8_t usint = bwinfo & 1;
            
                // uint64_t lhs = databuf[*rptr++];
                // uint64_t rhs = databuf[*rptr++];
                LR(l, r);
                uint64_t lhs = databuf[l];
                uint64_t rhs = databuf[r];
            
                if (bw == 64) {
                    output_buffer[counter] = lhs - rhs;
                } else {
                    uint64_t lhs_extended = usint
                        ? static_cast<uint64_t>(static_cast<int64_t>(lhs << shift) >> shift)
                        : (lhs << shift) >> shift;
            
                    uint64_t rhs_extended = usint
                        ? static_cast<uint64_t>(static_cast<int64_t>(rhs << shift) >> shift)
                        : (rhs << shift) >> shift;
            
                    output_buffer[counter] = lhs_extended - rhs_extended;
                }
            }
            return 1;
        case 2: 
            {
                // uint64_t lhs = databuf[*rptr++];
                // uint64_t rhs = databuf[*rptr++];
                uint64_t opType = *rptr++;
                LR(l, r);
                uint64_t lhs = databuf[l];
                uint64_t rhs = databuf[r];
                switch (opType) {
                    case 0: {
                        output_buffer[counter] = lhs * rhs;
                        break;
                    }
                    case 1: {
                        output_buffer[counter] = static_cast<uint64_t>(static_cast<int64_t>(lhs) * static_cast<int64_t>(rhs));
                        break;
                    }
                    case 2: {
                        output_buffer[counter] = lhs / rhs;
                        break;
                    }
                    case 3: {
                        output_buffer[counter] = static_cast<uint64_t>(static_cast<int64_t>(lhs) / static_cast<int64_t>(rhs));
                        break;
                    }
                    case 4: {
                        output_buffer[counter] = lhs % rhs;
                        break;
                    }
                }
            }
            return 1;
        case 3: 
            {
                size_t x = *rptr++;
                size_t shift = *rptr++;
                uint64_t val = databuf[x];
                uint64_t extend_bit = (val >> shift) & 1;
                if (extend_bit == 1) {
                    uint64_t nonzero_bit = 64 - (shift + 1);
                    uint64_t extend_mask = ((1ULL << nonzero_bit) - 1) << (shift + 1);
                    output_buffer[counter] = val | extend_mask;
                } else {
                    output_buffer[counter] = val;
                }
            }
            return 1;
        case 4: 
            {
                // uint64_t lhs = databuf[*rptr++];
                // uint64_t rhs = databuf[*rptr++];
                LR(l, r);
                uint64_t lhs = databuf[l];
                uint64_t rhs = databuf[r];
                output_buffer[counter] = lhs < rhs;
            }
            return 1;
        case 5:
            {
                LR(l, r);
                int64_t lhs = static_cast<int64_t>(databuf[l]);
                int64_t rhs = static_cast<int64_t>(databuf[r]);
                output_buffer[counter] = lhs < rhs;
            }
            return 1;
        case 6: 
            {
                // uint64_t lhs = databuf[*rptr++];
                // uint64_t rhs = databuf[*rptr++];
                LR(l, r);
                uint64_t lhs = databuf[l];
                uint64_t rhs = databuf[r];
                output_buffer[counter] = lhs <= rhs;
            }
            return 1;
        case 7:
            {
                LR(l, r);
                int64_t lhs = static_cast<int64_t>(databuf[l]);
                int64_t rhs = static_cast<int64_t>(databuf[r]);
                output_buffer[counter] = lhs <= rhs;
            }
            return 1;
        case 8:
            {
                // uint64_t lhs = databuf[*rptr++];
                // uint64_t rhs = databuf[*rptr++];
                LR(l, r);
                uint64_t lhs = databuf[l];
                uint64_t rhs = databuf[r];
                output_buffer[counter] = lhs == rhs;
            }
            return 1;
        case 9:
            {
                // uint64_t lhs = databuf[*rptr++];
                // uint64_t rhs = databuf[*rptr++];
                LR(l, r);
                uint64_t lhs = databuf[l];
                uint64_t rhs = databuf[r];
                output_buffer[counter] = lhs != rhs;
            }
            return 1;
        case 10:
            {
                LR(l, r);
                uint64_t val = databuf[l];
                uint64_t shift = databuf[r];
                output_buffer[counter] = val << shift;
            }
            return 1;
        case 11:
            {
                LR(l, r);
                uint64_t val = databuf[l];
                uint64_t shift = databuf[r];
                output_buffer[counter] = val >> shift;
            }
            return 1;
        case 12:
            {
                LR(l, r);
                int64_t val = static_cast<int64_t>(databuf[l]);
                uint64_t shift = databuf[r];
                output_buffer[counter] = static_cast<uint64_t>(val >> shift);
            }
            return 1;
        case 13:
            {
                // uint64_t lhs = databuf[*rptr++];
                // uint64_t rhs = databuf[*rptr++];
                LR(l, r);
                uint64_t lhs = databuf[l];
                uint64_t rhs = databuf[r];
                output_buffer[counter] = lhs & rhs;
            }
            return 1;
        case 14:
            {
                // uint64_t lhs = databuf[*rptr++];
                // uint64_t rhs = databuf[*rptr++];
                LR(l, r);
                uint64_t lhs = databuf[l];
                uint64_t rhs = databuf[r];
                output_buffer[counter] = lhs | rhs;
            }
            return 1;
        case 15: 
            {
                // uint64_t lhs = databuf[*rptr++];
                // uint64_t rhs = databuf[*rptr++];
                LR(l, r);
                uint64_t lhs = databuf[l];
                uint64_t rhs = databuf[r];
                output_buffer[counter] = lhs ^ rhs;
            }
            return 1;
        case 16:
            {
                uint64_t val = databuf[*rptr++];
                #if defined(__GNUC__) || defined(__clang__)
                            uint32_t ones = __builtin_popcountll(val);
                #else
                            uint32_t ones = 0;
                            while (val) {
                                ones += val & 1;
                                val >>= 1;
                            }
                #endif
                output_buffer[counter] = static_cast<uint64_t>(ones & 1);
            }
            return 1;
        case 17:
            {
                // uint32_t x = *rptr++;
                // uint32_t y = *rptr++;
                LR(x, y);
                uint32_t z = *rptr++;
                output_buffer[counter] = (databuf[x] << z) | databuf[y];
            }
            return 1;
        case 18:
            {
                // uint64_t x = *rptr++;
                // uint64_t y = *rptr++;
                LR(x, y);
                uint64_t z = *rptr++;
                output_buffer[counter] = (databuf[x] >> y) & databuf[z];
            }
            return 1;
        case 19:
            {
                const uint32_t* x = rptr++;
                const uint32_t* y = rptr++;
                const uint32_t* z = rptr++;
                output_buffer[counter] = databuf[*x] ? databuf[*y] : databuf[*z];
            }
            return 1;
        case 20:
            {
                output_buffer[counter] = databuf[*rptr++];
            }
            return 1;
        case 21:
            {
                size_t x = *rptr++;
                size_t y = *rptr++;
                size_t z_mask = *rptr++;

                size_t z = *rptr++;
                size_t tmp = y + (static_cast<size_t>(databuf[z]) & z_mask);
                if (__builtin_expect(x == 0, 0)) {
                    output_buffer[counter] = memory_64[tmp];
                } else {
                    output_buffer[counter] = static_cast<uint64_t>(memory_8[tmp]);
                }
            }
            return 1;
        case 22:
            {
                size_t x = *rptr++;
                uint64_t acc = 0;
                for (size_t j = 0; j < x; ++j) {
                    acc |= databuf[*rptr++];
                }
                output_buffer[counter] = acc;
            }
            return 1;
        case 23: 
            {
                size_t x = *rptr++;
                uint64_t acc = databuf[*rptr++];
                for (size_t j = 0; j < x-1; ++j) {
                    acc ^= databuf[*rptr++];
                }
                output_buffer[counter] = acc;
            }
            return 1;
        case 24:
            {
                size_t x = *rptr++;
                size_t x_mask = *rptr++;
                size_t y = *rptr++;
                size_t tmp = (databuf[x] & x_mask) + y;
                output_buffer[counter] = databuf[muxJT[tmp]];
            }
            return 1;
        case 25:
            {
                size_t n = *rptr++;
                size_t shift_idx = *rptr++;
                size_t shift_mask_idx = *rptr++;
                uint64_t shift_op = databuf[shift_idx] & databuf[shift_mask_idx];
                size_t output_bw = *rptr++;
                size_t min_words = (output_bw + 63) / 64;

                // resize scratch buffer for block indices
                scratch1.resize(n);
                for (size_t k = 0; k < n; ++k) {
                    scratch1[k] = databuf[*rptr++];
                }

                if (__builtin_expect(n == 0 || shift_op == 0, 0)) {
                    for (size_t w = 0; w < min_words; ++w) {
                        output_buffer[counter + w] = (w < n) ? scratch1[w] : 0ULL;
                    }
                    return min_words;
                }

                size_t word_shift = shift_op / 64;
                uint32_t bit_shift = shift_op & 63;

                size_t result_len = n + word_shift + 1;
                if (result_len < min_words) result_len = min_words;

                scratch2.assign(result_len, 0ULL);

                uint64_t carry = 0;
                for (size_t w = 0; w < n; ++w) {
                    uint64_t val = scratch1[w];
                    uint64_t shifted = val << bit_shift;
                    scratch2[w + word_shift] |= shifted | carry;
                    carry = (bit_shift == 0) ? 0 : (val >> (64 - bit_shift));
                }
                if (bit_shift != 0) {
                    scratch2[n + word_shift] |= carry;
                }

                for (size_t w = 0; w < min_words; ++w) {
                    output_buffer[counter + w] = scratch2[w];
                }
                return min_words;
            }
        case 26:
            {
                size_t input_len = *rptr++;
                size_t shift_idx = *rptr++;
                size_t shift_mask_idx = *rptr++;
                uint64_t shift = databuf[shift_idx] & databuf[shift_mask_idx];
                size_t output_bw = *rptr++;

                size_t word_shift = shift / 64;
                size_t bit_shift = shift & 63;
                size_t num_words = (output_bw + 63) / 64;

                scratch1.resize(input_len);
                for (size_t k = 0; k < input_len; ++k) {
                    scratch1[k] = databuf[*rptr++];
                }

                for (size_t w = 0; w < num_words; ++w) {
                    size_t src_idx = w + word_shift;
                    uint64_t low = (src_idx < input_len) ? scratch1[src_idx] : 0ULL;
                    uint64_t high = (bit_shift > 0 && (src_idx + 1) < input_len) ? scratch1[src_idx + 1] : 0ULL;
                    uint64_t val = (bit_shift == 0) ? low : ((low >> bit_shift) | (high << (64 - bit_shift)));
                    output_buffer[counter + w] = val;
                }
                return num_words;
            }
        case 27:
            {
                size_t input_len = *rptr++;
                size_t shift_idx = *rptr++;
                size_t shift_mask_idx = *rptr++;
                uint64_t shift = databuf[shift_idx] & databuf[shift_mask_idx];
                size_t output_bw = *rptr++;

                size_t word_shift = shift / 64;
                size_t bit_shift = shift & 63;
                size_t num_words = (output_bw + 63) / 64;

                scratch1.resize(input_len);
                for (size_t k = 0; k < input_len; ++k) {
                    scratch1[k] = databuf[*rptr++];
                }

                uint64_t sign_ext_val = (scratch1.back() & (1ULL << 63)) ? UINT64_MAX : 0;

                for (size_t w = 0; w < num_words; ++w) {
                    size_t src_idx = w + word_shift;
                    uint64_t low = (src_idx < input_len) ? scratch1[src_idx] : sign_ext_val;
                    uint64_t high = 0;
                    if (bit_shift > 0) {
                        high = ((src_idx + 1) < input_len) ? scratch1[src_idx + 1] : sign_ext_val;
                    }
                    int64_t val = (bit_shift == 0) ? low : (low >> bit_shift) | (high << (64 - bit_shift));
                    output_buffer[counter + w] = val;
                }
                return num_words;
            }
        case 28:
            {
                size_t u = *rptr++;
                size_t v = *rptr++;
                size_t w = *rptr++;
                size_t x = *rptr++;
                size_t y_mask = *rptr++;

                size_t y = *rptr++;
                size_t z = *rptr++;
                size_t tmp = x + (static_cast<size_t>(databuf[y]) & y_mask);
                if (databuf[u] != 0 && databuf[v] != 0) {
                    if (w == 0) {
                        memory_64[tmp] = databuf[z];
                    } else {
                        memory_8[tmp] = static_cast<uint8_t>(databuf[z]);
                    }
                }
            }
            return 1;
        default:
            return 0;
    }
}
#undef LR
