// StreetSetting.cpp
#include "StreetSetting.h"

namespace poker {

StreetSetting::StreetSetting(const std::vector<float>& betSizes,
                             const std::vector<float>& raiseSizes,
                             const std::vector<float>& donkSizes,
                             bool allin)
    : betSizes_(betSizes),
      raiseSizes_(raiseSizes),
      donkSizes_(donkSizes),
      allin_(allin)
{
    // Optional: Validate that the vectors are not empty if required.
}

const std::vector<float>& StreetSetting::getBetSizes() const {
    return betSizes_;
}

const std::vector<float>& StreetSetting::getRaiseSizes() const {
    return raiseSizes_;
}

const std::vector<float>& StreetSetting::getDonkSizes() const {
    return donkSizes_;
}

bool StreetSetting::isAllIn() const {
    return allin_;
}

} // namespace poker
