#ifndef LANDS_ACTION_HPP
#define LANDS_ACTION_HPP

#include <cstdint>
#include <optional>
#include <vector>

namespace lands {

// Every accepted intent or timer event, in the shape the action log stores (GDD §6.3).
enum class ActionType : uint8_t {
    PLAY_LAND,          // card_id = the land; target_id = Destroy / Flicker / Reclaim target (a card id)
    PITCH_LAND,         // card_id = the pitched land; extra_id = Rummage's discard
    CAST_COUNTER,       // card_id = Island; extra_id = the additional card; target_id = a stack id
    CAST_SAVE,          // card_id = Plains; target_id = the land being protected
    PASS_PRIORITY,
    CHOOSE_CARD,        // card_id = the chosen option (Extract / flickered ETBs)
    CHOOSE_SURVEIL,    // to_graveyard + top_order partition the peeked cards
    END_TURN,
    CONCEDE,
    // Timer events (GDD §4.4). The server's clocks fire them; the random pick they make is
    // re-derived on replay from the match's seeded RNG, so only the event itself is logged.
    MAIN_PHASE_TIMEOUT,
    PRIORITY_TIMEOUT,
    CHOICE_TIMEOUT,
    DISCONNECT_FORFEIT,
};

struct Action {
    ActionType type{ActionType::END_TURN};
    int player{-1};
    uint32_t card_id{0};
    std::optional<uint32_t> extra_id;
    std::optional<uint32_t> target_id;
    std::vector<uint32_t> to_graveyard;
    std::vector<uint32_t> top_order;

    bool operator==(const Action& other) const = default;
};

} // namespace lands

#endif // LANDS_ACTION_HPP
