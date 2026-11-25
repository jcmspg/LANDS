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
    // Plains Flicker re-triggers another land's deploy ETB; its choices are made fresh (GDD §3.5)
    FLICKER_DESTROY,    // pick an opponent land to destroy
    FLICKER_RECLAIM,    // pick a graveyard card to take
    FLICKER_EXTRACT,    // pick a card from the opponent's hand to discard
};

struct PendingChoice {
    ChoiceType type{ChoiceType::NONE};
    int player{-1};                 // the only player who may answer; the options are private to them
    std::vector<Card> options;
};

} // namespace lands

#endif // LANDS_CHOICE_HPP
