#include "lands/LandGameMatch.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <random>
#include <set>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace lands {

namespace {

bool contains(const std::vector<Action>& actions, const Action& action) {
    return std::find(actions.begin(), actions.end(), action) != actions.end();
}

// Every card id that appears in any zone, plus ids that appear nowhere.
std::vector<uint32_t> candidate_ids(const LandGameMatch& match) {
    std::set<uint32_t> ids{9999};
    for (int player = 0; player < 2; ++player) {
        for (const Card& card : match.hand(player)) { ids.insert(card.get_identifier()); }
        for (const Card& card : match.battlefield(player)) { ids.insert(card.get_identifier()); }
    }
    for (const Card& card : match.choice_options()) { ids.insert(card.get_identifier()); }
    for (uint32_t id = 0; id < Deck::deck_size_; id += 7) { ids.insert(id); }   // graveyard / library cards
    return std::vector<uint32_t>(ids.begin(), ids.end());
}

// Brute force: try plausible actions on copies of the match and require that the engine accepts
// exactly the ones legal_actions() lists (Concede aside).
void expect_legal_actions_are_exact(const LandGameMatch& match, int player) {
    const std::vector<Action> legal = match.legal_actions(player);
    std::vector<Action> candidates;
    auto add = [&](ActionType type, uint32_t card, std::optional<uint32_t> extra = std::nullopt,
                   std::optional<uint32_t> target = std::nullopt) {
        candidates.push_back(Action{type, player, card, extra, target, {}, {}});
    };

    const std::vector<uint32_t> ids = candidate_ids(match);
    std::vector<std::optional<uint32_t>> optional_ids{std::nullopt};
    for (uint32_t id : ids) { optional_ids.push_back(id); }

    add(ActionType::END_TURN, 0);
    add(ActionType::PASS_PRIORITY, 0);
    for (const Card& card : match.hand(player)) {
        const uint32_t id = card.get_identifier();
        for (const auto& target : optional_ids) {
            add(ActionType::PLAY_LAND, id, std::nullopt, target);
            add(ActionType::PITCH_LAND, id, target);
            add(ActionType::CAST_SAVE, id, std::nullopt, target);
        }
        for (uint32_t stack_id = 0; stack_id <= 7; ++stack_id) {
            for (const Card& extra : match.hand(player)) {
                add(ActionType::CAST_COUNTER, id, extra.get_identifier(), stack_id);
            }
            add(ActionType::CAST_COUNTER, id, 9999, stack_id);
        }
    }
    for (uint32_t id : ids) {
        add(ActionType::CHOOSE_CARD, id);
    }

    for (const Action& candidate : candidates) {
        LandGameMatch copy = match;
        EXPECT_EQ(copy.apply(candidate), contains(legal, candidate))
            << "type " << static_cast<int>(candidate.type) << " card " << candidate.card_id
            << " player " << player;
    }
    // every listed action must be accepted, including the structured Surveil answers
    for (const Action& action : legal) {
        LandGameMatch copy = match;
        EXPECT_TRUE(copy.apply(action)) << "listed but rejected: type " << static_cast<int>(action.type);
    }
}

// Plays one game to the end with uniformly random legal actions (preferring the mandatory deploy).
// Returns how many times each action type was applied.
std::map<ActionType, int> play_random_game(uint64_t seed, LandGameMatch& match, int check_every,
                                           int timer_percent = 0) {
    std::mt19937_64 rng(seed * 7919 + 1);
    std::map<ActionType, int> applied;

    for (int step = 0; step < 5000 && !match.game_over(); ++step) {
        const std::vector<Action> first = match.legal_actions(0);
        const std::vector<Action> second = match.legal_actions(1);
        EXPECT_NE(first.empty(), second.empty()) << "exactly one player may act";
        const int mover = first.empty() ? 1 : 0;
        std::vector<Action> options = first.empty() ? second : first;

        if (check_every > 0 && step % check_every == 0) {
            expect_legal_actions_are_exact(match, mover);
        }

        if (match.deploy_due() && match.active_player() == mover) {
            std::vector<Action> deploys;
            std::copy_if(options.begin(), options.end(), std::back_inserter(deploys),
                         [](const Action& action) { return action.type == ActionType::PLAY_LAND; });
            if (!deploys.empty()) {
                options = deploys;
            }
        }
        // sometimes the mover's clock runs out instead (the timer that is running for them)
        if (timer_percent > 0 && static_cast<int>(rng() % 100) < timer_percent) {
            ActionType timeout = ActionType::MAIN_PHASE_TIMEOUT;
            if (match.choice_player() == mover) {
                timeout = ActionType::CHOICE_TIMEOUT;
            } else if (match.priority_player() == mover) {
                timeout = ActionType::PRIORITY_TIMEOUT;
            }
            EXPECT_TRUE(match.apply(Action{timeout, mover, 0, std::nullopt, std::nullopt, {}, {}}));
            ++applied[timeout];
            continue;
        }

        const Action& chosen = options[std::uniform_int_distribution<size_t>(0, options.size() - 1)(rng)];
        EXPECT_TRUE(match.apply(chosen));
        ++applied[chosen.type];
    }
    return applied;
}

using Snapshot = std::tuple<std::vector<uint32_t>, std::vector<uint32_t>, std::vector<uint32_t>,
                            std::vector<uint32_t>, size_t, size_t, int, int, GameEndReason, int,
                            int, int>;

std::vector<uint32_t> ids_of(const std::vector<Card>& cards) {
    std::vector<uint32_t> ids;
    for (const Card& card : cards) { ids.push_back(card.get_identifier()); }
    return ids;
}

Snapshot snapshot_of(const LandGameMatch& match) {
    return {ids_of(match.hand(0)), ids_of(match.hand(1)), ids_of(match.battlefield(0)),
            ids_of(match.battlefield(1)), match.deck_size(), match.graveyard_size(),
            match.turn_number(), match.active_player(), match.end_reason(), match.winner_player(),
            match.afk_strikes(0), match.afk_strikes(1)};
}

} // namespace

TEST(MatchProperties, LegalActionsAreExactlyTheAcceptedActions) {
    for (uint64_t seed = 1; seed <= 12; ++seed) {
        LandGameMatch match(seed);
        play_random_game(seed, match, 6);
        EXPECT_TRUE(match.game_over()) << "seed " << seed << " did not terminate";
    }
}

TEST(MatchProperties, RandomGamesExerciseEveryActionType) {
    std::map<ActionType, int> totals;
    for (uint64_t seed = 1; seed <= 150; ++seed) {
        LandGameMatch match(seed);
        for (const auto& [type, count] : play_random_game(seed, match, 0)) {
            totals[type] += count;
        }
    }
    for (ActionType type : {ActionType::PLAY_LAND, ActionType::PITCH_LAND, ActionType::CAST_COUNTER,
                            ActionType::CAST_SAVE, ActionType::PASS_PRIORITY, ActionType::CHOOSE_CARD,
                            ActionType::CHOOSE_SURVEIL, ActionType::END_TURN}) {
        EXPECT_GT(totals[type], 0) << "action type " << static_cast<int>(type) << " never happened";
    }
}

TEST(MatchProperties, EveryRandomGameTerminates) {
    for (uint64_t seed = 1; seed <= 300; ++seed) {
        LandGameMatch match(seed);
        play_random_game(seed, match, 0);
        ASSERT_TRUE(match.game_over()) << "seed " << seed;
        EXPECT_LE(match.turn_number(), LandGameMatch::kMaxRounds);
        EXPECT_NE(match.end_reason(), GameEndReason::NONE);
    }
}

TEST(MatchProperties, ReplayingTheLogReproducesTheMatch) {
    for (uint64_t seed = 1; seed <= 60; ++seed) {
        LandGameMatch match(seed);
        play_random_game(seed, match, 0);

        const LandGameMatch replayed = LandGameMatch::replay(seed, match.action_log());
        EXPECT_EQ(snapshot_of(replayed), snapshot_of(match)) << "seed " << seed;
        EXPECT_EQ(replayed.action_log(), match.action_log());
    }
}

TEST(MatchProperties, ReplayingTheLogReproducesMatchesWithTimerEvents) {
    int forfeits = 0;
    for (uint64_t seed = 1; seed <= 120; ++seed) {
        LandGameMatch match(seed);
        play_random_game(seed, match, 0, 25);
        ASSERT_TRUE(match.game_over()) << "seed " << seed;

        const LandGameMatch replayed = LandGameMatch::replay(seed, match.action_log());
        EXPECT_EQ(snapshot_of(replayed), snapshot_of(match)) << "seed " << seed;
        EXPECT_EQ(replayed.action_log(), match.action_log());
        forfeits += match.end_reason() == GameEndReason::AFK_FORFEIT;
    }
    EXPECT_GT(forfeits, 0);                                         // the 3-strike path was exercised
}

TEST(MatchProperties, TimerEventsNeverBreakTermination) {
    std::map<ActionType, int> totals;
    for (uint64_t seed = 1; seed <= 200; ++seed) {
        LandGameMatch match(seed);
        for (const auto& [type, count] : play_random_game(seed, match, 0, 40)) {
            totals[type] += count;
        }
        ASSERT_TRUE(match.game_over()) << "seed " << seed;
        EXPECT_LE(match.afk_strikes(0), LandGameMatch::kMaxAfkStrikes);
        EXPECT_LE(match.afk_strikes(1), LandGameMatch::kMaxAfkStrikes);
    }
    EXPECT_GT(totals[ActionType::MAIN_PHASE_TIMEOUT], 0);
    EXPECT_GT(totals[ActionType::PRIORITY_TIMEOUT], 0);
    EXPECT_GT(totals[ActionType::CHOICE_TIMEOUT], 0);
}

TEST(MatchProperties, ReplayRejectsACorruptLog) {
    const Action out_of_turn{ActionType::END_TURN, 1, 0, std::nullopt, std::nullopt, {}, {}};
    EXPECT_THROW(LandGameMatch::replay(12345, {out_of_turn}), std::invalid_argument);
}

} // namespace lands
