#include "lands/WinEvaluator.hpp"
#include "lands/LandType.hpp"

namespace lands {

WinCondition WinEvaluator::evaluate(const std::array<uint8_t, 5>& land_count) const {
    std::bitset<5> land_types_bitset;

    for (size_t i = 0; i < land_count.size(); ++i) {
        if (land_count[i] >= MONO_WIN_COUNT) {
            return WinCondition::MONO;
        }
        if (land_count[i] > 0) {
            land_types_bitset.set(i);
        }
    }

    // DOMAIN WIN : All 5 land types are present, all lands types are present at least once
    if (land_types_bitset.count() == static_cast<uint8_t>(LandType::NUM_LAND_TYPES) && land_types_bitset.all()) {
        return WinCondition::DOMAIN;
    }
    return WinCondition::NONE;
}

} // namespace lands