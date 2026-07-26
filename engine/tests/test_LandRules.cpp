#include "lands/LandRules.hpp"

#include <gtest/gtest.h>
#include <stdexcept>

TEST(LandRules, GetLandDefinition) {
    EXPECT_EQ(lands::get(lands::LandType::PLAINS).deploy_ability, lands::AbilityId::FLICKER);
    EXPECT_EQ(lands::get(lands::LandType::PLAINS).pitch_ability, lands::AbilityId::SAVE);
    EXPECT_EQ(lands::get(lands::LandType::PLAINS).pitch_timing, lands::PitchTiming::Instant);
}

TEST(LandRules, GetInvalidLandTypeThrows) {
    constexpr auto invalid =
        static_cast<lands::LandType>(static_cast<uint8_t>(lands::LandType::NUM_LAND_TYPES));
    EXPECT_THROW(lands::get(invalid), std::out_of_range);
}
