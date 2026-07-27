#include "lands/Graveyard.hpp"

namespace lands {



void Graveyard::add(Card card) {
    cards_.push_back(card);
}

const std::vector<Card>& Graveyard::cards() const {
    return cards_;
}

Card Graveyard::take_card(size_t index) {
    Card card = cards_.at(index);
    cards_.erase(cards_.begin() + static_cast<std::ptrdiff_t>(index));
    return card;
}

size_t Graveyard::size() const {
    return cards_.size();
}


} // namespace lands