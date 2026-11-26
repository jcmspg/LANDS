#include "lands/LandGameMatch.hpp"
#include "lands/RandomBot.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

namespace lands {

namespace {

// Ids follow the listing order: P1's hand is 0-4, P2's is 5-10, then the library (11 is the top).
LandGameMatch make_stacked_match(const std::vector<LandType>& p0, const std::vector<LandType>& p1,
                                 const std::vector<LandType>& library) {
    uint32_t id = 0;
    std::vector<Card> p0_cards, p1_cards, library_cards;
    for (LandType land_type : p0) { p0_cards.push_back(Card(land_type, id++)); }
    for (LandType land_type : p1) { p1_cards.push_back(Card(land_type, id++)); }
    for (LandType land_type : library) { library_cards.push_back(Card(land_type, id++)); }

    std::vector<Card> cards;
    cards.insert(cards.end(), library_cards.rbegin(), library_cards.rend());
    cards.insert(cards.end(), p1_cards.rbegin(), p1_cards.rend());
    cards.insert(cards.end(), p0_cards.rbegin(), p0_cards.rend());
    return LandGameMatch{Deck(cards)};
}

std::vector<uint32_t> ids_of(const std::vector<CardView>& cards) {
    std::vector<uint32_t> ids;
    for (const CardView& card : cards) { ids.push_back(card.instance_id); }
    return ids;
}

std::vector<uint32_t> ids_of(const std::vector<Card>& cards) {
    std::vector<uint32_t> ids;
    for (const Card& card : cards) { ids.push_back(card.get_identifier()); }
    return ids;
}

const std::vector<LandType> kFive(5, LandType::FOREST);
const std::vector<LandType> kSix(6, LandType::MOUNTAIN);
const std::vector<LandType> kLibrary(10, LandType::MOUNTAIN);
// a Forest to pitch (id 0), then a Mountain (id 1: nothing to destroy, enters vanilla) and Forests
const std::vector<LandType> kForestThenMountain{LandType::FOREST, LandType::MOUNTAIN, LandType::FOREST, LandType::FOREST, LandType::FOREST};

} // namespace

TEST(Snapshot, ShowsTheViewersOwnHandAndOnlyACountOfTheOpponents) {
    LandGameMatch match(12345);

    const Snapshot first = match.snapshot_for(0);
    EXPECT_EQ(first.viewer, 0);
    EXPECT_EQ(first.you.id, 0);
    EXPECT_EQ(ids_of(first.you.hand), ids_of(match.hand(0)));
    EXPECT_EQ(first.you.hand_count, 5);
    EXPECT_TRUE(first.opponent.hand.empty());           // never revealed
    EXPECT_EQ(first.opponent.hand_count, 6);
    EXPECT_EQ(first.opponent.id, 1);

    const Snapshot second = match.snapshot_for(1);
    EXPECT_EQ(ids_of(second.you.hand), ids_of(match.hand(1)));
    EXPECT_TRUE(second.opponent.hand.empty());
    EXPECT_EQ(second.opponent.hand_count, 5);
}

TEST(Snapshot, CarriesTheTypesOfVisibleCards) {
    LandGameMatch match = make_stacked_match({LandType::ISLAND, LandType::SWAMP, LandType::PLAINS, LandType::MOUNTAIN, LandType::FOREST}, kSix, kLibrary);
    const Snapshot snapshot = match.snapshot_for(0);
    ASSERT_EQ(snapshot.you.hand.size(), 5);
    EXPECT_EQ(snapshot.you.hand[0], (CardView{0, LandType::ISLAND}));
    EXPECT_EQ(snapshot.you.hand[4], (CardView{4, LandType::FOREST}));
}

TEST(Snapshot, StartOfMatchState) {
    LandGameMatch match(12345);
    const Snapshot snapshot = match.snapshot_for(0);

    EXPECT_EQ(snapshot.turn_number, 1);
    EXPECT_EQ(snapshot.active_player, 0);
    EXPECT_EQ(snapshot.priority_holder, -1);
    EXPECT_FALSE(snapshot.choice.has_value());
    EXPECT_TRUE(snapshot.stack.empty());
    EXPECT_EQ(snapshot.library_count, 39);
    EXPECT_FALSE(snapshot.library_top_known.has_value());
    EXPECT_TRUE(snapshot.graveyard.empty());
    EXPECT_FALSE(snapshot.game_over);
    EXPECT_EQ(snapshot.winner_id, -1);
    EXPECT_EQ(snapshot.end_reason, GameEndReason::NONE);
    EXPECT_FALSE(snapshot.is_draw);
    EXPECT_FALSE(snapshot.tiebreak.has_value());
}

TEST(Snapshot, RejectsABadViewer) {
    LandGameMatch match(12345);
    EXPECT_THROW(match.snapshot_for(-1), std::out_of_range);
    EXPECT_THROW(match.snapshot_for(2), std::out_of_range);
}

TEST(Snapshot, TracksDeploysRemainingAndThePitchBudget) {
    LandGameMatch match = make_stacked_match(kForestThenMountain, kSix, kLibrary);

    Snapshot snapshot = match.snapshot_for(0);
    EXPECT_EQ(snapshot.you.deploys_remaining, 1);
    EXPECT_EQ(snapshot.opponent.deploys_remaining, 0);
    EXPECT_FALSE(snapshot.you.main_pitch_used);

    ASSERT_TRUE(match.pitch_land(0, 0));                // Ramp: one extra deploy
    snapshot = match.snapshot_for(0);
    EXPECT_EQ(snapshot.you.deploys_remaining, 2);
    EXPECT_TRUE(snapshot.you.main_pitch_used);
    EXPECT_EQ(match.snapshot_for(1).opponent.deploys_remaining, 2);   // public to both

    ASSERT_TRUE(match.play_land(0, 1));
    ASSERT_TRUE(match.play_land(0, 2));
    EXPECT_EQ(match.snapshot_for(0).you.deploys_remaining, 0);

    ASSERT_TRUE(match.end_turn(0));
    snapshot = match.snapshot_for(1);
    EXPECT_EQ(snapshot.you.deploys_remaining, 1);       // P2's turn
    EXPECT_EQ(snapshot.opponent.deploys_remaining, 0);
    EXPECT_FALSE(snapshot.opponent.main_pitch_used);
}

TEST(Snapshot, ShowsTheBattlefieldAndGraveyardToBothPlayers) {
    LandGameMatch match = make_stacked_match(kForestThenMountain, kSix, kLibrary);
    ASSERT_TRUE(match.pitch_land(0, 0));                // Forest to the graveyard
    ASSERT_TRUE(match.play_land(0, 1));

    for (int viewer = 0; viewer < 2; ++viewer) {
        const Snapshot snapshot = match.snapshot_for(viewer);
        const PlayerView& mine = viewer == 0 ? snapshot.you : snapshot.opponent;
        EXPECT_EQ(ids_of(mine.battlefield), (std::vector<uint32_t>{1}));
        EXPECT_EQ(ids_of(snapshot.graveyard), (std::vector<uint32_t>{0}));
        EXPECT_EQ(snapshot.graveyard[0].type, LandType::FOREST);
    }
}

TEST(Snapshot, ShowsTheStackWithItsTargetsToBothPlayers) {
    // both players hold an Island, so every push waits in a priority window
    LandGameMatch match = make_stacked_match({LandType::FOREST, LandType::MOUNTAIN, LandType::ISLAND, LandType::FOREST, LandType::FOREST},
                                             {LandType::FOREST, LandType::ISLAND, LandType::FOREST, LandType::FOREST, LandType::FOREST, LandType::FOREST}, kLibrary);
    ASSERT_TRUE(match.play_land(0, 0));
    ASSERT_TRUE(match.pass_priority(1));
    ASSERT_TRUE(match.end_turn(0));
    ASSERT_TRUE(match.play_land(1, 5));
    ASSERT_TRUE(match.pass_priority(0));
    ASSERT_TRUE(match.end_turn(1));
    ASSERT_TRUE(match.play_land(0, 1, 5u));             // the Mountain aims at P2's Forest
    ASSERT_EQ(match.priority_player(), 1);

    for (int viewer = 0; viewer < 2; ++viewer) {
        const Snapshot snapshot = match.snapshot_for(viewer);
        EXPECT_EQ(snapshot.priority_holder, 1);
        ASSERT_EQ(snapshot.stack.size(), 1);
        EXPECT_EQ(snapshot.stack[0].pusher, 0);
        EXPECT_EQ(snapshot.stack[0].kind, PlayMode::DEPLOY);
        EXPECT_EQ(snapshot.stack[0].card, (CardView{1, LandType::MOUNTAIN}));
        EXPECT_EQ(snapshot.stack[0].target_instance_id, std::optional<uint32_t>(5));
        EXPECT_FALSE(snapshot.stack[0].target_stack_id.has_value());
    }

    ASSERT_TRUE(match.cast_counter(1, 6, 7, match.stack_top().stack_id));
    ASSERT_EQ(match.priority_player(), 0);              // P1 may now counter the counter
    for (int viewer = 0; viewer < 2; ++viewer) {
        const Snapshot snapshot = match.snapshot_for(viewer);
        EXPECT_EQ(snapshot.priority_holder, 0);
        ASSERT_EQ(snapshot.stack.size(), 2);
        EXPECT_EQ(snapshot.stack[1].pusher, 1);
        EXPECT_EQ(snapshot.stack[1].kind, PlayMode::PITCH);
        EXPECT_EQ(snapshot.stack[1].ability, AbilityId::COUNTER);
        EXPECT_EQ(snapshot.stack[1].card, (CardView{6, LandType::ISLAND}));
        EXPECT_EQ(snapshot.stack[1].target_stack_id, std::optional<uint32_t>(snapshot.stack[0].stack_id));
    }
}

TEST(Snapshot, ExtractOptionsAreVisibleOnlyToTheChooser) {
    LandGameMatch match = make_stacked_match({LandType::SWAMP, LandType::FOREST, LandType::FOREST, LandType::FOREST, LandType::FOREST}, kSix, kLibrary);
    ASSERT_TRUE(match.play_land(0, 0));
    ASSERT_EQ(match.choice_type(), ChoiceType::EXTRACT);

    const Snapshot chooser = match.snapshot_for(0);
    ASSERT_TRUE(chooser.choice.has_value());
    EXPECT_EQ(chooser.choice->holder, 0);
    EXPECT_EQ(chooser.choice->type, ChoiceType::EXTRACT);
    EXPECT_EQ(ids_of(chooser.choice->options), ids_of(match.hand(1)));   // the opponent's whole hand

    const Snapshot other = match.snapshot_for(1);
    ASSERT_TRUE(other.choice.has_value());                              // they know a choice is pending...
    EXPECT_EQ(other.choice->holder, 0);
    EXPECT_EQ(other.choice->type, ChoiceType::EXTRACT);
    EXPECT_TRUE(other.choice->options.empty());                         // ...but not what is on offer
    EXPECT_TRUE(other.you.hand.size() == 6 && other.opponent.hand.empty());
}

TEST(Snapshot, SurveilPeekIsVisibleOnlyToTheSurveillingPlayer) {
    LandGameMatch match = make_stacked_match({LandType::SWAMP, LandType::FOREST, LandType::FOREST, LandType::FOREST, LandType::FOREST}, kSix,
                                             {LandType::PLAINS, LandType::ISLAND, LandType::MOUNTAIN});
    ASSERT_TRUE(match.pitch_land(0, 0));
    ASSERT_EQ(match.choice_type(), ChoiceType::SURVEIL);

    const Snapshot chooser = match.snapshot_for(0);
    ASSERT_TRUE(chooser.choice.has_value());
    EXPECT_EQ(ids_of(chooser.choice->options), (std::vector<uint32_t>{11, 12}));
    EXPECT_EQ(chooser.library_count, 3);

    const Snapshot other = match.snapshot_for(1);
    ASSERT_TRUE(other.choice.has_value());
    EXPECT_TRUE(other.choice->options.empty());
    EXPECT_FALSE(other.library_top_known.has_value());                  // the peek leaks nothing
}

TEST(Snapshot, AMemoryLapsedLandOnTopOfTheLibraryIsPublic) {
    LandGameMatch match = make_stacked_match({LandType::FOREST, LandType::FOREST, LandType::FOREST, LandType::FOREST, LandType::FOREST},
                                             {LandType::ISLAND, LandType::MOUNTAIN, LandType::MOUNTAIN, LandType::MOUNTAIN, LandType::MOUNTAIN, LandType::MOUNTAIN},
                                             kLibrary);
    ASSERT_TRUE(match.play_land(0, 0));
    ASSERT_TRUE(match.cast_counter(1, 5, 6, match.stack_top().stack_id));

    for (int viewer = 0; viewer < 2; ++viewer) {
        const Snapshot snapshot = match.snapshot_for(viewer);
        ASSERT_TRUE(snapshot.library_top_known.has_value());
        EXPECT_EQ(*snapshot.library_top_known, (CardView{0, LandType::FOREST}));
        EXPECT_EQ(snapshot.library_count, 11);
    }

    ASSERT_TRUE(match.end_turn(0));                     // P2 draws it
    EXPECT_FALSE(match.snapshot_for(0).library_top_known.has_value());
    EXPECT_FALSE(match.snapshot_for(1).library_top_known.has_value());
}

TEST(Snapshot, ReportsAWinWithoutATiebreak) {
    LandGameMatch match = make_stacked_match(kFive, kSix, kLibrary);
    ASSERT_TRUE(match.concede(1));

    const Snapshot snapshot = match.snapshot_for(0);
    EXPECT_TRUE(snapshot.game_over);
    EXPECT_EQ(snapshot.winner_id, 0);
    EXPECT_EQ(snapshot.end_reason, GameEndReason::CONCEDE);
    EXPECT_FALSE(snapshot.is_draw);
    EXPECT_FALSE(snapshot.tiebreak.has_value());
    EXPECT_EQ(snapshot.you.deploys_remaining, 0);
    EXPECT_EQ(snapshot.priority_holder, -1);
}

TEST(Snapshot, ReportsTheTiebreakScoresOfADeployLock) {
    // P1 ends with 4 Forests + an Island (2 types, 5 lands), P2 with 4 Forests (1 type, 4 lands)
    LandGameMatch match = make_stacked_match(
        {LandType::FOREST, LandType::FOREST, LandType::FOREST, LandType::FOREST, LandType::ISLAND},
        {LandType::FOREST, LandType::FOREST, LandType::FOREST, LandType::FOREST, LandType::MOUNTAIN, LandType::MOUNTAIN}, {});
    const uint32_t p1_cards[] = {0, 1, 2, 3, 4};
    const uint32_t p2_cards[] = {5, 6, 7, 8};
    for (int round = 0; round < 5; ++round) {
        ASSERT_TRUE(match.play_land(0, p1_cards[round]));
        if (match.priority_player() != -1) { ASSERT_TRUE(match.pass_priority(match.priority_player())); }
        ASSERT_TRUE(match.end_turn(0));
        if (round < 4) {
            ASSERT_TRUE(match.play_land(1, p2_cards[round]));
            if (match.priority_player() != -1) { ASSERT_TRUE(match.pass_priority(match.priority_player())); }
        }
        ASSERT_TRUE(match.end_turn(1));
    }

    const Snapshot snapshot = match.snapshot_for(1);
    ASSERT_TRUE(snapshot.game_over);
    EXPECT_EQ(snapshot.end_reason, GameEndReason::DEPLOY_LOCK);
    EXPECT_EQ(snapshot.winner_id, 0);
    ASSERT_TRUE(snapshot.tiebreak.has_value());
    EXPECT_EQ(snapshot.tiebreak->distinct_types, (std::array<int, 2>{2, 1}));
    EXPECT_EQ(snapshot.tiebreak->total_lands, (std::array<int, 2>{5, 4}));
}

TEST(Snapshot, NeverRevealsPrivateInformationInARandomGame) {
    for (uint64_t seed = 1; seed <= 40; ++seed) {
        LandGameMatch match(seed);
        RandomBot bot(seed, 5);
        do {
            for (int viewer = 0; viewer < 2; ++viewer) {
                const Snapshot snapshot = match.snapshot_for(viewer);
                EXPECT_EQ(ids_of(snapshot.you.hand), ids_of(match.hand(viewer)));
                EXPECT_EQ(snapshot.you.hand_count, match.hand_size(viewer));
                EXPECT_TRUE(snapshot.opponent.hand.empty());
                EXPECT_EQ(snapshot.opponent.hand_count, match.hand_size(1 - viewer));
                EXPECT_EQ(snapshot.library_count, match.deck_size());
                EXPECT_EQ(snapshot.graveyard.size(), match.graveyard_size());
                EXPECT_EQ(snapshot.stack.size(), match.stack_size());
                if (snapshot.choice.has_value() && snapshot.choice->holder != viewer) {
                    EXPECT_TRUE(snapshot.choice->options.empty());
                }
            }
        } while (bot.step(match));
    }
}

} // namespace lands
