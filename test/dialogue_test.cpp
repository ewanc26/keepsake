// Unit tests for dialogue::Tree traversal and dialogue::run's interactive
// loop, exercised against the real "watcher" NPC content in
// dialogue.cpp's npcRegistry() — dialogue/ has no external content format
// yet (see AGENTS.md), so this content *is* what run() actually plays.

#include <iostream>
#include <sstream>

#include "dialogue/dialogue.hpp"

namespace {

int g_failures = 0;

#define CHECK(expr)                                                            \
    do {                                                                       \
        if (!(expr)) {                                                         \
            std::cerr << "FAILED: " << #expr                                   \
                      << " at dialogue_test.cpp:" << __LINE__ << "\n";         \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

} // namespace

int main() {
    using namespace keepsake::dialogue;
    using keepsake::entity::Character;
    using keepsake::quest::Progress;

    // findNpcDef: known and unknown ids.
    {
        const NpcDef *watcher = findNpcDef("watcher");
        CHECK(watcher != nullptr);
        CHECK(findNpcDef("no_such_npc") == nullptr);
    }

    // Tree::find.
    {
        const NpcDef *watcher = findNpcDef("watcher");
        CHECK(watcher->tree.find("start") != nullptr);
        CHECK(watcher->tree.find("nonexistent_node") == nullptr);
    }

    // Direct accept path: option 2 at "start" sets the quest-started flag
    // and grants a health potion, then the "accept" node's only option
    // ends the dialogue (empty nextNodeId).
    {
        const NpcDef *watcher = findNpcDef("watcher");
        Character player;
        Progress progress;
        std::istringstream in("2\n1\n");
        std::ostringstream out;

        run(*watcher, player, progress, in, out);

        CHECK(keepsake::quest::hasFlag(progress, "quest.keep_cleared.started"));
        CHECK(player.itemCount("health_potion") == 1);
    }

    // Lore path then accept: option 1 ("What happened here?") goes to
    // "lore", then option 1 there also sets the flag and grants the item —
    // two different routes to the same outcome.
    {
        const NpcDef *watcher = findNpcDef("watcher");
        Character player;
        Progress progress;
        std::istringstream in("1\n1\n1\n");
        std::ostringstream out;

        run(*watcher, player, progress, in, out);

        CHECK(keepsake::quest::hasFlag(progress, "quest.keep_cleared.started"));
        CHECK(player.itemCount("health_potion") == 1);
        CHECK(out.str().find("Wardens") != std::string::npos);
    }

    // Declining ("Just passing through.") ends immediately with no flag and
    // no item.
    {
        const NpcDef *watcher = findNpcDef("watcher");
        Character player;
        Progress progress;
        std::istringstream in("3\n");
        std::ostringstream out;

        run(*watcher, player, progress, in, out);

        CHECK(
            !keepsake::quest::hasFlag(progress, "quest.keep_cleared.started"));
        CHECK(player.inventory.empty());
    }

    // The item grant is one-time per flag: reaching "accept" twice (two
    // separate run() calls sharing the same progress) only grants the
    // potion the first time.
    {
        const NpcDef *watcher = findNpcDef("watcher");
        Character player;
        Progress progress;

        std::istringstream in1("2\n1\n");
        std::ostringstream out1;
        run(*watcher, player, progress, in1, out1);
        CHECK(player.itemCount("health_potion") == 1);

        std::istringstream in2("2\n1\n");
        std::ostringstream out2;
        run(*watcher, player, progress, in2, out2);
        CHECK(player.itemCount("health_potion") == 1);
    }

    // Invalid input (out of range, non-numeric) is reprompted, not
    // crashed on, and doesn't advance the dialogue.
    {
        const NpcDef *watcher = findNpcDef("watcher");
        Character player;
        Progress progress;
        std::istringstream in("0\nabc\n99\n3\n");
        std::ostringstream out;

        run(*watcher, player, progress, in, out);

        CHECK(out.str().find("not one of your options") != std::string::npos);
    }

    // Input exhausted mid-dialogue (getline fails) returns cleanly instead
    // of looping forever.
    {
        const NpcDef *watcher = findNpcDef("watcher");
        Character player;
        Progress progress;
        std::istringstream in(""); // no input at all
        std::ostringstream out;

        run(*watcher, player, progress, in, out);
        CHECK(
            !keepsake::quest::hasFlag(progress, "quest.keep_cleared.started"));
    }

    std::cout << "dialogue_test " << (g_failures ? "FAILED" : "OK") << "\n";
    return g_failures ? 1 : 0;
}
