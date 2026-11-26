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

// While alive, actions applied on a timer event's behalf are not logged separately:
// the event itself is the log entry, and replay re-derives the random pick.
class ScopedLogPause {
    public:
        explicit ScopedLogPause(int& counter) : counter_(counter) { ++counter_; }
        ~ScopedLogPause() { --counter_; }
        ScopedLogPause(const ScopedLogPause&) = delete;
        ScopedLogPause& operator=(const ScopedLogPause&) = delete;

    private:
        int& counter_;
};

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

LandGameMatch::LandGameMatch(uint64_t seed, MatchConfig config)
    : LandGameMatch(shuffled_deck(seed), seed, config) {}

LandGameMatch::LandGameMatch(Deck deck, uint64_t rng_seed, MatchConfig config)
    : deck_(std::move(deck)), rng_(rng_seed ^ 0x9E3779B97F4A7C15ULL) {
    deal_to(players_[0], 5);
    deal_to(players_[1], config.second_player_gets_sixth_card ? 6 : 5);
    // without the compensation rule P1 takes the draw step of turn 1 like everyone else
    if (!config.first_player_skips_first_draw && deck_.deck_size() > 0) {
        players_[0].draw_from(deck_);
    }
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
    const Card* deployed = find_card(players_[player_index].hand(), card_id);
    if (deployed == nullptr) {
        return false;
    }
    // a second deploy is only legal if a Ramp pitch resolved this turn
    if (played_land_this_turn() && !ramp_available_) {
        return false;
    }
    const bool ramped = played_land_this_turn();
    const LandDefinition& def = get(deployed->get_land_type());
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

    record(Action{ActionType::PLAY_LAND, player_index, card_id, std::nullopt, target_card_id, {}, {}});
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

    record(Action{ActionType::PITCH_LAND, player_index, card_id, extra_card_id, std::nullopt, {}, {}});
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

    record(Action{ActionType::CAST_COUNTER, player_index, island_card_id, extra_card_id, target_stack_id, {}, {}});
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

    record(Action{ActionType::CAST_SAVE, player_index, plains_card_id, std::nullopt, land_card_id, {}, {}});
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
    record(Action{ActionType::PASS_PRIORITY, player_index, 0, std::nullopt, std::nullopt, {}, {}});
    priority_player_ = -1;
    resolve_stack();
    return true;
}

bool LandGameMatch::action_blocked() const {
    return game_over() || priority_player_ != -1 || pending_choice_.type != ChoiceType::NONE;
}

void LandGameMatch::record(Action action) {
    if (log_paused_ == 0) {
        action_log_.push_back(std::move(action));
    }
}

size_t LandGameMatch::pick_index(size_t count) {
    // modulo on the raw engine output keeps picks identical across standard libraries
    return static_cast<size_t>(rng_() % count);
}

int LandGameMatch::afk_strikes(int player_index) const {
    require_player_index(player_index);
    return afk_strikes_[player_index];
}

bool LandGameMatch::give_strike(int player_index) {
    if (++afk_strikes_[player_index] >= kMaxAfkStrikes) {
        end_game(GameEndReason::AFK_FORFEIT, 1 - player_index);
        return true;
    }
    return false;
}

bool LandGameMatch::main_phase_timeout(int player_index) {
    if (player_index < 0 || player_index >= 2 || player_index != active_player_ || action_blocked()) {
        return false;
    }
    record(Action{ActionType::MAIN_PHASE_TIMEOUT, player_index, 0, std::nullopt, std::nullopt, {}, {}});
    if (give_strike(player_index)) {
        return true;
    }

    ScopedLogPause pause(log_paused_);
    if (!deploy_due()) {
        return end_turn(player_index);      // the main phase simply runs out
    }

    // a uniformly random legal card, then a uniformly random legal target for it
    std::vector<uint32_t> card_ids;
    std::vector<Action> deploys;
    for (const Action& action : main_phase_actions(player_index)) {
        if (action.type == ActionType::PLAY_LAND) {
            deploys.push_back(action);
            if (std::find(card_ids.begin(), card_ids.end(), action.card_id) == card_ids.end()) {
                card_ids.push_back(action.card_id);
            }
        }
    }
    const uint32_t card_id = card_ids[pick_index(card_ids.size())];
    std::vector<Action> of_card;
    std::copy_if(deploys.begin(), deploys.end(), std::back_inserter(of_card),
                 [card_id](const Action& action) { return action.card_id == card_id; });
    return apply(of_card[pick_index(of_card.size())]);
}

bool LandGameMatch::priority_timeout(int player_index) {
    if (player_index < 0 || player_index >= 2 || player_index != priority_player_) {
        return false;
    }
    record(Action{ActionType::PRIORITY_TIMEOUT, player_index, 0, std::nullopt, std::nullopt, {}, {}});
    ScopedLogPause pause(log_paused_);
    return pass_priority(player_index);     // a slow pass is a legitimate bluff: no strike
}

bool LandGameMatch::choice_timeout(int player_index) {
    if (player_index < 0 || player_index >= 2 || pending_choice_.type == ChoiceType::NONE ||
        player_index != pending_choice_.player) {
        return false;
    }
    record(Action{ActionType::CHOICE_TIMEOUT, player_index, 0, std::nullopt, std::nullopt, {}, {}});
    if (give_strike(player_index)) {
        return true;
    }
    ScopedLogPause pause(log_paused_);
    const std::vector<Action> answers = choice_actions(player_index);
    return apply(answers[pick_index(answers.size())]);
}

bool LandGameMatch::disconnect_forfeit(int player_index) {
    if (player_index < 0 || player_index >= 2 || game_over()) {
        return false;
    }
    record(Action{ActionType::DISCONNECT_FORFEIT, player_index, 0, std::nullopt, std::nullopt, {}, {}});
    end_game(GameEndReason::DISCONNECT_FORFEIT, 1 - player_index);
    return true;
}

const std::vector<Action>& LandGameMatch::action_log() const {
    return action_log_;
}

bool LandGameMatch::apply(const Action& action) {
    switch (action.type) {
        case ActionType::PLAY_LAND:
            return play_land(action.player, action.card_id, action.target_id);
        case ActionType::PITCH_LAND:
            return pitch_land(action.player, action.card_id, action.extra_id);
        case ActionType::CAST_COUNTER:
            return action.extra_id.has_value() && action.target_id.has_value() &&
                   cast_counter(action.player, action.card_id, *action.extra_id, *action.target_id);
        case ActionType::CAST_SAVE:
            return action.target_id.has_value() &&
                   cast_save(action.player, action.card_id, *action.target_id);
        case ActionType::PASS_PRIORITY:
            return pass_priority(action.player);
        case ActionType::CHOOSE_CARD:
            return choose_card(action.player, action.card_id);
        case ActionType::CHOOSE_SURVEIL:
            return choose_surveil(action.player, action.to_graveyard, action.top_order);
        case ActionType::END_TURN:
            return end_turn(action.player);
        case ActionType::CONCEDE:
            return concede(action.player);
        case ActionType::MAIN_PHASE_TIMEOUT:
            return main_phase_timeout(action.player);
        case ActionType::PRIORITY_TIMEOUT:
            return priority_timeout(action.player);
        case ActionType::CHOICE_TIMEOUT:
            return choice_timeout(action.player);
        case ActionType::DISCONNECT_FORFEIT:
            return disconnect_forfeit(action.player);
    }
    return false;
}

LandGameMatch LandGameMatch::replay(uint64_t seed, const std::vector<Action>& log, MatchConfig config) {
    LandGameMatch match(seed, config);
    for (const Action& action : log) {
        if (!match.apply(action)) {
            throw std::invalid_argument("LandGameMatch::replay: the log holds an action the engine rejects");
        }
    }
    return match;
}

uint64_t LandGameMatch::state_digest() const {
    uint64_t hash = 1469598103934665603ULL;     // FNV-1a
    auto mix = [&hash](uint64_t value) {
        for (int shift = 0; shift < 64; shift += 8) {
            hash ^= (value >> shift) & 0xFFu;
            hash *= 1099511628211ULL;
        }
    };
    auto mix_cards = [&](const std::vector<Card>& cards) {
        mix(cards.size());
        for (const Card& card : cards) {
            mix(card.get_identifier());
            mix(static_cast<uint64_t>(card.get_land_type()));
        }
    };

    for (const PlayerState& player : players_) {
        mix_cards(player.hand());
        mix_cards(player.battlefield());
    }
    mix_cards(deck_.peek_top(deck_.deck_size()));
    mix_cards(graveyard_.cards());
    for (const StackItem& item : stack_.items()) {
        mix(item.stack_id);
        mix(static_cast<uint64_t>(item.ability));
        mix(item.card.get_identifier());
        mix(item.target_card_id.value_or(0xFFFFFFFFu));
    }
    mix_cards(pending_choice_.options);
    for (uint64_t value : {static_cast<uint64_t>(active_player_), static_cast<uint64_t>(turn_number_),
                           static_cast<uint64_t>(played_land_), static_cast<uint64_t>(pitched_this_turn_),
                           static_cast<uint64_t>(ramp_available_), static_cast<uint64_t>(priority_player_ + 1),
                           static_cast<uint64_t>(pending_choice_.type), static_cast<uint64_t>(pending_choice_.player + 1),
                           static_cast<uint64_t>(afk_strikes_[0]), static_cast<uint64_t>(afk_strikes_[1]),
                           static_cast<uint64_t>(end_reason_), static_cast<uint64_t>(winner_player_ + 1),
                           static_cast<uint64_t>(next_stack_id_)}) {
        mix(value);
    }
    mix(known_top_.has_value() ? known_top_->get_identifier() + 1u : 0u);
    return hash;
}

bool LandGameMatch::deploy_due() const {
    return !game_over() && !played_land_ && players_[active_player_].hand_size() > 0;
}

std::vector<Action> LandGameMatch::legal_actions(int player_index) const {
    require_player_index(player_index);
    if (game_over()) {
        return {};
    }
    if (pending_choice_.type != ChoiceType::NONE) {
        return player_index == pending_choice_.player ? choice_actions(player_index) : std::vector<Action>{};
    }
    if (priority_player_ != -1) {
        return player_index == priority_player_ ? priority_actions(player_index) : std::vector<Action>{};
    }
    return player_index == active_player_ ? main_phase_actions(player_index) : std::vector<Action>{};
}

std::vector<Action> LandGameMatch::choice_actions(int player_index) const {
    std::vector<Action> actions;
    const std::vector<Card>& options = pending_choice_.options;

    if (pending_choice_.type != ChoiceType::SURVEIL) {
        for (const Card& option : options) {
            actions.push_back(Action{ActionType::CHOOSE_CARD, player_index, option.get_identifier(), std::nullopt, std::nullopt, {}, {}});
        }
        return actions;
    }

    // Surveil: every subset to bin, and every order of the cards that stay
    for (unsigned mask = 0; mask < (1u << options.size()); ++mask) {
        std::vector<uint32_t> binned;
        std::vector<uint32_t> kept;
        for (size_t i = 0; i < options.size(); ++i) {
            ((mask >> i) & 1u ? binned : kept).push_back(options[i].get_identifier());
        }
        std::sort(kept.begin(), kept.end());
        do {
            actions.push_back(Action{ActionType::CHOOSE_SURVEIL, player_index, 0, std::nullopt, std::nullopt, binned, kept});
        } while (std::next_permutation(kept.begin(), kept.end()));
    }
    return actions;
}

std::vector<Action> LandGameMatch::priority_actions(int player_index) const {
    std::vector<Action> actions;
    actions.push_back(Action{ActionType::PASS_PRIORITY, player_index, 0, std::nullopt, std::nullopt, {}, {}});

    const PlayerState& player = players_[player_index];
    for (const Card& card : player.hand()) {
        const AbilityId pitch = get(card.get_land_type()).pitch_ability;
        if (pitch == AbilityId::COUNTER) {
            for (const Card& extra : player.hand()) {
                if (extra.get_identifier() == card.get_identifier()) {
                    continue;
                }
                for (const StackItem& item : stack_.items()) {
                    actions.push_back(Action{ActionType::CAST_COUNTER, player_index, card.get_identifier(),
                                             extra.get_identifier(), item.stack_id, {}, {}});
                }
            }
        } else if (pitch == AbilityId::SAVE) {
            for (const Card& land : player.battlefield()) {
                if (stack_.targets_card(land.get_identifier())) {
                    actions.push_back(Action{ActionType::CAST_SAVE, player_index, card.get_identifier(),
                                             std::nullopt, land.get_identifier(), {}, {}});
                }
            }
        }
    }
    return actions;
}

std::vector<Action> LandGameMatch::main_phase_actions(int player_index) const {
    std::vector<Action> actions;
    const PlayerState& player = players_[player_index];
    const PlayerState& opponent = players_[1 - player_index];

    // deploys: the mandatory one, or the extra one a resolved Ramp granted
    if (!played_land_ || ramp_available_) {
        const bool ramped = played_land_;
        for (const Card& card : player.hand()) {
            const AbilityId ability = ramped ? AbilityId::NONE : get(card.get_land_type()).deploy_ability;

            // candidate targets: no target, plus every card in the zone the ETB aims at
            std::vector<std::optional<uint32_t>> candidates{std::nullopt};
            const std::vector<Card>* zone = nullptr;
            if (ability == AbilityId::DESTROY) { zone = &opponent.battlefield(); }
            if (ability == AbilityId::FLICKER) { zone = &player.battlefield(); }
            if (ability == AbilityId::RECLAIM) { zone = &graveyard_.cards(); }
            if (zone != nullptr) {
                for (const Card& target : *zone) {
                    candidates.push_back(target.get_identifier());
                }
            }
            for (const std::optional<uint32_t>& target : candidates) {
                if (valid_deploy_target(player_index, ability, target)) {
                    actions.push_back(Action{ActionType::PLAY_LAND, player_index, card.get_identifier(),
                                             std::nullopt, target, {}, {}});
                }
            }
        }
    }

    // the one optional main-phase pitch
    if (!pitched_this_turn_) {
        for (const Card& card : player.hand()) {
            const LandDefinition& def = get(card.get_land_type());
            if (def.pitch_timing != PitchTiming::MainPhase) {
                continue;
            }
            if (def.pitch_ability == AbilityId::RUMMAGE) {
                for (const Card& extra : player.hand()) {
                    if (extra.get_identifier() != card.get_identifier()) {
                        actions.push_back(Action{ActionType::PITCH_LAND, player_index, card.get_identifier(),
                                                 extra.get_identifier(), std::nullopt, {}, {}});
                    }
                }
            } else {
                actions.push_back(Action{ActionType::PITCH_LAND, player_index, card.get_identifier(),
                                         std::nullopt, std::nullopt, {}, {}});
            }
        }
    }

    actions.push_back(Action{ActionType::END_TURN, player_index, 0, std::nullopt, std::nullopt, {}, {}});
    return actions;
}

bool LandGameMatch::game_over() const {
    return end_reason_ != GameEndReason::NONE;
}

int LandGameMatch::winner_player() const {
    return winner_player_;
}

GameEndReason LandGameMatch::end_reason() const {
    return end_reason_;
}

bool LandGameMatch::is_draw() const {
    return game_over() && winner_player_ == -1;
}

bool LandGameMatch::concede(int player_index) {
    if (player_index < 0 || player_index >= 2 || game_over()) {
        return false;
    }
    record(Action{ActionType::CONCEDE, player_index, 0, std::nullopt, std::nullopt, {}, {}});
    end_game(GameEndReason::CONCEDE, 1 - player_index);
    return true;
}

void LandGameMatch::end_game(GameEndReason reason, int winner_player) {
    end_reason_ = reason;
    winner_player_ = winner_player;
    // the match halts instantly: nothing pending can be answered any more
    priority_player_ = -1;
    pending_choice_ = PendingChoice{};
    while (!stack_.empty()) {
        stack_.pop();
    }
    countered_ids_.clear();
    saved_card_ids_.clear();
}

void LandGameMatch::end_game_by_tiebreak(GameEndReason reason) {
    WinEvaluator eval;
    end_game(reason, eval.tiebreak_winner(players_[0].land_counts(), players_[1].land_counts()));
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

    record(Action{ActionType::CHOOSE_CARD, player_index, card_id, std::nullopt, std::nullopt, {}, {}});
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

    // resolve every named id to a peeked card first; an unknown id is an illegal answer
    const std::vector<Card>& options = pending_choice_.options;
    std::vector<Card> binned;
    std::vector<Card> kept;
    for (uint32_t card_id : to_graveyard) {
        const Card* card = find_card(options, card_id);
        if (card == nullptr) {
            return false;
        }
        binned.push_back(*card);
    }
    for (uint32_t card_id : library_top_order) {
        const Card* card = find_card(options, card_id);
        if (card == nullptr) {
            return false;
        }
        kept.push_back(*card);
    }

    // every peeked card must be named exactly once
    if (binned.size() + kept.size() != options.size()) {
        return false;
    }
    for (const Card& option : options) {
        const auto same_card = [&option](const Card& card) { return card == option; };
        if (std::count_if(binned.begin(), binned.end(), same_card) +
            std::count_if(kept.begin(), kept.end(), same_card) != 1) {
            return false;
        }
    }

    record(Action{ActionType::CHOOSE_SURVEIL, player_index, 0, std::nullopt, std::nullopt, to_graveyard, library_top_order});

    // the peeked cards are the top of the library: take them off, then place them
    for (size_t i = 0; i < options.size(); ++i) {
        deck_.draw();
    }
    for (const Card& card : binned) {
        graveyard_.add(card);
    }
    for (auto it = kept.rbegin(); it != kept.rend(); ++it) {
        deck_.put_on_top(*it);
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
    // the mandatory deploy is due, but there is nothing to deploy
    if (players_[active_player_].hand_size() == 0) {
        end_game_by_tiebreak(GameEndReason::DEPLOY_LOCK);
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
    // the deploy was required but the hand is empty (e.g. the last cards were pitched)
    record(Action{ActionType::END_TURN, player_index, 0, std::nullopt, std::nullopt, {}, {}});
    if (!played_land_ && players_[player_index].hand_size() == 0) {
        end_game_by_tiebreak(GameEndReason::DEPLOY_LOCK);
        return true;
    }
    played_land_ = false;
    pitched_this_turn_ = false;
    ramp_available_ = false;
    if (active_player_ == 1 && turn_number_ >= kMaxRounds) {   // turn 50 completes
        end_game_by_tiebreak(GameEndReason::TURN_CAP);
        return true;
    }
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
            known_top_ = item.card;     // face-up knowledge for both players
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
                end_game(result == WinCondition::MONO ? GameEndReason::MONO : GameEndReason::DOMAIN,
                         item.caster_id);
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