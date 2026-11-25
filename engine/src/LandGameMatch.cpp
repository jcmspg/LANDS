#include "lands/LandGameMatch.hpp"
#include "lands/LandRules.hpp"
#include "lands/WinEvaluator.hpp"


#include <algorithm>
#include <stdexcept>
#include <utility>

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

const Card* find_card(const std::vector<Card>& hand, uint32_t card_id) {
    const auto it = std::find_if(hand.begin(), hand.end(), [card_id](const Card& card) {
        return card.get_identifier() == card_id;
    });
    return it == hand.end() ? nullptr : &*it;
}

Deck shuffled_deck(uint64_t seed) {
    Deck deck;
    deck.shuffle(seed);
    return deck;
}

} // namespace

LandGameMatch::LandGameMatch(uint64_t seed) : LandGameMatch(shuffled_deck(seed)) {}

LandGameMatch::LandGameMatch(Deck deck) : deck_(std::move(deck)) {
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

const std::vector<Card>& LandGameMatch::battlefield(int player_index) const {
    require_player_index(player_index);
    return players_[player_index].battlefield();
}

bool LandGameMatch::valid_deploy_target(int caster_index, AbilityId ability,
                                        const std::optional<uint32_t>& target_card_id) const {
    switch (ability) {
        case AbilityId::DESTROY: {
            const PlayerState& opponent = players_[1 - caster_index];
            if (opponent.battlefield().empty()) {
                return !target_card_id.has_value();     // no land to hit: enters vanilla
            }
            return target_card_id.has_value() && opponent.has_on_board(*target_card_id);
        }
        case AbilityId::FLICKER: {
            const PlayerState& caster = players_[caster_index];
            if (caster.battlefield().empty()) {
                return !target_card_id.has_value();     // nothing to flicker: enters vanilla
            }
            return target_card_id.has_value() && caster.has_on_board(*target_card_id);
        }
        case AbilityId::RECLAIM: {
            const std::vector<Card>& cards = graveyard_.cards();
            if (cards.empty()) {
                return !target_card_id.has_value();     // nothing to take: enters vanilla
            }
            return target_card_id.has_value() && find_card(cards, *target_card_id) != nullptr;
        }
        default:
            return !target_card_id.has_value();
    }
}

bool LandGameMatch::play_land(int player_index, uint32_t card_id,
                              std::optional<uint32_t> target_card_id) {
    if (player_index < 0 || player_index >= 2) {
        return false;
    }
    if (player_index != active_player_) {
        return false;
    }
    if (action_blocked()) {     // a window or choice is pending: answer it first
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
    const LandDefinition& def =
        get(find_card(players_[player_index].hand(), card_id)->get_land_type());
    const AbilityId ability = ramped ? AbilityId::NONE : def.deploy_ability;  // ramped deploys enter vanilla
    if (!valid_deploy_target(player_index, ability, target_card_id)) {
        return false;
    }
    Card card = players_[player_index].take_from_hand(card_id);

    StackItem item {
        next_stack_id_++,
        player_index,
        PlayMode::DEPLOY,
        card,
        ability,
        0,
        target_card_id
    };

    if (ramped) {
        ramp_available_ = false;
    }

    // the deploy is used up even if it ends up countered
    played_land_ = true;

    stack_.push(std::move(item));
    open_priority_window(player_index);

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
    if (action_blocked()) {
        return false;
    }
    if (pitched_this_turn_) {
        return false;
    }
    PlayerState& player = players_[player_index];
    const Card* pitched = find_card(player.hand(), card_id);
    if (pitched == nullptr) {
        return false;
    }

    const LandDefinition& def = get(pitched->get_land_type());
    if (def.pitch_timing != PitchTiming::MainPhase) {
        return false;
    }
    // Island / Plains pitches are reactive and go through cast_counter / cast_save
    if (def.pitch_ability != AbilityId::RAMP && def.pitch_ability != AbilityId::RUMMAGE &&
        def.pitch_ability != AbilityId::SURVEIL) {
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

    // the pitch is used up even if it ends up countered
    pitched_this_turn_ = true;

    stack_.push(std::move(item));
    open_priority_window(player_index);

    return true;
}

bool LandGameMatch::cast_counter(int player_index, uint32_t island_card_id,
                                 uint32_t extra_card_id, uint32_t target_stack_id) {
    if (player_index < 0 || player_index >= 2) {
        return false;
    }
    if (player_index != priority_player_) {
        return false;
    }
    PlayerState& player = players_[player_index];
    const Card* island = find_card(player.hand(), island_card_id);
    if (island == nullptr || island_card_id == extra_card_id || !player.has_card(extra_card_id)) {
        return false;
    }
    if (get(island->get_land_type()).pitch_ability != AbilityId::COUNTER) {
        return false;
    }
    if (!stack_.contains(target_stack_id)) {
        return false;
    }

    // all checks passed: both cost cards leave the hand
    Card counter_card = player.take_from_hand(island_card_id);
    graveyard_.add(player.take_from_hand(extra_card_id));

    StackItem item {
        next_stack_id_++,
        player_index,
        PlayMode::PITCH,
        counter_card,
        AbilityId::COUNTER,
        target_stack_id
    };

    stack_.push(std::move(item));
    open_priority_window(player_index);

    return true;
}

bool LandGameMatch::cast_save(int player_index, uint32_t plains_card_id, uint32_t land_card_id) {
    if (player_index < 0 || player_index >= 2) {
        return false;
    }
    if (player_index != priority_player_) {
        return false;
    }
    PlayerState& player = players_[player_index];
    const Card* plains = find_card(player.hand(), plains_card_id);
    if (plains == nullptr || get(plains->get_land_type()).pitch_ability != AbilityId::SAVE) {
        return false;
    }
    // must be a land the caster controls that some stack item is targeting
    if (!player.has_on_board(land_card_id) || !stack_.targets_card(land_card_id)) {
        return false;
    }

    StackItem item {
        next_stack_id_++,
        player_index,
        PlayMode::PITCH,
        player.take_from_hand(plains_card_id),
        AbilityId::SAVE,
        0,
        land_card_id
    };

    stack_.push(std::move(item));
    open_priority_window(player_index);

    return true;
}

bool LandGameMatch::pass_priority(int player_index) {
    if (player_index < 0 || player_index >= 2) {
        return false;
    }
    if (player_index != priority_player_) {
        return false;
    }
    priority_player_ = -1;
    resolve_stack();
    return true;
}

bool LandGameMatch::action_blocked() const {
    return priority_player_ != -1 || pending_choice_.type != ChoiceType::NONE;
}

int LandGameMatch::choice_player() const {
    return pending_choice_.player;
}

ChoiceType LandGameMatch::choice_type() const {
    return pending_choice_.type;
}

const std::vector<Card>& LandGameMatch::choice_options() const {
    return pending_choice_.options;
}

bool LandGameMatch::choose_card(int player_index, uint32_t card_id) {
    const ChoiceType type = pending_choice_.type;
    const bool single_card_choice = type == ChoiceType::EXTRACT || type == ChoiceType::FLICKER_EXTRACT ||
                                    type == ChoiceType::FLICKER_DESTROY || type == ChoiceType::FLICKER_RECLAIM;
    if (!single_card_choice || player_index != pending_choice_.player) {
        return false;
    }
    if (find_card(pending_choice_.options, card_id) == nullptr) {
        return false;
    }

    PlayerState& opponent = players_[1 - player_index];
    switch (type) {
        case ChoiceType::EXTRACT:
        case ChoiceType::FLICKER_EXTRACT:
            // the chosen card leaves the opponent's hand and becomes public in the graveyard
            graveyard_.add(opponent.take_from_hand(card_id));
            break;
        case ChoiceType::FLICKER_DESTROY:
            graveyard_.add(opponent.remove_from_board(card_id));
            break;
        case ChoiceType::FLICKER_RECLAIM:
            players_[player_index].add_to_hand(graveyard_.take_card(card_id));
            break;
        default:
            break;
    }
    pending_choice_ = PendingChoice{};
    return true;
}

bool LandGameMatch::is_saved(const std::optional<uint32_t>& card_id) const {
    return card_id.has_value() &&
           std::find(saved_card_ids_.begin(), saved_card_ids_.end(), *card_id) != saved_card_ids_.end();
}

void LandGameMatch::retrigger_etb(int caster_index, AbilityId ability) {
    // resolves inline: no priority window opens, so a flickered ETB cannot be countered
    const PlayerState& opponent = players_[1 - caster_index];
    switch (ability) {
        case AbilityId::DRAW:
            if (deck_.deck_size() > 0) {
                players_[caster_index].draw_from(deck_);
            }
            break;
        case AbilityId::DESTROY:
            if (!opponent.battlefield().empty()) {
                pending_choice_ = PendingChoice{ChoiceType::FLICKER_DESTROY, caster_index, opponent.battlefield()};
            }
            break;
        case AbilityId::RECLAIM:
            if (!graveyard_.cards().empty()) {
                pending_choice_ = PendingChoice{ChoiceType::FLICKER_RECLAIM, caster_index, graveyard_.cards()};
            }
            break;
        case AbilityId::EXTRACT:
            if (!opponent.hand().empty()) {
                pending_choice_ = PendingChoice{ChoiceType::FLICKER_EXTRACT, caster_index, opponent.hand()};
            }
            break;
        default:
            break;  // Flicker re-triggering a Flicker fizzles (anti-loop rule)
    }
}

bool LandGameMatch::choose_surveil(int player_index, const std::vector<uint32_t>& to_graveyard,
                                   const std::vector<uint32_t>& library_top_order) {
    if (pending_choice_.type != ChoiceType::SURVEIL || player_index != pending_choice_.player) {
        return false;
    }

    // every peeked card must be named exactly once, and nothing else
    std::vector<uint32_t> named = to_graveyard;
    named.insert(named.end(), library_top_order.begin(), library_top_order.end());
    const std::vector<Card>& options = pending_choice_.options;
    if (named.size() != options.size()) {
        return false;
    }
    for (const Card& option : options) {
        if (std::count(named.begin(), named.end(), option.get_identifier()) != 1) {
            return false;
        }
    }

    // the peeked cards are the top of the library: take them off, then place them
    for (size_t i = 0; i < options.size(); ++i) {
        deck_.draw();
    }
    for (uint32_t card_id : to_graveyard) {
        graveyard_.add(*find_card(options, card_id));
    }
    for (auto it = library_top_order.rbegin(); it != library_top_order.rend(); ++it) {
        deck_.put_on_top(*find_card(options, *it));
    }
    pending_choice_ = PendingChoice{};
    return true;
}

int LandGameMatch::priority_player() const {
    return priority_player_;
}

const StackItem& LandGameMatch::stack_top() const {
    return stack_.top();
}

bool LandGameMatch::can_respond(int player_index) const {
    const PlayerState& player = players_[player_index];

    // Counter costs an Island plus one other card, so Island + 1 is the minimum hand
    const bool can_counter = player.hand_size() >= 2 &&
        std::any_of(player.hand().begin(), player.hand().end(), [](const Card& card) {
            return get(card.get_land_type()).pitch_ability == AbilityId::COUNTER;
        });
    if (can_counter) {
        return true;
    }

    // Save costs one Plains and needs a stack item aiming at one of the player's lands
    const bool holds_plains = std::any_of(player.hand().begin(), player.hand().end(), [](const Card& card) {
        return get(card.get_land_type()).pitch_ability == AbilityId::SAVE;
    });
    return holds_plains && std::any_of(player.battlefield().begin(), player.battlefield().end(),
                                       [this](const Card& land) {
        return stack_.targets_card(land.get_identifier());
    });
}

void LandGameMatch::open_priority_window(int pusher_index) {
    priority_player_ = -1;      // the previous holder (if any) just acted
    const int opponent = 1 - pusher_index;
    if (can_respond(opponent)) {
        priority_player_ = opponent;
    } else {
        resolve_stack();    // auto-pass (GDD §4.3): the opponent cannot act
    }
}

void LandGameMatch::resolve_stack() {
    while (!stack_.empty()) {
        resolve_top_action();
    }
}

void LandGameMatch::begin_turn() {
    active_player_ = (active_player_ + 1) % 2;
    if (active_player_ == 0) {
        ++turn_number_;
    }
    // GDD §3.1: only P1's very first turn skips the draw, and that turn never goes
    // through begin_turn(). P2 draws on its first turn; an empty library skips the draw.
    if (deck_.deck_size() > 0) {
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
    if (action_blocked()) {
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

bool LandGameMatch::ramp_available() const {
    return ramp_available_;
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

    const bool countered = std::find(countered_ids_.begin(), countered_ids_.end(),
                                     item.stack_id) != countered_ids_.end();

    if (item.mode == PlayMode::DEPLOY) {
        if (countered) {
            // Memory Lapse: the countered land goes on top of the shared library
            deck_.put_on_top(item.card);
        } else {
            players_[item.caster_id].add_to_board(item.card);

            // ETB (GDD §3.5); a ramped deploy carries AbilityId::NONE and enters vanilla
            switch (item.ability) {
                case AbilityId::DRAW:
                    // empty library: the draw does nothing but the land still enters
                    if (deck_.deck_size() > 0) {
                        players_[item.caster_id].draw_from(deck_);
                    }
                    break;
                case AbilityId::DESTROY: {
                    // fizzles if the target was saved or already left the battlefield;
                    // the Mountain itself still enters either way
                    PlayerState& opponent = players_[1 - item.caster_id];
                    if (item.target_card_id.has_value() && !is_saved(item.target_card_id) &&
                        opponent.has_on_board(*item.target_card_id)) {
                        graveyard_.add(opponent.remove_from_board(*item.target_card_id));
                    }
                    break;
                }
                case AbilityId::RECLAIM:
                    // any card, including ones the opponent pitched or lost (shared graveyard)
                    if (item.target_card_id.has_value() &&
                        find_card(graveyard_.cards(), *item.target_card_id) != nullptr) {
                        players_[item.caster_id].add_to_hand(graveyard_.take_card(*item.target_card_id));
                    }
                    break;
                case AbilityId::EXTRACT: {
                    // look at the opponent's hand and take one card; no effect if it is empty
                    const std::vector<Card>& opponent_hand = players_[1 - item.caster_id].hand();
                    if (!opponent_hand.empty()) {
                        pending_choice_ = PendingChoice{ChoiceType::EXTRACT, item.caster_id, opponent_hand};
                    }
                    break;
                }
                case AbilityId::FLICKER: {
                    // fizzles if the target was saved; otherwise re-trigger the land's own deploy ETB
                    const Card* land = item.target_card_id.has_value()
                        ? find_card(players_[item.caster_id].battlefield(), *item.target_card_id)
                        : nullptr;
                    if (land != nullptr && !is_saved(item.target_card_id)) {
                        retrigger_etb(item.caster_id, get(land->get_land_type()).deploy_ability);
                    }
                    break;
                }
                default:
                    break;
            }

            WinEvaluator eval;
            const auto result = eval.evaluate(players_[item.caster_id].land_counts());
            if (result != WinCondition::NONE) { // DOMAIN OR MONO -> NOT THE PLAYER INDEX
                winner_ = result;
            }
        }
    } else {
        // pitched cards never touch the battlefield, countered or not
        graveyard_.add(item.card);

        if (!countered) {
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
                case AbilityId::COUNTER:
                    countered_ids_.push_back(item.target_stack_id);
                    break;
                case AbilityId::SAVE:
                    // everything below this item on the stack that targets the land fizzles
                    saved_card_ids_.push_back(*item.target_card_id);
                    break;
                case AbilityId::SURVEIL: {
                    // peek at the top 2 cards; nothing to decide if the library is empty
                    std::vector<Card> peeked = deck_.peek_top(2);
                    if (!peeked.empty()) {
                        pending_choice_ = PendingChoice{ChoiceType::SURVEIL, item.caster_id, std::move(peeked)};
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }

    if (stack_.empty()) {
        countered_ids_.clear();
        saved_card_ids_.clear();
    }
}

} // namespace lands