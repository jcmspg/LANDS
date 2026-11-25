#ifndef LANDS_CHOICE_HPP
#define LANDS_CHOICE_HPP

#include "lands/Card.hpp"

#include <cstdint>
#include <vector>

namespace lands {

// A mid-resolution decision that depends on hidden information (GDD §8.6).
enum class ChoiceType : uint8_t {
    NONE,
    EXTRACT,    // Swamp deploy: pick one card from the opponent's hand to discard
    SURVEIL,    // Swamp pitch: bin any of the top cards of the library, order the rest
};

struct PendingChoice {
    ChoiceType type{ChoiceType::NONE};
    int player{-1};                 // the only player who may answer; the options are private to them
    std::vector<Card> options;
};

} // namespace lands

#endif // LANDS_CHOICE_HPP
