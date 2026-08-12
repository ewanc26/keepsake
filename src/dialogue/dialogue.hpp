#ifndef KEEPSAKE_DIALOGUE_DIALOGUE_HPP
#define KEEPSAKE_DIALOGUE_DIALOGUE_HPP

#include <istream>
#include <ostream>
#include <string>
#include <vector>

#include "entity/character.hpp"
#include "quest/quest.hpp"

namespace keepsake::dialogue {

struct Option {
    std::string text;
    // Empty means this option ends the dialogue.
    std::string nextNodeId;
    // Empty means no flag is set.
    std::string setFlag;
    // Empty means no item is given.
    std::string giveItemId;
};

struct Node {
    std::string id;
    std::string text;
    std::vector<Option> options;
};

struct Tree {
    std::string startNodeId;
    std::vector<Node> nodes;

    const Node *find(const std::string &id) const;
};

struct NpcDef {
    std::string id;
    std::string name;
    Tree tree;
};

const NpcDef *findNpcDef(const std::string &id);

// Drives an interactive conversation: prints node text and numbered
// options to `out`, reads a choice from `in`, applies setFlag/giveItemId,
// and follows nextNodeId until an option with no nextNodeId is chosen.
void run(const NpcDef &npc, entity::Character &player,
         quest::Progress &progress, std::istream &in, std::ostream &out);

} // namespace keepsake::dialogue

#endif
