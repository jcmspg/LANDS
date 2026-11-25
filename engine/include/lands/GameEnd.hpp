#ifndef LANDS_GAME_END_HPP
#define LANDS_GAME_END_HPP

#include <cstdint>

namespace lands {

// Why a match ended (GDD §3.6, §8.10). A true draw is a TURN_CAP or DEPLOY_LOCK
// ending whose tiebreak ties, so it has no winner.
enum class GameEndReason : uint8_t {
    NONE,           // the match is still running
    DOMAIN,
    MONO,
    DEPLOY_LOCK,    // the active player had an empty hand when the deploy was required
    TURN_CAP,       // turn 50 (25 turns per player) completed
    CONCEDE,
    AFK_FORFEIT,            // the third AFK strike (GDD §3.6)
    DISCONNECT_FORFEIT,     // a blown reconnect window or exhausted grace budget (reported by the server)
};

} // namespace lands

#endif // LANDS_GAME_END_HPP
