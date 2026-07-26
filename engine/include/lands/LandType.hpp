#ifndef LANDS_LAND_TYPE_HPP
#define LANDS_LAND_TYPE_HPP

#include <cstdint>

#include "lands/AbilityId.hpp"
#include "lands/PlayMode.hpp"

namespace lands {

enum class LandType : uint8_t {
    PLAINS = 0,
    ISLAND = 1,
    SWAMP = 2,
    MOUNTAIN = 3,
    FOREST = 4,
    NUM_LAND_TYPES = 5,
};

struct LandDefinition {
    LandType type;
    AbilityId deploy_ability;
    AbilityId pitch_ability;
    PitchTiming pitch_timing;
};

} // namespace lands

#endif // LANDS_LAND_TYPE_HPP
