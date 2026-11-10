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
                  "An old man leans on a broken pike, watching the gate "
                  "rather than you, the way a man watches a door he has "
                  "no key for. \"Another one,\" he says. \"They all come "
                  "for the letter. Did you read yours, or just come "
                  "anyway?\"",
                  {
                      {"What happened here?", "lore", "", ""},
                      {"I'll help, if I can.", "accept",
                       "quest.keep_cleared.started", "health_potion"},
                      {"Just passing through.", "", "", ""},
                      {"Anything else you'd want looked at?", "tower", "",
                       ""},
                  }},
                 {"lore",
                  "\"The keep held the Wardens once — the ones the letter "
                  "means by the old keepers. This—\" he lifts a cord around "
                  "his neck with nothing on it. \"My mother's locket hung "
                  "here. It remembers names, that make. Whatever came up "
                  "out of the undercroft that night didn't take silver — "
                  "it took the only thing down here that could give it "
                  "back what it lost. I've been sitting on this gate ever "
                  "since, being a name it can't have instead. Sooner or "
                  "later it comes back for one of ours.\"",
                  {
                      {"I'll get it back.", "accept",
                       "quest.keep_cleared.started", "health_potion"},
                      {"Farewell, for now.", "", "", ""},
                  }},
                 {"accept",
                  "He presses a small clay vial into your hand. \"Careful "
                  "going down. Whatever's there isn't an animal — it's a "
                  "somebody who lost theirs, and it has been practicing "
                  "being us for a long time. Don't tell it your mark.\"",
                  {
                      {"I'll be careful.", "", "", ""},
                  }},
                 // Side quest "the_bell" (see quest.cpp): the Watcher can
                 // point you at the watchtower crow before you ever climb
                 // it. Purely optional — none of the three main quests
                 // route through it, and it never blocks the abyss.
                 {"tower",
                  "\"Bells,\" he says, and rubs his beard. \"The keep's "
                  "voice, once — storms, suppers, the ones who didn't come "
                  "home. Silent now. Something with wings like a barn "
                  "door folded itself over the clapper, and it won't be "
                  "argued off. Made off with half the tower's brass while "
                  "it was at it, from the look of the nest.\"",
                  {
                      {"I'll see about the bell.", "",
                       "quest.the_bell.started", ""},
                      {"Leave it to the birds.", "", "", ""},
                  }},
             },
         }},
        // Reachable from `abyss` for as long as quest.the_nameless.complete
        // is unset — see World::reconcile(). Two of the three branches
        // resolve the quest without a fight: "relieve" ends it peacefully
        // (sets quest.the_nameless.complete directly and hands over the
        // ring as a farewell gift, not loot); "goad" hands the encounter
        // back to `attack`, unchanged from before this tree existed.
        // "Ask nothing" just leaves — the player can `talk` again later.
        {"nameless_thing", "the Nameless Thing",
         Tree{
             "start",
             {
                 {"start",
                  "It doesn't attack. It just watches you, the way "
                  "something watches whoever finally opens a door it's "
                  "been holding shut. \"You came all this way,\" it says, "
                  "\"and you still haven't asked why.\"",
                  {
                      {"Why are you here?", "explain", "", ""},
                      {"I didn't come to talk.", "", "", ""},
                  }},
                 {"explain",
                  "\"Something older than the keep wanted out, once. I'm "
                  "what's left standing between it and everyone who's "
                  "walked in after — the Warden, the Knight, all the ones "
                  "you've already put down were just the outer doors. "
                  "I'm the last one, and I have been for longer than "
                  "you'd believe.\"",
                  {
                      {"Let me stand in your place.", "relieve",
                       "quest.the_nameless.complete", "ring_of_the_watcher"},
                      {"That's not my problem.", "goad", "", ""},
                      {"I need to think about that.", "", "", ""},
                  }},
                 {"relieve",
                  "Something in it that might be relief loosens its "
                  "shoulders. It sets a plain iron ring into your palm — "
                  "\"Not a reward. A door only opens from one side at a "
                  "time.\" — and steps back, out past where the light "
                  "reaches, and doesn't move again.",
                  {
                      {"...", "", "", ""},
                  }},
                 {"goad",
                  "\"Then we're done talking,\" it says, and doesn't wait "
                  "for you to agree.",
                  {
                      {"Fine.", "", "", ""},
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
