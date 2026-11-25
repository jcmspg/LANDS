#ifndef LANDS_LAND_GAME_MATCH_HPP
#define LANDS_LAND_GAME_MATCH_HPP

#include "lands/PlayerState.hpp"
#include "lands/WinEvaluator.hpp"
#include "lands/Graveyard.hpp"
#include "lands/ActionStack.hpp"
#include "lands/Choice.hpp"

#include <optional>
#include <vector>

namespace lands {

class LandGameMatch {
    public:
        explicit LandGameMatch(uint64_t seed);   // shuffle + deal

        // target_card_id is declared up front. Mountain's Destroy: an opponent land;
        // Forest's Reclaim: a card in the shared graveyard. It is required exactly
        // when a valid target exists, and rejected otherwise.
        bool play_land(int player_index, uint32_t card_id,
                       std::optional<uint32_t> target_card_id = std::nullopt);

        // Reactive Island Counter: pitch an Island plus one other card from hand to
        // counter any item on the stack. Only the player holding priority may cast it.
        bool cast_counter(int player_index, uint32_t island_card_id,
                          uint32_t extra_card_id, uint32_t target_stack_id);

        // Reactive Plains Save: pitch a Plains to make every stack item that targets
        // `land_card_id` (a land the caster controls) fizzle.
        bool cast_save(int player_index, uint32_t plains_card_id, uint32_t land_card_id);

        // Decline to respond; the whole stack then resolves top to bottom.
        bool pass_priority(int player_index);

        // Mid-resolution choices (Swamp Extract / Surveil). Resolution pauses until the
        // choosing player answers; every other action is rejected meanwhile.
        int choice_player() const;                          // -1 if no choice is pending
        ChoiceType choice_type() const;
        const std::vector<Card>& choice_options() const;    // private to choice_player()
        bool choose_extract(int player_index, uint32_t card_id);
        // to_graveyard + library_top_order must partition the options exactly;
        // library_top_order[0] becomes the new top card of the library.
        bool choose_surveil(int player_index, const std::vector<uint32_t>& to_graveyard,
                            const std::vector<uint32_t>& library_top_order);

        // Player who currently holds a priority window, or -1 if none is open.
        int priority_player() const;
        const StackItem& stack_top() const;     // throws if the stack is empty

        // Main-phase pitch (Mountain / Swamp / Forest), max 1 per turn.
        // Rummage needs extra_card_id (the card discarded as cost); Ramp must
        // not be given one. Illegal pitches return false and change nothing.
        bool pitch_land(int player_index, uint32_t card_id,
                        std::optional<uint32_t> extra_card_id = std::nullopt);

        WinCondition winner() const; // none until someone wins

        // getters for testing: deck size, hand sizes, land counts
        size_t deck_size() const;
        size_t hand_size(int player_index) const;
        size_t land_count(int player_index, LandType land_type) const;
        const std::vector<Card>& hand(int player_index) const;
        const std::vector<Card>& battlefield(int player_index) const;


        bool end_turn(int player_index);
        int active_player() const;
        int turn_number() const;
        bool played_land_this_turn() const;
        bool pitched_this_turn() const;

        size_t graveyard_size() const;

        size_t stack_size() const;

        private:
        Deck deck_;
        void deal_to(PlayerState& player, size_t num_cards);
        PlayerState players_[2];
        WinCondition winner_{WinCondition::NONE};

        // turn related content
        void begin_turn();

        int active_player_{0};
        int turn_number_{1};
        bool played_land_{false}; // false until a land is played
        bool pitched_this_turn_{false}; // false until a main-phase pitch resolves
        bool ramp_available_{false};    // set by a resolved Ramp, spent on the extra deploy
        Graveyard graveyard_;

        // priority window
        int priority_player_{-1};
        PendingChoice pending_choice_;
        bool action_blocked() const;   // a window or a choice is pending
        std::vector<uint32_t> countered_ids_;   // stack ids countered during this resolution
        std::vector<uint32_t> saved_card_ids_;  // lands saved during this resolution
        bool valid_deploy_target(int caster_index, AbilityId ability,
                                 const std::optional<uint32_t>& target_card_id) const;
        bool can_respond(int player_index) const;
        void open_priority_window(int pusher_index);
        void resolve_stack();

        ActionStack stack_;
        uint32_t next_stack_id_{1};
        void resolve_top_action();  // pop the top action and resolve it
};

} // namespace lands

#endif // LANDS_LAND_GAME_MATCH_HPP