#include "dialogue/dialogue.hpp"

#include <algorithm>

#include "entity/character.hpp"

namespace keepsake::dialogue {

const Node *Tree::find(const std::string &id) const {
    auto it = std::find_if(nodes.begin(), nodes.end(),
                            [&](const Node &n) { return n.id == id; });
    return it == nodes.end() ? nullptr : &(*it);
}

namespace {

const std::vector<NpcDef> &npcRegistry() {
    static const std::vector<NpcDef> npcs = {
        {"watcher", "the Watcher",
         Tree{
             "start",
             {
                 {"start",
                  "An old man leans on a broken pike, watching the gate. "
                  "\"Another wanderer,\" he says. \"Come to pick through "
                  "the bones of the place, or come to help?\"",
                  {
                      {"What happened here?", "lore", "", ""},
                      {"I'll help, if I can.", "accept",
                       "quest.keep_cleared.started", "health_potion"},
                      {"Just passing through.", "", "", ""},
                  }},
                 {"lore",
                  "\"The keep held the Wardens once, and this—\" he lifts "
                  "a tarnished locket on a cord around his neck, empty "
                  "now. \"My mother's. Taken the night something came up "
                  "out of the undercroft and didn't leave. Been sitting "
                  "on that gate ever since, waiting for someone with a "
                  "sword and no better sense.\"",
                  {
                      {"I'll get it back.", "accept",
                       "quest.keep_cleared.started", "health_potion"},
                      {"Farewell, for now.", "", "", ""},
                  }},
                 {"accept",
                  "He presses a small clay vial into your hand. \"Careful "
                  "going down. Whatever's there now isn't just an "
                  "animal.\"",
                  {
                      {"I'll be careful.", "", "", ""},
                  }},
             },
         }},
    };
    return npcs;
}

} // namespace

const NpcDef *findNpcDef(const std::string &id) {
    const auto &npcs = npcRegistry();
    auto it = std::find_if(npcs.begin(), npcs.end(),
                            [&](const NpcDef &n) { return n.id == id; });
    return it == npcs.end() ? nullptr : &(*it);
}

namespace {

void applyOption(const Option &opt, entity::Character &player,
                  quest::Progress &progress, std::ostream &out,
                  const std::string &npcName) {
    bool flagAlreadySet = false;
    if (!opt.setFlag.empty()) {
        flagAlreadySet = quest::hasFlag(progress, opt.setFlag);
        quest::setFlag(progress, opt.setFlag);
    }
    // Item grants tied to a flag are one-time: only hand it over the first
    // time that flag gets set, so re-visiting the same dialogue branch
    // doesn't farm items.
    bool shouldGiveItem =
        !opt.giveItemId.empty() && (opt.setFlag.empty() || !flagAlreadySet);
    if (shouldGiveItem) {
        player.addItem(opt.giveItemId);
        if (const auto *def = entity::findItemDef(opt.giveItemId)) {
            out << "(" << npcName << " gives you a " << def->name << ".)\n";
        }
    }
}

} // namespace

void run(const NpcDef &npc, entity::Character &player,
         quest::Progress &progress, std::istream &in, std::ostream &out) {
    const Node *node = npc.tree.find(npc.tree.startNodeId);
    while (node != nullptr) {
        out << "\n" << node->text << "\n";
        for (size_t i = 0; i < node->options.size(); ++i) {
            out << "  " << (i + 1) << ") " << node->options[i].text << "\n";
        }

        int choice = -1;
        for (;;) {
            out << "> ";
            std::string line;
            if (!std::getline(in, line)) return;
            try {
                choice = std::stoi(line);
            } catch (...) {
                choice = -1;
            }
            if (choice >= 1 &&
                static_cast<size_t>(choice) <= node->options.size()) {
                break;
            }
            out << "That's not one of your options.\n";
        }

        const Option &chosen = node->options[static_cast<size_t>(choice) - 1];
        applyOption(chosen, player, progress, out, npc.name);
        if (chosen.nextNodeId.empty()) {
            node = nullptr;
        } else {
            node = npc.tree.find(chosen.nextNodeId);
        }
    }
}

} // namespace keepsake::dialogue
