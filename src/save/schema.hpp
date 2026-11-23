#ifndef KEEPSAKE_SAVE_SCHEMA_HPP
#define KEEPSAKE_SAVE_SCHEMA_HPP

#include "save/json.hpp"

namespace keepsake::save {

// The shape written before click.croft.rpg.* records carried a version at
// all. An absent schemaVersion means exactly this shape, so every save that
// predates the field stays valid instead of being rejected.
inline constexpr int kInitialSchemaVersion = 0;

// Bump this whenever the stored shape changes, and add one migration below.
inline constexpr int kCurrentSchemaVersion = 1;

enum class SchemaStatus {
    Current,  // already at kCurrentSchemaVersion
    Migrated, // one or more migrations ran
    Newer,    // written by a build this one does not understand
    Invalid,  // not an object, or a version this build cannot interpret
};

struct SchemaResult {
    SchemaStatus status;
    int version;
};

// Run the ordered migration table over a save or record JSON object. The
// caller owns the copy: no stored record is ever rewritten in place. Applying
// this twice to the same object yields the same bytes and reports Current on
// the second pass. A version newer than kCurrentSchemaVersion is reported and
// left untouched, never downgraded.
SchemaResult migrateToCurrent(Json &record);

} // namespace keepsake::save

#endif
