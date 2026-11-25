#include "lands/PlayerState.hpp"

#include <algorithm>
#include <stdexcept>

namespace lands {

PlayerState::PlayerState() : hand_(), battlefield_(), land_counts_() {}

void PlayerState::draw_from(Deck& deck) {
    hand_.push_back(deck.draw());
}

bool PlayerState::play_land(uint32_t card_id) {
    if (!has_card(card_id)) {
        return false;
    }
    add_to_board(take_from_hand(card_id));
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

void PlayerState::add_to_hand(const Card& card) {
    hand_.push_back(card);
}

bool PlayerState::has_card(uint32_t card_id) const {
    return std::any_of(hand_.begin(), hand_.end(), [card_id](const Card& card) {
        return card.get_identifier() == card_id;
    });
}

Card PlayerState::take_from_hand(uint32_t card_id) {
    const auto it = std::find_if(hand_.begin(), hand_.end(), [card_id](const Card& card) {
        return card.get_identifier() == card_id;
    });
    if (it == hand_.end()) {
        throw std::out_of_range("PlayerState: card not in hand");
    }
    Card card = *it;
    hand_.erase(it);
    return card;
}

void PlayerState::add_to_board(const Card& card) {
    battlefield_.push_back(card);
    land_counts_[static_cast<size_t>(card.get_land_type())]++;
}

const std::vector<Card>& PlayerState::battlefield() const {
    return battlefield_;
}

bool PlayerState::has_on_board(uint32_t card_id) const {
    return std::any_of(battlefield_.begin(), battlefield_.end(), [card_id](const Card& card) {
        return card.get_identifier() == card_id;
    });
}

Card PlayerState::remove_from_board(uint32_t card_id) {
    const auto it = std::find_if(battlefield_.begin(), battlefield_.end(), [card_id](const Card& card) {
        return card.get_identifier() == card_id;
    });
    if (it == battlefield_.end()) {
        throw std::out_of_range("PlayerState: card not on the battlefield");
    }
    Card card = *it;
    battlefield_.erase(it);
    land_counts_[static_cast<size_t>(card.get_land_type())]--;
    return card;
}

} // namespace lands