#include "lands/LandGameMatch.hpp"

namespace lands {

LandGameMatch::LandGameMatch(uint64_t seed) {
    deck_.shuffle(seed);
    
    deal_to(players_[0], 5);
    deal_to(players_[1], 6);
}

size_t LandGameMatch::deck_size() const {
    return deck_.deck_size();
}

void LandGameMatch::deal_to(PlayerState& player, size_t num_cards) {
    for (int i = 0; i < num_cards; ++i) {
        player.draw_from(deck_);
    }
}

size_t LandGameMatch::hand_size(int player_index) const {
    return players_[player_index].hand_size();
}

size_t LandGameMatch::land_count(int player_index, LandType land_type) const {
    return players_[player_index].land_counts()[static_cast<size_t>(land_type)];
}

bool LandGameMatch::play_land(int player_index, size_t hand_index) {
    if (player_index < 0 || player_index >= 2) {
        return false;
    }
    if (hand_index >= players_[player_index].hand_size()) {
        return false;
    }
    if(!players_[player_index].play_land(hand_index)){
        return false;
    }
    WinEvaluator eval;
    const auto result = eval.evaluate(players_[player_index].land_counts());
    if (result != WinCondition::NONE) { // DOMAIN OR MONO -> NOT THE PLAYER INDEX
        winner_ = result;
    }
    return true;
    
}

WinCondition LandGameMatch::winner() const {
    return winner_;
}

} // namespace lands