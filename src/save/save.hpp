#ifndef KEEPSAKE_SAVE_SAVE_HPP
#define KEEPSAKE_SAVE_SAVE_HPP

#include <string>

#include "entity/character.hpp"
#include "quest/quest.hpp"
#include "save/json.hpp"
#include "save/schema.hpp"

namespace keepsake::save {

// Why a load did or did not produce a character. Missing is the ordinary
// "brand-new identity" state; NewerSchema means the record was written by a
// build ahead of this one and must not be treated as missing or overwritten.
enum class LoadStatus {
    Loaded,
    Missing,
    NewerSchema,
    Invalid,
};

struct SaveData {
    entity::Character character;
    quest::Progress progress;
    // The shape this data was read as. New saves are always written at
    // kCurrentSchemaVersion; the field lets main explain a newer save.
    int schemaVersion = kCurrentSchemaVersion;
};

Json toJson(const SaveData &data);

// Parses a save or record object, running any migrations on a copy. A record
// written by a newer schema is reported through the result and left
// unmodified; nothing is ever downgraded or silently reinterpreted.
LoadStatus fromJson(const Json &json, SaveData &out);

// `<data dir>/saves/<identityHash>.json` — every save is namespaced under
// the identity hash it belongs to (see identity::hash()), so once real
// sign-in exists, switching accounts switches save files without any
// extra bookkeeping.
std::string savePathFor(const std::string &identityHash);

bool writeSave(const std::string &path, const SaveData &data);
LoadStatus readSave(const std::string &path, SaveData &out);

} // namespace keepsake::save

#endif
