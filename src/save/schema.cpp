#include "save/schema.hpp"

namespace keepsake::save {

namespace {

bool readSchemaVersion(const Json &record, int &version) {
    const Json *value = record.find("schemaVersion");
    if (value == nullptr) {
        version = kInitialSchemaVersion;
        return true;
    }
    if (!value->isNumber()) return false;
    const double raw = value->asNumber(-1.0);
    if (raw < 0 || raw != static_cast<double>(static_cast<int>(raw))) {
        return false;
    }
    version = static_cast<int>(raw);
    return true;
}

// Each step is a pure function from version N to N+1. The table's length is
// pinned to kCurrentSchemaVersion below, so adding a version without its
// migration is a compile error rather than a save that cannot load.
void migrateV0ToV1(Json &record) {
    // v0 is the shape written before the field existed. Its fields are
    // otherwise unchanged, so the first migration only stamps the version
    // every later step starts from.
    record.set("schemaVersion", 1);
}

using Migration = void (*)(Json &);

constexpr Migration kMigrations[] = {migrateV0ToV1};
constexpr int kMigrationCount =
    static_cast<int>(sizeof(kMigrations) / sizeof(kMigrations[0]));
static_assert(kMigrationCount == kCurrentSchemaVersion - kInitialSchemaVersion,
              "every schema version needs exactly one migration");

} // namespace

SchemaResult migrateToCurrent(Json &record) {
    if (!record.isObject()) return {SchemaStatus::Invalid, 0};

    int version = kInitialSchemaVersion;
    if (!readSchemaVersion(record, version)) {
        return {SchemaStatus::Invalid, 0};
    }
    if (version > kCurrentSchemaVersion) {
        return {SchemaStatus::Newer, version};
    }
    if (version < kInitialSchemaVersion) {
        return {SchemaStatus::Invalid, version};
    }

    bool migrated = false;
    while (version < kCurrentSchemaVersion) {
        if (version < 0 || version >= kMigrationCount) {
            return {SchemaStatus::Invalid, version};
        }
        kMigrations[version](record);
        ++version;
        migrated = true;
    }

    // A current record that somehow lost the marker still ends up stamped,
    // so a written save always names its shape.
    record.set("schemaVersion", version);
    return {migrated ? SchemaStatus::Migrated : SchemaStatus::Current, version};
}

} // namespace keepsake::save
