#include "lands/RandomBot.hpp"

#include <gtest/gtest.h>

namespace lands {

TEST(RandomBot, PlaysAMatchToTheEnd) {
    LandGameMatch match(7);
    RandomBot bot(7);
    int steps = 0;
    while (bot.step(match)) {
        ASSERT_LT(++steps, 20000);
    }
    EXPECT_TRUE(match.game_over());
    EXPECT_GT(steps, 0);
}

TEST(RandomBot, StepDoesNothingOnceTheGameIsOver) {
    LandGameMatch match(7);
    ASSERT_TRUE(match.concede(0));
    RandomBot bot(7);

    EXPECT_FALSE(bot.step(match));
    EXPECT_EQ(match.action_log().size(), 1);                    // only the concede
}

TEST(RandomBot, SameSeedPlaysTheSameMatch) {
    LandGameMatch first(99);
    LandGameMatch second(99);
    RandomBot bot_one(99, 10, 20);
    RandomBot bot_two(99, 10, 20);
    while (bot_one.step(first)) {}
    while (bot_two.step(second)) {}

    EXPECT_EQ(first.action_log(), second.action_log());
    EXPECT_EQ(first.state_digest(), second.state_digest());
}

TEST(RandomBot, DifferentSeedsPlayDifferentMatches) {
    LandGameMatch first(1);
    LandGameMatch second(2);
    RandomBot bot_one(1);
    RandomBot bot_two(2);
    while (bot_one.step(first)) {}
    while (bot_two.step(second)) {}

    EXPECT_NE(first.state_digest(), second.state_digest());
}

TEST(RandomBot, TimerPercentInjectsTimerEvents) {
    int timer_events = 0;
    for (uint64_t seed = 1; seed <= 20; ++seed) {
        LandGameMatch match(seed);
        RandomBot bot(seed, 50);
        while (bot.step(match)) {}
        for (const Action& action : match.action_log()) {
            timer_events += action.type == ActionType::MAIN_PHASE_TIMEOUT ||
                            action.type == ActionType::PRIORITY_TIMEOUT ||
                            action.type == ActionType::CHOICE_TIMEOUT;
        }
    }
    EXPECT_GT(timer_events, 0);
}

TEST(RandomBot, WithoutTimerPercentNoTimerEventsHappen) {
    LandGameMatch match(5);
    RandomBot bot(5, 0);
    while (bot.step(match)) {}
    for (const Action& action : match.action_log()) {
        EXPECT_NE(action.type, ActionType::MAIN_PHASE_TIMEOUT);
        EXPECT_NE(action.type, ActionType::PRIORITY_TIMEOUT);
        EXPECT_NE(action.type, ActionType::CHOICE_TIMEOUT);
    }
}

TEST(RandomBot, MakesTheMandatoryDeployWhenOneIsPossible) {
    // without stalling, every turn the bot starts with a deploy due ends with one made
    for (uint64_t seed = 1; seed <= 20; ++seed) {
        LandGameMatch match(seed);
        RandomBot bot(seed);
        while (!match.game_over() && match.turn_number() < 3) {
            const int active = match.active_player();
            const bool was_due = match.deploy_due();
            bot.step(match);
            if (was_due && !match.game_over() && match.active_player() != active) {
                FAIL() << "the turn ended while a deploy was still due (seed " << seed << ")";
            }
        }
    }
}

TEST(RandomBot, IdleProfileLeavesDeploysUndone) {
    int skipped_deploys = 0;
    for (uint64_t seed = 1; seed <= 20; ++seed) {
        LandGameMatch match(seed);
        RandomBot bot(seed, 0, 95);
        while (!match.game_over() && match.turn_number() < 5) {
            const int active = match.active_player();
            const bool was_due = match.deploy_due();
            bot.step(match);
            skipped_deploys += was_due && !match.game_over() && match.active_player() != active;
        }
    }
    EXPECT_GT(skipped_deploys, 0);
}

} // namespace lands
