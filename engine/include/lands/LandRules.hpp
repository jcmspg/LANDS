#ifndef LANDS_LAND_RULES_HPP
#define LANDS_LAND_RULES_HPP

#include "lands/LandType.hpp"

namespace lands {

// Error policy (public API):
// - Validate inputs; contract/programmer violations throw
//   std::out_of_range or std::invalid_argument (not bare assert).
// - Bool-returning actions (play_land, end_turn) return false for illegal
//   game moves — those are not exceptions.

const LandDefinition& get(LandType type);

} // namespace lands

#endif // LANDS_LAND_RULES_HPP
