#ifndef KEEPSAKE_UI_TERMINAL_HPP
#define KEEPSAKE_UI_TERMINAL_HPP

#include <functional>
#include <istream>
#include <ostream>
#include <string>
#include <vector>

#include "entity/character.hpp"
#include "quest/quest.hpp"
#include "sync/record_store.hpp"
#include "world/world.hpp"

namespace keepsake::ui {

// Formatted lines describing whatever remote click.croft.rpg.event records
// have arrived since the last call — see sync::EventBridge::drain(). Kept
// as a plain function-returning-strings rather than a sync::EventBridge&
// parameter so this header (compiled unconditionally, unlike sync/
// firehose_watch.hpp) never has to depend on KEEPSAKE_WITH_WOLFRAM: main.cpp
// is the only caller that knows whether a bridge exists at all.
using RemoteEventPoll = std::function<std::vector<std::string>()>;

// Runs the interactive command loop until the player quits, is defeated,
// or the input stream ends. This is the only place that owns `store` —
// nothing else in the game reads or writes a save. `pollRemoteEvents`, if
// set, is called once per turn and its lines are printed as ambient
// "elsewhere" text — it never feeds back into `world`/`progress`, so a
// remote event can only ever change what's printed, never what's true.
void run(world::World &world, entity::Character &player,
         quest::Progress &progress, sync::RecordStore &store, std::istream &in,
         std::ostream &out, RemoteEventPoll pollRemoteEvents = nullptr);

} // namespace keepsake::ui

#endif
