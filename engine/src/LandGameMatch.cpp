#include "lands/LandGameMatch.hpp"
#include "lands/LandRules.hpp"
#include "lands/WinEvaluator.hpp"


#include <algorithm>
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

const std::vector<Card>& LandGameMatch::hand(int player_index) const {
    require_player_index(player_index);
    return players_[player_index].hand();
}

bool LandGameMatch::play_land(int player_index, uint32_t card_id) {
    if (player_index < 0 || player_index >= 2) {
        return false;
    }
    if (player_index != active_player_) {
        return false;
    }
    if (!players_[player_index].has_card(card_id)) {
        return false;
    }
    // a second deploy is only legal if a Ramp pitch resolved this turn
    if (played_land_this_turn() && !ramp_available_) {
        return false;
    }
    const bool ramped = played_land_this_turn();
    Card card = players_[player_index].take_from_hand(card_id);

    const LandDefinition& def = get(card.get_land_type());

    StackItem item {
        next_stack_id_++,
        player_index,
        PlayMode::DEPLOY,
        card,
        ramped ? AbilityId::NONE : def.deploy_ability  // ramped deploys enter vanilla
    };

    if (ramped) {
        ramp_available_ = false;
    }

    stack_.push(std::move(item));
    resolve_top_action();

    played_land_ = true;

    return true;
    
}

bool LandGameMatch::pitch_land(int player_index, uint32_t card_id,
                               std::optional<uint32_t> extra_card_id) {
    if (player_index < 0 || player_index >= 2) {
        return false;
    }
    if (player_index != active_player_) {
        return false;
    }
    if (pitched_this_turn_) {
        return false;
    }
    PlayerState& player = players_[player_index];
    if (!player.has_card(card_id)) {
        return false;
    }

    const auto& hand = player.hand();
    const auto it = std::find_if(hand.begin(), hand.end(), [card_id](const Card& card) {
        return card.get_identifier() == card_id;
    });
    const LandDefinition& def = get(it->get_land_type());
    if (def.pitch_timing != PitchTiming::MainPhase) {
        return false;
    }
    // Surveil needs a choice prompt, which does not exist yet
    if (def.pitch_ability != AbilityId::RAMP && def.pitch_ability != AbilityId::RUMMAGE) {
        return false;
    }

    const bool needs_extra = def.pitch_ability == AbilityId::RUMMAGE;
    if (needs_extra != extra_card_id.has_value()) {
        return false;
    }
    if (extra_card_id.has_value() &&
        (*extra_card_id == card_id || !player.has_card(*extra_card_id))) {
        return false;
    }

    // all checks passed: from here on the pitch is committed
    Card card = player.take_from_hand(card_id);
    if (extra_card_id.has_value()) {
        graveyard_.add(player.take_from_hand(*extra_card_id));
    }

    StackItem item {
        next_stack_id_++,
        player_index,
        PlayMode::PITCH,
        card,
        def.pitch_ability
    };

    stack_.push(std::move(item));
    resolve_top_action();

    pitched_this_turn_ = true;

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
    pitched_this_turn_ = false;
    ramp_available_ = false;
    begin_turn();
    return true;
}

bool LandGameMatch::played_land_this_turn() const {
    return played_land_;
}

bool LandGameMatch::pitched_this_turn() const {
    return pitched_this_turn_;
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

    if (item.mode == PlayMode::PITCH) {
        // pitched cards never touch the battlefield
        graveyard_.add(item.card);

        switch (item.ability) {
            case AbilityId::RAMP:
                ramp_available_ = true;
                break;
            case AbilityId::RUMMAGE:
                // draw 2, or as many as the shared library still has
                for (int i = 0; i < 2 && deck_.deck_size() > 0; ++i) {
                    players_[item.caster_id].draw_from(deck_);
                }
                break;
            default:
                break;  // Save / Counter / Surveil are not wired up yet
        }
    }
}

} // namespace lands