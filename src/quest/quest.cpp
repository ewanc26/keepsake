#include "quest/quest.hpp"

#include <algorithm>

namespace keepsake::quest {

bool hasFlag(const Progress &progress, const std::string &flag) {
    return std::find(progress.flags.begin(), progress.flags.end(), flag) !=
           progress.flags.end();
}

void setFlag(Progress &progress, const std::string &flag) {
    if (!hasFlag(progress, flag)) progress.flags.push_back(flag);
}

const std::vector<QuestDef> &allQuests() {
    static const std::vector<QuestDef> quests = {
        {"keep_cleared", "The Weight Below",
         "The Watcher's keepsake was taken by whatever now nests in the "
         "undercroft. Go and take it back.",
         "quest.keep_cleared.started", "quest.keep_cleared.complete"},
        {"the_keepsake", "What the Deep Kept",
         "The undercroft was only the antechamber. Whatever took the "
         "Watcher's keepsake carried it further down still — into the "
         "crypt below, and whatever the Rot Warden guards there.",
         "quest.the_keepsake.started", "quest.the_keepsake.complete"},
        {"the_nameless", "The Last Door",
         "Beneath the vault the Rot Warden kept sealed, a stair goes down "
         "further still, to whatever the Rot Warden itself was guarding "
         "against.",
         "quest.the_nameless.started", "quest.the_nameless.complete"},
        // Side quest, started only through the Watcher's "tower"
        // dialogue node (see dialogue.cpp) and completed by defeating the
        // belfry crow through ordinary combat. Never gates the main line.
        {"the_bell", "The Quiet Bell",
         "The Watcher asks you to bring the keep's voice back: something "
         "large has folded itself over the watchtower bell, and it won't "
         "be argued off.",
         "quest.the_bell.started", "quest.the_bell.complete"},
    };
    return quests;
}

std::string questState(const Progress &progress, const QuestDef &quest) {
    if (hasFlag(progress, quest.completeFlag)) return "complete";
    if (hasFlag(progress, quest.startedFlag)) return "in progress";
    return "not started";
}

void onEnemyDefeated(Progress &progress, const std::string &enemyId) {
    if (enemyId == "hollow_knight") {
        setFlag(progress, "quest.keep_cleared.complete");
    } else if (enemyId == "rot_warden") {
        setFlag(progress, "quest.the_keepsake.complete");
    } else if (enemyId == "nameless_thing") {
        // Combat is only one of two ways this quest resolves — see the
        // "nameless_thing" dialogue tree for the peaceful branch, which
        // sets quest.the_nameless.complete directly instead.
        setFlag(progress, "quest.the_nameless.complete");
    } else if (enemyId == "carrion_crow") {
        // Side quest the_bell — combat only; there is nothing to talk
        // down in the belfry.
        setFlag(progress, "quest.the_bell.complete");
    }
}

} // namespace keepsake::quest
