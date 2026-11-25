#include "lands/ActionStack.hpp"

#include <stdexcept>
#include <utility>

namespace lands {

void ActionStack::push(StackItem item) {
    items_.push_back(item);
}

StackItem ActionStack::pop() {
    if (items_.empty()) {
        throw std::runtime_error("Action stack is empty");
    }
    StackItem item = std::move(items_.back());
    items_.pop_back();
    return item;
}

const StackItem& ActionStack::top() const {
    if (items_.empty()) {
        throw std::runtime_error("Action stack is empty");
    }
    return items_.back();
}

bool ActionStack::contains(uint32_t stack_id) const {
    return std::any_of(items_.begin(), items_.end(), [stack_id](const StackItem& item) {
        return item.stack_id == stack_id;
    });
}

bool ActionStack::targets_card(uint32_t card_id) const {
    return std::any_of(items_.begin(), items_.end(), [card_id](const StackItem& item) {
        return item.target_card_id.has_value() && *item.target_card_id == card_id;
    });
}

const std::vector<StackItem>& ActionStack::items() const {
    return items_;
}

bool ActionStack::empty() const {
    return items_.empty();
}

size_t ActionStack::size() const {
    return items_.size();
}

} // namespace lands