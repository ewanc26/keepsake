#ifndef KEEPSAKE_SAVE_SAVE_HPP
#define KEEPSAKE_SAVE_SAVE_HPP

#include <string>

#include "entity/character.hpp"
#include "quest/quest.hpp"
#include "save/json.hpp"

namespace keepsake::save {

struct SaveData {
    entity::Character character;
    quest::Progress progress;
};

Json toJson(const SaveData &data);
// Returns false (leaving `out` unspecified) if `json` isn't a well-formed
// save — missing required fields, wrong types.
bool fromJson(const Json &json, SaveData &out);

// `<data dir>/saves/<identityHash>.json` — every save is namespaced under
// the identity hash it belongs to (see identity::hash()), so once real
// sign-in exists, switching accounts switches save files without any
// extra bookkeeping.
std::string savePathFor(const std::string &identityHash);

bool writeSave(const std::string &path, const SaveData &data);
bool readSave(const std::string &path, SaveData &out);

} // namespace keepsake::save

#endif
