#ifndef POKERSOLVER_DIC5COMPRAIRER_H
#define POKERSOLVER_DIC5COMPRAIRER_H

#include <string>
#include <vector>
#include <unordered_map>
#include "Compairer.h"     // Abstract interface
#include "Card.h"          // Our Card definition

namespace PokerSolver {

/**
 * @brief A concrete Compairer using a 5-card dictionary lookup for 7-card evaluation.
 */
class Dic5Compairer : public Compairer {
public: // <-- Made public section explicit
    /**
     * @brief Construct a new Dic5Compairer object.
     * @param resourceFilePath The path to the 5-card rank resource file.
     */
    explicit Dic5Compairer(const std::string& resourceFilePath = "resources/compairer/five_card_strength.txt");

    /// Default virtual destructor.
    virtual ~Dic5Compairer() override = default;

    // --- Implementations of Compairer pure virtual functions ---

    /**
     * @brief Evaluates and returns the rank for a 7-card hand (2 private + 5 board).
     */
    int getRank(const std::vector<Card>& privateHand,
                const std::vector<Card>& board) const override; // CORRECT override

    /**
     * @brief Compares two 7-card hands (private1+board vs private2+board).
     */
    ComparisonResult compare(const std::vector<Card>& privateHand1,
                             const std::vector<Card>& privateHand2,
                             const std::vector<Card>& board) const override; // CORRECT override

    // --- Optional: Direct 5-card rank lookup (Moved to Public) ---
    // Useful for internal checks or if you ever need to rank exactly 5 cards.
    // Note: No 'override' keyword here as it doesn't override the base interface method.
    int getRank(const std::vector<Card>& hand) const; // MOVED TO PUBLIC

private:
    // --- Helper Methods and Data ---

    // The lookup table maps a unique 64-bit key (from 5 cards) to the hand's ordinal rank.
    std::unordered_map<uint64_t, int> lookupTable_;

    /// Helper function to load the resource file into lookupTable_.
    void loadResourceFile(const std::string& resourceFilePath);

    /**
     * @brief Computes the lookup key for a given sorted 5-card hand.
     * @param sortedHand A vector of 5 cards (must be sorted).
     * @return uint64_t The computed key.
     */
    uint64_t computeHandKey(const std::vector<Card>& sortedHand) const;

}; // End class Dic5Compairer

} // namespace PokerSolver

#endif // POKERSOLVER_DIC5COMPRAIRER_H
