#ifndef LANDS_SNAPSHOT_HPP
#define LANDS_SNAPSHOT_HPP

#include "lands/AbilityId.hpp"
#include "lands/Choice.hpp"
#include "lands/GameEnd.hpp"
#include "lands/LandType.hpp"
#include "lands/PlayMode.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace lands {

// A per-player, masked view of a match (GDD §8.7): what that player may know and nothing else.
// Players are indexed 0 and 1 as everywhere in the engine; the protocol layer maps them to the
// GDD's 1 and 2.

struct CardView {
    uint32_t instance_id{0};
    LandType type{LandType::PLAINS};

    bool operator==(const CardView& other) const = default;
};

struct StackItemView {
    uint32_t stack_id{0};
    int pusher{-1};
    PlayMode kind{PlayMode::DEPLOY};
    AbilityId ability{AbilityId::NONE};
    CardView card;
    std::optional<uint32_t> target_instance_id;     // a land or graveyard card the item aims at
    std::optional<uint32_t> target_stack_id;        // a Counter's target
};

struct PlayerView {
    int id{-1};
    int afk_strikes{0};
    std::vector<CardView> hand;         // filled for the viewer only
    size_t hand_count{0};
    std::vector<CardView> battlefield;
    bool main_pitch_used{false};
    int deploys_remaining{0};
};

// The pending mid-resolution choice. Everyone sees who is choosing; only the chooser sees the
// options (they can reveal the opponent's hand or the top of the library).
struct ChoiceView {
    int holder{-1};
    ChoiceType type{ChoiceType::NONE};
    std::vector<CardView> options;
};

// How a deploy-lock or turn-cap ending was scored (GDD §3.6, §8.10).
struct TiebreakView {
    std::array<int, 2> distinct_types{0, 0};
    std::array<int, 2> total_lands{0, 0};
};

struct Snapshot {
    int viewer{-1};
    int turn_number{1};
    int active_player{0};
    int priority_holder{-1};            // -1 when no window is open
    std::optional<ChoiceView> choice;
    PlayerView you;
    PlayerView opponent;
    std::vector<StackItemView> stack;   // bottom first
    size_t library_count{0};
    std::optional<CardView> library_top_known;      // a Memory-Lapsed land on top is public (GDD §3.5)
    std::vector<CardView> graveyard;    // public, in the order cards arrived

    bool game_over{false};
    int winner_id{-1};                  // -1 while running and after a true draw
    GameEndReason end_reason{GameEndReason::NONE};
    bool is_draw{false};
    std::optional<TiebreakView> tiebreak;
};

} // namespace lands

#endif // LANDS_SNAPSHOT_HPP
