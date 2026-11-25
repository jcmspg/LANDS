#include "gtest/gtest.h"
#include "lands/LandGameMatch.hpp"

#include <algorithm>
#include <optional>
#include <set>
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

// A match where `player_index` holds every listed land type plus two Plains and the opponent
// holds no Island, so no priority window opens and every action resolves immediately.
LandGameMatch make_match_with(int player_index, const std::vector<LandType>& land_types) {
    return make_match_where([&](const LandGameMatch& match) {
        for (LandType land_type : land_types) {
            if (!find_in_hand(match, player_index, land_type).has_value()) {
                return false;
            }
        }
        // two spare Plains keep plain_card() satisfiable after pitches and deploys
        return count_in_hand(match, player_index, LandType::PLAINS) >= 2 &&
               !find_in_hand(match, 1 - player_index, LandType::ISLAND).has_value();
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
// unless `p0_island` is set (P0 only); P1 starts with exactly one Plains iff `p1_plains`.
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
            count_in_hand(match, 1, LandType::PLAINS) != (p1_plains ? 1 : 0)) {
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
            match.hand_size(0) < 4 ||
            count_in_hand(match, 1, LandType::ISLAND) != 0 ||                // P1's turn-1 draw can change this
            count_in_hand(match, 1, LandType::PLAINS) != (p1_plains ? 1 : 0)) {
            continue;
        }
        return match;
    }
    ADD_FAILURE() << "no seed produced the Destroy scenario";
    return LandGameMatch(0);
}

// a card in P0's hand that is not any of the given types
uint32_t first_card_not_of_types(const LandGameMatch& match, int player_index,
                                 const std::vector<LandType>& land_types) {
    for (const Card& card : match.hand(player_index)) {
        if (std::find(land_types.begin(), land_types.end(), card.get_land_type()) == land_types.end()) {
            return card.get_identifier();
        }
    }
    ADD_FAILURE() << "hand holds only the excluded land types";
    return 0;
}

// P0 holds a Forest, a Mountain and some third land; Rummage then puts the Mountain in the graveyard.
LandGameMatch make_reclaim_match(bool p1_island) {
    return make_match_where([p1_island](const LandGameMatch& match) {
        const auto& hand = match.hand(0);
        const bool third = std::any_of(hand.begin(), hand.end(), [](const Card& card) {
            return card.get_land_type() != LandType::FOREST && card.get_land_type() != LandType::MOUNTAIN;
        });
        return third &&
               find_in_hand(match, 0, LandType::FOREST).has_value() &&
               find_in_hand(match, 0, LandType::MOUNTAIN).has_value() &&
               !find_in_hand(match, 0, LandType::ISLAND).has_value() &&
               count_in_hand(match, 1, LandType::ISLAND) == (p1_island ? 1 : 0);
    });
}

size_t total_lands(const LandGameMatch& match, int player_index) {
    size_t total = 0;
    for (uint8_t t = 0; t < static_cast<uint8_t>(LandType::NUM_LAND_TYPES); ++t) {
        total += match.land_count(player_index, static_cast<LandType>(t));
    }
    return total;
}

std::vector<uint32_t> ids_of(const std::vector<Card>& cards) {
    std::vector<uint32_t> ids;
    for (const Card& card : cards) {
        ids.push_back(card.get_identifier());
    }
    return ids;
}

bool hand_holds(const LandGameMatch& match, int player_index, uint32_t card_id) {
    const auto& hand = match.hand(player_index);
    return std::any_of(hand.begin(), hand.end(), [card_id](const Card& card) {
        return card.get_identifier() == card_id;
    });
}


// Builds a match from explicit hands and a library (listed top card first). Card ids follow the
// listing order: P1's hand is 0-4, P2's is 5-10, then the library (11 is the top card). hand(0)[i]
// is p0[i], so the id of a hand card equals its listed position.
LandGameMatch make_stacked_match(const std::vector<LandType>& p0, const std::vector<LandType>& p1,
                                 const std::vector<LandType>& library) {
    uint32_t id = 0;
    std::vector<Card> p0_cards, p1_cards, library_cards;
    for (LandType land_type : p0) { p0_cards.push_back(Card(land_type, id++)); }
    for (LandType land_type : p1) { p1_cards.push_back(Card(land_type, id++)); }
    for (LandType land_type : library) { library_cards.push_back(Card(land_type, id++)); }

    std::vector<Card> cards;    // bottom of the library first, the top card last
    cards.insert(cards.end(), library_cards.rbegin(), library_cards.rend());
    cards.insert(cards.end(), p1_cards.rbegin(), p1_cards.rend());
    cards.insert(cards.end(), p0_cards.rbegin(), p0_cards.rend());
    return LandGameMatch{Deck(cards)};
}

// play a land, then let the opponent pass on any window it opened
bool play_settled(LandGameMatch& match, int player_index, uint32_t card_id,
                  std::optional<uint32_t> target = std::nullopt) {
    const bool played = match.play_land(player_index, card_id, target);
    settle(match);
    return played;
}

bool contains_type(const std::vector<Action>& actions, ActionType type) {
    return std::any_of(actions.begin(), actions.end(), [type](const Action& action) { return action.type == type; });
}

// A card whose deploy needs no target and opens no choice right now: not an Island (Draw
// changes hand sizes) or Swamp (Extract pauses for a choice), not a Mountain while the opponent
// has lands, not a Forest while the graveyard has cards, not a Plains while you control a land.
// A ramped deploy enters vanilla, so then any card will do. Use it for "just deploy something".
const Card& plain_card(const LandGameMatch& match, int player_index) {
    if (match.played_land_this_turn() && match.ramp_available()) {
        return match.hand(player_index).front();
    }
    for (const Card& card : match.hand(player_index)) {
        const LandType land_type = card.get_land_type();
        if (land_type == LandType::PLAINS && total_lands(match, player_index) > 0) {
            continue;
        }
        if (land_type == LandType::ISLAND || land_type == LandType::SWAMP) {
            continue;
        }
        if (land_type == LandType::MOUNTAIN && total_lands(match, 1 - player_index) > 0) {
            continue;
        }
        if (land_type == LandType::FOREST && match.graveyard_size() > 0) {
            continue;
        }
        return card;
    }
    ADD_FAILURE() << "no plain card in hand";
    return match.hand(player_index).front();
}

uint32_t plain_card_id(const LandGameMatch& match, int player_index) {
    return plain_card(match, player_index).get_identifier();
}

} // namespace

enum class P1Action { NONE, DEPLOY, RUMMAGE };

// Brings a match to P0's turn 2 with a Plains in hand and `first_land` already on P0's
// battlefield (`battlefield(0)[0]`). On its turn 1, P1 does `p1_action`: nothing, deploy a
// plain card (`battlefield(1)[0]`), or pitch Mountain's Rummage (graveyard holds 2 cards).
// P0 never holds an Island unless `first_land` is one; P1 holds exactly one iff `p1_island`.
// A Swamp first land has its Extract answered with the first option.
LandGameMatch make_flicker_scenario(LandType first_land, P1Action p1_action = P1Action::NONE,
                                    bool p1_island = false) {
    for (uint64_t seed = 1; seed < 50000; ++seed) {
        LandGameMatch match(seed);
        const size_t p0_islands = count_in_hand(match, 0, LandType::ISLAND);
        if (p0_islands != (first_land == LandType::ISLAND ? 1 : 0) ||
            count_in_hand(match, 1, LandType::ISLAND) != (p1_island ? 1 : 0) ||
            !find_in_hand(match, 0, first_land).has_value() ||
            count_in_hand(match, 0, LandType::PLAINS) < (first_land == LandType::PLAINS ? 2 : 1) ||
            (p1_action == P1Action::RUMMAGE && !find_in_hand(match, 1, LandType::MOUNTAIN).has_value())) {
            continue;
        }

        if (!match.play_land(0, *find_in_hand(match, 0, first_land))) { continue; }
        settle(match);
        if (match.choice_type() == ChoiceType::EXTRACT &&
            !match.choose_card(0, match.choice_options().front().get_identifier())) { continue; }
        if (!match.end_turn(0)) { continue; }

        if (p1_action == P1Action::DEPLOY) {
            if (!match.play_land(1, plain_card_id(match, 1))) { continue; }
            settle(match);
        } else if (p1_action == P1Action::RUMMAGE) {
            const uint32_t mountain = *find_in_hand(match, 1, LandType::MOUNTAIN);
            if (!match.pitch_land(1, mountain, first_card_other_than(match, 1, mountain))) { continue; }
            settle(match);
        }
        if (match.choice_player() != -1 || !match.end_turn(1)) { continue; }
        if (!find_in_hand(match, 0, LandType::PLAINS).has_value() ||
            count_in_hand(match, 1, LandType::ISLAND) != (p1_island ? 1 : 0)) { continue; }   // P1's turn-1 draw
        return match;
    }
    ADD_FAILURE() << "no seed produced the Flicker scenario";
    return LandGameMatch(0);
}

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

TEST(LandGameMatch, StackedDeckIsDealtExactlyAsGiven) {
    // 11 cards for the opening hands: P1 takes the top 5, P2 the next 6, then 2 stay in the library
    std::vector<Card> cards;
    for (uint32_t id = 0; id < 13; ++id) {
        cards.push_back(Card(static_cast<LandType>(id % 5), id));
    }
    LandGameMatch match{Deck(cards)};

    EXPECT_EQ(match.hand_size(0), 5);
    EXPECT_EQ(match.hand_size(1), 6);
    EXPECT_EQ(match.deck_size(), 2);
    EXPECT_EQ(match.hand(0)[0], Card(static_cast<LandType>(12 % 5), 12));   // back() is the top
    EXPECT_EQ(match.hand(0)[4], Card(static_cast<LandType>(8 % 5), 8));
    EXPECT_EQ(match.hand(1)[0], Card(static_cast<LandType>(7 % 5), 7));
    EXPECT_EQ(match.hand(1)[5], Card(static_cast<LandType>(2 % 5), 2));
}

TEST(LandGameMatch, SeededConstructorStillShufflesTheFullDeck) {
    LandGameMatch a(12345);
    LandGameMatch b(12345);
    EXPECT_EQ(ids_of(a.hand(0)), ids_of(b.hand(0)));            // deterministic per seed
    EXPECT_EQ(ids_of(a.hand(1)), ids_of(b.hand(1)));
    EXPECT_EQ(a.deck_size(), 39);
}

TEST(LandGameMatch, TurnManagement) {

    LandGameMatch match2(42);
    EXPECT_EQ(match2.active_player(), 0);
    EXPECT_EQ(match2.turn_number(), 1);
    EXPECT_EQ(match2.hand_size(0), 5);
    EXPECT_EQ(match2.hand_size(1), 6);
    EXPECT_EQ(match2.deck_size(), 39);
    EXPECT_FALSE(match2.end_turn(1));           // not P2's turn
    EXPECT_TRUE(match2.end_turn(0));            // -> P2, still turn 1; P2 draws normally (GDD 3.1)
    EXPECT_EQ(match2.active_player(), 1);
    EXPECT_EQ(match2.turn_number(), 1);
    EXPECT_EQ(match2.hand_size(1), 7);          // 6 + 1
    EXPECT_EQ(match2.deck_size(), 38);
    EXPECT_TRUE(match2.end_turn(1));            // -> P1, turn 2, P1 draws
    EXPECT_EQ(match2.active_player(), 0);
    EXPECT_EQ(match2.turn_number(), 2);
    EXPECT_EQ(match2.hand_size(0), 6);          // 5 + 1
    EXPECT_EQ(match2.deck_size(), 37);
}

TEST(LandGameMatch, PlayerOneSkipsOnlyTheFirstDraw) {
    LandGameMatch match(42);
    EXPECT_EQ(match.hand_size(0), 5);           // no draw on P1's turn 1
    EXPECT_EQ(match.deck_size(), 39);
}

TEST(LandGameMatch, EmptyLibrarySkipsTheDrawStep) {
    LandGameMatch match(42);
    while (match.deck_size() > 0) {
        ASSERT_TRUE(match.end_turn(match.active_player()));
    }
    const int active = match.active_player();
    const size_t hand_before = match.hand_size(1 - active);

    EXPECT_TRUE(match.end_turn(active));                    // would have drawn: play continues instead
    EXPECT_EQ(match.hand_size(1 - active), hand_before);
    EXPECT_EQ(match.deck_size(), 0);
}

TEST(LandGameMatch, PlayLand) {

    LandGameMatch match3(12345);

    EXPECT_TRUE(match3.play_land(0, plain_card_id(match3, 0)));
    settle(match3);
    EXPECT_TRUE(match3.played_land_this_turn());
    EXPECT_FALSE(match3.play_land(0, plain_card_id(match3, 0)));  // second play same turn
    
    // assert hand/lands unchanged from after the first play
    EXPECT_TRUE(match3.end_turn(0));
    EXPECT_FALSE(match3.played_land_this_turn());
    EXPECT_TRUE(match3.play_land(1, plain_card_id(match3, 1)));   // opponent can play
    settle(match3);
    
}

TEST(LandGameMatch, PlayLandMovesExactlyOneCardToBoard) {
    LandGameMatch match(12345);

    const Card played = plain_card(match, 0);

    EXPECT_TRUE(match.play_land(0, played.get_identifier()));
    settle(match);
    EXPECT_EQ(match.hand_size(0), 4);           // 5 - 1, not 5 - 2
    EXPECT_EQ(match.hand_size(1), 6);

    EXPECT_EQ(total_lands(match, 0), 1);        // added once, not twice
    EXPECT_EQ(match.land_count(0, played.get_land_type()), 1);
}

TEST(LandGameMatch, PlayLandByCardIdIgnoresHandPosition) {
    LandGameMatch match(12345);

    const Card last = plain_card(match, 0);
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
    EXPECT_TRUE(match.play_land(0, plain_card_id(match, 0)));
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
    EXPECT_TRUE(match.play_land(0, plain_card_id(match, 0)));
    EXPECT_TRUE(match.play_land(0, plain_card_id(match, 0)));     // the ramped deploy
    EXPECT_FALSE(match.play_land(0, first_card_id(match, 0)));    // only one extra
    EXPECT_EQ(total_lands(match, 0), 2);
    EXPECT_EQ(match.hand_size(0), 2);               // 5 - 1 pitched - 2 deployed
    EXPECT_EQ(match.graveyard_size(), 1);
}

TEST(LandGameMatch, RampPitchedAfterFirstDeployStillGrantsExtraDeploy) {
    LandGameMatch match = make_match_with(0, {LandType::FOREST});
    const uint32_t forest = *find_in_hand(match, 0, LandType::FOREST);

    EXPECT_TRUE(match.play_land(0, plain_card_id(match, 0)));
    EXPECT_EQ(match.hand_size(0), 4);

    // a second deploy is illegal until Ramp resolves
    EXPECT_FALSE(match.play_land(0, first_card_id(match, 0)));

    EXPECT_TRUE(match.pitch_land(0, forest));
    EXPECT_TRUE(match.play_land(0, plain_card_id(match, 0)));
    EXPECT_FALSE(match.play_land(0, first_card_id(match, 0)));
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
    EXPECT_TRUE(match.play_land(0, plain_card_id(match, 0)));
    EXPECT_FALSE(match.play_land(0, first_card_id(match, 0)));    // ramp expired
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

TEST(LandGameMatch, PitchRejectsInstantSpeedPitches) {
    LandGameMatch match = make_match_with(0, {LandType::PLAINS, LandType::ISLAND});

    for (LandType land_type : {LandType::PLAINS, LandType::ISLAND}) {
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
    EXPECT_TRUE(match.play_land(0, plain_card_id(match, 0)));   // normal deploy
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
    const Card deployed = plain_card(match, 0);

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

    EXPECT_TRUE(match.play_land(0, plain_card_id(match, 0)));
    EXPECT_EQ(match.priority_player(), -1);
    EXPECT_EQ(match.stack_size(), 0);
    EXPECT_EQ(total_lands(match, 0), 1);
}

TEST(LandGameMatch, PassPriorityResolvesPendingDeploy) {
    LandGameMatch match = make_match_where_only_p1_can_counter();
    const Card deployed = plain_card(match, 0);
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
    ASSERT_TRUE(match.play_land(0, plain_card_id(match, 0)));
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

    ASSERT_TRUE(match.play_land(0, plain_card_id(match, 0)));
    EXPECT_FALSE(match.pass_priority(0));               // the pusher holds no priority
    EXPECT_FALSE(match.pass_priority(-1));
    EXPECT_FALSE(match.pass_priority(2));
    EXPECT_EQ(match.priority_player(), 1);
    EXPECT_EQ(match.stack_size(), 1);
}

TEST(LandGameMatch, CounterSendsDeployedLandToTopOfLibrary) {
    LandGameMatch match = make_match_where_only_p1_can_counter();
    const Card deployed = plain_card(match, 0);
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

    // the deploy is still used up, and the countered land is the next card drawn: P1's, the counterer's
    EXPECT_TRUE(match.played_land_this_turn());
    EXPECT_FALSE(match.play_land(0, first_card_id(match, 0)));
    EXPECT_TRUE(match.end_turn(0));                     // P1 draws on its turn: Memory Lapse
    EXPECT_TRUE(hand_holds(match, 1, deployed.get_identifier()));
    EXPECT_FALSE(hand_holds(match, 0, deployed.get_identifier()));
    EXPECT_EQ(match.deck_size(), 39);
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
        return count_in_hand(m, 0, LandType::PLAINS) >= 2 &&
               count_in_hand(m, 1, LandType::ISLAND) == 1 &&
               count_in_hand(m, 0, LandType::ISLAND) == 0 &&
               count_in_hand(m, 0, LandType::FOREST) >= 1;
    });
    const uint32_t forest = *find_in_hand(match, 0, LandType::FOREST);
    const uint32_t island = *find_in_hand(match, 1, LandType::ISLAND);
    ASSERT_TRUE(match.pitch_land(0, forest));
    ASSERT_TRUE(match.cast_counter(1, island, first_card_other_than(match, 1, island),
                                   match.stack_top().stack_id));

    EXPECT_TRUE(match.play_land(0, plain_card_id(match, 0)));
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
    const Card deployed = plain_card(match, 0);
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

    ASSERT_TRUE(match.play_land(0, plain_card_id(match, 0)));
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
    LandGameMatch match = make_match_where([](const LandGameMatch& m) {
        return count_in_hand(m, 0, LandType::PLAINS) >= 1 &&
               count_in_hand(m, 1, LandType::FOREST) >= 1 &&
               count_in_hand(m, 1, LandType::MOUNTAIN) >= 1 &&
               count_in_hand(m, 1, LandType::PLAINS) >= 1 &&
               count_in_hand(m, 0, LandType::ISLAND) == 0 &&
               count_in_hand(m, 1, LandType::ISLAND) == 0;
    });
    ASSERT_TRUE(match.play_land(0, plain_card_id(match, 0)));       // P0 puts a land on the board
    ASSERT_TRUE(match.end_turn(0));
    const uint32_t mountain = *find_in_hand(match, 1, LandType::MOUNTAIN);
    const uint32_t target = match.battlefield(0)[0].get_identifier();

    ASSERT_TRUE(match.pitch_land(1, *find_in_hand(match, 1, LandType::FOREST)));
    ASSERT_TRUE(match.play_land(1, plain_card_id(match, 1)));
    EXPECT_FALSE(match.play_land(1, mountain, target));             // vanilla: no target allowed
    EXPECT_TRUE(match.play_land(1, mountain));
    EXPECT_EQ(match.land_count(1, LandType::MOUNTAIN), 1);
    EXPECT_EQ(total_lands(match, 0), 1);                            // the ramped Mountain destroyed nothing
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

TEST(LandGameMatch, ReclaimReturnsTargetedGraveyardCardToHand) {
    LandGameMatch match = make_reclaim_match(false);
    const uint32_t forest = *find_in_hand(match, 0, LandType::FOREST);
    const uint32_t mountain = *find_in_hand(match, 0, LandType::MOUNTAIN);
    ASSERT_TRUE(match.pitch_land(0, mountain, first_card_not_of_types(match, 0, {LandType::FOREST, LandType::MOUNTAIN})));
    ASSERT_EQ(match.graveyard_size(), 2);
    const size_t hand_before = match.hand_size(0);

    EXPECT_TRUE(match.play_land(0, forest, mountain));
    EXPECT_EQ(match.land_count(0, LandType::FOREST), 1);
    EXPECT_EQ(match.hand_size(0), hand_before - 1 + 1);             // Forest out, reclaimed card in
    EXPECT_TRUE(match.hand(0).end() != std::find_if(match.hand(0).begin(), match.hand(0).end(),
        [mountain](const Card& card) { return card.get_identifier() == mountain; }));
    EXPECT_EQ(match.graveyard_size(), 1);
    EXPECT_EQ(total_lands(match, 0), 1);                            // reclaiming deploys nothing extra
}

TEST(LandGameMatch, ReclaimRequiresGraveyardTargetWhenNotEmpty) {
    LandGameMatch match = make_reclaim_match(false);
    const uint32_t forest = *find_in_hand(match, 0, LandType::FOREST);
    const uint32_t mountain = *find_in_hand(match, 0, LandType::MOUNTAIN);
    ASSERT_TRUE(match.pitch_land(0, mountain, first_card_not_of_types(match, 0, {LandType::FOREST, LandType::MOUNTAIN})));
    const size_t hand_before = match.hand_size(0);

    EXPECT_FALSE(match.play_land(0, forest));                       // a target exists, so one is required
    EXPECT_FALSE(match.play_land(0, forest, 9999));                 // no such card
    EXPECT_FALSE(match.play_land(0, forest, first_card_not_of_types(match, 0, {LandType::FOREST})));  // in hand
    EXPECT_FALSE(match.played_land_this_turn());
    EXPECT_EQ(match.hand_size(0), hand_before);
    EXPECT_EQ(match.graveyard_size(), 2);
    EXPECT_EQ(match.stack_size(), 0);
}

TEST(LandGameMatch, ReclaimEntersVanillaWhenGraveyardIsEmpty) {
    LandGameMatch match = make_match_with(0, {LandType::FOREST});
    const uint32_t forest = *find_in_hand(match, 0, LandType::FOREST);

    EXPECT_FALSE(match.play_land(0, forest, 9999));                 // nothing to take
    EXPECT_TRUE(match.play_land(0, forest));
    EXPECT_EQ(match.hand_size(0), 4);
    EXPECT_EQ(match.land_count(0, LandType::FOREST), 1);
}

TEST(LandGameMatch, ReclaimCanTakeCardsTheOpponentPitched) {
    LandGameMatch match = make_match_where([](const LandGameMatch& m) {
        return count_in_hand(m, 0, LandType::FOREST) >= 1 &&
               count_in_hand(m, 1, LandType::MOUNTAIN) >= 1 &&
               count_in_hand(m, 0, LandType::ISLAND) == 0 &&
               count_in_hand(m, 1, LandType::ISLAND) == 0;
    });
    const uint32_t forest = *find_in_hand(match, 0, LandType::FOREST);
    const uint32_t p1_mountain = *find_in_hand(match, 1, LandType::MOUNTAIN);
    ASSERT_TRUE(match.end_turn(0));                                 // P1's turn
    ASSERT_TRUE(match.pitch_land(1, p1_mountain, first_card_other_than(match, 1, p1_mountain)));
    ASSERT_TRUE(match.end_turn(1));                                 // P0's turn 2

    EXPECT_TRUE(match.play_land(0, forest, p1_mountain));
    settle(match);                                                  // Rummage may have drawn P1 an Island
    EXPECT_TRUE(match.hand(0).end() != std::find_if(match.hand(0).begin(), match.hand(0).end(),
        [p1_mountain](const Card& card) { return card.get_identifier() == p1_mountain; }));
    EXPECT_EQ(match.graveyard_size(), 1);                           // only the opponent's cost card remains
}

TEST(LandGameMatch, CounteredForestReclaimsNothing) {
    LandGameMatch match = make_reclaim_match(true);
    const uint32_t forest = *find_in_hand(match, 0, LandType::FOREST);
    const uint32_t mountain = *find_in_hand(match, 0, LandType::MOUNTAIN);
    const uint32_t island = *find_in_hand(match, 1, LandType::ISLAND);
    ASSERT_TRUE(match.pitch_land(0, mountain, first_card_not_of_types(match, 0, {LandType::FOREST, LandType::MOUNTAIN})));
    settle(match);                                                  // P1 declines to counter the pitch
    ASSERT_TRUE(match.play_land(0, forest, mountain));
    ASSERT_EQ(match.priority_player(), 1);
    const size_t p0_hand_before = match.hand_size(0);

    EXPECT_TRUE(match.cast_counter(1, island, first_card_other_than(match, 1, island),
                                   match.stack_top().stack_id));
    EXPECT_EQ(match.hand_size(0), p0_hand_before);                  // nothing came back
    EXPECT_EQ(match.land_count(0, LandType::FOREST), 0);
    EXPECT_EQ(match.graveyard_size(), 4);                           // Mountain, discard, Island, cost card
    EXPECT_EQ(match.deck_size(), 38);                               // the Forest went on top of the library
}

TEST(LandGameMatch, DeploySwampPausesForExtractChoice) {
    LandGameMatch match = make_match_with(0, {LandType::SWAMP});

    EXPECT_TRUE(match.play_land(0, *find_in_hand(match, 0, LandType::SWAMP)));
    EXPECT_EQ(match.land_count(0, LandType::SWAMP), 1);
    EXPECT_EQ(match.choice_player(), 0);
    EXPECT_EQ(match.choice_type(), ChoiceType::EXTRACT);
    EXPECT_EQ(ids_of(match.choice_options()), ids_of(match.hand(1)));   // the opponent's whole hand
    EXPECT_EQ(match.hand_size(1), 6);                                   // nothing discarded yet
    EXPECT_EQ(match.graveyard_size(), 0);
    EXPECT_EQ(match.stack_size(), 0);
}

TEST(LandGameMatch, PendingChoiceBlocksOtherActions) {
    LandGameMatch match = make_match_with(0, {LandType::SWAMP, LandType::FOREST});
    ASSERT_TRUE(match.play_land(0, *find_in_hand(match, 0, LandType::SWAMP)));
    ASSERT_EQ(match.choice_type(), ChoiceType::EXTRACT);

    EXPECT_FALSE(match.pitch_land(0, *find_in_hand(match, 0, LandType::FOREST)));
    EXPECT_FALSE(match.end_turn(0));
    EXPECT_FALSE(match.pass_priority(0));
    EXPECT_FALSE(match.pass_priority(1));
    EXPECT_EQ(match.choice_type(), ChoiceType::EXTRACT);
    EXPECT_FALSE(match.pitched_this_turn());
}

TEST(LandGameMatch, ExtractDiscardsTheChosenOpponentCard) {
    LandGameMatch match = make_match_with(0, {LandType::SWAMP});
    ASSERT_TRUE(match.play_land(0, *find_in_hand(match, 0, LandType::SWAMP)));
    const Card chosen = match.hand(1)[2];

    EXPECT_TRUE(match.choose_card(0, chosen.get_identifier()));
    EXPECT_EQ(match.choice_player(), -1);
    EXPECT_EQ(match.choice_type(), ChoiceType::NONE);
    EXPECT_TRUE(match.choice_options().empty());
    EXPECT_EQ(match.hand_size(1), 5);
    EXPECT_FALSE(hand_holds(match, 1, chosen.get_identifier()));
    ASSERT_EQ(match.graveyard_size(), 1);                               // public the moment it lands
    EXPECT_EQ(match.hand_size(0), 4);                                   // P0's own hand is untouched
    EXPECT_TRUE(match.end_turn(0));                                     // the game moves on
}

TEST(LandGameMatch, ExtractRejectionsKeepTheChoicePending) {
    LandGameMatch match = make_match_with(0, {LandType::SWAMP});
    EXPECT_FALSE(match.choose_card(0, 0));                           // nothing pending yet

    ASSERT_TRUE(match.play_land(0, *find_in_hand(match, 0, LandType::SWAMP)));
    const uint32_t opponent_card = first_card_id(match, 1);

    EXPECT_FALSE(match.choose_card(1, opponent_card));               // only the Swamp's controller answers
    EXPECT_FALSE(match.choose_card(-1, opponent_card));
    EXPECT_FALSE(match.choose_card(2, opponent_card));
    EXPECT_FALSE(match.choose_card(0, first_card_id(match, 0)));     // own card is not an option
    EXPECT_FALSE(match.choose_card(0, 9999));
    EXPECT_FALSE(match.choose_surveil(0, {}, {}));                      // wrong kind of answer

    EXPECT_EQ(match.choice_player(), 0);
    EXPECT_EQ(match.hand_size(1), 6);
    EXPECT_EQ(match.graveyard_size(), 0);
}

TEST(LandGameMatch, RampedSwampEntersVanilla) {
    LandGameMatch match = make_match_with(0, {LandType::FOREST, LandType::SWAMP});
    ASSERT_TRUE(match.pitch_land(0, *find_in_hand(match, 0, LandType::FOREST)));
    ASSERT_TRUE(match.play_land(0, plain_card_id(match, 0)));

    EXPECT_TRUE(match.play_land(0, *find_in_hand(match, 0, LandType::SWAMP)));  // the ramped deploy
    EXPECT_EQ(match.land_count(0, LandType::SWAMP), 1);
    EXPECT_EQ(match.choice_player(), -1);
}

TEST(LandGameMatch, CounteredSwampOffersNoChoice) {
    LandGameMatch match = make_match_where([](const LandGameMatch& m) {
        return count_in_hand(m, 1, LandType::ISLAND) == 1 &&
               count_in_hand(m, 0, LandType::ISLAND) == 0 &&
               count_in_hand(m, 0, LandType::SWAMP) >= 1;
    });
    const uint32_t island = *find_in_hand(match, 1, LandType::ISLAND);
    ASSERT_TRUE(match.play_land(0, *find_in_hand(match, 0, LandType::SWAMP)));
    ASSERT_EQ(match.priority_player(), 1);

    EXPECT_TRUE(match.cast_counter(1, island, first_card_other_than(match, 1, island),
                                   match.stack_top().stack_id));
    EXPECT_EQ(match.choice_player(), -1);
    EXPECT_EQ(match.land_count(0, LandType::SWAMP), 0);
    EXPECT_EQ(match.hand_size(1), 4);                                   // only the counter's costs left it
}

TEST(LandGameMatch, PitchSurveilPausesForChoice) {
    LandGameMatch match = make_match_with(0, {LandType::SWAMP});
    const uint32_t swamp = *find_in_hand(match, 0, LandType::SWAMP);

    EXPECT_TRUE(match.pitch_land(0, swamp));
    EXPECT_TRUE(match.pitched_this_turn());
    EXPECT_EQ(match.choice_player(), 0);
    EXPECT_EQ(match.choice_type(), ChoiceType::SURVEIL);
    EXPECT_EQ(match.choice_options().size(), 2);                        // the top 2 cards
    EXPECT_EQ(match.deck_size(), 39);                                   // peeked, not drawn
    EXPECT_EQ(match.graveyard_size(), 1);                               // the pitched Swamp
    EXPECT_EQ(total_lands(match, 0), 0);
    EXPECT_FALSE(match.end_turn(0));                                    // blocked until answered
    EXPECT_FALSE(match.pitch_land(0, plain_card_id(match, 0)));
}

TEST(LandGameMatch, SurveilBinsAndKeepsTheChosenCards) {
    LandGameMatch match = make_match_with(0, {LandType::SWAMP});
    ASSERT_TRUE(match.pitch_land(0, *find_in_hand(match, 0, LandType::SWAMP)));
    const uint32_t top = match.choice_options()[0].get_identifier();
    const uint32_t second = match.choice_options()[1].get_identifier();

    EXPECT_TRUE(match.choose_surveil(0, {top}, {second}));
    EXPECT_EQ(match.choice_player(), -1);
    EXPECT_EQ(match.deck_size(), 38);                                   // one card binned
    EXPECT_EQ(match.graveyard_size(), 2);

    EXPECT_TRUE(match.end_turn(0));                                     // P1 draws the kept card
    EXPECT_TRUE(hand_holds(match, 1, second));
    EXPECT_FALSE(hand_holds(match, 1, top));
}

TEST(LandGameMatch, SurveilOrdersTheKeptCards) {
    LandGameMatch match = make_match_with(0, {LandType::SWAMP});
    ASSERT_TRUE(match.pitch_land(0, *find_in_hand(match, 0, LandType::SWAMP)));
    const uint32_t first = match.choice_options()[0].get_identifier();
    const uint32_t second = match.choice_options()[1].get_identifier();

    LandGameMatch reversed = match;
    EXPECT_TRUE(match.choose_surveil(0, {}, {first, second}));          // library_top_order[0] is the new top
    EXPECT_EQ(match.deck_size(), 39);
    EXPECT_EQ(match.graveyard_size(), 1);
    ASSERT_TRUE(match.end_turn(0));                                     // P1 draws first
    EXPECT_TRUE(hand_holds(match, 1, first));
    ASSERT_TRUE(match.end_turn(1));                                     // P0 draws second
    EXPECT_TRUE(hand_holds(match, 0, second));

    EXPECT_TRUE(reversed.choose_surveil(0, {}, {second, first}));
    ASSERT_TRUE(reversed.end_turn(0));
    EXPECT_TRUE(hand_holds(reversed, 1, second));
    ASSERT_TRUE(reversed.end_turn(1));
    EXPECT_TRUE(hand_holds(reversed, 0, first));
}

TEST(LandGameMatch, SurveilCanBinEverything) {
    LandGameMatch match = make_match_with(0, {LandType::SWAMP});
    ASSERT_TRUE(match.pitch_land(0, *find_in_hand(match, 0, LandType::SWAMP)));
    const std::vector<uint32_t> both = ids_of(match.choice_options());

    EXPECT_TRUE(match.choose_surveil(0, both, {}));
    EXPECT_EQ(match.deck_size(), 37);
    EXPECT_EQ(match.graveyard_size(), 3);                               // Swamp + both surveilled cards
}

TEST(LandGameMatch, SurveilRejectionsKeepTheChoicePending) {
    LandGameMatch match = make_match_with(0, {LandType::SWAMP});
    EXPECT_FALSE(match.choose_surveil(0, {}, {}));                      // nothing pending yet

    ASSERT_TRUE(match.pitch_land(0, *find_in_hand(match, 0, LandType::SWAMP)));
    const uint32_t a = match.choice_options()[0].get_identifier();
    const uint32_t b = match.choice_options()[1].get_identifier();

    EXPECT_FALSE(match.choose_surveil(1, {a}, {b}));                    // not the surveilling player
    EXPECT_FALSE(match.choose_surveil(0, {a}, {}));                     // b is unaccounted for
    EXPECT_FALSE(match.choose_surveil(0, {}, {a}));
    EXPECT_FALSE(match.choose_surveil(0, {a}, {a, b}));                 // a named twice
    EXPECT_FALSE(match.choose_surveil(0, {a, a}, {b}));
    EXPECT_FALSE(match.choose_surveil(0, {a, b}, {a}));
    EXPECT_FALSE(match.choose_surveil(0, {a}, {9999}));                 // not a peeked card
    EXPECT_FALSE(match.choose_surveil(0, {a, b, 9999}, {}));            // too many ids
    EXPECT_FALSE(match.choose_card(0, a));                           // wrong kind of answer

    EXPECT_EQ(match.choice_type(), ChoiceType::SURVEIL);
    EXPECT_EQ(match.deck_size(), 39);
    EXPECT_EQ(match.graveyard_size(), 1);
}

TEST(LandGameMatch, SurveilWithShortLibraryPeeksWhatRemains) {
    for (uint64_t seed = 1; seed < 1000; ++seed) {
        LandGameMatch match(seed);
        while (match.deck_size() > 1) {
            ASSERT_TRUE(match.end_turn(match.active_player()));
        }
        const int player = match.active_player();
        const std::optional<uint32_t> swamp = find_in_hand(match, player, LandType::SWAMP);
        if (!swamp.has_value()) {
            continue;
        }

        EXPECT_TRUE(match.pitch_land(player, *swamp));
        settle(match);                                                  // the opponent may hold an Island
        ASSERT_EQ(match.choice_type(), ChoiceType::SURVEIL);
        EXPECT_EQ(match.choice_options().size(), 1);
        const uint32_t only = match.choice_options()[0].get_identifier();
        EXPECT_TRUE(match.choose_surveil(player, {}, {only}));
        EXPECT_EQ(match.deck_size(), 1);
        return;
    }
    FAIL() << "no seed produced a Swamp in the active player's hand";
}

TEST(LandGameMatch, SurveilWithEmptyLibraryOffersNoChoice) {
    for (uint64_t seed = 1; seed < 1000; ++seed) {
        LandGameMatch match(seed);
        while (match.deck_size() > 0) {
            ASSERT_TRUE(match.end_turn(match.active_player()));
        }
        const int player = match.active_player();
        const std::optional<uint32_t> swamp = find_in_hand(match, player, LandType::SWAMP);
        if (!swamp.has_value()) {
            continue;
        }

        EXPECT_TRUE(match.pitch_land(player, *swamp));
        settle(match);
        EXPECT_EQ(match.choice_player(), -1);
        EXPECT_EQ(match.deck_size(), 0);
        EXPECT_TRUE(match.pitched_this_turn());
        return;
    }
    FAIL() << "no seed produced a Swamp in the active player's hand";
}

TEST(LandGameMatch, CounteredSurveilOffersNoChoice) {
    LandGameMatch match = make_match_where([](const LandGameMatch& m) {
        return count_in_hand(m, 1, LandType::ISLAND) == 1 &&
               count_in_hand(m, 0, LandType::ISLAND) == 0 &&
               count_in_hand(m, 0, LandType::SWAMP) >= 1;
    });
    const uint32_t island = *find_in_hand(match, 1, LandType::ISLAND);
    ASSERT_TRUE(match.pitch_land(0, *find_in_hand(match, 0, LandType::SWAMP)));
    ASSERT_EQ(match.priority_player(), 1);

    EXPECT_TRUE(match.cast_counter(1, island, first_card_other_than(match, 1, island),
                                   match.stack_top().stack_id));
    EXPECT_EQ(match.choice_player(), -1);
    EXPECT_EQ(match.deck_size(), 39);
    EXPECT_EQ(match.graveyard_size(), 3);                               // Swamp, Island, cost card
    EXPECT_TRUE(match.pitched_this_turn());
}

TEST(LandGameMatch, FlickerRetriggersIslandDraw) {
    LandGameMatch match = make_flicker_scenario(LandType::ISLAND);
    const uint32_t island = match.battlefield(0)[0].get_identifier();
    const size_t hand_before = match.hand_size(0);
    const size_t deck_before = match.deck_size();

    EXPECT_TRUE(match.play_land(0, *find_in_hand(match, 0, LandType::PLAINS), island));
    EXPECT_EQ(match.choice_player(), -1);                           // Draw needs no choice
    EXPECT_EQ(match.hand_size(0), hand_before - 1 + 1);             // Plains out, flickered draw in
    EXPECT_EQ(match.deck_size(), deck_before - 1);
    EXPECT_EQ(match.land_count(0, LandType::PLAINS), 1);
    EXPECT_EQ(match.land_count(0, LandType::ISLAND), 1);            // flickering never duplicates the land
}

TEST(LandGameMatch, FlickerRequiresAnotherLandYouControl) {
    LandGameMatch match = make_flicker_scenario(LandType::FOREST, P1Action::DEPLOY);
    const uint32_t plains = *find_in_hand(match, 0, LandType::PLAINS);
    const uint32_t own_land = match.battlefield(0)[0].get_identifier();
    const uint32_t opponent_land = match.battlefield(1)[0].get_identifier();
    const size_t hand_before = match.hand_size(0);

    EXPECT_FALSE(match.play_land(0, plains));                       // a land exists, so a target is required
    EXPECT_FALSE(match.play_land(0, plains, opponent_land));        // not yours
    EXPECT_FALSE(match.play_land(0, plains, plains));               // cannot target itself (still in hand)
    EXPECT_FALSE(match.play_land(0, plains, 9999));
    EXPECT_FALSE(match.played_land_this_turn());
    EXPECT_EQ(match.hand_size(0), hand_before);
    EXPECT_EQ(match.stack_size(), 0);

    EXPECT_TRUE(match.play_land(0, plains, own_land));
}

TEST(LandGameMatch, FlickerEntersVanillaWithoutALandToTarget) {
    LandGameMatch match = make_match_with(0, {LandType::PLAINS});
    const uint32_t plains = *find_in_hand(match, 0, LandType::PLAINS);

    EXPECT_FALSE(match.play_land(0, plains, 9999));                 // nothing to flicker
    EXPECT_TRUE(match.play_land(0, plains));
    EXPECT_EQ(match.land_count(0, LandType::PLAINS), 1);
    EXPECT_EQ(match.choice_player(), -1);
}

TEST(LandGameMatch, FlickeringAPlainsFizzlesAntiLoop) {
    LandGameMatch match = make_flicker_scenario(LandType::PLAINS);
    const uint32_t first_plains = match.battlefield(0)[0].get_identifier();
    const size_t hand_before = match.hand_size(0);
    const size_t deck_before = match.deck_size();

    EXPECT_TRUE(match.play_land(0, *find_in_hand(match, 0, LandType::PLAINS), first_plains));
    EXPECT_EQ(match.choice_player(), -1);
    EXPECT_EQ(match.land_count(0, LandType::PLAINS), 2);
    EXPECT_EQ(match.hand_size(0), hand_before - 1);                 // the re-trigger did nothing
    EXPECT_EQ(match.deck_size(), deck_before);
    EXPECT_EQ(match.graveyard_size(), 0);
}

TEST(LandGameMatch, FlickerRetriggersDestroyWithAFreshTarget) {
    LandGameMatch match = make_flicker_scenario(LandType::MOUNTAIN, P1Action::DEPLOY);
    const uint32_t mountain = match.battlefield(0)[0].get_identifier();
    const Card victim = match.battlefield(1)[0];

    EXPECT_TRUE(match.play_land(0, *find_in_hand(match, 0, LandType::PLAINS), mountain));
    ASSERT_EQ(match.choice_type(), ChoiceType::FLICKER_DESTROY);
    EXPECT_EQ(match.choice_player(), 0);
    EXPECT_EQ(ids_of(match.choice_options()), ids_of(match.battlefield(1)));
    EXPECT_FALSE(match.end_turn(0));                                // resolution is paused

    EXPECT_TRUE(match.choose_card(0, victim.get_identifier()));
    EXPECT_TRUE(match.battlefield(1).empty());
    EXPECT_EQ(match.graveyard_size(), 1);
    EXPECT_EQ(match.land_count(0, LandType::MOUNTAIN), 1);          // the flickered Mountain stays put
    EXPECT_EQ(match.land_count(0, LandType::PLAINS), 1);
    EXPECT_EQ(match.choice_player(), -1);
}

TEST(LandGameMatch, FlickerRetriggersReclaimWithAFreshTarget) {
    LandGameMatch match = make_flicker_scenario(LandType::FOREST, P1Action::RUMMAGE);
    const uint32_t forest = match.battlefield(0)[0].get_identifier();
    ASSERT_EQ(match.graveyard_size(), 2);

    EXPECT_TRUE(match.play_land(0, *find_in_hand(match, 0, LandType::PLAINS), forest));
    settle(match);                                                  // P1 may hold an Island by now
    ASSERT_EQ(match.choice_type(), ChoiceType::FLICKER_RECLAIM);
    EXPECT_EQ(match.choice_player(), 0);
    const uint32_t taken = match.choice_options()[0].get_identifier();
    EXPECT_EQ(match.choice_options().size(), 2);

    EXPECT_TRUE(match.choose_card(0, taken));
    EXPECT_TRUE(hand_holds(match, 0, taken));
    EXPECT_EQ(match.graveyard_size(), 1);
}

TEST(LandGameMatch, FlickerRetriggersExtractWithAFreshPick) {
    LandGameMatch match = make_flicker_scenario(LandType::SWAMP);
    const uint32_t swamp = match.battlefield(0)[0].get_identifier();
    ASSERT_EQ(match.graveyard_size(), 1);                           // the first Extract already took a card
    const size_t p1_hand_before = match.hand_size(1);

    EXPECT_TRUE(match.play_land(0, *find_in_hand(match, 0, LandType::PLAINS), swamp));
    ASSERT_EQ(match.choice_type(), ChoiceType::FLICKER_EXTRACT);
    EXPECT_EQ(ids_of(match.choice_options()), ids_of(match.hand(1)));
    const uint32_t picked = match.choice_options()[1].get_identifier();

    EXPECT_TRUE(match.choose_card(0, picked));
    EXPECT_EQ(match.hand_size(1), p1_hand_before - 1);
    EXPECT_FALSE(hand_holds(match, 1, picked));
    EXPECT_EQ(match.graveyard_size(), 2);
}

TEST(LandGameMatch, FlickerWithNothingToChooseOffersNoChoice) {
    LandGameMatch match = make_flicker_scenario(LandType::MOUNTAIN);   // P1 controls no land
    const uint32_t mountain = match.battlefield(0)[0].get_identifier();

    EXPECT_TRUE(match.play_land(0, *find_in_hand(match, 0, LandType::PLAINS), mountain));
    EXPECT_EQ(match.choice_player(), -1);
    EXPECT_EQ(match.land_count(0, LandType::PLAINS), 1);
    EXPECT_EQ(match.graveyard_size(), 0);
}

TEST(LandGameMatch, FlickeredEtbCannotBeCountered) {
    LandGameMatch match = make_flicker_scenario(LandType::MOUNTAIN, P1Action::DEPLOY, true);
    const Card victim = match.battlefield(1)[0];
    ASSERT_TRUE(match.play_land(0, *find_in_hand(match, 0, LandType::PLAINS),
                                match.battlefield(0)[0].get_identifier()));
    ASSERT_EQ(match.priority_player(), 1);                          // the Plains deploy itself can be answered

    EXPECT_TRUE(match.pass_priority(1));
    ASSERT_EQ(match.choice_type(), ChoiceType::FLICKER_DESTROY);
    EXPECT_EQ(match.priority_player(), -1);                         // no window for the re-triggered ETB
    EXPECT_TRUE(match.choose_card(0, victim.get_identifier()));
    EXPECT_EQ(match.priority_player(), -1);
    EXPECT_TRUE(match.battlefield(1).empty());
}

TEST(LandGameMatch, CounteredPlainsFlickersNothing) {
    LandGameMatch match = make_flicker_scenario(LandType::MOUNTAIN, P1Action::DEPLOY, true);
    const uint32_t island = *find_in_hand(match, 1, LandType::ISLAND);
    ASSERT_TRUE(match.play_land(0, *find_in_hand(match, 0, LandType::PLAINS),
                                match.battlefield(0)[0].get_identifier()));
    ASSERT_EQ(match.priority_player(), 1);

    EXPECT_TRUE(match.cast_counter(1, island, first_card_other_than(match, 1, island),
                                   match.stack_top().stack_id));
    EXPECT_EQ(match.choice_player(), -1);
    EXPECT_EQ(match.land_count(0, LandType::PLAINS), 0);
    EXPECT_EQ(match.battlefield(1).size(), 1);                      // nothing was destroyed
}

TEST(LandGameMatch, FlickerChoiceRejectionsKeepTheChoicePending) {
    LandGameMatch match = make_flicker_scenario(LandType::MOUNTAIN, P1Action::DEPLOY);
    const uint32_t victim = match.battlefield(1)[0].get_identifier();
    ASSERT_TRUE(match.play_land(0, *find_in_hand(match, 0, LandType::PLAINS),
                                match.battlefield(0)[0].get_identifier()));
    ASSERT_EQ(match.choice_type(), ChoiceType::FLICKER_DESTROY);

    EXPECT_FALSE(match.choose_card(1, victim));                     // only the flickering player picks
    EXPECT_FALSE(match.choose_card(0, match.battlefield(0)[0].get_identifier()));   // own land is not an option
    EXPECT_FALSE(match.choose_card(0, 9999));
    EXPECT_FALSE(match.choose_surveil(0, {}, {}));
    EXPECT_EQ(match.choice_type(), ChoiceType::FLICKER_DESTROY);
    EXPECT_EQ(match.battlefield(1).size(), 1);
    EXPECT_EQ(match.graveyard_size(), 0);
}

TEST(LandGameMatch, NewMatchIsRunning) {
    LandGameMatch match(12345);
    EXPECT_FALSE(match.game_over());
    EXPECT_EQ(match.winner_player(), -1);
    EXPECT_EQ(match.end_reason(), GameEndReason::NONE);
    EXPECT_FALSE(match.is_draw());
}

TEST(LandGameMatch, MonoWinEndsTheGame) {
    const std::vector<LandType> five_forests(5, LandType::FOREST);
    LandGameMatch match = make_stacked_match(five_forests, std::vector<LandType>(6, LandType::MOUNTAIN),
                                             std::vector<LandType>(10, LandType::MOUNTAIN));
    for (uint32_t id = 0; id < 5; ++id) {
        EXPECT_FALSE(match.game_over());
        ASSERT_TRUE(play_settled(match, 0, id));
        if (id < 4) {
            ASSERT_TRUE(match.end_turn(0));
            ASSERT_TRUE(match.end_turn(1));
        }
    }
    EXPECT_TRUE(match.game_over());
    EXPECT_EQ(match.winner(), WinCondition::MONO);
    EXPECT_EQ(match.winner_player(), 0);
    EXPECT_EQ(match.end_reason(), GameEndReason::MONO);
    EXPECT_FALSE(match.is_draw());
    EXPECT_EQ(match.land_count(0, LandType::FOREST), 5);
}

TEST(LandGameMatch, DomainWinEndsTheGameBeforeTheFinalEtbAsks) {
    // Forest, Mountain and Island enter cleanly; Plains flickers the Forest (empty graveyard, no
    // choice); the Swamp completes the Domain, so its pending Extract is dropped with the game.
    LandGameMatch match = make_stacked_match(
        {LandType::FOREST, LandType::MOUNTAIN, LandType::ISLAND, LandType::PLAINS, LandType::SWAMP},
        std::vector<LandType>(6, LandType::MOUNTAIN), std::vector<LandType>(10, LandType::MOUNTAIN));
    ASSERT_TRUE(play_settled(match, 0, 0));
    ASSERT_TRUE(match.end_turn(0));
    ASSERT_TRUE(match.end_turn(1));
    ASSERT_TRUE(play_settled(match, 0, 1));
    ASSERT_TRUE(match.end_turn(0));
    ASSERT_TRUE(match.end_turn(1));
    ASSERT_TRUE(play_settled(match, 0, 2));
    ASSERT_TRUE(match.end_turn(0));
    ASSERT_TRUE(match.end_turn(1));
    ASSERT_TRUE(play_settled(match, 0, 3, 0));                      // Plains flickers the Forest
    ASSERT_EQ(match.choice_player(), -1);
    ASSERT_TRUE(match.end_turn(0));
    ASSERT_TRUE(match.end_turn(1));
    EXPECT_FALSE(match.game_over());

    EXPECT_TRUE(play_settled(match, 0, 4));                         // Swamp: the fifth type
    EXPECT_TRUE(match.game_over());
    EXPECT_EQ(match.winner(), WinCondition::DOMAIN);
    EXPECT_EQ(match.winner_player(), 0);
    EXPECT_EQ(match.end_reason(), GameEndReason::DOMAIN);
    EXPECT_EQ(match.choice_player(), -1);
    EXPECT_EQ(match.hand_size(1), 6 + 4);                           // the Extract never happened
}

TEST(LandGameMatch, EveryActionIsRejectedOnceTheGameIsOver) {
    LandGameMatch match = make_stacked_match(std::vector<LandType>(5, LandType::FOREST),
                                             std::vector<LandType>(6, LandType::MOUNTAIN),
                                             std::vector<LandType>(10, LandType::MOUNTAIN));
    ASSERT_TRUE(match.concede(1));
    ASSERT_TRUE(match.game_over());

    EXPECT_FALSE(match.play_land(0, 0));
    EXPECT_FALSE(match.pitch_land(0, 0));
    EXPECT_FALSE(match.end_turn(0));
    EXPECT_FALSE(match.pass_priority(0));
    EXPECT_FALSE(match.pass_priority(1));
    EXPECT_FALSE(match.cast_counter(0, 0, 1, 1));
    EXPECT_FALSE(match.cast_save(0, 0, 1));
    EXPECT_FALSE(match.choose_card(0, 0));
    EXPECT_FALSE(match.choose_surveil(0, {}, {}));
    EXPECT_FALSE(match.concede(0));
    EXPECT_FALSE(match.concede(1));
    EXPECT_EQ(match.hand_size(0), 5);
    EXPECT_EQ(match.winner_player(), 0);                            // the result never changes
}

TEST(LandGameMatch, ConcedeGivesTheOpponentTheWinAtAnyTime) {
    LandGameMatch match = make_stacked_match({LandType::SWAMP, LandType::FOREST, LandType::FOREST, LandType::FOREST, LandType::FOREST},
                                             std::vector<LandType>(6, LandType::MOUNTAIN),
                                             std::vector<LandType>(10, LandType::MOUNTAIN));
    ASSERT_TRUE(play_settled(match, 0, 0));                         // Swamp: Extract is now pending
    ASSERT_EQ(match.choice_type(), ChoiceType::EXTRACT);

    EXPECT_FALSE(match.concede(-1));
    EXPECT_FALSE(match.concede(2));
    EXPECT_TRUE(match.concede(0));                                  // the choosing player may still concede
    EXPECT_TRUE(match.game_over());
    EXPECT_EQ(match.winner_player(), 1);
    EXPECT_EQ(match.end_reason(), GameEndReason::CONCEDE);
    EXPECT_EQ(match.winner(), WinCondition::NONE);                  // not a Domain / Mono win
    EXPECT_EQ(match.choice_player(), -1);
}

TEST(LandGameMatch, ConcedeDuringAPriorityWindowClearsIt) {
    LandGameMatch match = make_match_where_only_p1_can_counter();
    ASSERT_TRUE(match.play_land(0, plain_card_id(match, 0)));
    ASSERT_EQ(match.priority_player(), 1);

    EXPECT_TRUE(match.concede(1));
    EXPECT_EQ(match.winner_player(), 0);
    EXPECT_EQ(match.priority_player(), -1);
    EXPECT_EQ(match.stack_size(), 0);
}

TEST(LandGameMatch, TurnCapEndsInADrawWhenBoardsAreEqual) {
    LandGameMatch match(42);
    for (int round = 1; round < LandGameMatch::kMaxRounds; ++round) {
        ASSERT_TRUE(match.end_turn(0));
        ASSERT_TRUE(match.end_turn(1));
        ASSERT_FALSE(match.game_over()) << "ended early in round " << round;
    }
    EXPECT_EQ(match.turn_number(), LandGameMatch::kMaxRounds);
    ASSERT_TRUE(match.end_turn(0));
    EXPECT_FALSE(match.game_over());                                // P2's 25th turn is still to play

    EXPECT_TRUE(match.end_turn(1));                                 // turn 50 completes
    EXPECT_TRUE(match.game_over());
    EXPECT_EQ(match.end_reason(), GameEndReason::TURN_CAP);
    EXPECT_TRUE(match.is_draw());
    EXPECT_EQ(match.winner_player(), -1);
    EXPECT_EQ(match.deck_size(), 0);                                // the library ran out on the way; play continued
}

TEST(LandGameMatch, TurnCapAwardsTheTiebreakWinner) {
    LandGameMatch match = make_stacked_match(std::vector<LandType>(5, LandType::FOREST),
                                             std::vector<LandType>(6, LandType::MOUNTAIN),
                                             std::vector<LandType>(40, LandType::MOUNTAIN));
    ASSERT_TRUE(play_settled(match, 0, 0));                         // P1 has one land, P2 none
    for (int round = 1; round < LandGameMatch::kMaxRounds; ++round) {
        ASSERT_TRUE(match.end_turn(0));
        ASSERT_TRUE(match.end_turn(1));
    }
    ASSERT_TRUE(match.end_turn(0));
    ASSERT_TRUE(match.end_turn(1));

    EXPECT_TRUE(match.game_over());
    EXPECT_EQ(match.end_reason(), GameEndReason::TURN_CAP);
    EXPECT_FALSE(match.is_draw());
    EXPECT_EQ(match.winner_player(), 0);
    EXPECT_EQ(match.winner(), WinCondition::NONE);
}

// Library empty, so hands only shrink: P1 holds 5 cards and P2 holds 6. After round 5 P1's hand is
// empty when its turn-6 deploy comes due. `p1_plays[r]` / `p2_plays[r]` name the card ids to deploy in round r.
LandGameMatch run_deploy_lock_game(const std::vector<LandType>& p0, const std::vector<LandType>& p1,
                                   const std::vector<int>& p1_plays) {
    LandGameMatch match = make_stacked_match(p0, p1, {});
    for (uint32_t round = 0; round < 5; ++round) {
        EXPECT_TRUE(play_settled(match, 0, round));
        EXPECT_TRUE(match.end_turn(0));
        if (p1_plays[round] >= 0) {
            EXPECT_TRUE(play_settled(match, 1, static_cast<uint32_t>(p1_plays[round])));
        }
        EXPECT_FALSE(match.game_over());
        EXPECT_TRUE(match.end_turn(1));                             // round 5: P1's turn 6 starts empty-handed
    }
    return match;
}

TEST(LandGameMatch, DeployLockAtTurnStartScoresByDistinctTypes) {
    // P1: 4 Forests + Island (2 types); P2: 4 Forests (1 type)
    LandGameMatch match = run_deploy_lock_game(
        {LandType::FOREST, LandType::FOREST, LandType::FOREST, LandType::FOREST, LandType::ISLAND},
        {LandType::FOREST, LandType::FOREST, LandType::FOREST, LandType::FOREST, LandType::ISLAND, LandType::ISLAND},
        {5, 6, 7, 8, -1});

    EXPECT_TRUE(match.game_over());
    EXPECT_EQ(match.end_reason(), GameEndReason::DEPLOY_LOCK);
    EXPECT_EQ(match.hand_size(0), 0);
    EXPECT_EQ(match.winner_player(), 0);
    EXPECT_FALSE(match.is_draw());
}

TEST(LandGameMatch, DeployLockScoresByTotalLandsWhenTypesTie) {
    // both control Forests + an Island, but P2 skipped a deploy: 5 lands vs 4
    LandGameMatch match = run_deploy_lock_game(
        {LandType::FOREST, LandType::FOREST, LandType::FOREST, LandType::FOREST, LandType::ISLAND},
        {LandType::FOREST, LandType::FOREST, LandType::FOREST, LandType::ISLAND, LandType::ISLAND, LandType::ISLAND},
        {5, 6, 7, -1, 8});

    EXPECT_EQ(match.end_reason(), GameEndReason::DEPLOY_LOCK);
    EXPECT_EQ(match.land_count(0, LandType::FOREST) + match.land_count(0, LandType::ISLAND), 5);
    EXPECT_EQ(match.land_count(1, LandType::FOREST) + match.land_count(1, LandType::ISLAND), 4);
    EXPECT_EQ(match.winner_player(), 0);
}

TEST(LandGameMatch, DeployLockWithIdenticalBoardsIsADraw) {
    LandGameMatch match = run_deploy_lock_game(
        {LandType::FOREST, LandType::FOREST, LandType::FOREST, LandType::FOREST, LandType::ISLAND},
        {LandType::FOREST, LandType::FOREST, LandType::FOREST, LandType::FOREST, LandType::ISLAND, LandType::ISLAND},
        {5, 6, 7, 8, 9});

    EXPECT_EQ(match.end_reason(), GameEndReason::DEPLOY_LOCK);
    EXPECT_TRUE(match.is_draw());
    EXPECT_EQ(match.winner_player(), -1);
}

TEST(LandGameMatch, EmptyHandAfterTheDeployIsNotALockYet) {
    LandGameMatch match = make_stacked_match(
        {LandType::FOREST, LandType::FOREST, LandType::FOREST, LandType::FOREST, LandType::ISLAND},
        std::vector<LandType>(6, LandType::MOUNTAIN), {});
    for (uint32_t round = 0; round < 4; ++round) {
        ASSERT_TRUE(play_settled(match, 0, round));
        ASSERT_TRUE(match.end_turn(0));
        ASSERT_TRUE(match.end_turn(1));
    }
    ASSERT_TRUE(match.play_land(0, 4));                             // the last card in hand
    EXPECT_EQ(match.hand_size(0), 0);
    EXPECT_TRUE(match.end_turn(0));                                 // the deploy was made: not a lock
    EXPECT_FALSE(match.game_over());
    EXPECT_TRUE(match.end_turn(1));                                 // P1's next deploy is now due with nothing to play
    EXPECT_EQ(match.end_reason(), GameEndReason::DEPLOY_LOCK);
    EXPECT_EQ(match.winner_player(), 0);                            // 5 lands vs. an empty board
}

TEST(LandGameMatch, PitchingTheLastCardLocksTheDeploy) {
    LandGameMatch match = make_stacked_match(std::vector<LandType>(5, LandType::FOREST),
                                             std::vector<LandType>(6, LandType::MOUNTAIN), {});
    for (uint32_t round = 0; round < 4; ++round) {
        ASSERT_TRUE(play_settled(match, 0, round));
        ASSERT_TRUE(match.end_turn(0));
        ASSERT_TRUE(match.end_turn(1));
    }
    ASSERT_TRUE(match.pitch_land(0, 4));                            // Ramp on the last card: nothing left to deploy
    ASSERT_EQ(match.hand_size(0), 0);
    ASSERT_FALSE(match.game_over());

    EXPECT_TRUE(match.end_turn(0));
    EXPECT_TRUE(match.game_over());
    EXPECT_EQ(match.end_reason(), GameEndReason::DEPLOY_LOCK);
    EXPECT_EQ(match.winner_player(), 0);                            // 4 Forests vs. an empty board
}

TEST(LandGameMatch, ActionLogRecordsAcceptedActionsInOrder) {
    LandGameMatch match = make_stacked_match({LandType::FOREST, LandType::FOREST, LandType::FOREST, LandType::FOREST, LandType::MOUNTAIN},
                                             std::vector<LandType>(6, LandType::MOUNTAIN),
                                             std::vector<LandType>(10, LandType::MOUNTAIN));
    EXPECT_TRUE(match.action_log().empty());

    EXPECT_FALSE(match.play_land(0, 9999));                         // rejected: not logged
    EXPECT_FALSE(match.end_turn(1));
    EXPECT_TRUE(match.pitch_land(0, 0));                            // Forest -> Ramp
    EXPECT_TRUE(match.play_land(0, 4));                             // the Mountain: nothing to destroy yet
    EXPECT_TRUE(match.end_turn(0));

    const std::vector<Action> expected{
        Action{ActionType::PITCH_LAND, 0, 0, std::nullopt, std::nullopt, {}, {}},
        Action{ActionType::PLAY_LAND, 0, 4, std::nullopt, std::nullopt, {}, {}},
        Action{ActionType::END_TURN, 0, 0, std::nullopt, std::nullopt, {}, {}},
    };
    EXPECT_EQ(match.action_log(), expected);
}

TEST(LandGameMatch, ActionLogRecordsTargetsCostsAndAnswers) {
    LandGameMatch match = make_stacked_match({LandType::MOUNTAIN, LandType::SWAMP, LandType::FOREST, LandType::FOREST, LandType::FOREST},
                                             std::vector<LandType>(6, LandType::FOREST), std::vector<LandType>(10, LandType::FOREST));
    ASSERT_TRUE(match.pitch_land(0, 0, 4));                         // Mountain Rummage, discarding card 4
    ASSERT_TRUE(match.play_land(0, 1));                             // Swamp: Extract is pending
    ASSERT_TRUE(match.choose_card(0, 6));
    ASSERT_TRUE(match.concede(1));

    ASSERT_EQ(match.action_log().size(), 4);
    EXPECT_EQ(match.action_log()[0], (Action{ActionType::PITCH_LAND, 0, 0, 4u, std::nullopt, {}, {}}));
    EXPECT_EQ(match.action_log()[2], (Action{ActionType::CHOOSE_CARD, 0, 6, std::nullopt, std::nullopt, {}, {}}));
    EXPECT_EQ(match.action_log()[3].type, ActionType::CONCEDE);
}

TEST(LandGameMatch, ApplyDispatchesToTheMatchingAction) {
    LandGameMatch match = make_stacked_match(std::vector<LandType>(5, LandType::FOREST),
                                             std::vector<LandType>(6, LandType::MOUNTAIN), {});
    EXPECT_FALSE(match.apply(Action{ActionType::PLAY_LAND, 1, 5, std::nullopt, std::nullopt, {}, {}}));  // not P2's turn
    EXPECT_FALSE(match.apply(Action{ActionType::CAST_COUNTER, 0, 0, std::nullopt, 1u, {}, {}}));         // missing the extra card
    EXPECT_FALSE(match.apply(Action{ActionType::CAST_SAVE, 0, 0, std::nullopt, std::nullopt, {}, {}}));  // missing the land
    EXPECT_TRUE(match.apply(Action{ActionType::PLAY_LAND, 0, 0, std::nullopt, std::nullopt, {}, {}}));
    EXPECT_EQ(match.land_count(0, LandType::FOREST), 1);
    EXPECT_TRUE(match.apply(Action{ActionType::END_TURN, 0, 0, std::nullopt, std::nullopt, {}, {}}));
    EXPECT_EQ(match.active_player(), 1);
}

TEST(LandGameMatch, DeployDueUntilTheDeployIsMade) {
    LandGameMatch match = make_stacked_match(std::vector<LandType>(5, LandType::FOREST),
                                             std::vector<LandType>(6, LandType::MOUNTAIN), {});
    EXPECT_TRUE(match.deploy_due());
    ASSERT_TRUE(match.play_land(0, 0));
    EXPECT_FALSE(match.deploy_due());
    ASSERT_TRUE(match.end_turn(0));
    EXPECT_TRUE(match.deploy_due());                                // P2 owes its deploy now
    ASSERT_TRUE(match.concede(0));
    EXPECT_FALSE(match.deploy_due());                               // nothing is due once the game is over
}

TEST(LandGameMatch, LegalActionsAtTheStartOfAMatch) {
    LandGameMatch match = make_stacked_match({LandType::FOREST, LandType::FOREST, LandType::MOUNTAIN, LandType::SWAMP, LandType::ISLAND},
                                             std::vector<LandType>(6, LandType::MOUNTAIN), {});

    EXPECT_TRUE(match.legal_actions(1).empty());                    // not P2's move
    const std::vector<Action> legal = match.legal_actions(0);
    auto count = [&](ActionType type) {
        return std::count_if(legal.begin(), legal.end(), [type](const Action& a) { return a.type == type; });
    };
    EXPECT_EQ(count(ActionType::PLAY_LAND), 5);                     // no target exists yet, so one deploy per card
    EXPECT_EQ(count(ActionType::PITCH_LAND), 2 + 4 + 1);            // Forest x2 (Ramp), Mountain (4 discards), Swamp (Surveil); Island is reactive
    EXPECT_EQ(count(ActionType::END_TURN), 1);
    EXPECT_EQ(legal.size(), 5 + 7 + 1);
    EXPECT_THROW(match.legal_actions(2), std::out_of_range);
    EXPECT_THROW(match.legal_actions(-1), std::out_of_range);
}

TEST(LandGameMatch, LegalActionsOfAPriorityWindow) {
    LandGameMatch match = make_match_where_only_p1_can_counter();
    ASSERT_TRUE(match.play_land(0, plain_card_id(match, 0)));
    ASSERT_EQ(match.priority_player(), 1);

    EXPECT_TRUE(match.legal_actions(0).empty());                    // the pusher waits
    const std::vector<Action> legal = match.legal_actions(1);
    EXPECT_TRUE(contains_type(legal, ActionType::PASS_PRIORITY));
    EXPECT_TRUE(contains_type(legal, ActionType::CAST_COUNTER));
    EXPECT_FALSE(contains_type(legal, ActionType::PLAY_LAND));
    EXPECT_FALSE(contains_type(legal, ActionType::END_TURN));
    for (const Action& action : legal) {
        if (action.type == ActionType::CAST_COUNTER) {
            EXPECT_EQ(*action.target_id, match.stack_top().stack_id);   // the only item on the stack
        }
    }
}

TEST(LandGameMatch, LegalActionsOfAPendingChoice) {
    LandGameMatch match = make_stacked_match({LandType::SWAMP, LandType::FOREST, LandType::FOREST, LandType::FOREST, LandType::FOREST},
                                             std::vector<LandType>(6, LandType::MOUNTAIN), {});
    ASSERT_TRUE(match.play_land(0, 0));
    ASSERT_EQ(match.choice_type(), ChoiceType::EXTRACT);

    EXPECT_TRUE(match.legal_actions(1).empty());
    const std::vector<Action> legal = match.legal_actions(0);
    EXPECT_EQ(legal.size(), 6);                                     // one pick per card in P2's hand
    for (const Action& action : legal) {
        EXPECT_EQ(action.type, ActionType::CHOOSE_CARD);
    }
}

TEST(LandGameMatch, LegalSurveilAnswersCoverEveryPartition) {
    LandGameMatch match = make_stacked_match({LandType::SWAMP, LandType::FOREST, LandType::FOREST, LandType::FOREST, LandType::FOREST},
                                             std::vector<LandType>(6, LandType::MOUNTAIN),
                                             std::vector<LandType>(5, LandType::PLAINS));
    ASSERT_TRUE(match.pitch_land(0, 0));
    ASSERT_EQ(match.choice_type(), ChoiceType::SURVEIL);

    // 2 peeked cards: bin both, bin either one, or keep both in either order = 5 answers
    EXPECT_EQ(match.legal_actions(0).size(), 5);
}

TEST(LandGameMatch, NoLegalActionsOnceTheGameIsOver) {
    LandGameMatch match(12345);
    ASSERT_TRUE(match.concede(0));
    EXPECT_TRUE(match.legal_actions(0).empty());
    EXPECT_TRUE(match.legal_actions(1).empty());
}

TEST(LandGameMatch, MainPhaseTimeoutAutoDeploysARandomLegalCard) {
    LandGameMatch match = make_stacked_match({LandType::FOREST, LandType::PLAINS, LandType::MOUNTAIN, LandType::FOREST, LandType::PLAINS},
                                             std::vector<LandType>(6, LandType::MOUNTAIN),
                                             std::vector<LandType>(10, LandType::MOUNTAIN));
    EXPECT_EQ(match.afk_strikes(0), 0);

    EXPECT_TRUE(match.main_phase_timeout(0));
    EXPECT_EQ(match.afk_strikes(0), 1);
    EXPECT_EQ(match.afk_strikes(1), 0);
    EXPECT_TRUE(match.played_land_this_turn());
    EXPECT_EQ(total_lands(match, 0), 1);
    EXPECT_EQ(match.hand_size(0), 4);
    EXPECT_FALSE(match.deploy_due());
}

TEST(LandGameMatch, MainPhaseTimeoutPicksEveryCardEventually) {
    std::set<size_t> picked_positions;
    for (uint64_t seed = 1; seed <= 300; ++seed) {
        LandGameMatch match(seed);
        const std::vector<Card> hand_before = match.hand(0);
        if (!match.main_phase_timeout(0)) {
            continue;
        }
        settle(match);
        for (size_t i = 0; i < hand_before.size(); ++i) {
            if (!hand_holds(match, 0, hand_before[i].get_identifier()) &&
                match.land_count(0, hand_before[i].get_land_type()) > 0) {
                picked_positions.insert(i);
            }
        }
    }
    EXPECT_EQ(picked_positions.size(), 5);                          // every hand position was chosen at some point
}

TEST(LandGameMatch, MainPhaseTimeoutChoosesAValidTarget) {
    // P2 holds a land; P1 holds only Mountains, so a legal deploy must aim at that land
    LandGameMatch match = make_stacked_match({LandType::FOREST, LandType::MOUNTAIN, LandType::MOUNTAIN, LandType::MOUNTAIN, LandType::MOUNTAIN},
                                             std::vector<LandType>(6, LandType::FOREST), std::vector<LandType>(10, LandType::FOREST));
    ASSERT_TRUE(match.play_land(0, 0));
    ASSERT_TRUE(match.end_turn(0));
    ASSERT_TRUE(match.play_land(1, 5));
    ASSERT_TRUE(match.end_turn(1));
    ASSERT_EQ(total_lands(match, 1), 1);

    EXPECT_TRUE(match.main_phase_timeout(0));
    EXPECT_EQ(total_lands(match, 1), 0);                            // the Mountain destroyed P2's only land
    EXPECT_EQ(match.land_count(0, LandType::MOUNTAIN), 1);
    EXPECT_EQ(match.graveyard_size(), 1);
}

TEST(LandGameMatch, MainPhaseTimeoutAfterTheDeployEndsTheTurn) {
    LandGameMatch match = make_stacked_match(std::vector<LandType>(5, LandType::FOREST),
                                             std::vector<LandType>(6, LandType::MOUNTAIN), {});
    ASSERT_TRUE(match.play_land(0, 0));

    EXPECT_TRUE(match.main_phase_timeout(0));
    EXPECT_EQ(match.active_player(), 1);
    EXPECT_EQ(match.afk_strikes(0), 1);
    EXPECT_EQ(total_lands(match, 0), 1);                            // no second deploy was forced
}

TEST(LandGameMatch, MainPhaseTimeoutOnlyAppliesToTheActivePlayersFreeClock) {
    LandGameMatch match = make_stacked_match({LandType::SWAMP, LandType::FOREST, LandType::FOREST, LandType::FOREST, LandType::FOREST},
                                             std::vector<LandType>(6, LandType::MOUNTAIN), {});
    EXPECT_FALSE(match.main_phase_timeout(1));                      // not P2's turn
    EXPECT_FALSE(match.main_phase_timeout(-1));
    EXPECT_FALSE(match.main_phase_timeout(2));

    ASSERT_TRUE(match.play_land(0, 0));                             // Swamp: the clock pauses for the choice
    ASSERT_EQ(match.choice_player(), 0);
    EXPECT_FALSE(match.main_phase_timeout(0));
    EXPECT_EQ(match.afk_strikes(0), 0);
    EXPECT_EQ(match.afk_strikes(1), 0);

    ASSERT_TRUE(match.concede(0));
    EXPECT_FALSE(match.main_phase_timeout(1));
}

TEST(LandGameMatch, PriorityTimeoutPassesWithoutAStrike) {
    LandGameMatch match = make_match_where_only_p1_can_counter();
    const Card deployed = plain_card(match, 0);
    EXPECT_FALSE(match.priority_timeout(1));                        // no window yet

    ASSERT_TRUE(match.play_land(0, deployed.get_identifier()));
    EXPECT_FALSE(match.priority_timeout(0));                        // the pusher holds no priority
    EXPECT_TRUE(match.priority_timeout(1));
    EXPECT_EQ(match.priority_player(), -1);
    EXPECT_EQ(match.stack_size(), 0);
    EXPECT_EQ(match.land_count(0, deployed.get_land_type()), 1);    // it resolved
    EXPECT_EQ(match.afk_strikes(1), 0);
}

TEST(LandGameMatch, ChoiceTimeoutMakesARandomLegalChoiceAndStrikes) {
    LandGameMatch match = make_stacked_match({LandType::SWAMP, LandType::FOREST, LandType::FOREST, LandType::FOREST, LandType::FOREST},
                                             std::vector<LandType>(6, LandType::MOUNTAIN), {});
    EXPECT_FALSE(match.choice_timeout(0));                          // nothing pending
    ASSERT_TRUE(match.play_land(0, 0));
    ASSERT_EQ(match.choice_type(), ChoiceType::EXTRACT);

    EXPECT_FALSE(match.choice_timeout(1));                          // not the choosing player
    EXPECT_TRUE(match.choice_timeout(0));
    EXPECT_EQ(match.choice_player(), -1);
    EXPECT_EQ(match.hand_size(1), 5);                               // exactly one card was extracted
    EXPECT_EQ(match.graveyard_size(), 1);
    EXPECT_EQ(match.afk_strikes(0), 1);
}

TEST(LandGameMatch, ChoiceTimeoutResolvesSurveilToo) {
    LandGameMatch match = make_stacked_match({LandType::SWAMP, LandType::FOREST, LandType::FOREST, LandType::FOREST, LandType::FOREST},
                                             std::vector<LandType>(6, LandType::MOUNTAIN),
                                             std::vector<LandType>(5, LandType::PLAINS));
    ASSERT_TRUE(match.pitch_land(0, 0));
    ASSERT_EQ(match.choice_type(), ChoiceType::SURVEIL);

    EXPECT_TRUE(match.choice_timeout(0));
    EXPECT_EQ(match.choice_player(), -1);
    EXPECT_EQ(match.deck_size() + (match.graveyard_size() - 1), 5);     // the 2 peeked cards went to the bin or stayed
    EXPECT_EQ(match.afk_strikes(0), 1);
}

TEST(LandGameMatch, ThirdAfkStrikeForfeitsInsteadOfActing) {
    LandGameMatch match = make_stacked_match(std::vector<LandType>(5, LandType::FOREST),
                                             std::vector<LandType>(6, LandType::MOUNTAIN),
                                             std::vector<LandType>(10, LandType::MOUNTAIN));
    ASSERT_TRUE(match.main_phase_timeout(0));                       // strike 1: auto-deploy
    ASSERT_TRUE(match.main_phase_timeout(0));                       // strike 2: the turn ends
    ASSERT_TRUE(match.end_turn(1));                                 // P2 plays its turn normally
    ASSERT_EQ(match.afk_strikes(0), 2);
    ASSERT_FALSE(match.game_over());

    EXPECT_TRUE(match.main_phase_timeout(0));                       // strike 3
    EXPECT_EQ(match.afk_strikes(0), 3);
    EXPECT_TRUE(match.game_over());
    EXPECT_EQ(match.end_reason(), GameEndReason::AFK_FORFEIT);
    EXPECT_EQ(match.winner_player(), 1);
    EXPECT_EQ(total_lands(match, 0), 1);                            // the third timeout deployed nothing
    EXPECT_FALSE(match.is_draw());
}

TEST(LandGameMatch, DisconnectForfeitEndsTheMatch) {
    LandGameMatch match(12345);
    EXPECT_FALSE(match.disconnect_forfeit(-1));
    EXPECT_FALSE(match.disconnect_forfeit(2));

    EXPECT_TRUE(match.disconnect_forfeit(1));                       // the player who is not on move may drop too
    EXPECT_TRUE(match.game_over());
    EXPECT_EQ(match.end_reason(), GameEndReason::DISCONNECT_FORFEIT);
    EXPECT_EQ(match.winner_player(), 0);
    EXPECT_FALSE(match.disconnect_forfeit(0));
}

TEST(LandGameMatch, AfkStrikesRejectBadPlayerIndex) {
    LandGameMatch match(12345);
    EXPECT_THROW(match.afk_strikes(-1), std::out_of_range);
    EXPECT_THROW(match.afk_strikes(2), std::out_of_range);
}

TEST(LandGameMatch, TimerEventsAreLoggedAsEventsNotAsTheirActions) {
    LandGameMatch match = make_stacked_match({LandType::SWAMP, LandType::FOREST, LandType::FOREST, LandType::FOREST, LandType::FOREST},
                                             std::vector<LandType>(6, LandType::MOUNTAIN), {});
    ASSERT_TRUE(match.play_land(0, 0));                             // Extract pending
    ASSERT_TRUE(match.choice_timeout(0));

    ASSERT_EQ(match.action_log().size(), 2);
    EXPECT_EQ(match.action_log()[1], (Action{ActionType::CHOICE_TIMEOUT, 0, 0, std::nullopt, std::nullopt, {}, {}}));
}

TEST(LandGameMatch, TimerPicksAreDeterministicPerSeed) {
    LandGameMatch a(777);
    LandGameMatch b(777);
    ASSERT_TRUE(a.main_phase_timeout(0));
    ASSERT_TRUE(b.main_phase_timeout(0));
    EXPECT_EQ(ids_of(a.hand(0)), ids_of(b.hand(0)));
    EXPECT_EQ(ids_of(a.battlefield(0)), ids_of(b.battlefield(0)));
}

TEST(LandGameMatch, StateDigestMatchesForIdenticalMatches) {
    LandGameMatch a(31);
    LandGameMatch b(31);
    EXPECT_EQ(a.state_digest(), b.state_digest());
    EXPECT_NE(a.state_digest(), LandGameMatch(32).state_digest());
}

TEST(LandGameMatch, StateDigestChangesWithEveryObservableChange) {
    LandGameMatch match = make_stacked_match({LandType::FOREST, LandType::FOREST, LandType::MOUNTAIN, LandType::SWAMP, LandType::ISLAND},
                                             std::vector<LandType>(6, LandType::FOREST), std::vector<LandType>(10, LandType::FOREST));
    std::set<uint64_t> seen{match.state_digest()};

    ASSERT_TRUE(match.pitch_land(0, 0));                            // Ramp: hand, graveyard, flags
    EXPECT_TRUE(seen.insert(match.state_digest()).second);
    ASSERT_TRUE(match.play_land(0, 2));                             // a deploy: battlefield
    EXPECT_TRUE(seen.insert(match.state_digest()).second);
    ASSERT_TRUE(match.main_phase_timeout(0));                       // the deploy is done: the turn ends, +1 strike
    EXPECT_TRUE(seen.insert(match.state_digest()).second);
    ASSERT_TRUE(match.concede(1));                                  // the result
    EXPECT_TRUE(seen.insert(match.state_digest()).second);
}

TEST(LandGameMatch, StateDigestSeesLibraryOrder) {
    // same cards in the same zones, but the library is stacked in a different order
    LandGameMatch a = make_stacked_match(std::vector<LandType>(5, LandType::FOREST), std::vector<LandType>(6, LandType::FOREST),
                                         {LandType::PLAINS, LandType::ISLAND});
    LandGameMatch b = make_stacked_match(std::vector<LandType>(5, LandType::FOREST), std::vector<LandType>(6, LandType::FOREST),
                                         {LandType::ISLAND, LandType::PLAINS});
    EXPECT_NE(a.state_digest(), b.state_digest());
}

} // namespace lands