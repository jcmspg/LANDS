#include "lands/WinEvaluator.hpp"
#include "lands/LandType.hpp"

#include <algorithm>
#include <numeric>

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

int WinEvaluator::tiebreak_winner(const std::array<uint8_t, 5>& player_one,
                                  const std::array<uint8_t, 5>& player_two) const {
    auto distinct_types = [](const std::array<uint8_t, 5>& counts) {
        return std::count_if(counts.begin(), counts.end(), [](uint8_t count) { return count > 0; });
    };
    auto total_lands = [](const std::array<uint8_t, 5>& counts) {
        return std::accumulate(counts.begin(), counts.end(), 0);
    };

    if (distinct_types(player_one) != distinct_types(player_two)) {
        return distinct_types(player_one) > distinct_types(player_two) ? 0 : 1;
    }
    if (total_lands(player_one) != total_lands(player_two)) {
        return total_lands(player_one) > total_lands(player_two) ? 0 : 1;
    }
    return -1;  // true draw
}

} // namespace lands