// Simulation harness (GDD §10, Phase 1): plays many random-bot matches and checks the engine's
// guarantees on every one of them:
//   - the match terminates (boundedness),
//   - the action log replays to the identical final state (bit-for-bit determinism),
//   - every listed legal action is accepted.
// It also reports outcomes and which mechanics the bots exercised. Exit code 0 only if every
// match passed.
//
// usage: lands_sim [--matches N] [--seed S] [--timer-percent P] [--idle-percent P]
//                  [--config both|draw-skip-only|sixth-card-only|all]
// With --config all (the default) it runs the three first-player compensation packages of GDD
// §3.1 side by side; a single config also prints the endings and action coverage.

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

struct Stats {
    std::map<GameEndReason, uint64_t> endings;
    std::map<ActionType, uint64_t> actions;
    uint64_t wins[2] = {0, 0};
    uint64_t draws = 0;
    uint64_t rounds = 0;
    uint64_t failures = 0;
    uint64_t played() const { return wins[0] + wins[1] + draws; }
};

struct Profile {
    uint64_t matches;
    uint64_t first_seed;
    int timer_percent;
    int idle_percent;
};

Stats run_batch(const Profile& profile, lands::MatchConfig config) {
    Stats stats;
    for (uint64_t i = 0; i < profile.matches; ++i) {
        const uint64_t seed = profile.first_seed + i;
        LandGameMatch match(seed, config);
        lands::RandomBot bot(seed, profile.timer_percent, profile.idle_percent);

        int steps = 0;
        while (bot.step(match)) {
            if (++steps > kMaxStepsPerMatch) {
                break;
            }
        }
        if (!match.game_over()) {
            std::cerr << "seed " << seed << ": did not terminate\n";
            ++stats.failures;
            continue;
        }

        const LandGameMatch replayed = LandGameMatch::replay(seed, match.action_log(), config);
        if (replayed.state_digest() != match.state_digest()) {
            std::cerr << "seed " << seed << ": replay diverged\n";
            ++stats.failures;
            continue;
        }

        ++stats.endings[match.end_reason()];
        for (const lands::Action& action : match.action_log()) {
            ++stats.actions[action.type];
        }
        if (match.is_draw()) { ++stats.draws; } else { ++stats.wins[match.winner_player()]; }
        stats.rounds += static_cast<uint64_t>(match.turn_number());
    }
    return stats;
}

double percent(uint64_t part, uint64_t whole) {
    return whole == 0 ? 0.0 : 100.0 * static_cast<double>(part) / static_cast<double>(whole);
}

void print_summary(const char* label, const Stats& stats) {
    std::cout << std::setw(18) << std::left << label << std::right
              << " P1 " << std::setw(5) << percent(stats.wins[0], stats.played()) << "%"
              << "  P2 " << std::setw(5) << percent(stats.wins[1], stats.played()) << "%"
              << "  draw " << std::setw(5) << percent(stats.draws, stats.played()) << "%"
              << "  rounds " << std::setw(5)
              << (stats.played() == 0 ? 0.0 : static_cast<double>(stats.rounds) / static_cast<double>(stats.played()))
              << "  failed " << stats.failures << "\n";
}

void print_details(const Stats& stats) {
    std::cout << "endings:\n";
    for (const auto& [reason, count] : stats.endings) {
        std::cout << "  " << std::setw(20) << std::left << name_of(reason) << count << "\n";
    }
    std::cout << "actions applied:\n";
    for (const auto& [type, count] : stats.actions) {
        std::cout << "  " << std::setw(20) << std::left << name_of(type) << count << "\n";
    }
}

} // namespace

int main(int argc, char** argv) {
    Profile profile{10000, 1, 5, 0};
    std::string config_name = "all";
    for (int i = 1; i + 1 < argc; i += 2) {
        if (std::strcmp(argv[i], "--matches") == 0) { profile.matches = std::strtoull(argv[i + 1], nullptr, 10); }
        else if (std::strcmp(argv[i], "--seed") == 0) { profile.first_seed = std::strtoull(argv[i + 1], nullptr, 10); }
        else if (std::strcmp(argv[i], "--timer-percent") == 0) { profile.timer_percent = std::atoi(argv[i + 1]); }
        else if (std::strcmp(argv[i], "--idle-percent") == 0) { profile.idle_percent = std::atoi(argv[i + 1]); }
        else if (std::strcmp(argv[i], "--config") == 0) { config_name = argv[i + 1]; }
        else {
            std::cerr << "usage: lands_sim [--matches N] [--seed S] [--timer-percent P] [--idle-percent P]\n"
                         "                 [--config both|draw-skip-only|sixth-card-only|all]\n";
            return 2;
        }
    }

    const std::map<std::string, lands::MatchConfig> configs{
        {"both", lands::MatchConfig::both()},
        {"draw-skip-only", lands::MatchConfig::draw_skip_only()},
        {"sixth-card-only", lands::MatchConfig::sixth_card_only()},
    };
    if (config_name != "all" && configs.find(config_name) == configs.end()) {
        std::cerr << "unknown --config " << config_name << "\n";
        return 2;
    }

    std::cout << std::fixed << std::setprecision(1);
    std::cout << "matches per config: " << profile.matches << ", timer events: " << profile.timer_percent
              << "%, idle: " << profile.idle_percent << "%\n";

    uint64_t failures = 0;
    Stats last;
    for (const auto& [name, config] : configs) {
        if (config_name != "all" && config_name != name) {
            continue;
        }
        last = run_batch(profile, config);
        print_summary(name.c_str(), last);
        failures += last.failures;
    }
    if (config_name != "all") {
        print_details(last);
    }
    return failures == 0 ? 0 : 1;
}
