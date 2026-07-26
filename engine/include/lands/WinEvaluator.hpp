#ifndef LANDS_WIN_EVALUATOR_HPP
#define LANDS_WIN_EVALUATOR_HPP

#include "lands/LandType.hpp"

#include <cstdint>
#include <array>
#include <vector>
#include <string>
#include <unordered_map>
#include <bitset>

namespace lands {

static constexpr uint8_t MONO_WIN_COUNT = 5;

enum class WinCondition : uint8_t {
    NONE = 0,
    DOMAIN = 1,
    MONO = 2,
};
    
class WinEvaluator {
    public:
        WinEvaluator() = default;
        ~WinEvaluator() = default;

        WinCondition evaluate(const std::array<uint8_t, 5>& land_count) const;
};

} // namespace lands

#endif // LANDS_WIN_EVALUATOR_HPP