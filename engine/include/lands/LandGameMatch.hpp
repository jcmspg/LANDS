#ifndef LANDS_LAND_GAME_MATCH_HPP
#define LANDS_LAND_GAME_MATCH_HPP

#include "lands/PlayerState.hpp"
#include "lands/WinEvaluator.hpp"
#include "lands/Graveyard.hpp"

namespace lands {

class LandGameMatch {
    public:
        explicit LandGameMatch(uint64_t seed);   // shuffle + deal

        bool play_land(int player_index, size_t hand_index);

        WinCondition winner() const; // none until someone wins

        // getters for testing: deck size, hand sizes, land counts
        size_t deck_size() const;
        size_t hand_size(int player_index) const;
        size_t land_count(int player_index, LandType land_type) const;


        bool end_turn(int player_index);
        int active_player() const;
        int turn_number() const;
        bool played_land_this_turn() const;

        size_t graveyard_size() const;

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
        Graveyard graveyard_;
};

} // namespace lands

#endif // LANDS_LAND_GAME_MATCH_HPP