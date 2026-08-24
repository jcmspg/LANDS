#include "lands/LandGameMatch.hpp"
#include "lands/LandRules.hpp"
#include "lands/WinEvaluator.hpp"


#include <stdexcept>

namespace lands {

namespace {

void require_player_index(int player_index) {
    if (player_index < 0 || player_index >= 2) {
        throw std::out_of_range("LandGameMatch: player_index out of range");
    }
}

void require_land_type(LandType land_type) {
    if (static_cast<size_t>(land_type) >= static_cast<size_t>(LandType::NUM_LAND_TYPES)) {
        throw std::out_of_range("LandGameMatch: LandType out of range");
    }
}

} // namespace

LandGameMatch::LandGameMatch(uint64_t seed) {
    deck_.shuffle(seed);
    
    deal_to(players_[0], 5);
    deal_to(players_[1], 6);
}

size_t LandGameMatch::deck_size() const {
    return deck_.deck_size();
}

void LandGameMatch::deal_to(PlayerState& player, size_t num_cards) {
    for (size_t i = 0; i < num_cards; ++i) {
        player.draw_from(deck_);
    }
}

size_t LandGameMatch::hand_size(int player_index) const {
    require_player_index(player_index);
    return players_[player_index].hand_size();
}

size_t LandGameMatch::land_count(int player_index, LandType land_type) const {
    require_player_index(player_index);
    require_land_type(land_type);
    return players_[player_index].land_counts()[static_cast<size_t>(land_type)];
}

bool LandGameMatch::play_land(int player_index, size_t hand_index) {
    if (player_index < 0 || player_index >= 2) {
        return false;
    }
    if (player_index != active_player_) {
        return false;
    }
    if (hand_index >= players_[player_index].hand_size()) {
        return false;
    }
    if (played_land_this_turn()) {
        return false;
    }
    if (!players_[player_index].play_land(hand_index)) {
        return false;
    }
    Card card = players_[player_index].take_from_hand(hand_index);

    const LandDefinition& def = get(card.get_land_type());

    StackItem item {
        next_stack_id_++,
        player_index,
        PlayMode::DEPLOY,
        card,
        def.deploy_ability
    };

    stack_.push(std::move(item));
    resolve_top_action();

    played_land_ = true;

    return true;
    
}

void LandGameMatch::begin_turn() {
    active_player_ = (active_player_ + 1) % 2;
    if (active_player_ == 0) {
        ++turn_number_;
    }
    if (turn_number_ >=2) {
        players_[active_player_].draw_from(deck_);
    }
}

bool LandGameMatch::end_turn(int player_index) {
    if (player_index < 0 || player_index >= 2) {
        return false;
    }
    if (player_index != active_player_) {
        return false;
    }
    played_land_ = false;
    begin_turn();
    return true;
}

bool LandGameMatch::played_land_this_turn() const {
    return played_land_;
}

int LandGameMatch::active_player() const {
    return active_player_;
}

int LandGameMatch::turn_number() const {
    return turn_number_;
}

WinCondition LandGameMatch::winner() const {
    return winner_;
}

size_t LandGameMatch::graveyard_size() const {
    return graveyard_.size();
}

size_t LandGameMatch::stack_size() const {
    return stack_.size();
}

void LandGameMatch::resolve_top_action() {
    StackItem item = stack_.pop();

    if (item.mode == PlayMode::DEPLOY) {
        players_[item.caster_id].add_to_board(item.card.get_land_type());

        WinEvaluator eval;
        const auto result = eval.evaluate(players_[item.caster_id].land_counts());
        if (result != WinCondition::NONE) { // DOMAIN OR MONO -> NOT THE PLAYER INDEX
            winner_ = result;
        }
    }

    //pitch ability -> ignore for now
}

} // namespace lands