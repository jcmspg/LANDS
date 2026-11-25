#include "lands/Graveyard.hpp"

#include <algorithm>
#include <stdexcept>

namespace lands {



void Graveyard::add(Card card) {
    cards_.push_back(card);
}

const std::vector<Card>& Graveyard::cards() const {
    return cards_;
}

Card Graveyard::take_card(uint32_t card_id) {
    const auto it = std::find_if(cards_.begin(), cards_.end(), [card_id](const Card& card) {
        return card.get_identifier() == card_id;
    });
    if (it == cards_.end()) {
        throw std::out_of_range("Graveyard: card not in graveyard");
    }
    Card card = *it;
    cards_.erase(it);
    return card;
}

size_t Graveyard::size() const {
    return cards_.size();
}


} // namespace lands