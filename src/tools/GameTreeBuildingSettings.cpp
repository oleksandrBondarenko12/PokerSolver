#include "GameTreeBuildingSettings.h"

namespace poker {


GameTreeBuildingSettings::GameTreeBuildingSettings(const StreetSetting& flopIP,
                                                   const StreetSetting& turnIP,
                                                   const StreetSetting& riverIP,
                                                   const StreetSetting& flopOOP,
                                                   const StreetSetting& turnOOP,
                                                   const StreetSetting& riverOOP)
    : flop_ip_(flopIP),
      turn_ip_(turnIP),
      river_ip_(riverIP),
      flop_oop_(flopOOP),
      turn_oop_(turnOOP),
      river_oop_(riverOOP)
{

}

const StreetSetting& GameTreeBuildingSettings::getSetting(const std::string& player, const std::string& round) const {
    if (player == "ip") {
        if (round == "flop") {
            return flop_ip_;
        }
        else if (round == "turn") {
            return turn_ip_;
        }
        else if (round == "river") {
            return river_ip_;
        }
    }
    else if (player == "oop") {
        if (round == "flop") {
            return flop_oop_;
        }
        else if (round == "turn") {
            return turn_oop_;
        }
        else if (round == "river") {
            return river_oop_;
        }
    }
    throw std::invalid_argument("Invalid player or round in getSetting");
}

const StreetSetting& GameTreeBuildingSettings::getFlopIP() const {
    return flop_ip_;
}

const StreetSetting& GameTreeBuildingSettings::getTurnIP() const {
    return turn_ip_;
}

const StreetSetting& GameTreeBuildingSettings::getRiverIP() const {
    return river_ip_;
}

const StreetSetting& GameTreeBuildingSettings::getFlopOOP() const {
    return flop_oop_;
}

const StreetSetting& GameTreeBuildingSettings::getTurnOOP() const {
    return turn_oop_;
}

const StreetSetting& GameTreeBuildingSettings::getRiverOOP() const {
    return river_oop_;
}

} // namespace poker
