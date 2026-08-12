// Unit tests for quest::Progress flag handling and questState transitions —
// pure logic, no save file or PDS involved (see AGENTS.md's module boundary
// rules: quest/ never includes anything from sync/).

#include <iostream>

#include "quest/quest.hpp"

namespace {

int g_failures = 0;

#define CHECK(expr)                                                            \
    do {                                                                       \
        if (!(expr)) {                                                         \
            std::cerr << "FAILED: " << #expr                                   \
                      << " at quest_test.cpp:" << __LINE__ << "\n";            \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

} // namespace

int main() {
    using namespace keepsake::quest;

    // hasFlag/setFlag
    {
        Progress p;
        CHECK(!hasFlag(p, "quest.keep_cleared.started"));
        setFlag(p, "quest.keep_cleared.started");
        CHECK(hasFlag(p, "quest.keep_cleared.started"));
        CHECK(p.flags.size() == 1);

        // setFlag is a no-op if already set — must not duplicate.
        setFlag(p, "quest.keep_cleared.started");
        CHECK(p.flags.size() == 1);
    }

    // questState transitions
    {
        CHECK(!allQuests().empty());
        const QuestDef &quest = allQuests().front();

        Progress p;
        CHECK(questState(p, quest) == "not started");

        setFlag(p, quest.startedFlag);
        CHECK(questState(p, quest) == "in progress");

        setFlag(p, quest.completeFlag);
        CHECK(questState(p, quest) == "complete");
    }

    // A complete flag alone (without the started flag) still reads as
    // complete — completeFlag takes priority in questState's check order.
    {
        const QuestDef &quest = allQuests().front();
        Progress p;
        setFlag(p, quest.completeFlag);
        CHECK(questState(p, quest) == "complete");
    }

    // onEnemyDefeated: only the hollow_knight completes keep_cleared.
    {
        Progress p;
        onEnemyDefeated(p, "hollow_knight");
        CHECK(hasFlag(p, "quest.keep_cleared.complete"));
    }
    {
        Progress p;
        onEnemyDefeated(p, "some_other_enemy");
        CHECK(!hasFlag(p, "quest.keep_cleared.complete"));
        CHECK(p.flags.empty());
    }

    std::cout << "quest_test " << (g_failures ? "FAILED" : "OK") << "\n";
    return g_failures ? 1 : 0;
}
