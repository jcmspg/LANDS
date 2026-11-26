#include "lands/LandGameMatch.hpp"

#include <algorithm>
#include <numeric>
#include <stdexcept>

namespace lands {

namespace {

CardView view_of(const Card& card) {
    return CardView{card.get_identifier(), card.get_land_type()};
}

void require_viewer(int viewer_index) {
    if (viewer_index < 0 || viewer_index >= 2) {
        throw std::out_of_range("LandGameMatch::snapshot_for: viewer out of range");
    }
}

std::vector<CardView> views_of(const std::vector<Card>& cards) {
    std::vector<CardView> views;
    views.reserve(cards.size());
    for (const Card& card : cards) {
        views.push_back(view_of(card));
    }
    return views;
}

} // namespace

Snapshot LandGameMatch::snapshot_for(int viewer_index) const {
    require_viewer(viewer_index);
    const int other_index = 1 - viewer_index;

    Snapshot snapshot;
    snapshot.viewer = viewer_index;
    snapshot.turn_number = turn_number_;
    snapshot.active_player = active_player_;
    snapshot.priority_holder = priority_player_;

    auto fill_player = [this](int index, bool reveal_hand) {
        PlayerView view;
        view.id = index;
        view.afk_strikes = afk_strikes_[index];
        view.hand_count = players_[index].hand_size();
        if (reveal_hand) {
            view.hand = views_of(players_[index].hand());
        }
        view.battlefield = views_of(players_[index].battlefield());
        if (index == active_player_ && !game_over()) {
            view.main_pitch_used = pitched_this_turn_;
            view.deploys_remaining = (played_land_ ? 0 : 1) + (ramp_available_ ? 1 : 0);
        }
        return view;
    };
    snapshot.you = fill_player(viewer_index, true);
    snapshot.opponent = fill_player(other_index, false);

    for (const StackItem& item : stack_.items()) {
        StackItemView view;
        view.stack_id = item.stack_id;
        view.pusher = item.caster_id;
        view.kind = item.mode;
        view.ability = item.ability;
        view.card = view_of(item.card);
        view.target_instance_id = item.target_card_id;
        if (item.target_stack_id != 0) {
            view.target_stack_id = item.target_stack_id;
        }
        snapshot.stack.push_back(view);
    }

    if (pending_choice_.type != ChoiceType::NONE) {
        ChoiceView choice;
        choice.holder = pending_choice_.player;
        choice.type = pending_choice_.type;
        if (viewer_index == pending_choice_.player) {
            choice.options = views_of(pending_choice_.options);
        }
        snapshot.choice = choice;
    }

    snapshot.library_count = deck_.deck_size();
    // a Memory-Lapsed land stays public knowledge only while it is still the top card
    if (known_top_.has_value()) {
        const std::vector<Card> top = deck_.peek_top(1);
        if (!top.empty() && top[0] == *known_top_) {
            snapshot.library_top_known = view_of(top[0]);
        }
    }
    snapshot.graveyard = views_of(graveyard_.cards());

    snapshot.game_over = game_over();
    snapshot.winner_id = winner_player_;
    snapshot.end_reason = end_reason_;
    snapshot.is_draw = is_draw();
    if (end_reason_ == GameEndReason::DEPLOY_LOCK || end_reason_ == GameEndReason::TURN_CAP) {
        TiebreakView tiebreak;
        for (int index = 0; index < 2; ++index) {
            const std::array<uint8_t, 5>& counts = players_[index].land_counts();
            tiebreak.distinct_types[index] = static_cast<int>(
                std::count_if(counts.begin(), counts.end(), [](uint8_t count) { return count > 0; }));
            tiebreak.total_lands[index] = std::accumulate(counts.begin(), counts.end(), 0);
        }
        snapshot.tiebreak = tiebreak;
    }
    return snapshot;
}

} // namespace lands
