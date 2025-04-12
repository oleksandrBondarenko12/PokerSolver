#include "lookup8.h"
#include <cstring>  // for std::memcpy

namespace PokerSolverUtils {

// -----------------------------------------------------------------------------
// hash1: Process a key provided as a pointer to ub1 (bytes).
//
// The function initializes three 64–bit variables with a constant, the key length,
// and an external seed "level". It then processes the key in 24–byte blocks
// and mixes them using mix64(). Finally, it processes any leftover bytes,
// calls mix64() one last time, and returns the final hash value (c).
// -----------------------------------------------------------------------------
ub8 hash1(const ub1* key, ub8 length, ub8 level) {
    ub8 a, b, c;
    a = b = c = 0x9e3779b97f4a7c13ULL + (length << 2) + level;

    // Process blocks of 24 bytes.
    while (length > 24) {
        a += (static_cast<ub8>(key[0])       |
              static_cast<ub8>(key[1]) << 8  |
              static_cast<ub8>(key[2]) << 16 |
              static_cast<ub8>(key[3]) << 24 |
              static_cast<ub8>(key[4]) << 32 |
              static_cast<ub8>(key[5]) << 40 |
              static_cast<ub8>(key[6]) << 48 |
              static_cast<ub8>(key[7]) << 56);
        b += (static_cast<ub8>(key[8])        |
              static_cast<ub8>(key[9]) << 8   |
              static_cast<ub8>(key[10]) << 16 |
              static_cast<ub8>(key[11]) << 24 |
              static_cast<ub8>(key[12]) << 32 |
              static_cast<ub8>(key[13]) << 40 |
              static_cast<ub8>(key[14]) << 48 |
              static_cast<ub8>(key[15]) << 56);
        c += (static_cast<ub8>(key[16])       |
              static_cast<ub8>(key[17]) << 8  |
              static_cast<ub8>(key[18]) << 16 |
              static_cast<ub8>(key[19]) << 24 |
              static_cast<ub8>(key[20]) << 32 |
              static_cast<ub8>(key[21]) << 40 |
              static_cast<ub8>(key[22]) << 48 |
              static_cast<ub8>(key[23]) << 56);
        mix64(a, b, c);
        key += 24;
        length -= 24;
    }

    // Process remaining bytes (tail)
    c += length;
    switch (length) { // Fall-through switch to add remaining bytes.
        case 24: c += static_cast<ub8>(key[23]) << 56;
        case 23: c += static_cast<ub8>(key[22]) << 48;
        case 22: c += static_cast<ub8>(key[21]) << 40;
        case 21: c += static_cast<ub8>(key[20]) << 32;
        case 20: c += static_cast<ub8>(key[19]) << 24;
        case 19: c += static_cast<ub8>(key[18]) << 16;
        case 18: c += static_cast<ub8>(key[17]) << 8;
        case 17: c += static_cast<ub8>(key[16]);
        case 16: b += static_cast<ub8>(key[15]) << 56;
        case 15: b += static_cast<ub8>(key[14]) << 48;
        case 14: b += static_cast<ub8>(key[13]) << 40;
        case 13: b += static_cast<ub8>(key[12]) << 32;
        case 12: b += static_cast<ub8>(key[11]) << 24;
        case 11: b += static_cast<ub8>(key[10]) << 16;
        case 10: b += static_cast<ub8>(key[9]) << 8;
        case 9 : b += static_cast<ub8>(key[8]);
        case 8 : a += static_cast<ub8>(key[7]) << 56;
        case 7 : a += static_cast<ub8>(key[6]) << 48;
        case 6 : a += static_cast<ub8>(key[5]) << 40;
        case 5 : a += static_cast<ub8>(key[4]) << 32;
        case 4 : a += static_cast<ub8>(key[3]) << 24;
        case 3 : a += static_cast<ub8>(key[2]) << 16;
        case 2 : a += static_cast<ub8>(key[1]) << 8;
        case 1 : a += static_cast<ub8>(key[0]);
                break;
        default: break;
    }
    mix64(a, b, c);
    return c;
}

// -----------------------------------------------------------------------------
// hash2: Accepts a key as an array of ub8 values.
// We reinterpret the ub8 array as a byte sequence.
ub8 hash2(const ub8* key, ub8 length, ub8 level) {
    // The length here is in ub8 elements; convert to bytes.
    return hash1(reinterpret_cast<const ub1*>(key), length * sizeof(ub8), level);
}

// -----------------------------------------------------------------------------
// hash3: An alternative hash with a different initial seed.
// (This could be used to break symmetry or for multiple independent hashes.)
ub8 hash3(const ub1* key, ub8 length, ub8 level) {
    ub8 a, b, c;
    a = b = c = 0xDEADBEEFDEADBEEFULL + (length << 2) + level;
    while (length > 24) {
        a += (static_cast<ub8>(key[0])       |
              static_cast<ub8>(key[1]) << 8  |
              static_cast<ub8>(key[2]) << 16 |
              static_cast<ub8>(key[3]) << 24 |
              static_cast<ub8>(key[4]) << 32 |
              static_cast<ub8>(key[5]) << 40 |
              static_cast<ub8>(key[6]) << 48 |
              static_cast<ub8>(key[7]) << 56);
        b += (static_cast<ub8>(key[8])        |
              static_cast<ub8>(key[9]) << 8   |
              static_cast<ub8>(key[10]) << 16 |
              static_cast<ub8>(key[11]) << 24 |
              static_cast<ub8>(key[12]) << 32 |
              static_cast<ub8>(key[13]) << 40 |
              static_cast<ub8>(key[14]) << 48 |
              static_cast<ub8>(key[15]) << 56);
        c += (static_cast<ub8>(key[16])       |
              static_cast<ub8>(key[17]) << 8  |
              static_cast<ub8>(key[18]) << 16 |
              static_cast<ub8>(key[19]) << 24 |
              static_cast<ub8>(key[20]) << 32 |
              static_cast<ub8>(key[21]) << 40 |
              static_cast<ub8>(key[22]) << 48 |
              static_cast<ub8>(key[23]) << 56);
        mix64(a, b, c);
        key += 24;
        length -= 24;
    }
    c += length;
    switch (length) {
        case 24: c += static_cast<ub8>(key[23]) << 56;
        case 23: c += static_cast<ub8>(key[22]) << 48;
        case 22: c += static_cast<ub8>(key[21]) << 40;
        case 21: c += static_cast<ub8>(key[20]) << 32;
        case 20: c += static_cast<ub8>(key[19]) << 24;
        case 19: c += static_cast<ub8>(key[18]) << 16;
        case 18: c += static_cast<ub8>(key[17]) << 8;
        case 17: c += static_cast<ub8>(key[16]);
        case 16: b += static_cast<ub8>(key[15]) << 56;
        case 15: b += static_cast<ub8>(key[14]) << 48;
        case 14: b += static_cast<ub8>(key[13]) << 40;
        case 13: b += static_cast<ub8>(key[12]) << 32;
        case 12: b += static_cast<ub8>(key[11]) << 24;
        case 11: b += static_cast<ub8>(key[10]) << 16;
        case 10: b += static_cast<ub8>(key[9]) << 8;
        case 9 : b += static_cast<ub8>(key[8]);
        case 8 : a += static_cast<ub8>(key[7]) << 56;
        case 7 : a += static_cast<ub8>(key[6]) << 48;
        case 6 : a += static_cast<ub8>(key[5]) << 40;
        case 5 : a += static_cast<ub8>(key[4]) << 32;
        case 4 : a += static_cast<ub8>(key[3]) << 24;
        case 3 : a += static_cast<ub8>(key[2]) << 16;
        case 2 : a += static_cast<ub8>(key[1]) << 8;
        case 1 : a += static_cast<ub8>(key[0]);
                break;
        default: break;
    }
    mix64(a, b, c);
    return c;
}

} // namespace PokerSolverUtils
