#ifndef LANDS_ABILITY_ID_HPP
#define LANDS_ABILITY_ID_HPP

#include <cstdint>

namespace lands {

enum class AbilityId : uint8_t {
    NONE,

    // Deploy abilities
    FLICKER,
    DRAW,
    DESTROY,
    EXTRACT,
    RECLAIM,

    // Pitch abilities
    SAVE,
    COUNTER,
    SURVEIL,
    RUMMAGE,
    RAMP,
};

} // namespace lands

#endif // LANDS_ABILITY_ID_HPP
