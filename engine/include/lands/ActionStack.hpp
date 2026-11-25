#ifndef LANDS_ACTION_STACK_HPP
#define LANDS_ACTION_STACK_HPP

#include "lands/Card.hpp"
#include "lands/PlayMode.hpp"
#include "lands/AbilityId.hpp"

#include <algorithm>
#include <vector>

namespace lands {

struct StackItem {
    uint32_t stack_id{0};
    int caster_id{-1};
    PlayMode mode{PlayMode::DEPLOY};
    Card card;
    AbilityId ability{AbilityId::NONE};
    uint32_t target_stack_id{0};    // 0 = no target; stack ids start at 1
};

class ActionStack {
    public:
        void push(StackItem item);
        StackItem pop();                 // throw if empty
        const StackItem& top() const;    // throw if empty
        bool empty() const;
        bool contains(uint32_t stack_id) const;
        size_t size() const;

    private:
        std::vector<StackItem> items_;
};

} // namespace lands

#endif // LANDS_ACTION_STACK_HPP