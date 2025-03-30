#ifndef POKER_GAMETREEBUILDINGSETTINGS_H
#define POKER_GAMETREEBUILDINGSETTINGS_H


#include "StreetSetting.h"
#include <stdexcept>
#include <string>

namespace poker {


class GameTreeBuildingSettings {
public:
    /** 
    * @brief Constructs a GameTreeBuildingSettings object.
    *
    * @param flopIP StreetSetting for the in-position player on the flop.
    * @param turnIP StreetSetting for the in-position player on the turn.
    * @param riverIP StreetSetting for the in-position player on the river.
    * @param flopOOP StreetSetting for the out-of-position player on the flop.
    * @param turnOOP StreetSetting for the out-of-position player on the turn.
    * @param riverOOP StreetSetting for the out-of-position player on the river.
    */
    GameTreeBuildingSettings(const StreetSetting& flopIP,
                             const StreetSetting& turnIP,
                             const StreetSetting& riverIP,
                             const StreetSetting& flopOOP,
                             const StreetSetting& turnOOP,
                             const StreetSetting& riverOOP);

    /**
     * @brief Retrieves the appropriate StreetSetting based on player and round.
     *
     * @param player Either "ip" (in position) or "oop" (out of position).
     * @param round The round as a string ("flop", "turn", or "river").
     * @return const StreetSetting& The corresponding StreetSetting.
     * 
     * @throws std::invalid_argument if an invalid player or round is specified.
     */
    const StreetSetting& getSetting(const std::string& player, const std::string& round) const;

    // Additional getters for individual settings.
    const StreetSetting& getFlopIP() const;
    const StreetSetting& getTurnIP() const;
    const StreetSetting& getRiverIP() const;
    const StreetSetting& getFlopOOP() const;
    const StreetSetting& getTurnOOP() const;
    const StreetSetting& getRiverOOP() const;


private:
    StreetSetting flop_ip_;
    StreetSetting turn_ip_;
    StreetSetting river_ip_;
    StreetSetting flop_oop_;
    StreetSetting turn_oop_;
    StreetSetting river_oop_;
};

} // namespace poker

#endif // POKER_GAMETREEBUILDINGSETTINGS
