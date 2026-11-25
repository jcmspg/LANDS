#ifndef LANDS_LAND_GAME_MATCH_HPP
#define LANDS_LAND_GAME_MATCH_HPP

#include "lands/PlayerState.hpp"
#include "lands/WinEvaluator.hpp"
#include "lands/Graveyard.hpp"
#include "lands/ActionStack.hpp"

#include <optional>

namespace lands {

class LandGameMatch {
    public:
        explicit LandGameMatch(uint64_t seed);   // shuffle + deal

        bool play_land(int player_index, uint32_t card_id);

        // Main-phase pitch (Mountain / Swamp / Forest), max 1 per turn.
        // Rummage needs extra_card_id (the card discarded as cost); Ramp must
        // not be given one. Illegal pitches return false and change nothing.
        bool pitch_land(int player_index, uint32_t card_id,
                        std::optional<uint32_t> extra_card_id = std::nullopt);

        WinCondition winner() const; // none until someone wins

        // getters for testing: deck size, hand sizes, land counts
        size_t deck_size() const;
        size_t hand_size(int player_index) const;
        size_t land_count(int player_index, LandType land_type) const;
        const std::vector<Card>& hand(int player_index) const;


        bool end_turn(int player_index);
        int active_player() const;
        int turn_number() const;
        bool played_land_this_turn() const;
        bool pitched_this_turn() const;

        size_t graveyard_size() const;

        size_t stack_size() const;

        private:
        Deck deck_;
        void deal_to(PlayerState& player, size_t num_cards);
        PlayerState players_[2];
        WinCondition winner_{WinCondition::NONE};

        // turn related content
        void begin_turn();

        int active_player_{0};
        int turn_number_{1};
        bool played_land_{false}; // false until a land is played
        bool pitched_this_turn_{false}; // false until a main-phase pitch resolves
        bool ramp_available_{false};    // set by a resolved Ramp, spent on the extra deploy
        Graveyard graveyard_;

        ActionStack stack_;
        uint32_t next_stack_id_{1};
        void resolve_top_action();  // pop the top action and resolve it
};

} // namespace lands

#endif // LANDS_LAND_GAME_MATCH_HPP