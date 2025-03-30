#ifndef POKER_STREETSETTING_H
#define POKER_STREETSETTING_H


#include <vector>


namespace poker {


class StreetSetting {
public:
    /**
    * @brief Constructs a StreetSetting object.
    *
    * @param betSizes Vector of bet sizes for the street.
    * @param raiseSizes Vector of raise sizes for the street.
    * @param donkSizes Vector for donk bet sizes for the street.
    * @param allin Flag indicating if all-in actions are allowed.
    */
    StreetSetting(const std::vector<float>& betSizes,
                  const std::vector<float>& raiseSizes,
                  const std::vector<float>& donkSizes,
                  bool allin);

    // Getters for the parameters.
    const std::vector<float>& getBetSizes() const;
    const std::vector<float>& getRaiseSizes() const;
    const std::vector<float>& getDonkSizes() const;
    bool isAllIn() const;

private:
    std::vector<float> betSizes_;
    std::vector<float> raiseSizes_; 
    std::vector<float> donkSizes_;
    bool allin_;
};

} // namespace poker

#endif // POKER_STREETSETTING_H
