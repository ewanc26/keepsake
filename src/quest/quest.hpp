#ifndef KEEPSAKE_QUEST_QUEST_HPP
#define KEEPSAKE_QUEST_QUEST_HPP

#include <string>
#include <vector>

namespace keepsake::quest {

// Everything about a save that isn't the character sheet: where the player
// is, and which quest flags have been set. Maps to the future
// click.croft.rpg.progress record.
struct Progress {
    std::string location = "gatehouse";
    std::vector<std::string> flags;
};

bool hasFlag(const Progress &progress, const std::string &flag);
// No-op if the flag is already set.
void setFlag(Progress &progress, const std::string &flag);

struct QuestDef {
    std::string id;
    std::string name;
    std::string description;
    std::string startedFlag;
    std::string completeFlag;
};

const std::vector<QuestDef> &allQuests();

// One of "not started", "in progress", "complete".
std::string questState(const Progress &progress, const QuestDef &quest);

// Content-specific hook: some enemies are quest objectives. Called after a
// combat victory with the defeated enemy's id.
void onEnemyDefeated(Progress &progress, const std::string &enemyId);

} // namespace keepsake::quest

#endif
