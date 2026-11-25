#include "gtest/gtest.h"
#include "lands/LandGameMatch.hpp"

#include <algorithm>
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

size_t count_in_hand(const LandGameMatch& match, int player_index, LandType land_type) {
    size_t count = 0;
    for (const Card& card : match.hand(player_index)) {
        if (card.get_land_type() == land_type) {
            ++count;
        }
    }
    return count;
}

// first card in the player's hand that is not of `land_type`
uint32_t first_card_not_of_type(const LandGameMatch& match, int player_index, LandType land_type) {
    for (const Card& card : match.hand(player_index)) {
        if (card.get_land_type() != land_type) {
            return card.get_identifier();
        }
    }
    ADD_FAILURE() << "hand holds only that land type";
    return 0;
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

// deterministic: the first seed whose fresh match satisfies `matches`
template <typename Predicate>
LandGameMatch make_match_where(Predicate matches) {
    for (uint64_t seed = 1; seed < 10000; ++seed) {
        LandGameMatch match(seed);
        if (matches(match)) {
            return match;
        }
    }
    ADD_FAILURE() << "no seed found with the requested starting hands";
    return LandGameMatch(0);
}

// P1 can counter (holds an Island), P0 cannot: P0's actions open a window for P1 only
LandGameMatch make_match_where_only_p1_can_counter() {
    return make_match_where([](const LandGameMatch& match) {
        return count_in_hand(match, 1, LandType::ISLAND) == 1 &&
               count_in_hand(match, 0, LandType::ISLAND) == 0;
    });
}

// A match where `player_index` holds every listed land type and the opponent holds
// no Island, so no priority window opens and every action resolves immediately.
LandGameMatch make_match_with(int player_index, const std::vector<LandType>& land_types) {
    return make_match_where([&](const LandGameMatch& match) {
        for (LandType land_type : land_types) {
            if (!find_in_hand(match, player_index, land_type).has_value()) {
                return false;
            }
        }
        return !find_in_hand(match, 1 - player_index, LandType::ISLAND).has_value();
    });
}

// resolve a window left open by the previous action, if any
void settle(LandGameMatch& match) {
    if (match.priority_player() != -1) {
        EXPECT_TRUE(match.pass_priority(match.priority_player()));
    }
}

// Scenario for Destroy / Save: P0 deployed a land and P1 deployed one (`battlefield(1)[0]`),
// and it is now P0's turn 2 with a Mountain in hand. Neither player starts with an Island
// unless `p0_island` is set (P0 only); P1 starts with a Plains iff `p1_plains`.
// `keep` lists land types P0 must still hold afterwards (Mountain is always kept).
LandGameMatch make_destroy_scenario(bool p1_plains, bool p0_island,
                                    const std::vector<LandType>& keep = {}) {
    auto not_kept = [&](const Card& card) {
        const LandType land_type = card.get_land_type();
        return land_type != LandType::MOUNTAIN && land_type != LandType::ISLAND &&
               std::find(keep.begin(), keep.end(), land_type) == keep.end();
    };
    for (uint64_t seed = 1; seed < 20000; ++seed) {
        LandGameMatch match(seed);
        if (!find_in_hand(match, 0, LandType::MOUNTAIN).has_value() ||
            find_in_hand(match, 0, LandType::ISLAND).has_value() != p0_island ||
            find_in_hand(match, 1, LandType::ISLAND).has_value() ||
            find_in_hand(match, 1, LandType::PLAINS).has_value() != p1_plains) {
            continue;
        }
        bool kept_all = true;
        for (LandType land_type : keep) {
            kept_all = kept_all && find_in_hand(match, 0, land_type).has_value();
        }
        const auto& p0_hand = match.hand(0);
        const auto& p1_hand = match.hand(1);
        const auto p0_deploy = std::find_if(p0_hand.begin(), p0_hand.end(), not_kept);
        const auto p1_deploy = std::find_if(p1_hand.begin(), p1_hand.end(), [](const Card& card) {
            return card.get_land_type() != LandType::MOUNTAIN && card.get_land_type() != LandType::PLAINS;
        });
        if (!kept_all || p0_deploy == p0_hand.end() || p1_deploy == p1_hand.end()) {
            continue;
        }

        const uint32_t p0_id = p0_deploy->get_identifier();
        const uint32_t p1_id = p1_deploy->get_identifier();
        if (!match.play_land(0, p0_id)) { continue; }
        settle(match);
        if (!match.end_turn(0) || !match.play_land(1, p1_id)) { continue; }
        settle(match);
        if (!match.end_turn(1)) { continue; }

        // P0 drew a card: it must not have changed the scenario
        if (find_in_hand(match, 0, LandType::ISLAND).has_value() != p0_island ||
            !find_in_hand(match, 0, LandType::MOUNTAIN).has_value() ||
            match.hand_size(0) < 4) {
            continue;
        }
        return match;
    }
    ADD_FAILURE() << "no seed produced the Destroy scenario";
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
    settle(match3);
    EXPECT_TRUE(match3.played_land_this_turn());
    EXPECT_FALSE(match3.play_land(0, first_card_id(match3, 0)));  // second play same turn
    
    // assert hand/lands unchanged from after the first play
    EXPECT_TRUE(match3.end_turn(0));
    EXPECT_FALSE(match3.played_land_this_turn());
    EXPECT_TRUE(match3.play_land(1, first_card_id(match3, 1)));   // opponent can play
    settle(match3);
    
}

TEST(LandGameMatch, PlayLandMovesExactlyOneCardToBoard) {
    LandGameMatch match(12345);

    const Card played = match.hand(0).front();

    EXPECT_TRUE(match.play_land(0, played.get_identifier()));
    settle(match);
    EXPECT_EQ(match.hand_size(0), 4);           // 5 - 1, not 5 - 2
    EXPECT_EQ(match.hand_size(1), 6);

    EXPECT_EQ(total_lands(match, 0), 1);        // added once, not twice
    EXPECT_EQ(match.land_count(0, played.get_land_type()), 1);
}

TEST(LandGameMatch, PlayLandByCardIdIgnoresHandPosition) {
    LandGameMatch match(12345);

    const Card last = match.hand(0).back();
    EXPECT_TRUE(match.play_land(0, last.get_identifier()));
    settle(match);
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
    settle(match);
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
        settle(match);
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
        settle(match);
        EXPECT_EQ(match.land_count(player, LandType::ISLAND), 1);
        EXPECT_EQ(match.hand_size(player), hand_before - 1);
        EXPECT_EQ(match.deck_size(), 0);
        return;
    }
    FAIL() << "no seed produced an Island in the active player's hand";
}

TEST(LandGameMatch, DeployOpensPriorityWindowWhenOpponentCanCounter) {
    LandGameMatch match = make_match_where_only_p1_can_counter();
    const Card deployed = match.hand(0).front();

    EXPECT_EQ(match.priority_player(), -1);
    EXPECT_TRUE(match.play_land(0, deployed.get_identifier()));
    EXPECT_EQ(match.priority_player(), 1);              // the opponent of the pusher
    EXPECT_EQ(match.stack_size(), 1);
    EXPECT_EQ(match.stack_top().mode, PlayMode::DEPLOY);
    EXPECT_EQ(match.stack_top().caster_id, 0);
    EXPECT_EQ(match.stack_top().card, deployed);
    EXPECT_EQ(total_lands(match, 0), 0);                // still on the stack, not the battlefield
    EXPECT_EQ(match.hand_size(0), 4);
    EXPECT_TRUE(match.played_land_this_turn());
}

TEST(LandGameMatch, DeployAutoPassesWhenOpponentHasNoIsland) {
    LandGameMatch match = make_match_with(0, {LandType::FOREST});

    EXPECT_TRUE(match.play_land(0, match.hand(0).front().get_identifier()));
    EXPECT_EQ(match.priority_player(), -1);
    EXPECT_EQ(match.stack_size(), 0);
    EXPECT_EQ(total_lands(match, 0), 1);
}

TEST(LandGameMatch, PassPriorityResolvesPendingDeploy) {
    LandGameMatch match = make_match_where_only_p1_can_counter();
    const Card deployed = match.hand(0).front();
    ASSERT_TRUE(match.play_land(0, deployed.get_identifier()));

    EXPECT_TRUE(match.pass_priority(1));
    EXPECT_EQ(match.priority_player(), -1);
    EXPECT_EQ(match.stack_size(), 0);
    EXPECT_EQ(match.land_count(0, deployed.get_land_type()), 1);
    EXPECT_EQ(total_lands(match, 0), 1);
    EXPECT_EQ(match.graveyard_size(), 0);
    EXPECT_FALSE(match.pass_priority(1));               // nothing left to pass on
}

TEST(LandGameMatch, PriorityWindowBlocksOtherActions) {
    LandGameMatch match = make_match_where([](const LandGameMatch& m) {
        return count_in_hand(m, 1, LandType::ISLAND) == 1 &&
               count_in_hand(m, 0, LandType::ISLAND) == 0 &&
               count_in_hand(m, 0, LandType::FOREST) >= 1;
    });
    const uint32_t forest = *find_in_hand(match, 0, LandType::FOREST);
    ASSERT_TRUE(match.play_land(0, first_card_not_of_type(match, 0, LandType::FOREST)));
    ASSERT_EQ(match.priority_player(), 1);

    EXPECT_FALSE(match.play_land(0, forest));
    EXPECT_FALSE(match.pitch_land(0, forest));
    EXPECT_FALSE(match.end_turn(0));
    EXPECT_FALSE(match.play_land(1, first_card_id(match, 1)));  // not P2's turn anyway
    EXPECT_EQ(match.priority_player(), 1);
    EXPECT_EQ(match.stack_size(), 1);
    EXPECT_EQ(match.hand_size(0), 4);
    EXPECT_EQ(match.graveyard_size(), 0);
}

TEST(LandGameMatch, PassPriorityRejectsWrongPlayerAndNoWindow) {
    LandGameMatch match = make_match_where_only_p1_can_counter();
    EXPECT_FALSE(match.pass_priority(0));               // no window open
    EXPECT_FALSE(match.pass_priority(1));

    ASSERT_TRUE(match.play_land(0, first_card_id(match, 0)));
    EXPECT_FALSE(match.pass_priority(0));               // the pusher holds no priority
    EXPECT_FALSE(match.pass_priority(-1));
    EXPECT_FALSE(match.pass_priority(2));
    EXPECT_EQ(match.priority_player(), 1);
    EXPECT_EQ(match.stack_size(), 1);
}

TEST(LandGameMatch, CounterSendsDeployedLandToTopOfLibrary) {
    LandGameMatch match = make_match_where_only_p1_can_counter();
    const Card deployed = match.hand(0).front();
    const uint32_t island = *find_in_hand(match, 1, LandType::ISLAND);
    const uint32_t extra = first_card_other_than(match, 1, island);
    ASSERT_TRUE(match.play_land(0, deployed.get_identifier()));

    EXPECT_TRUE(match.cast_counter(1, island, extra, match.stack_top().stack_id));
    EXPECT_EQ(match.priority_player(), -1);             // P0 cannot respond: auto-pass
    EXPECT_EQ(match.stack_size(), 0);
    EXPECT_EQ(total_lands(match, 0), 0);                // the land never entered
    EXPECT_EQ(match.hand_size(0), 4);
    EXPECT_EQ(match.hand_size(1), 4);                   // 6 - Island - extra
    EXPECT_EQ(match.graveyard_size(), 2);               // Island + cost card; the land is not there
    EXPECT_EQ(match.deck_size(), 40);                   // the land went back on top

    // the deploy is still used up, and the countered land is the next card drawn
    EXPECT_TRUE(match.played_land_this_turn());
    EXPECT_FALSE(match.play_land(0, first_card_id(match, 0)));
    EXPECT_TRUE(match.end_turn(0));
    EXPECT_TRUE(match.end_turn(1));                     // P0 draws: Memory Lapse
    EXPECT_NE(std::find(match.hand(0).begin(), match.hand(0).end(), deployed), match.hand(0).end());
}

TEST(LandGameMatch, CounterFizzlesPitchEffect) {
    LandGameMatch match = make_match_where([](const LandGameMatch& m) {
        return count_in_hand(m, 1, LandType::ISLAND) == 1 &&
               count_in_hand(m, 0, LandType::ISLAND) == 0 &&
               count_in_hand(m, 0, LandType::MOUNTAIN) >= 1;
    });
    const uint32_t mountain = *find_in_hand(match, 0, LandType::MOUNTAIN);
    const uint32_t discard = first_card_other_than(match, 0, mountain);
    const uint32_t island = *find_in_hand(match, 1, LandType::ISLAND);
    ASSERT_TRUE(match.pitch_land(0, mountain, discard));
    ASSERT_EQ(match.priority_player(), 1);

    EXPECT_TRUE(match.cast_counter(1, island, first_card_other_than(match, 1, island),
                                   match.stack_top().stack_id));
    EXPECT_EQ(match.stack_size(), 0);
    EXPECT_EQ(match.hand_size(0), 3);                   // 5 - 2, Rummage never drew
    EXPECT_EQ(match.deck_size(), 39);
    EXPECT_EQ(match.graveyard_size(), 4);               // Mountain, discard, Island, cost card
    EXPECT_TRUE(match.pitched_this_turn());             // the pitch is used up
}

TEST(LandGameMatch, CounteredRampGrantsNoExtraDeploy) {
    LandGameMatch match = make_match_where([](const LandGameMatch& m) {
        return count_in_hand(m, 1, LandType::ISLAND) == 1 &&
               count_in_hand(m, 0, LandType::ISLAND) == 0 &&
               count_in_hand(m, 0, LandType::FOREST) >= 1;
    });
    const uint32_t forest = *find_in_hand(match, 0, LandType::FOREST);
    const uint32_t island = *find_in_hand(match, 1, LandType::ISLAND);
    ASSERT_TRUE(match.pitch_land(0, forest));
    ASSERT_TRUE(match.cast_counter(1, island, first_card_other_than(match, 1, island),
                                   match.stack_top().stack_id));

    EXPECT_TRUE(match.play_land(0, first_card_id(match, 0)));
    EXPECT_FALSE(match.play_land(0, first_card_id(match, 0)));  // no ramp was granted
    EXPECT_EQ(total_lands(match, 0), 1);
}

TEST(LandGameMatch, CounterWarResolvesLifo) {
    LandGameMatch match = make_match_where([](const LandGameMatch& m) {
        return count_in_hand(m, 0, LandType::ISLAND) == 1 &&
               count_in_hand(m, 1, LandType::ISLAND) == 1;
    });
    const uint32_t island0 = *find_in_hand(match, 0, LandType::ISLAND);
    const uint32_t island1 = *find_in_hand(match, 1, LandType::ISLAND);
    const Card deployed = match.hand(0)[0].get_land_type() == LandType::ISLAND
                              ? match.hand(0)[1] : match.hand(0)[0];
    ASSERT_TRUE(match.play_land(0, deployed.get_identifier()));
    ASSERT_EQ(match.priority_player(), 1);

    // P2 counters the land, P1 counters the counter, P2 has no Island left to answer
    const uint32_t land_id = match.stack_top().stack_id;
    ASSERT_TRUE(match.cast_counter(1, island1, first_card_other_than(match, 1, island1), land_id));
    ASSERT_EQ(match.priority_player(), 0);
    EXPECT_EQ(match.stack_size(), 2);
    ASSERT_TRUE(match.cast_counter(0, island0, first_card_other_than(match, 0, island0),
                                   match.stack_top().stack_id));

    EXPECT_EQ(match.priority_player(), -1);
    EXPECT_EQ(match.stack_size(), 0);
    EXPECT_EQ(match.land_count(0, deployed.get_land_type()), 1);    // Counter 1 was countered
    EXPECT_EQ(match.deck_size(), 39);                               // nothing went back on top
    EXPECT_EQ(match.graveyard_size(), 4);                           // two Islands + two cost cards
    EXPECT_EQ(match.hand_size(0), 2);                               // 5 - land - Island - cost
    EXPECT_EQ(match.hand_size(1), 4);                               // 6 - Island - cost
}

TEST(LandGameMatch, CounterRejectionsLeaveStateUntouched) {
    LandGameMatch match = make_match_where([](const LandGameMatch& m) {
        return count_in_hand(m, 1, LandType::ISLAND) == 1 &&
               count_in_hand(m, 1, LandType::FOREST) >= 1 &&
               count_in_hand(m, 0, LandType::ISLAND) == 0;
    });
    const uint32_t island = *find_in_hand(match, 1, LandType::ISLAND);
    const uint32_t forest = *find_in_hand(match, 1, LandType::FOREST);
    const uint32_t extra = first_card_other_than(match, 1, island);

    // no window open yet
    EXPECT_FALSE(match.cast_counter(1, island, extra, 1));

    ASSERT_TRUE(match.play_land(0, first_card_id(match, 0)));
    const uint32_t target = match.stack_top().stack_id;

    EXPECT_FALSE(match.cast_counter(0, island, extra, target));         // P1 holds priority, not P0
    EXPECT_FALSE(match.cast_counter(-1, island, extra, target));
    EXPECT_FALSE(match.cast_counter(2, island, extra, target));
    EXPECT_FALSE(match.cast_counter(1, forest, extra, target));         // not an Island
    EXPECT_FALSE(match.cast_counter(1, 9999, extra, target));           // no such card
    EXPECT_FALSE(match.cast_counter(1, island, island, target));        // cannot pay with itself
    EXPECT_FALSE(match.cast_counter(1, island, 9999, target));          // cost card missing
    EXPECT_FALSE(match.cast_counter(1, island, first_card_id(match, 0), target));   // opponent's card
    EXPECT_FALSE(match.cast_counter(1, island, extra, 9999));           // target not on the stack

    EXPECT_EQ(match.priority_player(), 1);
    EXPECT_EQ(match.stack_size(), 1);
    EXPECT_EQ(match.hand_size(1), 6);
    EXPECT_EQ(match.graveyard_size(), 0);
}

TEST(LandGameMatch, DestroyRemovesTargetedOpponentLand) {
    LandGameMatch match = make_destroy_scenario(false, false);
    const Card target = match.battlefield(1)[0];
    const uint32_t mountain = *find_in_hand(match, 0, LandType::MOUNTAIN);
    const size_t hand_before = match.hand_size(0);

    EXPECT_TRUE(match.play_land(0, mountain, target.get_identifier()));
    EXPECT_EQ(match.stack_size(), 0);
    EXPECT_TRUE(match.battlefield(1).empty());
    EXPECT_EQ(total_lands(match, 1), 0);
    EXPECT_EQ(match.land_count(0, LandType::MOUNTAIN), 1);          // the Mountain still enters
    EXPECT_EQ(match.hand_size(0), hand_before - 1);
    ASSERT_EQ(match.graveyard_size(), 1);                           // destroyed lands go to the shared graveyard
}

TEST(LandGameMatch, DestroyRequiresValidOpponentTarget) {
    LandGameMatch match = make_destroy_scenario(false, false);
    const uint32_t mountain = *find_in_hand(match, 0, LandType::MOUNTAIN);
    const uint32_t own_land = match.battlefield(0)[0].get_identifier();
    const size_t hand_before = match.hand_size(0);

    EXPECT_FALSE(match.play_land(0, mountain));                     // a target exists, so one is required
    EXPECT_FALSE(match.play_land(0, mountain, own_land));           // not an opponent land
    EXPECT_FALSE(match.play_land(0, mountain, 9999));               // no such card
    EXPECT_FALSE(match.play_land(0, mountain, mountain));           // in hand, not on the battlefield
    EXPECT_FALSE(match.played_land_this_turn());
    EXPECT_EQ(match.hand_size(0), hand_before);
    EXPECT_EQ(match.stack_size(), 0);
    EXPECT_EQ(total_lands(match, 1), 1);
    EXPECT_EQ(match.graveyard_size(), 0);
}

TEST(LandGameMatch, DestroyEntersVanillaWhenOpponentHasNoLands) {
    LandGameMatch match = make_match_with(0, {LandType::MOUNTAIN});
    const uint32_t mountain = *find_in_hand(match, 0, LandType::MOUNTAIN);

    EXPECT_FALSE(match.play_land(0, mountain, 9999));               // nothing to target
    EXPECT_TRUE(match.play_land(0, mountain));
    EXPECT_EQ(match.land_count(0, LandType::MOUNTAIN), 1);
    EXPECT_EQ(match.graveyard_size(), 0);
}

TEST(LandGameMatch, LandsWithoutTargetsRejectATarget) {
    LandGameMatch match = make_destroy_scenario(false, false, {LandType::FOREST});
    const uint32_t forest = *find_in_hand(match, 0, LandType::FOREST);
    const uint32_t target = match.battlefield(1)[0].get_identifier();

    EXPECT_FALSE(match.play_land(0, forest, target));
    EXPECT_TRUE(match.play_land(0, forest));
    EXPECT_EQ(total_lands(match, 1), 1);                            // nothing was destroyed
}

TEST(LandGameMatch, RampedMountainEntersVanilla) {
    LandGameMatch match = make_destroy_scenario(false, false, {LandType::FOREST});
    const uint32_t forest = *find_in_hand(match, 0, LandType::FOREST);
    const uint32_t mountain = *find_in_hand(match, 0, LandType::MOUNTAIN);
    const uint32_t target = match.battlefield(1)[0].get_identifier();

    ASSERT_TRUE(match.pitch_land(0, forest));
    ASSERT_TRUE(match.play_land(0, first_card_not_of_type(match, 0, LandType::MOUNTAIN)));
    EXPECT_FALSE(match.play_land(0, mountain, target));             // vanilla: no target allowed
    EXPECT_TRUE(match.play_land(0, mountain));
    EXPECT_EQ(match.land_count(0, LandType::MOUNTAIN), 1);
    EXPECT_EQ(total_lands(match, 1), 1);                            // the ramped Mountain destroyed nothing
}

TEST(LandGameMatch, SaveOpensWindowOnlyWhenOpponentLandIsTargeted) {
    LandGameMatch match = make_destroy_scenario(true, false, {LandType::FOREST});
    const uint32_t forest = *find_in_hand(match, 0, LandType::FOREST);

    EXPECT_TRUE(match.play_land(0, forest));                        // targets nothing: auto-pass
    EXPECT_EQ(match.priority_player(), -1);
    EXPECT_TRUE(match.end_turn(0));
    EXPECT_TRUE(match.end_turn(1));

    LandGameMatch targeted = make_destroy_scenario(true, false);
    EXPECT_TRUE(targeted.play_land(0, *find_in_hand(targeted, 0, LandType::MOUNTAIN),
                                   targeted.battlefield(1)[0].get_identifier()));
    EXPECT_EQ(targeted.priority_player(), 1);                       // P2 holds a Plains and is a target
    EXPECT_EQ(targeted.stack_size(), 1);
    EXPECT_EQ(total_lands(targeted, 1), 1);                         // still unresolved
}

TEST(LandGameMatch, SaveMakesDestroyFizzleButMountainStillEnters) {
    LandGameMatch match = make_destroy_scenario(true, false);
    const Card saved_land = match.battlefield(1)[0];
    const uint32_t plains = *find_in_hand(match, 1, LandType::PLAINS);
    ASSERT_TRUE(match.play_land(0, *find_in_hand(match, 0, LandType::MOUNTAIN), saved_land.get_identifier()));
    ASSERT_EQ(match.priority_player(), 1);
    const size_t p1_hand_before = match.hand_size(1);

    EXPECT_TRUE(match.cast_save(1, plains, saved_land.get_identifier()));
    EXPECT_EQ(match.priority_player(), -1);                         // P0 holds no counter: auto-pass
    EXPECT_EQ(match.stack_size(), 0);
    ASSERT_EQ(match.battlefield(1).size(), 1);
    EXPECT_EQ(match.battlefield(1)[0], saved_land);                 // survived
    EXPECT_EQ(match.land_count(0, LandType::MOUNTAIN), 1);          // saved against, not countered
    EXPECT_EQ(match.hand_size(1), p1_hand_before - 1);
    EXPECT_EQ(match.graveyard_size(), 1);                           // just the Plains
}

TEST(LandGameMatch, SaveRejectionsLeaveStateUntouched) {
    LandGameMatch match = make_destroy_scenario(true, false);
    const Card target = match.battlefield(1)[0];
    const uint32_t plains = *find_in_hand(match, 1, LandType::PLAINS);
    const uint32_t not_plains = first_card_not_of_type(match, 1, LandType::PLAINS);
    const uint32_t own_land = match.battlefield(0)[0].get_identifier();

    EXPECT_FALSE(match.cast_save(1, plains, target.get_identifier()));  // no window open

    ASSERT_TRUE(match.play_land(0, *find_in_hand(match, 0, LandType::MOUNTAIN), target.get_identifier()));
    EXPECT_FALSE(match.cast_save(0, plains, target.get_identifier()));  // not the priority holder
    EXPECT_FALSE(match.cast_save(-1, plains, target.get_identifier()));
    EXPECT_FALSE(match.cast_save(1, not_plains, target.get_identifier()));  // not a Plains
    EXPECT_FALSE(match.cast_save(1, 9999, target.get_identifier()));    // no such card
    EXPECT_FALSE(match.cast_save(1, plains, own_land));                 // opponent's land, and untargeted
    EXPECT_FALSE(match.cast_save(1, plains, 9999));                     // no such land

    EXPECT_EQ(match.priority_player(), 1);
    EXPECT_EQ(match.stack_size(), 1);
    EXPECT_EQ(match.graveyard_size(), 0);
}

TEST(LandGameMatch, CounteredSaveLetsDestroyResolve) {
    LandGameMatch match = make_destroy_scenario(true, true);
    const Card target = match.battlefield(1)[0];
    const uint32_t plains = *find_in_hand(match, 1, LandType::PLAINS);
    const uint32_t island = *find_in_hand(match, 0, LandType::ISLAND);
    ASSERT_TRUE(match.play_land(0, *find_in_hand(match, 0, LandType::MOUNTAIN), target.get_identifier()));
    ASSERT_EQ(match.priority_player(), 1);
    ASSERT_TRUE(match.cast_save(1, plains, target.get_identifier()));
    ASSERT_EQ(match.priority_player(), 0);                          // P0 can answer with its Island

    EXPECT_TRUE(match.cast_counter(0, island, first_card_other_than(match, 0, island),
                                   match.stack_top().stack_id));    // counter the Save
    EXPECT_EQ(match.stack_size(), 0);
    EXPECT_TRUE(match.battlefield(1).empty());                      // the Save never applied
    EXPECT_EQ(match.land_count(0, LandType::MOUNTAIN), 1);
    EXPECT_EQ(match.graveyard_size(), 4);                           // Plains, Island, cost card, destroyed land
}

} // namespace lands