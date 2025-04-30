#include "compairer/Dic5Compairer.h" // Adjust path if necessary

#include <fstream>   // For std::ifstream, std::ofstream
#include <sstream>   // For std::stringstream, std::ostringstream
#include <stdexcept> // For std::runtime_error
#include <limits>    // For std::numeric_limits
#include <algorithm> // For std::min
#include <iostream>  // For error reporting / debug output
#include <optional>  // Needed for std::optional
#include <iomanip>   // For std::hex/std::dec output formatting
#include <filesystem> // For path manipulation (C++17)

// Use aliases for namespaces to reduce typing
namespace core = poker_solver::core;
namespace utils = poker_solver::utils;
namespace fs = std::filesystem; // Filesystem alias

namespace poker_solver {
namespace eval {

// --- Static Bitmask Helpers (Implementation remains the same) ---
constexpr uint64_t kSuit0Mask = 0x1111111111111ULL;
constexpr uint64_t kSuit1Mask = 0x2222222222222ULL;
constexpr uint64_t kSuit2Mask = 0x4444444444444ULL;
constexpr uint64_t kSuit3Mask = 0x8888888888888ULL;
constexpr uint64_t kRankHashSum0 = 0x5555555555555555ULL;
constexpr uint64_t kRankHashSum1 = 0x3333333333333333ULL;
constexpr uint64_t kRankHashSum2 = 0x0F0F0F0F0F0F0F0FULL;

uint64_t Dic5Compairer::RanksHash(uint64_t cards_mask) {
    cards_mask = (cards_mask & kRankHashSum0) + ((cards_mask >> 1) & kRankHashSum0);
    cards_mask = (cards_mask & kRankHashSum1) + ((cards_mask >> 2) & kRankHashSum1);
    return cards_mask;
}

bool Dic5Compairer::IsFlush(uint64_t cards_mask) {
    uint64_t count_mask = cards_mask;
    count_mask = count_mask - ((count_mask >> 1) & kRankHashSum0);
    count_mask = (count_mask & kRankHashSum1) + ((count_mask >> 2) & kRankHashSum1);
    count_mask = (count_mask + (count_mask >> 4)) & kRankHashSum2;
    count_mask = (count_mask + (count_mask >> 8));
    count_mask = (count_mask + (count_mask >> 16));
    count_mask = (count_mask + (count_mask >> 32));
    int pop_count = static_cast<int>(count_mask & 0x7F);
    if (pop_count != 5) return false;
    if ((cards_mask & kSuit0Mask) == cards_mask) return true;
    if ((cards_mask & kSuit1Mask) == cards_mask) return true;
    if ((cards_mask & kSuit2Mask) == cards_mask) return true;
    if ((cards_mask & kSuit3Mask) == cards_mask) return true;
    return false;
}

// --- Constructor ---

Dic5Compairer::Dic5Compairer(const std::string& dictionary_filepath)
    : dictionary_path_(dictionary_filepath) {

    // Derive cache path from text path (e.g., change .txt to .bin)
    cache_path_ = dictionary_path_;
    cache_path_.replace_extension(".bin");

    std::cout << "[INFO] Trying to load hand ranks from cache: "
              << cache_path_.string() << std::endl;

    if (LoadBinaryCache(cache_path_)) {
        std::cout << "[INFO] Successfully loaded ranks from binary cache." << std::endl;
         std::cout << "[INFO] Flush Ranks Map Size: " << flush_ranks_.size() << std::endl;
         std::cout << "[INFO] Non-Flush Ranks Map Size: " << non_flush_ranks_.size() << std::endl;
        return; // Success!
    }

    // Cache loading failed, try loading from text
    std::cout << "[INFO] Binary cache not found or invalid. Loading from text file: "
              << dictionary_path_.string() << std::endl;
    try {
        LoadDictionaryFromText(dictionary_path_.string());
        std::cout << "[INFO] Successfully loaded ranks from text file." << std::endl;
        std::cout << "[INFO] Flush Ranks Map Size: " << flush_ranks_.size() << std::endl;
        std::cout << "[INFO] Non-Flush Ranks Map Size: " << non_flush_ranks_.size() << std::endl;

        // Attempt to save the binary cache for next time
        std::cout << "[INFO] Attempting to save binary cache to: "
                  << cache_path_.string() << std::endl;
        if (!SaveBinaryCache(cache_path_)) {
            // Warning only, not a fatal error for this run
            std::cerr << "[WARNING] Failed to save binary cache file: "
                      << cache_path_.string() << std::endl;
        } else {
             std::cout << "[INFO] Successfully saved binary cache." << std::endl;
        }

    } catch (const std::exception& e) {
        std::ostringstream oss;
        oss << "Failed to load dictionary from text file '"
            << dictionary_path_.string() << "' after cache miss: " << e.what();
        throw std::runtime_error(oss.str());
    }
}

// --- Binary Cache Load/Save ---

bool Dic5Compairer::LoadBinaryCache(const fs::path& cache_filepath) {
    std::ifstream infile(cache_filepath, std::ios::binary);
    if (!infile.is_open()) {
        return false; // Cache file doesn't exist or cannot be opened
    }

    try {
        // Read flush map size
        size_t flush_size = 0;
        infile.read(reinterpret_cast<char*>(&flush_size), sizeof(flush_size));
        // Check stream state after read
        if (!infile || flush_size == 0) throw std::runtime_error("Invalid flush map size in cache.");
        flush_ranks_.clear();
        flush_ranks_.reserve(flush_size);
        // Read flush map data
        for (size_t i = 0; i < flush_size; ++i) {
            uint64_t key = 0;
            int value = 0;
            infile.read(reinterpret_cast<char*>(&key), sizeof(key));
            infile.read(reinterpret_cast<char*>(&value), sizeof(value));
            if (!infile) throw std::runtime_error("Read error in flush map data cache.");
            flush_ranks_[key] = value;
        }

        // Read non-flush map size
        size_t non_flush_size = 0;
        infile.read(reinterpret_cast<char*>(&non_flush_size), sizeof(non_flush_size));
        // Check stream state after read
         if (!infile || non_flush_size == 0) throw std::runtime_error("Invalid non-flush map size in cache.");
        non_flush_ranks_.clear();
        non_flush_ranks_.reserve(non_flush_size);
        // Read non-flush map data
        for (size_t i = 0; i < non_flush_size; ++i) {
            uint64_t key = 0;
            int value = 0;
            infile.read(reinterpret_cast<char*>(&key), sizeof(key));
            infile.read(reinterpret_cast<char*>(&value), sizeof(value));
             if (!infile) throw std::runtime_error("Read error in non-flush map data cache.");
            non_flush_ranks_[key] = value;
        }

        // Basic check: ensure maps were populated
        if (flush_ranks_.empty() || non_flush_ranks_.empty()) {
             throw std::runtime_error("Loaded empty maps from cache file.");
        }

    } catch (const std::exception& e) {
        std::cerr << "[WARNING] Failed to load binary cache '" << cache_filepath.string()
                  << "': " << e.what() << ". Falling back to text file." << std::endl;
        flush_ranks_.clear(); // Ensure maps are empty on failure
        non_flush_ranks_.clear();
        return false;
    }

    return true; // Successfully loaded from cache
}


bool Dic5Compairer::SaveBinaryCache(const fs::path& cache_filepath) const {
    std::ofstream outfile(cache_filepath, std::ios::binary | std::ios::trunc);
    if (!outfile.is_open()) {
        return false; // Cannot open file for writing
    }

    try {
        // Write flush map
        size_t flush_size = flush_ranks_.size();
        outfile.write(reinterpret_cast<const char*>(&flush_size), sizeof(flush_size));
        if (!outfile) throw std::runtime_error("Write error for flush map size.");
        for (const auto& pair : flush_ranks_) {
            outfile.write(reinterpret_cast<const char*>(&pair.first), sizeof(pair.first));
            outfile.write(reinterpret_cast<const char*>(&pair.second), sizeof(pair.second));
            if (!outfile) throw std::runtime_error("Write error for flush map data.");
        }

        // Write non-flush map
        size_t non_flush_size = non_flush_ranks_.size();
        outfile.write(reinterpret_cast<const char*>(&non_flush_size), sizeof(non_flush_size));
        if (!outfile) throw std::runtime_error("Write error for non-flush map size.");
        for (const auto& pair : non_flush_ranks_) {
            outfile.write(reinterpret_cast<const char*>(&pair.first), sizeof(pair.first));
            outfile.write(reinterpret_cast<const char*>(&pair.second), sizeof(pair.second));
            if (!outfile) throw std::runtime_error("Write error for non-flush map data.");
        }
    } catch (const std::exception& e) {
         std::cerr << "[ERROR] Failed during binary cache save '" << cache_filepath.string()
                   << "': " << e.what() << std::endl;
         // Attempt to remove potentially corrupted cache file
         try { fs::remove(cache_filepath); } catch (...) { /* Ignore remove errors */ }
         return false;
    }

    return true; // Successfully saved
}


// --- Dictionary Loading (from Text) ---

void Dic5Compairer::LoadDictionaryFromText(const std::string& filepath) {
    std::ifstream infile(filepath);
    if (!infile.is_open()) {
        std::ostringstream oss;
        oss << "Cannot open hand rank dictionary file: " << filepath;
        throw std::runtime_error(oss.str());
    }

    std::string line;
    int line_num = 0;
    int rank = 0;
    uint64_t hand_mask = 0;
    uint64_t rank_hash = 0;

    flush_ranks_.clear(); // Clear maps before loading text
    non_flush_ranks_.clear();
    flush_ranks_.reserve(6000);
    non_flush_ranks_.reserve(8000);

    while (std::getline(infile, line)) {
        line_num++;
        std::stringstream ss(line);
        std::string segment;
        std::vector<std::string> parts;
        while(std::getline(ss, segment, ',')) { parts.push_back(segment); }
        if (parts.size() != 2) continue; // Skip malformed lines silently now
        try { rank = std::stoi(parts[1]); } catch (...) { continue; }

        std::stringstream hand_ss(parts[0]);
        std::string card_str_part;
        std::vector<int> card_ints;
        card_ints.reserve(5);
        bool parse_error = false;
        while(std::getline(hand_ss, card_str_part, '-')) {
            std::string card_to_parse;
            if (card_str_part.length() == 3 && card_str_part.substr(0, 2) == "10") {
                card_to_parse = 'T'; card_to_parse += card_str_part[2];
            } else { card_to_parse = card_str_part; }
            std::optional<int> card_int_opt = core::Card::StringToInt(card_to_parse);
            if (!card_int_opt) { parse_error = true; break; }
            card_ints.push_back(card_int_opt.value());
        }
        if (parse_error || card_ints.size() != 5) continue;

        try { hand_mask = core::Card::CardIntsToUint64(card_ints); } catch (...) { continue; }

        if (IsFlush(hand_mask)) {
            flush_ranks_[hand_mask] = rank;
        } else {
            rank_hash = RanksHash(hand_mask);
            non_flush_ranks_[rank_hash] = rank; // Overwrite duplicates silently
        }
    }
     if (flush_ranks_.empty() && non_flush_ranks_.empty()) {
         throw std::runtime_error("Text dictionary file loaded successfully but resulted in empty rank maps.");
     }
}

// --- Private Lookup Helper ---
int Dic5Compairer::Lookup5CardRank(uint64_t hand_mask) const {
    bool is_flush = IsFlush(hand_mask);
    if (is_flush) {
        auto it = flush_ranks_.find(hand_mask);
        return (it != flush_ranks_.end()) ? it->second : kInvalidRank;
    } else {
        uint64_t rank_hash = RanksHash(hand_mask);
        auto it = non_flush_ranks_.find(rank_hash);
        return (it != non_flush_ranks_.end()) ? it->second : kInvalidRank;
    }
}

// --- Private Rank Calculation Helper ---
int Dic5Compairer::GetBestRankForCards(const std::vector<int>& cards) const {
    size_t num_cards = cards.size();
    if (num_cards < 5) return kInvalidRank;
    if (num_cards == 5) {
        try { uint64_t hand_mask = core::Card::CardIntsToUint64(cards); return Lookup5CardRank(hand_mask); }
        catch (...) { return kInvalidRank; }
    }
    int min_rank = kInvalidRank;
    std::vector<int> cards_copy = cards;
    utils::Combinations<int> combos(std::move(cards_copy), 5);
    for (const std::vector<int>& five_card_combo : combos.GetCombinations()) {
        try {
            uint64_t hand_mask = core::Card::CardIntsToUint64(five_card_combo);
            int current_rank = Lookup5CardRank(hand_mask);
            if (current_rank != kInvalidRank) min_rank = std::min(min_rank, current_rank);
        } catch (...) { continue; }
    }
    return min_rank;
}

// --- Public Interface Methods (Implementations remain the same) ---
int Dic5Compairer::GetHandRank(const std::vector<int>& private_hand, const std::vector<int>& public_board) const {
    if (private_hand.size() != 2) return kInvalidRank;
    std::vector<int> all_cards = private_hand; all_cards.insert(all_cards.end(), public_board.begin(), public_board.end());
    uint64_t combined_mask = 0; try { combined_mask = core::Card::CardIntsToUint64(all_cards); } catch (...) { return kInvalidRank; }
    int pop_count = 0; uint64_t count_mask = combined_mask; while(count_mask > 0) { count_mask &= (count_mask - 1); pop_count++; }
    if (pop_count != all_cards.size()) return kInvalidRank;
    return GetBestRankForCards(all_cards);
}
int Dic5Compairer::GetHandRank(uint64_t private_mask, uint64_t public_mask) const {
    if (core::Card::DoBoardsOverlap(private_mask, public_mask)) return kInvalidRank;
    uint64_t combined_mask = private_mask | public_mask; std::vector<int> all_cards = core::Card::Uint64ToCardInts(combined_mask);
    return GetBestRankForCards(all_cards);
}
core::ComparisonResult Dic5Compairer::CompareHands(const std::vector<int>& private_hand1, const std::vector<int>& private_hand2, const std::vector<int>& public_board) const {
    std::vector<int> hand1_combined = private_hand1; hand1_combined.insert(hand1_combined.end(), public_board.begin(), public_board.end());
    uint64_t mask1 = 0; try { mask1 = core::Card::CardIntsToUint64(hand1_combined); } catch (...) { return core::ComparisonResult::kTie; }
    std::vector<int> hand2_combined = private_hand2; hand2_combined.insert(hand2_combined.end(), public_board.begin(), public_board.end());
    uint64_t mask2 = 0; try { mask2 = core::Card::CardIntsToUint64(hand2_combined); } catch (...) { return core::ComparisonResult::kTie; }
    uint64_t private1_mask = 0; try { private1_mask = core::Card::CardIntsToUint64(private_hand1); } catch (...) { return core::ComparisonResult::kTie; }
    uint64_t private2_mask = 0; try { private2_mask = core::Card::CardIntsToUint64(private_hand2); } catch (...) { return core::ComparisonResult::kTie; }
    if (core::Card::DoBoardsOverlap(private1_mask, private2_mask)) { return core::ComparisonResult::kTie; }
    int rank1 = GetBestRankForCards(hand1_combined); int rank2 = GetBestRankForCards(hand2_combined);
    if (rank1 < rank2) { return core::ComparisonResult::kPlayer1Wins; } else if (rank2 < rank1) { return core::ComparisonResult::kPlayer2Wins; } else {
        if (rank1 == kInvalidRank && rank2 == kInvalidRank) { return core::ComparisonResult::kTie; } else if (rank1 == kInvalidRank) { return core::ComparisonResult::kPlayer2Wins; } else if (rank2 == kInvalidRank) { return core::ComparisonResult::kPlayer1Wins; } return core::ComparisonResult::kTie; }
}
core::ComparisonResult Dic5Compairer::CompareHands(uint64_t private_mask1, uint64_t private_mask2, uint64_t public_mask) const {
    if (core::Card::DoBoardsOverlap(private_mask1, public_mask) || core::Card::DoBoardsOverlap(private_mask2, public_mask) || core::Card::DoBoardsOverlap(private_mask1, private_mask2)) { return core::ComparisonResult::kTie; }
    int rank1 = GetHandRank(private_mask1, public_mask); int rank2 = GetHandRank(private_mask2, public_mask);
    if (rank1 < rank2) { return core::ComparisonResult::kPlayer1Wins; } else if (rank2 < rank1) { return core::ComparisonResult::kPlayer2Wins; } else {
        if (rank1 == kInvalidRank && rank2 == kInvalidRank) { return core::ComparisonResult::kTie; } else if (rank1 == kInvalidRank) { return core::ComparisonResult::kPlayer2Wins; } else if (rank2 == kInvalidRank) { return core::ComparisonResult::kPlayer1Wins; } return core::ComparisonResult::kTie; }
}


} // namespace eval
} // namespace poker_solver
