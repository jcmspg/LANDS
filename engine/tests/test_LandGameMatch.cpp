#include "gtest/gtest.h"
#include "lands/LandGameMatch.hpp"

#include <optional>
#include <stdexcept>
#include <vector>

namespace lands {

namespace {

uint32_t first_card_id(const LandGameMatch& match, int player_index) {
    return match.hand(player_index).front().get_identifier();
}

std::optional<uint32_t> find_in_hand(const LandGameMatch& match, int player_index, LandType land_type) {
    for (const Card& card : match.hand(player_index)) {
        if (card.get_land_type() == land_type) {
            return card.get_identifier();
        }
    }
    return std::nullopt;
}

// first card in the player's hand that is not `excluded` (used as a pitch cost)
uint32_t first_card_other_than(const LandGameMatch& match, int player_index, uint32_t excluded) {
    for (const Card& card : match.hand(player_index)) {
        if (card.get_identifier() != excluded) {
            return card.get_identifier();
        }
    }
    ADD_FAILURE() << "no other card in hand";
    return excluded;
}

// deterministic: the first seed whose starting hand for `player_index` holds every land type listed
LandGameMatch make_match_with(int player_index, const std::vector<LandType>& land_types) {
    for (uint64_t seed = 1; seed < 10000; ++seed) {
        LandGameMatch match(seed);
        bool has_all = true;
        for (LandType land_type : land_types) {
            has_all = has_all && find_in_hand(match, player_index, land_type).has_value();
        }
        if (has_all) {
            return match;
        }
    }
    ADD_FAILURE() << "no seed found with the requested starting hand";
    return LandGameMatch(0);
}

size_t total_lands(const LandGameMatch& match, int player_index) {
    size_t total = 0;
    for (uint8_t t = 0; t < static_cast<uint8_t>(LandType::NUM_LAND_TYPES); ++t) {
        total += match.land_count(player_index, static_cast<LandType>(t));
    }
    return total;
}

} // namespace

TEST(LandGameMatch, InitialState) {
    LandGameMatch match(12345);
    EXPECT_EQ(match.deck_size(), Deck::deck_size_ - 5 - 6);
    EXPECT_EQ(match.deck_size(), 39);
    EXPECT_EQ(match.hand_size(0), 5);
    EXPECT_EQ(match.hand_size(1), 6);
    EXPECT_EQ(match.winner(), WinCondition::NONE);

    EXPECT_EQ(match.land_count(0, LandType::ISLAND), 0);
    EXPECT_EQ(match.land_count(1, LandType::ISLAND), 0);
    EXPECT_EQ(match.land_count(0, LandType::MOUNTAIN), 0);
    EXPECT_EQ(match.land_count(1, LandType::MOUNTAIN), 0);
    EXPECT_EQ(match.land_count(0, LandType::PLAINS), 0);
    EXPECT_EQ(match.land_count(1, LandType::PLAINS), 0);
    EXPECT_EQ(match.land_count(0, LandType::SWAMP), 0);
    EXPECT_EQ(match.land_count(1, LandType::SWAMP), 0);
    EXPECT_EQ(match.land_count(0, LandType::FOREST), 0);
    EXPECT_EQ(match.land_count(1, LandType::FOREST), 0);

    EXPECT_EQ(match.graveyard_size(), 0);
}

TEST(LandGameMatch, TurnManagement) {

    LandGameMatch match2(42);
    EXPECT_EQ(match2.active_player(), 0);
    EXPECT_EQ(match2.turn_number(), 1);
    EXPECT_EQ(match2.hand_size(0), 5);
    EXPECT_EQ(match2.hand_size(1), 6);
    EXPECT_EQ(match2.deck_size(), 39);
    EXPECT_FALSE(match2.end_turn(1));           // not P2's turn
    EXPECT_TRUE(match2.end_turn(0));            // → P2, still turn 1, no draw
    EXPECT_EQ(match2.active_player(), 1);
    EXPECT_EQ(match2.turn_number(), 1);
    EXPECT_EQ(match2.hand_size(1), 6);
    EXPECT_EQ(match2.deck_size(), 39);
    EXPECT_TRUE(match2.end_turn(1));            // → P1, turn 2, P1 draws
    EXPECT_EQ(match2.active_player(), 0);
    EXPECT_EQ(match2.turn_number(), 2);
    EXPECT_EQ(match2.hand_size(0), 6);          // 5 + 1
    EXPECT_EQ(match2.deck_size(), 38);
    

}

TEST(LandGameMatch, PlayLand) {

    LandGameMatch match3(12345);

    EXPECT_TRUE(match3.play_land(0, first_card_id(match3, 0)));
    EXPECT_TRUE(match3.played_land_this_turn());
    EXPECT_FALSE(match3.play_land(0, first_card_id(match3, 0)));  // second play same turn
    
    // assert hand/lands unchanged from after the first play
    EXPECT_TRUE(match3.end_turn(0));
    EXPECT_FALSE(match3.played_land_this_turn());
    EXPECT_TRUE(match3.play_land(1, first_card_id(match3, 1)));   // opponent can play
    
}

TEST(LandGameMatch, PlayLandMovesExactlyOneCardToBoard) {
    LandGameMatch match(12345);

    const Card played = match.hand(0).front();

    EXPECT_TRUE(match.play_land(0, played.get_identifier()));
    EXPECT_EQ(match.hand_size(0), 4);           // 5 - 1, not 5 - 2
    EXPECT_EQ(match.hand_size(1), 6);

    EXPECT_EQ(total_lands(match, 0), 1);        // added once, not twice
    EXPECT_EQ(match.land_count(0, played.get_land_type()), 1);
}

TEST(LandGameMatch, PlayLandByCardIdIgnoresHandPosition) {
    LandGameMatch match(12345);

    const Card last = match.hand(0).back();
    EXPECT_TRUE(match.play_land(0, last.get_identifier()));
    EXPECT_EQ(match.land_count(0, last.get_land_type()), 1);
    for (const Card& card : match.hand(0)) {
        EXPECT_NE(card.get_identifier(), last.get_identifier());
    }
}

TEST(LandGameMatch, PlayLandRejectsCardNotInOwnHand) {
    LandGameMatch match(12345);

    const uint32_t opponent_card = first_card_id(match, 1);
    EXPECT_FALSE(match.play_land(0, opponent_card));    // opponent's card
    EXPECT_FALSE(match.play_land(0, 9999));             // no such card
    EXPECT_EQ(match.hand_size(0), 5);
    EXPECT_EQ(match.hand_size(1), 6);
    EXPECT_FALSE(match.played_land_this_turn());
    EXPECT_EQ(match.stack_size(), 0);
}

TEST(LandGameMatch, GettersRejectBadPlayerIndex) {
    LandGameMatch match(12345);
    EXPECT_THROW(match.hand_size(-1), std::out_of_range);
    EXPECT_THROW(match.hand_size(2), std::out_of_range);
    EXPECT_THROW(match.land_count(-1, LandType::PLAINS), std::out_of_range);
    EXPECT_THROW(match.land_count(2, LandType::PLAINS), std::out_of_range);

    constexpr auto invalid =
        static_cast<LandType>(static_cast<uint8_t>(LandType::NUM_LAND_TYPES));
    EXPECT_THROW(match.land_count(0, invalid), std::out_of_range);
}

TEST(LandGameMatch, HandRejectsBadPlayerIndex) {
    LandGameMatch match(12345);
    EXPECT_THROW(match.hand(-1), std::out_of_range);
    EXPECT_THROW(match.hand(2), std::out_of_range);
}

TEST(LandGameMatch, ActionsRejectBadPlayerIndex) {
    LandGameMatch match(12345);
    EXPECT_FALSE(match.play_land(-1, 0));
    EXPECT_FALSE(match.play_land(2, 0));
    EXPECT_FALSE(match.end_turn(-1));
    EXPECT_FALSE(match.end_turn(2));
}

TEST(LandGameMatch, Stack) {
    LandGameMatch match(12345);
    EXPECT_EQ(match.stack_size(), 0);
    EXPECT_TRUE(match.play_land(0, first_card_id(match, 0)));
    EXPECT_EQ(match.stack_size(), 0);
    EXPECT_TRUE(match.end_turn(0));
    EXPECT_EQ(match.stack_size(), 0);
    EXPECT_EQ(match.winner(), WinCondition::NONE);
}

TEST(LandGameMatch, PitchRampMovesCardToGraveyard) {
    LandGameMatch match = make_match_with(0, {LandType::FOREST});
    const uint32_t forest = *find_in_hand(match, 0, LandType::FOREST);

    EXPECT_TRUE(match.pitch_land(0, forest));
    EXPECT_TRUE(match.pitched_this_turn());
    EXPECT_EQ(match.hand_size(0), 4);
    EXPECT_EQ(match.graveyard_size(), 1);
    EXPECT_EQ(match.deck_size(), 39);               // Ramp draws nothing
    EXPECT_EQ(match.stack_size(), 0);
    EXPECT_EQ(total_lands(match, 0), 0);            // pitched cards never reach the battlefield
    EXPECT_FALSE(match.played_land_this_turn());    // a pitch is not the mandatory deploy
}

TEST(LandGameMatch, RampAllowsOneExtraDeploy) {
    LandGameMatch match = make_match_with(0, {LandType::FOREST});

    EXPECT_TRUE(match.pitch_land(0, *find_in_hand(match, 0, LandType::FOREST)));
    EXPECT_TRUE(match.play_land(0, match.hand(0)[0].get_identifier()));
    EXPECT_TRUE(match.play_land(0, match.hand(0)[0].get_identifier()));     // the ramped deploy
    EXPECT_FALSE(match.play_land(0, match.hand(0)[0].get_identifier()));    // only one extra
    EXPECT_EQ(total_lands(match, 0), 2);
    EXPECT_EQ(match.hand_size(0), 2);               // 5 - 1 pitched - 2 deployed
    EXPECT_EQ(match.graveyard_size(), 1);
}

TEST(LandGameMatch, RampPitchedAfterFirstDeployStillGrantsExtraDeploy) {
    LandGameMatch match = make_match_with(0, {LandType::FOREST});
    const uint32_t forest = *find_in_hand(match, 0, LandType::FOREST);

    EXPECT_TRUE(match.play_land(0, first_card_other_than(match, 0, forest)));
    EXPECT_EQ(match.hand_size(0), 4);

    // a second deploy is illegal until Ramp resolves
    EXPECT_FALSE(match.play_land(0, first_card_other_than(match, 0, forest)));

    EXPECT_TRUE(match.pitch_land(0, forest));
    EXPECT_TRUE(match.play_land(0, first_card_other_than(match, 0, forest)));
    EXPECT_FALSE(match.play_land(0, first_card_other_than(match, 0, forest)));
    EXPECT_EQ(total_lands(match, 0), 2);
    EXPECT_EQ(match.hand_size(0), 2);
    EXPECT_EQ(match.graveyard_size(), 1);
}

TEST(LandGameMatch, UnspentRampDoesNotCarryOver) {
    LandGameMatch match = make_match_with(0, {LandType::FOREST});

    EXPECT_TRUE(match.pitch_land(0, *find_in_hand(match, 0, LandType::FOREST)));
    EXPECT_TRUE(match.end_turn(0));
    EXPECT_TRUE(match.end_turn(1));

    EXPECT_FALSE(match.pitched_this_turn());
    EXPECT_TRUE(match.play_land(0, match.hand(0)[0].get_identifier()));
    EXPECT_FALSE(match.play_land(0, match.hand(0)[0].get_identifier()));    // ramp expired
}

TEST(LandGameMatch, OnlyOneMainPhasePitchPerTurn) {
    LandGameMatch match = make_match_with(0, {LandType::FOREST, LandType::MOUNTAIN});
    const uint32_t forest = *find_in_hand(match, 0, LandType::FOREST);
    const uint32_t mountain = *find_in_hand(match, 0, LandType::MOUNTAIN);

    EXPECT_TRUE(match.pitch_land(0, forest));
    EXPECT_FALSE(match.pitch_land(0, mountain, first_card_other_than(match, 0, mountain)));
    EXPECT_EQ(match.hand_size(0), 4);
    EXPECT_EQ(match.graveyard_size(), 1);

    EXPECT_TRUE(match.end_turn(0));
    EXPECT_TRUE(match.end_turn(1));
    EXPECT_TRUE(match.pitch_land(0, mountain, first_card_other_than(match, 0, mountain)));  // new turn
}

TEST(LandGameMatch, PitchRummageDiscardsOneAndDrawsTwo) {
    LandGameMatch match = make_match_with(0, {LandType::MOUNTAIN});
    const uint32_t mountain = *find_in_hand(match, 0, LandType::MOUNTAIN);
    const uint32_t discard = first_card_other_than(match, 0, mountain);

    EXPECT_TRUE(match.pitch_land(0, mountain, discard));
    EXPECT_TRUE(match.pitched_this_turn());
    EXPECT_EQ(match.hand_size(0), 5);               // 5 - 2 + 2
    EXPECT_EQ(match.graveyard_size(), 2);           // the Mountain and the discarded card
    EXPECT_EQ(match.deck_size(), 37);
    EXPECT_EQ(match.stack_size(), 0);
    EXPECT_EQ(total_lands(match, 0), 0);
    for (const Card& card : match.hand(0)) {
        EXPECT_NE(card.get_identifier(), mountain);
        EXPECT_NE(card.get_identifier(), discard);
    }
}

TEST(LandGameMatch, PitchRummageRequiresDistinctCostCardInHand) {
    LandGameMatch match = make_match_with(0, {LandType::MOUNTAIN});
    const uint32_t mountain = *find_in_hand(match, 0, LandType::MOUNTAIN);

    EXPECT_FALSE(match.pitch_land(0, mountain));                            // no cost card
    EXPECT_FALSE(match.pitch_land(0, mountain, mountain));                  // cannot pay with itself
    EXPECT_FALSE(match.pitch_land(0, mountain, 9999));                      // no such card
    EXPECT_FALSE(match.pitch_land(0, mountain, first_card_id(match, 1)));   // opponent's card

    EXPECT_FALSE(match.pitched_this_turn());
    EXPECT_EQ(match.hand_size(0), 5);
    EXPECT_EQ(match.hand_size(1), 6);
    EXPECT_EQ(match.graveyard_size(), 0);
    EXPECT_EQ(match.deck_size(), 39);
}

TEST(LandGameMatch, PitchRampRejectsCostCard) {
    LandGameMatch match = make_match_with(0, {LandType::FOREST});
    const uint32_t forest = *find_in_hand(match, 0, LandType::FOREST);

    EXPECT_FALSE(match.pitch_land(0, forest, first_card_other_than(match, 0, forest)));
    EXPECT_FALSE(match.pitched_this_turn());
    EXPECT_EQ(match.hand_size(0), 5);
    EXPECT_EQ(match.graveyard_size(), 0);
}

TEST(LandGameMatch, PitchRummageWithShortLibraryDrawsWhatRemains) {
    for (uint64_t seed = 1; seed < 1000; ++seed) {
        LandGameMatch match(seed);
        while (match.deck_size() > 1) {
            ASSERT_TRUE(match.end_turn(match.active_player()));
        }
        const int player = match.active_player();
        const std::optional<uint32_t> mountain = find_in_hand(match, player, LandType::MOUNTAIN);
        if (!mountain.has_value()) {
            continue;
        }
        const size_t hand_before = match.hand_size(player);

        EXPECT_TRUE(match.pitch_land(player, *mountain, first_card_other_than(match, player, *mountain)));
        EXPECT_EQ(match.deck_size(), 0);
        EXPECT_EQ(match.hand_size(player), hand_before - 2 + 1);    // only 1 card was left to draw
        return;
    }
    FAIL() << "no seed produced a Mountain in the active player's hand";
}

TEST(LandGameMatch, PitchRejectsInstantSpeedAndUnwiredPitches) {
    LandGameMatch match = make_match_with(0, {LandType::PLAINS, LandType::ISLAND, LandType::SWAMP});

    for (LandType land_type : {LandType::PLAINS, LandType::ISLAND, LandType::SWAMP}) {
        const uint32_t card = *find_in_hand(match, 0, land_type);
        EXPECT_FALSE(match.pitch_land(0, card));
        EXPECT_FALSE(match.pitch_land(0, card, first_card_other_than(match, 0, card)));
    }
    EXPECT_FALSE(match.pitched_this_turn());
    EXPECT_EQ(match.hand_size(0), 5);
    EXPECT_EQ(match.graveyard_size(), 0);
}

TEST(LandGameMatch, PitchRejectsWrongPlayerAndBadIds) {
    LandGameMatch match = make_match_with(1, {LandType::FOREST});
    const uint32_t forest = *find_in_hand(match, 1, LandType::FOREST);

    EXPECT_FALSE(match.pitch_land(1, forest));      // not P2's turn
    EXPECT_FALSE(match.pitch_land(-1, forest));
    EXPECT_FALSE(match.pitch_land(2, forest));
    EXPECT_FALSE(match.pitch_land(0, forest));      // P1 does not hold that card
    EXPECT_FALSE(match.pitch_land(0, 9999));
    EXPECT_EQ(match.hand_size(0), 5);
    EXPECT_EQ(match.hand_size(1), 6);
    EXPECT_EQ(match.graveyard_size(), 0);

    EXPECT_TRUE(match.end_turn(0));
    EXPECT_TRUE(match.pitch_land(1, forest));       // now it is
}

TEST(LandGameMatch, DeployIslandDrawsACard) {
    LandGameMatch match = make_match_with(0, {LandType::ISLAND});
    const uint32_t island = *find_in_hand(match, 0, LandType::ISLAND);

    EXPECT_TRUE(match.play_land(0, island));
    EXPECT_EQ(match.land_count(0, LandType::ISLAND), 1);
    EXPECT_EQ(match.hand_size(0), 5);               // 5 - 1 deployed + 1 drawn
    EXPECT_EQ(match.deck_size(), 38);
    EXPECT_EQ(match.graveyard_size(), 0);
    EXPECT_EQ(match.stack_size(), 0);
}

TEST(LandGameMatch, DeployNonIslandDrawsNothing) {
    LandGameMatch match = make_match_with(0, {LandType::FOREST});
    const uint32_t forest = *find_in_hand(match, 0, LandType::FOREST);

    EXPECT_TRUE(match.play_land(0, forest));
    EXPECT_EQ(match.hand_size(0), 4);
    EXPECT_EQ(match.deck_size(), 39);
}

TEST(LandGameMatch, RampedIslandEntersVanillaAndDrawsNothing) {
    LandGameMatch match = make_match_with(0, {LandType::FOREST, LandType::ISLAND});
    const uint32_t forest = *find_in_hand(match, 0, LandType::FOREST);
    const uint32_t island = *find_in_hand(match, 0, LandType::ISLAND);

    EXPECT_TRUE(match.pitch_land(0, forest));
    EXPECT_TRUE(match.play_land(0, first_card_other_than(match, 0, island)));   // normal deploy
    ASSERT_FALSE(match.hand(0).empty());
    const size_t hand_before = match.hand_size(0);
    const size_t deck_before = match.deck_size();

    EXPECT_TRUE(match.play_land(0, island));                                    // the ramped deploy
    EXPECT_EQ(match.land_count(0, LandType::ISLAND), 1);
    EXPECT_EQ(match.hand_size(0), hand_before - 1);     // no draw: vanilla
    EXPECT_EQ(match.deck_size(), deck_before);
}

TEST(LandGameMatch, DeployIslandWithEmptyLibraryStillEnters) {
    for (uint64_t seed = 1; seed < 1000; ++seed) {
        LandGameMatch match(seed);
        while (match.deck_size() > 0) {
            ASSERT_TRUE(match.end_turn(match.active_player()));
        }
        const int player = match.active_player();
        const std::optional<uint32_t> island = find_in_hand(match, player, LandType::ISLAND);
        if (!island.has_value()) {
            continue;
        }
        const size_t hand_before = match.hand_size(player);

        EXPECT_TRUE(match.play_land(player, *island));
        EXPECT_EQ(match.land_count(player, LandType::ISLAND), 1);
        EXPECT_EQ(match.hand_size(player), hand_before - 1);
        EXPECT_EQ(match.deck_size(), 0);
        return;
    }
    FAIL() << "no seed produced an Island in the active player's hand";
}

} // namespace lands