// Simulation harness (GDD §10, Phase 1): plays many random-bot matches and checks the engine's
// guarantees on every one of them:
//   - the match terminates (boundedness),
//   - the action log replays to the identical final state (bit-for-bit determinism),
//   - every listed legal action is accepted.
// It also reports outcomes and which mechanics the bots exercised. Exit code 0 only if every
// match passed.
//
// usage: lands_sim [--matches N] [--seed S] [--timer-percent P] [--idle-percent P]

#include "lands/LandGameMatch.hpp"
#include "lands/RandomBot.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>

namespace {

using lands::ActionType;
using lands::GameEndReason;
using lands::LandGameMatch;

const char* name_of(ActionType type) {
    switch (type) {
        case ActionType::PLAY_LAND: return "PLAY_LAND";
        case ActionType::PITCH_LAND: return "PITCH_LAND";
        case ActionType::CAST_COUNTER: return "CAST_COUNTER";
        case ActionType::CAST_SAVE: return "CAST_SAVE";
        case ActionType::PASS_PRIORITY: return "PASS_PRIORITY";
        case ActionType::CHOOSE_CARD: return "CHOOSE_CARD";
        case ActionType::CHOOSE_SURVEIL: return "CHOOSE_SURVEIL";
        case ActionType::END_TURN: return "END_TURN";
        case ActionType::CONCEDE: return "CONCEDE";
        case ActionType::MAIN_PHASE_TIMEOUT: return "MAIN_PHASE_TIMEOUT";
        case ActionType::PRIORITY_TIMEOUT: return "PRIORITY_TIMEOUT";
        case ActionType::CHOICE_TIMEOUT: return "CHOICE_TIMEOUT";
        case ActionType::DISCONNECT_FORFEIT: return "DISCONNECT_FORFEIT";
    }
    return "?";
}

const char* name_of(GameEndReason reason) {
    switch (reason) {
        case GameEndReason::NONE: return "NONE";
        case GameEndReason::DOMAIN: return "DOMAIN";
        case GameEndReason::MONO: return "MONO";
        case GameEndReason::DEPLOY_LOCK: return "DEPLOY_LOCK";
        case GameEndReason::TURN_CAP: return "TURN_CAP";
        case GameEndReason::CONCEDE: return "CONCEDE";
        case GameEndReason::AFK_FORFEIT: return "AFK_FORFEIT";
        case GameEndReason::DISCONNECT_FORFEIT: return "DISCONNECT_FORFEIT";
    }
    return "?";
}

constexpr int kMaxStepsPerMatch = 20000;

} // namespace

int main(int argc, char** argv) {
    uint64_t matches = 10000;
    uint64_t first_seed = 1;
    int timer_percent = 5;
    int idle_percent = 0;
    for (int i = 1; i + 1 < argc; i += 2) {
        if (std::strcmp(argv[i], "--matches") == 0) { matches = std::strtoull(argv[i + 1], nullptr, 10); }
        else if (std::strcmp(argv[i], "--seed") == 0) { first_seed = std::strtoull(argv[i + 1], nullptr, 10); }
        else if (std::strcmp(argv[i], "--timer-percent") == 0) { timer_percent = std::atoi(argv[i + 1]); }
        else if (std::strcmp(argv[i], "--idle-percent") == 0) { idle_percent = std::atoi(argv[i + 1]); }
        else {
            std::cerr << "usage: lands_sim [--matches N] [--seed S] [--timer-percent P] [--idle-percent P]\n";
            return 2;
        }
    }

    std::map<GameEndReason, uint64_t> endings;
    std::map<ActionType, uint64_t> actions;
    uint64_t wins[2] = {0, 0};
    uint64_t draws = 0;
    uint64_t rounds = 0;
    uint64_t failures = 0;

    for (uint64_t i = 0; i < matches; ++i) {
        const uint64_t seed = first_seed + i;
        LandGameMatch match(seed);
        lands::RandomBot bot(seed, timer_percent, idle_percent);

        int steps = 0;
        while (bot.step(match)) {
            if (++steps > kMaxStepsPerMatch) {
                break;
            }
        }
        if (!match.game_over()) {
            std::cerr << "seed " << seed << ": did not terminate\n";
            ++failures;
            continue;
        }

        const LandGameMatch replayed = LandGameMatch::replay(seed, match.action_log());
        if (replayed.state_digest() != match.state_digest()) {
            std::cerr << "seed " << seed << ": replay diverged\n";
            ++failures;
            continue;
        }

        ++endings[match.end_reason()];
        for (const lands::Action& action : match.action_log()) {
            ++actions[action.type];
        }
        if (match.is_draw()) { ++draws; } else { ++wins[match.winner_player()]; }
        rounds += static_cast<uint64_t>(match.turn_number());
    }

    const uint64_t played = matches - failures;
    std::cout << "matches: " << matches << " (" << failures << " failed), timer events: " << timer_percent << "%, idle: " << idle_percent << "%\n";
    if (played > 0) {
        std::cout << std::fixed << std::setprecision(1);
        std::cout << "P1 wins " << 100.0 * static_cast<double>(wins[0]) / static_cast<double>(played)
                  << "%, P2 wins " << 100.0 * static_cast<double>(wins[1]) / static_cast<double>(played)
                  << "%, draws " << 100.0 * static_cast<double>(draws) / static_cast<double>(played) << "%\n";
        std::cout << "average rounds: " << static_cast<double>(rounds) / static_cast<double>(played) << "\n";
    }
    std::cout << "endings:\n";
    for (const auto& [reason, count] : endings) {
        std::cout << "  " << std::setw(20) << std::left << name_of(reason) << count << "\n";
    }
    std::cout << "actions applied:\n";
    for (const auto& [type, count] : actions) {
        std::cout << "  " << std::setw(20) << std::left << name_of(type) << count << "\n";
    }
    return failures == 0 ? 0 : 1;
}
