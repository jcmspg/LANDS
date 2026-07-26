#ifndef LANDS_PLAY_MODE_HPP
#define LANDS_PLAY_MODE_HPP

#include <cstdint>

namespace lands {

enum class PlayMode : uint8_t {
    DEPLOY,
    PITCH,
};

enum class PitchTiming : uint8_t {
    MainPhase,  // Mountain, Swamp, Forest
    Instant,    // Island, Plains
};

} // namespace lands

#endif // LANDS_PLAY_MODE_HPP
