#pragma once
#include <cstdint>

namespace PokerSolverUtils {

// Type aliases for clarity.
using ub8 = std::uint64_t;
using ub4 = std::uint32_t;
using ub1 = std::uint8_t;

// Returns 2^n as an unsigned 64-bit integer.
constexpr ub8 hashSize(int n) noexcept {
    return static_cast<ub8>(1) << n;
}

// Returns a bitmask with the lower n bits set (i.e. 2^n - 1).
constexpr ub8 hashMask(int n) noexcept {
    return hashSize(n) - 1;
}

// -----------------------------------------------------------------------------
// mix64: A reversible mixing function for three 64-bit values.
//
// This function is based on Bob Jenkins’ mix routines but refactored as an
// inline function. It modifies a, b, and c in place. The purpose is to mix
// the bits of the three inputs so that each bit of the result depends on
// every bit of the input. This is used in constructing a hash key from a
// combination of cards.
// -----------------------------------------------------------------------------
inline void mix64(ub8 &a, ub8 &b, ub8 &c) noexcept {
    a -= b; a -= c; a ^= (c >> 43);
    b -= c; b -= a; b ^= (a << 9);
    c -= a; c -= b; c ^= (b >> 8);
    a -= b; a -= c; a ^= (c >> 38);
    b -= c; b -= a; b ^= (a << 23);
    c -= a; c -= b; c ^= (b >> 5);
    a -= b; a -= c; a ^= (c >> 35);
    b -= c; b -= a; b ^= (a << 49);
    c -= a; c -= b; c ^= (b >> 11);
    a -= b; a -= c; a ^= (c >> 12);
    b -= c; b -= a; b ^= (a << 18);
    c -= a; c -= b; c ^= (b >> 22);
}

// -----------------------------------------------------------------------------
// Hash function prototypes.
// These functions convert a variable–length key (given as a pointer to bytes)
// into a 64–bit hash value. They differ slightly in their initialization seeds
// or the data types they accept.
// -----------------------------------------------------------------------------
ub8 hash1(const ub1* key, ub8 length, ub8 level);
ub8 hash2(const ub8* key, ub8 length, ub8 level);
ub8 hash3(const ub1* key, ub8 length, ub8 level);

} // namespace PokerSolverUtils
