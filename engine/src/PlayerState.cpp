#include "lands/PlayerState.hpp"

namespace lands {

PlayerState::PlayerState() : hand_(), land_counts_() {}

void PlayerState::draw_from(Deck& deck) {
    hand_.push_back(deck.draw());
}

bool PlayerState::play_land(size_t hand_index) {
    if (hand_index >= hand_.size()) {
        return false;
    }
    Card card = hand_[hand_index];
    hand_.erase(hand_.begin() + hand_index);
    land_counts_[static_cast<size_t>(card.get_land_type())]++;
    return true;
}

size_t PlayerState::hand_size() const {
    return hand_.size();
}

const std::vector<Card>& PlayerState::hand() const {
    return hand_;
}

const std::array<uint8_t, 5>& PlayerState::land_counts() const {
    return land_counts_;
}

} // namespace lands