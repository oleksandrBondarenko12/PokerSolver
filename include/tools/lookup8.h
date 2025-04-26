#ifndef POKER_SOLVER_HASHING_LOOKUP8_H_
#define POKER_SOLVER_HASHING_LOOKUP8_H_

#include <cstddef> // For size_t
#include <cstdint> // For uint64_t, uint8_t

namespace poker_solver {
namespace hashing {

// Original implementation by Bob Jenkins, January 1997. Public Domain.
// Adapted to C++ and Google Style Guide.
// Source: http://burtleburtle.net/bob/c/lookup8.c
// Note: The original author noted in 2009 that lookup3.c is generally faster
// for 64-bit results. Consider modern alternatives like MurmurHash3 or xxHash
// for new projects unless lookup8 compatibility is required.

// Computes the size of a hash table, generally a power of 2.
constexpr uint64_t HashTableSize(int n) {
  return static_cast<uint64_t>(1) << n;
}

// Computes the mask for a hash table size, assuming size is a power of 2.
constexpr uint64_t HashMask(int n) {
  return HashTableSize(n) - 1;
}

// Core mixing function for lookup8.
// Mixes three 64-bit values reversibly.
inline void mix(uint64_t& a, uint64_t& b, uint64_t& c) {
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

// Hashes a variable-length key into a 64-bit value.
// This is the most portable version, operating on bytes.
//
// Args:
//   key: Pointer to the byte array key.
//   length: The length of the key in bytes.
//   initval: The initial value (seed), can be any 64-bit value.
//
// Returns:
//   A 64-bit hash value.
uint64_t lookup8_hash(const uint8_t* key, size_t length, uint64_t initval = 0);

// Note: The original C code included hash2() and hash3() which were faster
// but less portable (requiring aligned uint64_t arrays or specific
// endianness). They are omitted here for better C++ practice and portability.
// If extreme performance on specific platforms is needed and constraints are
// acceptable, they could be adapted, but careful attention to alignment and
// strict aliasing rules in C++ is required.

} // namespace hashing
} // namespace poker_solver

#endif // POKER_SOLVER_HASHING_LOOKUP8_H_
