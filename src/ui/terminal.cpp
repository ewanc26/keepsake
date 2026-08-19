#include "ui/terminal.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

#include "combat/combat.hpp"
#include "dialogue/dialogue.hpp"
#include "save/save.hpp"

namespace keepsake::ui {

namespace {

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
}

std::string trim(const std::string &s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// A command is its first whitespace-delimited word; the rest of the line,
// re-joined and lowercased, is the argument multi-word item/NPC names
// match against.
struct Command {
    std::string verb;
    std::string arg;
};

Command parseCommand(const std::string &line) {
    std::istringstream stream(line);
    std::string verb;
    stream >> verb;
    std::string rest;
    std::getline(stream, rest);
    return Command{toLower(verb), trim(toLower(rest))};
}

const entity::ItemDef *matchItemAmong(const std::string &query,
                                      const std::vector<std::string> &ids) {
    for (const auto &id : ids) {
        const auto *def = entity::findItemDef(id);
        if (def == nullptr) continue;
        if (query == id || query == def->name) return def;
    }
    return nullptr;
}

void printHelp(std::ostream &out) {
    out << "Commands: look, go <direction>, take <item>, use <item>, "
           "talk <npc>, attack, inventory, stats, quests, save, quit\n";
}

void printLocation(const world::Location &loc, std::ostream &out) {
    out << "\n" << loc.name << "\n" << loc.description << "\n";

    if (!loc.itemIds.empty()) {
        out << "You see: ";
        for (size_t i = 0; i < loc.itemIds.size(); ++i) {
            const auto *def = entity::findItemDef(loc.itemIds[i]);
            out << (def != nullptr ? def->name : loc.itemIds[i]);
            if (i + 1 < loc.itemIds.size()) out << ", ";
        }
        out << ".\n";
    }
    if (loc.npcId) {
        if (const auto *npc = dialogue::findNpcDef(*loc.npcId)) {
            out << npc->name << " is here.\n";
        }
    }
    if (loc.enemyId) {
        if (const auto *enemy = combat::findEnemyDef(*loc.enemyId)) {
            out << enemy->name << " blocks the way.\n";
        }
    }
    if (!loc.flavorNpcNames.empty()) {
        out << "You notice familiar faces here: ";
        for (size_t i = 0; i < loc.flavorNpcNames.size(); ++i) {
            out << loc.flavorNpcNames[i];
            if (i + 1 < loc.flavorNpcNames.size()) out << ", ";
        }
        out << ".\n";
    }
    if (!loc.exits.empty()) {
        out << "Exits: ";
        for (size_t i = 0; i < loc.exits.size(); ++i) {
            out << loc.exits[i].direction;
            if (i + 1 < loc.exits.size()) out << ", ";
        }
        out << ".\n";
    }
}

bool doSave(entity::Character &player, quest::Progress &progress,
            sync::RecordStore &store) {
    return store.save(save::SaveData{player, progress});
}

// Every quest broadcasts under its id/name on the not-complete -> complete
// transition, regardless of whether that transition happened through
// combat (combat::run + quest::onEnemyDefeated) or a dialogue branch that
// sets a quest's completeFlag directly (see "nameless_thing" in
// dialogue.cpp) — so both `attack` and `talk` snapshot flags before acting
// and hand the snapshot here afterward. `locationId` and `cause` describe
// what actually happened, for the event line.
using QuestFlagSnapshot = std::vector<std::pair<std::string, bool>>;

QuestFlagSnapshot snapshotQuestFlags(const quest::Progress &progress) {
    QuestFlagSnapshot snapshot;
    for (const auto &q : quest::allQuests()) {
        snapshot.emplace_back(q.id, quest::hasFlag(progress, q.completeFlag));
    }
    return snapshot;
}

void broadcastNewlyCompleted(const QuestFlagSnapshot &before,
                             const quest::Progress &progress,
                             sync::RecordStore &store,
                             const std::string &locationId,
                             const std::string &cause) {
    for (const auto &q : quest::allQuests()) {
        bool wasComplete = false;
        for (const auto &entry : before) {
            if (entry.first == q.id) wasComplete = entry.second;
        }
        // Broadcasts nothing when signed out — RecordStore's default
        // implementation of these is a no-op.
        if (!wasComplete && quest::hasFlag(progress, q.completeFlag)) {
            store.recordAchievement(q.id, q.name);
            store.recordEvent("questCompleted", locationId, cause);
        }
    }
}

} // namespace

void run(world::World &world, entity::Character &player,
         quest::Progress &progress, sync::RecordStore &store, std::istream &in,
         std::ostream &out, RemoteEventPoll pollRemoteEvents) {
    world::Location *current = world.find(progress.location);
    if (current == nullptr) {
        // A corrupt or hand-edited save pointed at a location that no
        // longer exists; recover to the start rather than crash.
        progress.location = "gatehouse";
        current = world.find(progress.location);
    }

    out << "== Keepsake ==\n";
    printLocation(*current, out);

    std::string line;
    while (true) {
        if (pollRemoteEvents) {
            for (const auto &eventLine : pollRemoteEvents()) {
                out << "\n(Elsewhere) " << eventLine << "\n";
            }
        }
        out << "\n> ";
        if (!std::getline(in, line)) {
            out << "\n";
            doSave(player, progress, store);
            break;
        }

        Command cmd = parseCommand(line);
        if (cmd.verb.empty()) continue;

        if (cmd.verb == "help") {
            printHelp(out);
        } else if (cmd.verb == "look" || cmd.verb == "l") {
            printLocation(*current, out);
        } else if (cmd.verb == "go") {
            if (cmd.arg.empty()) {
                out << "Go where?\n";
                continue;
            }
            const world::Exit *match = nullptr;
            for (const auto &exit : current->exits) {
                if (toLower(exit.direction) == cmd.arg) {
                    match = &exit;
                    break;
                }
            }
            if (match == nullptr) {
                out << "You can't go that way.\n";
                continue;
            }
            world::Location *next = world.find(match->targetId);
            if (next == nullptr) {
                out << "That way is blocked.\n";
                continue;
            }
            progress.location = next->id;
            current = next;
            printLocation(*current, out);
        } else if (cmd.verb == "take") {
            if (cmd.arg.empty()) {
                out << "Take what?\n";
                continue;
            }
            const auto *def = matchItemAmong(cmd.arg, current->itemIds);
            if (def == nullptr) {
                out << "There's nothing like that here.\n";
                continue;
            }
            player.addItem(def->id);
            current->itemIds.erase(std::remove(current->itemIds.begin(),
                                               current->itemIds.end(), def->id),
                                   current->itemIds.end());
            // Persisted so World::reconcile() knows not to respawn this on
            // the next load — see world.cpp.
            quest::setFlag(progress,
                           "item.taken." + current->id + "." + def->id);
            out << "You take the " << def->name << ".\n";
        } else if (cmd.verb == "use") {
            if (cmd.arg.empty()) {
                out << "Use what?\n";
                continue;
            }
            std::vector<std::string> owned;
            for (const auto &stack : player.inventory)
                owned.push_back(stack.itemId);
            const auto *def = matchItemAmong(cmd.arg, owned);
            if (def == nullptr) {
                out << "You don't have that.\n";
                continue;
            }
            if (def->consumable) {
                player.hp = std::min(player.maxHp, player.hp + def->healAmount);
                player.removeItem(def->id, 1);
                out << "You drink the " << def->name << ". (" << player.hp
                    << "/" << player.maxHp << " HP)\n";
            } else if (def->attackBonus > 0 || def->defenseBonus > 0) {
                // A permanent upgrade — one or both stats, applied together
                // so an item like a ring can grant both at once.
                player.attack += def->attackBonus;
                player.defense += def->defenseBonus;
                player.removeItem(def->id, 1);
                out << "The " << def->name << " settles into place.";
                if (def->attackBonus > 0) out << " Attack +" << def->attackBonus << ".";
                if (def->defenseBonus > 0) out << " Defense +" << def->defenseBonus << ".";
                out << "\n";
            } else {
                out << "Nothing happens, but it feels important to keep.\n";
            }
        } else if (cmd.verb == "talk") {
            if (cmd.arg.empty()) {
                out << "Talk to whom?\n";
                continue;
            }
            if (!current->npcId || cmd.arg != *current->npcId) {
                out << "There's no one like that here.\n";
                continue;
            }
            const auto *npc = dialogue::findNpcDef(*current->npcId);
            if (npc != nullptr) {
                QuestFlagSnapshot before = snapshotQuestFlags(progress);
                dialogue::run(*npc, player, progress, in, out);
                // A dialogue branch may have resolved a quest on its own
                // (see "nameless_thing"'s peaceful branch) — reconcile so
                // any world change that unlocks (or, here, an enemy/NPC
                // that should now stand down) takes effect immediately,
                // same as the post-combat path below.
                world.reconcile(progress);
                broadcastNewlyCompleted(before, progress, store, current->id,
                                        npc->name + " let you pass.");
            }
        } else if (cmd.verb == "attack") {
            if (!current->enemyId) {
                out << "There's nothing to fight here.\n";
                continue;
            }
            const auto *enemy = combat::findEnemyDef(*current->enemyId);
            if (enemy == nullptr) {
                out << "There's nothing to fight here.\n";
                continue;
            }
            combat::Result result = combat::run(player, *enemy, out);
            if (result == combat::Result::Victory) {
                QuestFlagSnapshot before = snapshotQuestFlags(progress);
                quest::onEnemyDefeated(progress, enemy->id);
                current->enemyId.reset();
                // Re-run so a quest completing this kill (e.g. the crypt
                // stair opening once keep_cleared completes) takes effect
                // immediately rather than only on the next load —
                // world::World::reconcile() is safe to call repeatedly.
                world.reconcile(progress);
                broadcastNewlyCompleted(before, progress, store, current->id,
                                        enemy->name + " has fallen.");
            } else {
                out << "\nYour last save is untouched — reload to try "
                       "again.\n";
                return;
            }
        } else if (cmd.verb == "inventory" || cmd.verb == "i") {
            if (player.inventory.empty()) {
                out << "You aren't carrying anything.\n";
            } else {
                for (const auto &stack : player.inventory) {
                    const auto *def = entity::findItemDef(stack.itemId);
                    out << "  " << (def != nullptr ? def->name : stack.itemId)
                        << " x" << stack.count << "\n";
                }
            }
        } else if (cmd.verb == "stats") {
            out << player.className << ", level " << player.level << "\n"
                << "HP " << player.hp << "/" << player.maxHp << "  "
                << "Attack " << player.attack << "  "
                << "Defense " << player.defense << "  "
                << "XP " << player.xp << "/" << (player.level * 20) << "\n";
        } else if (cmd.verb == "quests") {
            for (const auto &q : quest::allQuests()) {
                out << q.name << " — " << quest::questState(progress, q)
                    << "\n  " << q.description << "\n";
            }
        } else if (cmd.verb == "save") {
            out << (doSave(player, progress, store) ? "Saved.\n"
                                                    : "Save failed.\n");
        } else if (cmd.verb == "quit") {
            bool ok = doSave(player, progress, store);
            out << (ok ? "Saved. Farewell.\n"
                       : "Save failed, exiting anyway.\n");
            return;
        } else {
            out << "I don't understand that. Type 'help' for a list of "
                   "commands.\n";
        }
    }
}

} // namespace keepsake::ui
