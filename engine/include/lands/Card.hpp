#ifndef LANDS_CARD_HPP
#define LANDS_CARD_HPP

#include "lands/LandType.hpp"

#include <cstdint>

namespace lands {

class Card {
    public:
        Card(LandType land_type, uint32_t identifier);
        ~Card() = default;

        LandType get_land_type() const;
        uint32_t get_identifier() const;

    private:
        LandType land_type_;
        uint32_t identifier_;
};

} // namespace lands

#endif // LANDS_CARD_HPP