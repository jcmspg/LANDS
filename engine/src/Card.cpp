#include "lands/Card.hpp"

namespace lands {

Card::Card(LandType land_type, uint32_t identifier) : land_type_(land_type), identifier_(identifier) {}

LandType Card::get_land_type() const {
    return land_type_;
}

uint32_t Card::get_identifier() const {
    return identifier_;
}

bool Card::operator==(const Card& other) const {
    return land_type_ == other.land_type_ && identifier_ == other.identifier_;
}


} // namespace lands

