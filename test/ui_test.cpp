// Unit tests for ui::run — the terminal command loop, the largest
// previously-untested seam: every in-game verb (go/take/use/talk/attack/
// quests/save/quit) plus the corrupt-save location recovery and the
// RemoteEventPoll "(Elsewhere)" printing, driven end to end with scripted
// input through the real World/Character/Progress content. The RecordStore
// is a test stub recording calls, so no save file or PDS is touched;
// ui/terminal.cpp only needs `src/` and never includes wolfram (see
// AGENTS.md's module boundary rules), so this needs no network either.

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "quest/quest.hpp"
#include "ui/terminal.hpp"
#include "world/world.hpp"

namespace {

int g_failures = 0;

#define CHECK(expr)                                                            \
    do {                                                                       \
        if (!(expr)) {                                                         \
            std::cerr << "FAILED: " << #expr                                   \
                      << " at ui_test.cpp:" << __LINE__ << "\n";               \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

// Records what the loop did to the store: how many saves, the last saved
// snapshot, and every achievement/event broadcast handed to it.
class RecordingStore : public keepsake::sync::RecordStore {
  public:
    bool load(keepsake::save::SaveData &) override { return false; }
    bool save(const keepsake::save::SaveData &data) override {
        lastSaved = data;
        ++saveCount;
        return true;
    }
    void recordAchievement(const std::string &id,
                           const std::string &) override {
        achievements.push_back(id);
    }
    void recordEvent(const std::string &kind, const std::string &locationId,
                     const std::string &) override {
        events.push_back(kind + "@" + locationId);
    }

    int saveCount = 0;
    keepsake::save::SaveData lastSaved;
    std::vector<std::string> achievements;
    std::vector<std::string> events;
};

} // namespace

int main() {
    using keepsake::entity::Character;
    using keepsake::quest::Progress;

    // A save pointing at a location that doesn't exist recovers to the
    // gatehouse instead of crashing — and the recovery is what gets
    // saved back.
    {
        keepsake::world::World world = keepsake::world::World::createDefault();
        Character player;
        Progress progress;
        progress.location = "no_such_room";
        RecordingStore store;

        std::istringstream in("quit\n");
        std::ostringstream out;
        keepsake::ui::run(world, player, progress, store, in, out);

        CHECK(progress.location == "gatehouse");
        CHECK(store.saveCount == 1);
        CHECK(out.str().find("the Gatehouse") != std::string::npos);
    }

    // Walking and looking: the armory run — go north, go east, look —
    // ends at the armory with the sword visible to take.
    {
        keepsake::world::World world = keepsake::world::World::createDefault();
        Character player;
        Progress progress;
        RecordingStore store;

        std::istringstream in("go north\ngo east\nlook\nquit\n");
        std::ostringstream out;
        keepsake::ui::run(world, player, progress, store, in, out);

        CHECK(progress.location == "armory");
        CHECK(out.str().find("the Armory") != std::string::npos);
        CHECK(out.str().find("You can't go that way.") == std::string::npos);
    }

    // take + use: pick up the iron sword in the armory and apply it —
    // a permanent +3 attack through the one-shot upgrade path.
    {
        keepsake::world::World world = keepsake::world::World::createDefault();
        Character player;
        Progress progress;
        RecordingStore store;

        std::istringstream in(
            "go north\ngo east\ntake iron sword\nuse iron sword\nstats\n"
            "quit\n");
        std::ostringstream out;
        keepsake::ui::run(world, player, progress, store, in, out);

        CHECK(player.attack == 7); // 4 base + 3 sword
        CHECK(player.itemCount("iron_sword") == 0);
        // The take persisted a flag so reconcile() won't respawn it.
        CHECK(keepsake::quest::hasFlag(progress, "item.taken.armory.iron_sword"));
        CHECK(out.str().find("Attack +3.") != std::string::npos);

        // And it really doesn't respawn: a fresh world reconciled against
        // the same progress has no sword in the armory.
        keepsake::world::World reloaded =
            keepsake::world::World::createDefault();
        reloaded.reconcile(progress);
        CHECK(reloaded.find("armory")->itemIds.empty());
    }

    // talk: the Watcher's watchtower branch starts the_bell through plain
    // dialogue (option 4, then "I'll see about the bell."), and the
    // quests command lists it in progress.
    {
        keepsake::world::World world = keepsake::world::World::createDefault();
        Character player;
        Progress progress;
        RecordingStore store;

        std::istringstream in("go north\ntalk watcher\n4\n1\nquests\nquit\n");
        std::ostringstream out;
        keepsake::ui::run(world, player, progress, store, in, out);

        CHECK(keepsake::quest::hasFlag(progress, "quest.the_bell.started"));
        CHECK(out.str().find("The Quiet Bell — in progress") !=
              std::string::npos);
        // Dialogue alone broadcasts nothing — no quest completed here.
        CHECK(store.achievements.empty());
    }

    // attack + broadcast: a one-hit kill of the belfry crow (attack far
    // above its HP) completes the_bell, reconciles the belfry — the crow
    // is gone from the live world — and broadcasts the achievement and
    // event to the store, exactly the records a signed-in PDS write and
    // firehose other players would see.
    {
        keepsake::world::World world = keepsake::world::World::createDefault();
        Character player;
        player.attack = 50; // one strike kills before the crow can counter
        Progress progress;
        progress.location = "watchtower_top";
        RecordingStore store;

        std::istringstream in("attack\nlook\nquit\n");
        std::ostringstream out;
        keepsake::ui::run(world, player, progress, store, in, out);

        CHECK(keepsake::quest::hasFlag(progress, "quest.the_bell.complete"));
        CHECK(world.find("watchtower_top")->enemyId == std::nullopt);
        CHECK(out.str().find("a carrion crow falls") != std::string::npos);
        CHECK(store.achievements.size() == 1);
        CHECK(store.achievements[0] == "the_bell");
        CHECK(store.events.size() == 1);
        CHECK(store.events[0] == "questCompleted@watchtower_top");

        // The loot was a real pickup, and a re-reconciled world keeps the
        // belfry peaceful.
        CHECK(player.itemCount("brass_spyglass") == 1);
        Progress sameProgress = progress;
        world.reconcile(sameProgress);
        CHECK(world.find("watchtower_top")->enemyId == std::nullopt);
    }

    // Remote event poll: drained events print as "(Elsewhere)" ambient
    // lines between turns, display-only — nothing the poll returns ever
    // needs to touch World/Progress (see terminal.hpp).
    {
        keepsake::world::World world = keepsake::world::World::createDefault();
        Character player;
        Progress progress;
        RecordingStore store;

        // One queued line for the first turn, then nothing.
        int turns = 0;
        keepsake::ui::RemoteEventPoll poll = [&turns]() {
            std::vector<std::string> lines;
            if (turns++ == 0) {
                lines.push_back(
                    "[did:plc:nobody] enemyDefeated at abyss: some other "
                    "player's news.");
            }
            return lines;
        };

        std::istringstream in("look\nquit\n");
        std::ostringstream out;
        keepsake::ui::run(world, player, progress, store, in, out, poll);

        std::string text = out.str();
        CHECK(text.find("(Elsewhere) [did:plc:nobody] enemyDefeated at "
                        "abyss: some other player's news.") !=
              std::string::npos);
        CHECK(world.find("courtyard")->npcId.has_value()); // untouched
    }

    // Unknown verbs fall through to a hint, not a crash — and the session
    // continues afterwards.
    {
        keepsake::world::World world = keepsake::world::World::createDefault();
        Character player;
        Progress progress;
        RecordingStore store;

        std::istringstream in("frobnicate\nquit\n");
        std::ostringstream out;
        keepsake::ui::run(world, player, progress, store, in, out);

        CHECK(out.str().find("I don't understand that.") != std::string::npos);
        CHECK(store.saveCount == 1); // quit still saved
    }

    // Reading a lore note: `use` prints the note's text, keeps the note
    // (re-readable any number of times), and neither consumes it nor
    // touches any stat. Seeded worlds surface notes through the
    // applyWorldSeed cache spots; here one is placed directly so the
    // loop's read path itself is deterministic.
    {
        keepsake::world::World world = keepsake::world::World::createDefault();
        Character player;
        Progress progress;
        progress.location = "chapel";
        if (keepsake::world::Location *chapelRoom = world.find("chapel")) {
            chapelRoom->itemIds.push_back("note.watch_log");
        }
        RecordingStore store;

        std::istringstream in("look\ntake watch log page\nuse watch log "
                             "page\nuse watch log page\ninventory\nquit\n");
        std::ostringstream out;
        keepsake::ui::run(world, player, progress, store, in, out);

        const std::string text = out.str();
        CHECK(text.find("You take the watch log page.") != std::string::npos);
        CHECK(text.find("You read the watch log page:") != std::string::npos);
        // The registry's actual note body reached the player.
        CHECK(text.find("undercroft door") != std::string::npos);
        // Twice-read, still held: nothing consumed, no stat moved.
        CHECK(player.itemCount("note.watch_log") == 1);
        CHECK(player.attack == 4 && player.defense == 1);
        CHECK(keepsake::quest::hasFlag(progress,
                                       "item.taken.chapel.note.watch_log"));
    }

    // The opening (the premise): the banner hooks the letter, the
    // gatehouse carries it, and the first-minute experience is reading it
    // — take and re-read work on turn one, while the world stays intact.
    {
        keepsake::world::World world = keepsake::world::World::createDefault();
        Character player;
        Progress progress;
        RecordingStore store;

        std::istringstream in("look\ntake open letter\nuse open letter\n"
                             "use open letter\nquit\n");
        std::ostringstream out;
        keepsake::ui::run(world, player, progress, store, in, out);

        std::string text = out.str();
        CHECK(text.find("You came because of the letter.") !=
              std::string::npos);
        CHECK(text.find("You take the open letter.") != std::string::npos);
        CHECK(text.find("You read the open letter:") != std::string::npos);
        // The letter's premise beats survive to the player verbatim.
        CHECK(text.find("remembers names") != std::string::npos);
        CHECK(text.find("begin politely") != std::string::npos);
        // Re-readable, never consumed, no stat moved.
        CHECK(player.itemCount("note.open_letter") == 1);
        CHECK(player.attack == 4 && player.defense == 1);
        CHECK(keepsake::quest::hasFlag(
            progress, "item.taken.gatehouse.note.open_letter"));
    }

    std::cout << "ui_test " << (g_failures ? "FAILED" : "OK") << "\n";
    return g_failures ? 1 : 0;
}
