#ifndef POKERSOLVER_DIC5COMPRAIRER_H
#define POKERSOLVER_DIC5COMPRAIRER_H

#include <string>
#include <vector>
#include <unordered_map>
#include "Compairer.h"    // Abstract interface with virtual getRank() & compare()
#include "Card.h"         // Our Card definition

namespace PokerSolver {

/**
 * @brief A concrete 5‐card compairer using a dictionary lookup.
 *
 * This class loads a resource file (precomputed lookup table)
 * that maps a unique key for a 5–card hand to an ordinal rank.
 * It uses a lookup function from lookup8 to compute the key from a sorted hand.
 */
class Dic5Compairer : public Compairer {
public:
    /**
     * @brief Construct a new Dic5Compairer object.
     * @param resourceFilePath The relative or absolute path to the resource file.
     */
    explicit Dic5Compairer(const std::string& resourceFilePath = "resources/compairer/five_card_strength.txt");

    /// Default virtual destructor.
    virtual ~Dic5Compairer() override = default;

    /**
     * @brief Returns the ordinal rank of a 5–card hand (lower is stronger).
     * @param hand The five cards to evaluate.
     * @return int The precomputed ordinal rank.
     * @throws std::invalid_argument if hand does not contain exactly 5 cards.
     * @throws std::runtime_error if the hand’s key is not found in the lookup table.
     */
    virtual int getRank(const std::vector<Card>& hand) const override;

    /**
     * @brief Compares two 5–card hands.
     * @param hand1 The first hand.
     * @param hand2 The second hand.
     * @return int Returns -1 if hand1 is stronger, 1 if hand2 is stronger, or 0 if equal.
     */
    virtual int compare(const std::vector<Card>& hand1, const std::vector<Card>& hand2) const override;

private:
    // The lookup table maps a unique 64-bit key to the hand's ordinal rank.
    std::unordered_map<uint64_t, int> lookupTable_;

    /// Helper function to load the resource file into lookupTable_.
    void loadResourceFile(const std::string& resourceFilePath);

    /**
     * @brief Computes the lookup key for a given sorted 5–card hand.
     *
     * Each card is converted into its unique byte value (0-51) and placed
     * into a vector. The lookup8 routine then mixes these bytes into a 64-bit key.
     *
     * @param sortedHand A vector of 5 cards (already sorted in canonical order).
     * @return uint64_t The computed key.
     */
    uint64_t computeHandKey(const std::vector<Card>& sortedHand) const;
};

} // namespace PokerSolver

#endif // POKERSOLVER_DIC5COMPRAIRER_H
