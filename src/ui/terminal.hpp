#ifndef KEEPSAKE_UI_TERMINAL_HPP
#define KEEPSAKE_UI_TERMINAL_HPP

#include <istream>
#include <ostream>

#include "entity/character.hpp"
#include "quest/quest.hpp"
#include "sync/record_store.hpp"
#include "world/world.hpp"

namespace keepsake::ui {

// Runs the interactive command loop until the player quits, is defeated,
// or the input stream ends. This is the only place that owns `store` —
// nothing else in the game reads or writes a save.
void run(world::World &world, entity::Character &player,
         quest::Progress &progress, sync::RecordStore &store,
         std::istream &in, std::ostream &out);

} // namespace keepsake::ui

#endif
