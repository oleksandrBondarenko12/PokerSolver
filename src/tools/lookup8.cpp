#include "lookup8.h" // Ensure PATH_TO_ is adjusted if needed

#include <cstring> // For memcpy, if needed for aligned reads (safer alternative)

namespace poker_solver {
namespace hashing {

uint64_t lookup8_hash(const uint8_t* key, size_t length, uint64_t initval) {
  uint64_t a, b, c;
  size_t len = length;

  // Set up the internal state
  a = b = initval;
  // The golden ratio; an arbitrary value
  c = 0x9e3779b97f4a7c13ULL;

  // Handle most of the key in 24-byte chunks
  while (len >= 24) {
    // Note: Original code used direct pointer addition and shifts.
    // Using memcpy is safer regarding strict aliasing rules in C++,
    // although potentially slightly slower if the compiler doesn't optimize
    // it well for aligned cases. For performance-critical code on known
    // platforms that permit it, the original shift/add logic could be
    // reinstated with careful casting (e.g., reinterpret_cast) and alignment
    // checks, but memcpy is preferred for general C++. Let's stick to the
    // original's bit manipulation logic for closer fidelity, assuming the
    // platform handles unaligned access or the caller ensures alignment if
    // performance is critical. We'll use direct array access.
    uint64_t k0 = (static_cast<uint64_t>(key[ 0])      ) |
                  (static_cast<uint64_t>(key[ 1]) <<  8) |
                  (static_cast<uint64_t>(key[ 2]) << 16) |
                  (static_cast<uint64_t>(key[ 3]) << 24) |
                  (static_cast<uint64_t>(key[ 4]) << 32) |
                  (static_cast<uint64_t>(key[ 5]) << 40) |
                  (static_cast<uint64_t>(key[ 6]) << 48) |
                  (static_cast<uint64_t>(key[ 7]) << 56);
    uint64_t k1 = (static_cast<uint64_t>(key[ 8])      ) |
                  (static_cast<uint64_t>(key[ 9]) <<  8) |
                  (static_cast<uint64_t>(key[10]) << 16) |
                  (static_cast<uint64_t>(key[11]) << 24) |
                  (static_cast<uint64_t>(key[12]) << 32) |
                  (static_cast<uint64_t>(key[13]) << 40) |
                  (static_cast<uint64_t>(key[14]) << 48) |
                  (static_cast<uint64_t>(key[15]) << 56);
    uint64_t k2 = (static_cast<uint64_t>(key[16])      ) |
                  (static_cast<uint64_t>(key[17]) <<  8) |
                  (static_cast<uint64_t>(key[18]) << 16) |
                  (static_cast<uint64_t>(key[19]) << 24) |
                  (static_cast<uint64_t>(key[20]) << 32) |
                  (static_cast<uint64_t>(key[21]) << 40) |
                  (static_cast<uint64_t>(key[22]) << 48) |
                  (static_cast<uint64_t>(key[23]) << 56);

    a += k0;
    b += k1;
    c += k2;
    mix(a, b, c);
    key += 24;
    len -= 24;
  }

  // Handle the last 23 bytes
  // The original uses fall-through switch, which is discouraged by Google style.
  // Let's rewrite it using explicit additions based on remaining length.
  c += length; // Add original length to the mix

  // We add bytes to a, b, c based on the remaining length 'len'.
  // The original switch statement essentially did this:
  // 'c' gets bytes 22 down to 17 (if len >= 17)
  // 'b' gets bytes 15 down to 8  (if len >= 9)
  // 'a' gets bytes 7 down to 0   (if len >= 1)
  // We need to be careful with shifts.

  // Add remaining bytes to c
  if (len >= 23) c += (static_cast<uint64_t>(key[22]) << 56);
  if (len >= 22) c += (static_cast<uint64_t>(key[21]) << 48);
  if (len >= 21) c += (static_cast<uint64_t>(key[20]) << 40);
  if (len >= 20) c += (static_cast<uint64_t>(key[19]) << 32);
  if (len >= 19) c += (static_cast<uint64_t>(key[18]) << 24);
  if (len >= 18) c += (static_cast<uint64_t>(key[17]) << 16);
  if (len >= 17) c += (static_cast<uint64_t>(key[16]) << 8);
  // The first byte of c is reserved for the length, added above.

  // Add remaining bytes to b
  if (len >= 16) b += (static_cast<uint64_t>(key[15]) << 56);
  if (len >= 15) b += (static_cast<uint64_t>(key[14]) << 48);
  if (len >= 14) b += (static_cast<uint64_t>(key[13]) << 40);
  if (len >= 13) b += (static_cast<uint64_t>(key[12]) << 32);
  if (len >= 12) b += (static_cast<uint64_t>(key[11]) << 24);
  if (len >= 11) b += (static_cast<uint64_t>(key[10]) << 16);
  if (len >= 10) b += (static_cast<uint64_t>(key[ 9]) << 8);
  if (len >=  9) b += (static_cast<uint64_t>(key[ 8])     );

  // Add remaining bytes to a
  if (len >=  8) a += (static_cast<uint64_t>(key[ 7]) << 56);
  if (len >=  7) a += (static_cast<uint64_t>(key[ 6]) << 48);
  if (len >=  6) a += (static_cast<uint64_t>(key[ 5]) << 40);
  if (len >=  5) a += (static_cast<uint64_t>(key[ 4]) << 32);
  if (len >=  4) a += (static_cast<uint64_t>(key[ 3]) << 24);
  if (len >=  3) a += (static_cast<uint64_t>(key[ 2]) << 16);
  if (len >=  2) a += (static_cast<uint64_t>(key[ 1]) << 8);
  if (len >=  1) a += (static_cast<uint64_t>(key[ 0])     );
  // case 0: nothing left to add

  mix(a, b, c);

  // Report the result
  return c;
}

} // namespace hashing
} // namespace poker_solver
