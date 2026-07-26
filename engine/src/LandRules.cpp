#include "lands/LandRules.hpp"

#include <cstddef>
#include <stdexcept>

namespace lands {

namespace {

constexpr LandDefinition kLandDefinitions[] = {
    {LandType::PLAINS,   AbilityId::FLICKER, AbilityId::SAVE,     PitchTiming::Instant},
    {LandType::ISLAND,   AbilityId::DRAW,    AbilityId::COUNTER,  PitchTiming::Instant},
    {LandType::SWAMP,    AbilityId::EXTRACT, AbilityId::SURVEIL,  PitchTiming::MainPhase},
    {LandType::MOUNTAIN, AbilityId::DESTROY, AbilityId::RUMMAGE,  PitchTiming::MainPhase},
    {LandType::FOREST,   AbilityId::RECLAIM, AbilityId::RAMP,     PitchTiming::MainPhase},
};

static_assert(
    sizeof(kLandDefinitions) / sizeof(kLandDefinitions[0]) ==
        static_cast<std::size_t>(LandType::NUM_LAND_TYPES),
    "Land definition table must cover every land type");

} // namespace

const LandDefinition& get(LandType type) {
    const auto index = static_cast<std::size_t>(type);
    if (index >= static_cast<std::size_t>(LandType::NUM_LAND_TYPES)) {
        throw std::out_of_range("lands::get: invalid LandType");
    }
    return kLandDefinitions[index];
}

} // namespace lands
