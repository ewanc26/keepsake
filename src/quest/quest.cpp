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
    }
}

} // namespace keepsake::quest
