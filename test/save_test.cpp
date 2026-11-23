// Unit tests for save::toJson/fromJson (Character/Progress <-> JSON) and
// writeSave/readSave (file round trip). $XDG_DATA_HOME is redirected to a
// throwaway temp directory so savePathFor()/writeSave()/readSave() never
// touch the real save directory.

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include "save/save.hpp"

namespace {

int g_failures = 0;

#define CHECK(expr)                                                            \
    do {                                                                       \
        if (!(expr)) {                                                         \
            std::cerr << "FAILED: " << #expr                                   \
                      << " at save_test.cpp:" << __LINE__ << "\n";             \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

} // namespace

int main() {
    using namespace keepsake::save;
    using keepsake::entity::Character;
    using keepsake::entity::ItemStack;
    using keepsake::quest::Progress;

    namespace fs = std::filesystem;
    fs::path tempDir = fs::temp_directory_path() / "keepsake_save_test_xdg";
    fs::remove_all(tempDir);
    setenv("XDG_DATA_HOME", tempDir.string().c_str(), 1);

    // toJson -> fromJson round trip preserves every field, including
    // inventory stacks.
    {
        SaveData data;
        data.character.className = "Wanderer";
        data.character.level = 3;
        data.character.xp = 17;
        data.character.hp = 12;
        data.character.maxHp = 30;
        data.character.attack = 6;
        data.character.defense = 2;
        data.character.worldSeed = "deadbeef";
        data.character.derivationVersion = 1;
        data.character.createdAt = "2026-01-01T00:00:00Z";
        data.character.inventory = {ItemStack{"health_potion", 2},
                                    ItemStack{"iron_sword", 1}};
        data.progress.location = "undercroft";
        data.progress.flags = {"quest.keep_cleared.started"};

        Json json = toJson(data);
        SaveData roundTripped;
        CHECK(fromJson(json, roundTripped) == LoadStatus::Loaded);
        CHECK(roundTripped.schemaVersion == kCurrentSchemaVersion);

        CHECK(roundTripped.character.className == "Wanderer");
        CHECK(roundTripped.character.level == 3);
        CHECK(roundTripped.character.xp == 17);
        CHECK(roundTripped.character.hp == 12);
        CHECK(roundTripped.character.maxHp == 30);
        CHECK(roundTripped.character.attack == 6);
        CHECK(roundTripped.character.defense == 2);
        CHECK(roundTripped.character.worldSeed == "deadbeef");
        CHECK(roundTripped.character.derivationVersion == 1);
        CHECK(roundTripped.character.createdAt == "2026-01-01T00:00:00Z");
        CHECK(roundTripped.character.inventory.size() == 2);
        CHECK(roundTripped.character.inventory[0].itemId == "health_potion");
        CHECK(roundTripped.character.inventory[0].count == 2);
        CHECK(roundTripped.character.inventory[1].itemId == "iron_sword");
        CHECK(roundTripped.progress.location == "undercroft");
        CHECK(roundTripped.progress.flags.size() == 1);
        CHECK(roundTripped.progress.flags[0] == "quest.keep_cleared.started");
    }

    // fromJson rejects malformed saves rather than filling in guesses.
    {
        SaveData out;
        Json notAnObject = Json::array();
        CHECK(fromJson(notAnObject, out) == LoadStatus::Invalid);

        Json missingProgress = Json::object();
        missingProgress.set("character", Json::object());
        CHECK(fromJson(missingProgress, out) == LoadStatus::Invalid);

        Json missingRequiredField = Json::object();
        Json character = Json::object();
        // className omitted — required.
        character.set("level", 1);
        character.set("xp", 0);
        character.set("hp", 20);
        character.set("maxHp", 20);
        character.set("attack", 4);
        character.set("defense", 1);
        missingRequiredField.set("character", std::move(character));
        Json progress = Json::object();
        progress.set("location", "gatehouse");
        missingRequiredField.set("progress", std::move(progress));
        CHECK(fromJson(missingRequiredField, out) == LoadStatus::Invalid);
    }

    // savePathFor is namespaced under the identity hash.
    {
        std::string path = savePathFor("abc123");
        CHECK(path.find("abc123.json") != std::string::npos);
        CHECK(path.find("saves") != std::string::npos);
    }

    // writeSave/readSave round-trip through an actual file.
    {
        SaveData data;
        data.character.className = "Wanderer";
        data.character.level = 1;
        data.character.xp = 0;
        data.character.hp = 20;
        data.character.maxHp = 20;
        data.character.attack = 4;
        data.character.defense = 1;
        data.progress.location = "gatehouse";

        std::string path = (tempDir / "roundtrip.json").string();
        CHECK(writeSave(path, data));

        SaveData readBack;
        CHECK(readSave(path, readBack) == LoadStatus::Loaded);
        CHECK(readBack.character.className == "Wanderer");
        CHECK(readBack.progress.location == "gatehouse");
        CHECK(readBack.schemaVersion == kCurrentSchemaVersion);

        // A missing file is the ordinary new-identity state, not a fault.
        SaveData missing;
        CHECK(readSave((tempDir / "does_not_exist.json").string(), missing) ==
              LoadStatus::Missing);
    }

    // The initial shape, captured as a fixture, loads through the migration
    // table. Applying the table twice is the same as applying it once, and a
    // current record is a no-op.
    {
        std::ifstream in(std::string(WF_TEST_FIXTURE_DIR) +
                         "/save/v0-local.json");
        CHECK(static_cast<bool>(in));
        std::ostringstream buf;
        buf << in.rdbuf();

        Json original;
        CHECK(Json::parse(buf.str(), original));

        Json once = original;
        Json twice = original;
        const SchemaResult first = migrateToCurrent(once);
        const SchemaResult second = migrateToCurrent(twice);
        CHECK(first.status == SchemaStatus::Migrated);
        CHECK(first.version == kCurrentSchemaVersion);
        CHECK(second.status == SchemaStatus::Migrated);
        CHECK(once.dump() == twice.dump());

        Json again = once;
        const SchemaResult third = migrateToCurrent(again);
        CHECK(third.status == SchemaStatus::Current);
        CHECK(again.dump() == once.dump());

        SaveData v0;
        CHECK(fromJson(original, v0) == LoadStatus::Loaded);
        CHECK(v0.schemaVersion == kCurrentSchemaVersion);
        CHECK(v0.character.className == "Wanderer");
        CHECK(v0.character.level == 3);
        // Pre-#10 characters carry derivation version 0 (the legacy FNV-1a
        // seed) and are never re-derived by loading them.
        CHECK(v0.character.derivationVersion == 0);
        CHECK(v0.progress.location == "gatehouse");

        const Json written = toJson(v0);
        CHECK(written.find("schemaVersion") != nullptr);
        CHECK(written.find("schemaVersion")->asInt() ==
              kCurrentSchemaVersion);
    }

    // A save from a newer build is reported, not downgraded and not rewritten.
    {
        Json newer = Json::object();
        newer.set("schemaVersion", kCurrentSchemaVersion + 1);
        newer.set("character", Json::object());
        newer.set("progress", Json::object());

        SaveData out;
        CHECK(fromJson(newer, out) == LoadStatus::NewerSchema);
        CHECK(out.schemaVersion == kCurrentSchemaVersion + 1);

        Json untouched = newer;
        const std::string before = untouched.dump();
        const SchemaResult result = migrateToCurrent(untouched);
        CHECK(result.status == SchemaStatus::Newer);
        CHECK(result.version == kCurrentSchemaVersion + 1);
        CHECK(untouched.dump() == before);
    }

    // A version of the wrong type is invalid, not silently v0.
    {
        Json wrongType = Json::object();
        wrongType.set("schemaVersion", "two");
        CHECK(migrateToCurrent(wrongType).status == SchemaStatus::Invalid);
    }

    fs::remove_all(tempDir);

    std::cout << "save_test " << (g_failures ? "FAILED" : "OK") << "\n";
    return g_failures ? 1 : 0;
}
