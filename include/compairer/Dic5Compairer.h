#ifndef POKER_SOLVER_EVAL_DIC5_COMPAIRER_H_
#define POKER_SOLVER_EVAL_DIC5_COMPAIRER_H_

#include "compairer/Compairer.h" // Base class interface
#include "Card.h"      // For Card utilities
#include "Library.h"  // For Combinations
#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <filesystem> // Required for path manipulation (C++17)

namespace poker_solver {
namespace eval {

// Concrete implementation of Compairer using a pre-computed dictionary
// of 5-card hand ranks loaded from a file (with binary caching).
class Dic5Compairer : public core::Compairer {
 public:
  // --- Constants ---
  static constexpr int kInvalidRank = 999999;

  // --- Static Bitmask Helpers ---
  static uint64_t RanksHash(uint64_t cards_mask);
  static bool IsFlush(uint64_t cards_mask);

  // --- Constructor ---
  // Loads the 5-card hand rank dictionary. Tries loading from a binary cache
  // first (derived from dictionary_filepath, e.g., .bin), falls back to
  // parsing the text file and creating the cache if necessary.
  // Args:
  //   dictionary_filepath: Path to the *text* file containing 5-card hand ranks
  //                        (e.g., "five_card_strength.txt").
  // Throws:
  //   std::runtime_error if the dictionary cannot be loaded from either cache
  //                      or text file, or if cache creation fails.
  explicit Dic5Compairer(const std::string& dictionary_filepath);

  // --- Overridden Interface Methods ---
  core::ComparisonResult CompareHands(
      const std::vector<int>& private_hand1,
      const std::vector<int>& private_hand2,
      const std::vector<int>& public_board) const override;

  core::ComparisonResult CompareHands(uint64_t private_mask1,
                                        uint64_t private_mask2,
                                        uint64_t public_mask) const override;

  int GetHandRank(const std::vector<int>& private_hand,
                  const std::vector<int>& public_board) const override;

  int GetHandRank(uint64_t private_mask,
                  uint64_t public_mask) const override;

  // --- Helper Methods (Public for testing) ---
  int GetBestRankForCards(const std::vector<int>& cards) const;


 private:
   // --- Private Helper Methods ---
   // Loads ranks from the text dictionary file.
   void LoadDictionaryFromText(const std::string& filepath);
   // Loads ranks from the binary cache file. Returns true on success.
   bool LoadBinaryCache(const std::filesystem::path& cache_filepath);
   // Saves the loaded ranks to the binary cache file. Returns true on success.
   bool SaveBinaryCache(const std::filesystem::path& cache_filepath) const;
   // Performs the lookup for a specific 5-card hand mask.
   int Lookup5CardRank(uint64_t hand_mask) const;

   // --- Member Variables ---
   std::unordered_map<uint64_t, int> flush_ranks_;
   std::unordered_map<uint64_t, int> non_flush_ranks_;
   // Store the path for potential error messages or future use
   std::filesystem::path dictionary_path_;
   std::filesystem::path cache_path_;

};

} // namespace eval
} // namespace poker_solver

#endif // POKER_SOLVER_EVAL_DIC5_COMPAIRER_H_
