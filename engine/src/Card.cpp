#include "lands/Card.hpp"

namespace lands {

Card::Card(LandType land_type, uint32_t identifier) : land_type_(land_type), identifier_(identifier) {}

LandType Card::get_land_type() const {
    return land_type_;
}

uint32_t Card::get_identifier() const {
    return identifier_;
}

} // namespace lands

