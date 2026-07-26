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
        bool play_land(size_t hand_index);
        const std::vector<Card>& hand() const;
        const std::array<uint8_t, 5>& land_counts() const;
        size_t hand_size() const;
    
    private:
        std::vector<Card> hand_;
        std::array<uint8_t, 5> land_counts_{}; // zeros 
};

} // namespace lands

#endif // LANDS_PLAYER_STATE_HPP