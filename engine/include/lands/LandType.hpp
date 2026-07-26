#ifndef LANDS_LAND_TYPE_HPP
#define LANDS_LAND_TYPE_HPP

#include <cstdint>


namespace lands {

enum class LandType : uint8_t {
    PLAINS = 0,
    ISLAND = 1,
    SWAMP = 2,
    MOUNTAIN = 3,
    FOREST = 4,
    NUM_LAND_TYPES = 5,
};


} // namespace lands

#endif // LANDS_LAND_TYPE_HPP