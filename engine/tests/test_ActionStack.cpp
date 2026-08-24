#include "lands/ActionStack.hpp"

#include <gtest/gtest.h>

namespace lands {

TEST(ActionStack, PushAndPop) {
    ActionStack stack;
    stack.push(StackItem{1, 1, PlayMode::DEPLOY, Card(LandType::FOREST, 1), AbilityId::NONE});
    EXPECT_EQ(stack.size(), 1);
    EXPECT_EQ(stack.top().stack_id, 1);
    EXPECT_EQ(stack.top().caster_id, 1);
    EXPECT_EQ(stack.top().mode, PlayMode::DEPLOY);
    EXPECT_EQ(stack.top().card.get_land_type(), LandType::FOREST);
    EXPECT_EQ(stack.top().card.get_identifier(), 1);
    EXPECT_EQ(stack.top().ability, AbilityId::NONE);
    StackItem item = stack.pop();
    EXPECT_EQ(item.stack_id, 1);
    EXPECT_EQ(item.caster_id, 1);
    EXPECT_EQ(item.mode, PlayMode::DEPLOY);
    EXPECT_EQ(item.card.get_land_type(), LandType::FOREST);
    EXPECT_EQ(item.card.get_identifier(), 1);
    EXPECT_EQ(item.ability, AbilityId::NONE);
    EXPECT_TRUE(stack.empty());
    EXPECT_EQ(stack.size(), 0);
    EXPECT_THROW(stack.top(), std::runtime_error);
    EXPECT_THROW(stack.pop(), std::runtime_error);
    
}

} // namespace lands