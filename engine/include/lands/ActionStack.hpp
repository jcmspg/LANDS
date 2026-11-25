#ifndef LANDS_ACTION_STACK_HPP
#define LANDS_ACTION_STACK_HPP

#include "lands/Card.hpp"
#include "lands/PlayMode.hpp"
#include "lands/AbilityId.hpp"

#include <algorithm>
#include <optional>
#include <vector>

namespace lands {

struct StackItem {
    uint32_t stack_id{0};
    int caster_id{-1};
    PlayMode mode{PlayMode::DEPLOY};
    Card card;
    AbilityId ability{AbilityId::NONE};
    uint32_t target_stack_id{0};    // 0 = no target; stack ids start at 1
    std::optional<uint32_t> target_card_id;  // a land (Destroy / Save) declared at push time
};

class ActionStack {
    public:
        void push(StackItem item);
        StackItem pop();                 // throw if empty
        const StackItem& top() const;    // throw if empty
        bool empty() const;
        bool contains(uint32_t stack_id) const;
        bool targets_card(uint32_t card_id) const;   // does any item target this card?
        size_t size() const;

    private:
        std::vector<StackItem> items_;
};

} // namespace lands

#endif // LANDS_ACTION_STACK_HPP