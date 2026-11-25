#ifndef LANDS_RANDOM_BOT_HPP
#define LANDS_RANDOM_BOT_HPP

#include "lands/LandGameMatch.hpp"

#include <cstdint>
#include <random>

namespace lands {

// A mock player for stress tests and the simulation harness (GDD §10, Phase 1): it plays
// uniformly random legal actions for whoever is to move, always making the mandatory deploy
// when one is possible, and now and then lets that player's clock run out instead. A stalling
// profile (idle_percent) skips actions so games reach the deck-out and turn-cap endings.
class RandomBot {
    public:
        explicit RandomBot(uint64_t seed, int timer_percent = 0, int idle_percent = 0);

        // Performs one action (or timer event) for the player who is to move.
        // Returns false, doing nothing, once the match is over.
        bool step(LandGameMatch& match);

    private:
        std::mt19937_64 rng_;
        int timer_percent_;     // chance (0-100) that the mover's clock runs out instead of acting
        int idle_percent_;      // chance (0-100) that the active player just ends the turn: long, stalling games
};

} // namespace lands

#endif // LANDS_RANDOM_BOT_HPP
