#ifndef LANDS_PLAYER_STATE_HPP
#define LANDS_PLAYER_STATE_HPP

#include "lands/Deck.hpp"

#include <cstdint>
#include <vector>
#include <array>

namespace lands {

class PlayerState {
    public:
        PlayerState();
        ~PlayerState() = default;

        void draw_from(Deck& deck);
        bool play_land(uint32_t card_id);   // false if the card is not in hand
        const std::vector<Card>& hand() const;
        const std::array<uint8_t, 5>& land_counts() const;
        size_t hand_size() const;

        void add_to_hand(const Card& card);
        bool has_card(uint32_t card_id) const;
        Card take_from_hand(uint32_t card_id);   // throws if the card is not in hand
        void add_to_board(const Card& card);
        const std::vector<Card>& battlefield() const;
        bool has_on_board(uint32_t card_id) const;
        Card remove_from_board(uint32_t card_id);   // throws if the card is not on the battlefield
    
    private:
        std::vector<Card> hand_;
        std::vector<Card> battlefield_;
        std::array<uint8_t, 5> land_counts_{}; // zeros 
};

} // namespace lands

#endif // LANDS_PLAYER_STATE_HPP