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

bool ActionStack::empty() const {
    return items_.empty();
}

size_t ActionStack::size() const {
    return items_.size();
}

} // namespace lands