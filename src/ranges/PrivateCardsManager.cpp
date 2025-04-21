#include "ranges/PrivateCardsManager.h"
#include <unordered_map>

namespace PokerSolver {

namespace {

/* -------------------------------------------------------------------------
 *  A tiny helper: 52 × 52 unique index from an unordered pair of cards.
 *  Cards are given as their unique 0‑51 bytes.
 * ---------------------------------------------------------------------- */
inline int canonicalPairIndex(uint8_t c1, uint8_t c2) noexcept {
    return (c1 > c2) ? (c1 * 52 + c2) : (c2 * 52 + c1);
}

/* True when two 64‑bit board masks share at least one card. */
inline bool masksOverlap(uint64_t m1, uint64_t m2) noexcept {
    return (m1 & m2) != 0ULL;
}

} //‑‑ anonymous ----------------------------------------------------------------


/* ============== 1. Constructors =========================================== */

PrivateCardsManager::PrivateCardsManager()
        : playerCount_(0), initialBoard_(0ULL) {}

PrivateCardsManager::PrivateCardsManager(const std::vector<std::vector<PrivateCards>>& ranges,
                                         int playerCount,
                                         uint64_t initialBoard)
        : privateRanges_(ranges),
          playerCount_(playerCount),
          initialBoard_(initialBoard)
{
    if (static_cast<int>(privateRanges_.size()) != playerCount_) {
        throw std::invalid_argument("PrivateCardsManager: ranges size != playerCount");
    }

    /* ---------------------------------------------------------------------
     * Build a reverse‑lookup table so we can translate an index coming from
     * player A’s ordering into the corresponding index in player B’s ordering.
     *
     *   key   -> vector<int>(playerCount)  (‑1 if the combo doesn’t exist)
     *
     * where key is the canonical hash described above.
     * ------------------------------------------------------------------ */
    std::unordered_map<int, std::vector<int>> tmpTable;

    for (int p = 0; p < playerCount_; ++p) {
        const auto& range = privateRanges_[p];
        for (std::size_t i = 0; i < range.size(); ++i) {
            const PrivateCards& pc = range[i];
            int key = canonicalPairIndex(pc.card1().toByte(), pc.card2().toByte());

            auto& entry = tmpTable[key];
            if (entry.empty()) {                   // first time we see this key
                entry.assign(playerCount_, -1);    // initialise with “not present”
            }
            entry[p] = static_cast<int>(i);
        }
    }

    /* Convert the map into a dense vector‑of‑vector so look‑ups are O(1).      */
    cardIndexLUT_.assign(52 * 52, std::vector<int>(playerCount_, -1));
    for (const auto& kv : tmpTable) {
        cardIndexLUT_[kv.first] = kv.second;       // kv.first already canonical
    }

    updateRelativeProbabilities();                // normalise each range
}


/* ============== 2. Public interface  ===================================== */

const std::vector<PrivateCards>&
PrivateCardsManager::getPreflopCards(int player) const
{
    if (player < 0 || player >= playerCount_)
        throw std::out_of_range("getPreflopCards: player index out of range");
    return privateRanges_[player];
}


/* Translate index from `fromPlayer`’s ordering into `toPlayer`’s ordering.
 * Returns ‑1 if the exact two‑card combo is **absent** from the target range. */
int PrivateCardsManager::mapIndexBetweenPlayers(int fromPlayer,
                                                int toPlayer,
                                                int index) const
{
    if (fromPlayer < 0 || fromPlayer >= playerCount_ ||
        toPlayer   < 0 || toPlayer   >= playerCount_)
        throw std::out_of_range("mapIndexBetweenPlayers: player index out of range");

    const auto& srcRange = privateRanges_[fromPlayer];
    if (index < 0 || index >= static_cast<int>(srcRange.size()))
        throw std::out_of_range("mapIndexBetweenPlayers: hand index out of range");

    const PrivateCards& pc     = srcRange[index];
    int key = canonicalPairIndex(pc.card1().toByte(), pc.card2().toByte());

    return cardIndexLUT_[key][toPlayer];           // may legitimately be –1
}


/* Build initial reach‑probability vector:
 *   – 0         if the combo clashes with the *given* board mask
 *   – weight    otherwise.
 *
 * Caller can normalise if they wish; we just propagate raw weights. */
std::vector<float>
PrivateCardsManager::computeInitialReachProbabilities(int player,
                                                      uint64_t boardMask) const
{
    const auto& range = getPreflopCards(player);

    std::vector<float> probs(range.size(), 0.0f);
    for (std::size_t i = 0; i < range.size(); ++i) {
        uint64_t mask = range[i].toBoardMask();
        probs[i] = masksOverlap(mask, boardMask) ? 0.0f
                                                 : range[i].weight();
    }
    return probs;
}


/* Simple normalisation: for each player divide every weight by the player’s
 * total so the sum is 1.  (No opponent‑dependent terms here — see commentary.) */
void PrivateCardsManager::updateRelativeProbabilities()
{
    for (auto& range : privateRanges_) {
        float sum = std::accumulate(range.begin(), range.end(), 0.0f,
                                    [](float acc, const PrivateCards& pc) {
                                        return acc + pc.weight();
                                    });
        if (sum <= 0.f) continue;                 // avoid divide‑by‑zero
        for (auto& pc : range) {
            pc.setRelativeProbability(pc.weight() / sum);
        }
    }
}

} // namespace PokerSolver
