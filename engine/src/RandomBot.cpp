#include "lands/RandomBot.hpp"

#include <algorithm>
#include <iterator>

namespace lands {

RandomBot::RandomBot(uint64_t seed, int timer_percent, int idle_percent)
    : rng_(seed * 7919 + 1), timer_percent_(timer_percent), idle_percent_(idle_percent) {}

bool RandomBot::step(LandGameMatch& match) {
    if (match.game_over()) {
        return false;
    }

    // exactly one player can act at any moment
    std::vector<Action> options = match.legal_actions(0);
    int mover = 0;
    if (options.empty()) {
        options = match.legal_actions(1);
        mover = 1;
    }

    // sometimes the clock that is running for the mover runs out
    if (timer_percent_ > 0 && static_cast<int>(rng_() % 100) < timer_percent_) {
        ActionType timeout = ActionType::MAIN_PHASE_TIMEOUT;
        if (match.choice_player() == mover) {
            timeout = ActionType::CHOICE_TIMEOUT;
        } else if (match.priority_player() == mover) {
            timeout = ActionType::PRIORITY_TIMEOUT;
        }
        return match.apply(Action{timeout, mover, 0, std::nullopt, std::nullopt, {}, {}});
    }

    // a stalling player passes the turn whenever the engine lets them (it never forces a deploy)
    if (idle_percent_ > 0 && static_cast<int>(rng_() % 100) < idle_percent_) {
        const auto end_turn = std::find_if(options.begin(), options.end(),
                                           [](const Action& action) { return action.type == ActionType::END_TURN; });
        if (end_turn != options.end()) {
            return match.apply(*end_turn);
        }
    }

    if (match.deploy_due() && match.active_player() == mover) {
        std::vector<Action> deploys;
        std::copy_if(options.begin(), options.end(), std::back_inserter(deploys),
                     [](const Action& action) { return action.type == ActionType::PLAY_LAND; });
        if (!deploys.empty()) {
            options = deploys;
        }
    }
    return match.apply(options[static_cast<size_t>(rng_() % options.size())]);
}

} // namespace lands
