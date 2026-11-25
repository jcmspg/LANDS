#ifndef LANDS_MATCH_CONFIG_HPP
#define LANDS_MATCH_CONFIG_HPP

namespace lands {

// The first-player compensation package (GDD §3.1). The shipped ruleset compensates Player 2
// twice; the Phase 1 simulation compares the three packages below to choose the final one:
//   (a) draw-skip only: first_player_skips_first_draw, no sixth card
//   (b) sixth card only: second_player_gets_sixth_card, P1 draws on turn 1
//   (c) both (the default)
struct MatchConfig {
    bool first_player_skips_first_draw{true};   // P1 skips the draw step of turn 1
    bool second_player_gets_sixth_card{true};   // P2 opens with 6 cards instead of 5

    static MatchConfig draw_skip_only() { return MatchConfig{true, false}; }
    static MatchConfig sixth_card_only() { return MatchConfig{false, true}; }
    static MatchConfig both() { return MatchConfig{true, true}; }
};

} // namespace lands

#endif // LANDS_MATCH_CONFIG_HPP
