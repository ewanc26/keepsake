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

    // "nameless_thing" peaceful branch: "Why are you here?" -> "Let me
    // stand in your place." resolves quest.the_nameless.complete directly
    // (not via quest::onEnemyDefeated) and hands over the ring as a gift.
    {
        const NpcDef *nameless = findNpcDef("nameless_thing");
        CHECK(nameless != nullptr);

        Character player;
        Progress progress;
        std::istringstream in("1\n1\n1\n");
        std::ostringstream out;

        run(*nameless, player, progress, in, out);

        CHECK(keepsake::quest::hasFlag(progress, "quest.the_nameless.complete"));
        CHECK(player.itemCount("ring_of_the_watcher") == 1);
    }

    // The "goad" branch leaves the quest unresolved — the encounter falls
    // through to `attack` instead, same as declining to talk at all.
    {
        const NpcDef *nameless = findNpcDef("nameless_thing");
        Character player;
        Progress progress;
        std::istringstream in("1\n2\n1\n");
        std::ostringstream out;

        run(*nameless, player, progress, in, out);

        CHECK(
            !keepsake::quest::hasFlag(progress, "quest.the_nameless.complete"));
        CHECK(player.inventory.empty());
    }

    // The watchtower branch (side quest the_bell): option 4 at "start"
    // reaches the "tower" node; "I'll see about the bell." sets the
    // quest-started flag, "Leave it to the birds." leaves it unset —
    // and either way no item is granted and keep_cleared stays untouched.
    {
        const NpcDef *watcher = findNpcDef("watcher");
        CHECK(watcher->tree.find("tower") != nullptr);

        Character player;
        Progress progress;
        std::istringstream in("4\n1\n");
        std::ostringstream out;
        run(*watcher, player, progress, in, out);

        CHECK(keepsake::quest::hasFlag(progress, "quest.the_bell.started"));
        CHECK(!keepsake::quest::hasFlag(progress, "quest.keep_cleared.started"));
        CHECK(player.inventory.empty());
        CHECK(out.str().find("barn door") != std::string::npos);

        // Declining the ask leaves the quest untouched.
        Character player2;
        Progress progress2;
        std::istringstream in2("4\n2\n");
        std::ostringstream out2;
        run(*watcher, player2, progress2, in2, out2);

        CHECK(!keepsake::quest::hasFlag(progress2, "quest.the_bell.started"));
    }

    // The abyss acknowledges a taught locket: the gated option (the
    // requireFlag field, exercised here against real content) only
    // appears while the flag is set, and it leads to the thing reading
    // the hollow and standing down the peaceful way.
    //
    // Unlocked gate: an untaught locket hides the option, and the
    // numbering stays dense — no gap where the option would have been.
    {
        const NpcDef *thing = findNpcDef("nameless_thing");
        CHECK(thing != nullptr);
        Progress plain;
        Character player;
        std::istringstream in("1\n2\n");
        std::ostringstream out;
        run(*thing, player, plain, in, out);
        CHECK(out.str().find("Hold up the locket.") == std::string::npos);
        CHECK(out.str().find("cannot wear") == std::string::npos);
    }

    // Taught gate: the option appears, numbering stays dense, and the
    // locket's witness resolves the last quest peacefully.
    {
        const NpcDef *thing = findNpcDef("nameless_thing");
        Progress taught;
        keepsake::quest::setFlag(taught, "locket.taught");
        Character player;
        std::istringstream in("3\n1\n1\n");
        std::ostringstream out;
        run(*thing, player, taught, in, out);

        CHECK(out.str().find("Hold up the locket.") != std::string::npos);
        CHECK(out.str().find("the one thing down here I cannot wear") !=
              std::string::npos);
        CHECK(keepsake::quest::hasFlag(taught, "quest.the_nameless.complete"));
        CHECK(player.itemCount("ring_of_the_watcher") == 1);
    }

    std::cout << "dialogue_test " << (g_failures ? "FAILED" : "OK") << "\n";
    return g_failures ? 1 : 0;
}
