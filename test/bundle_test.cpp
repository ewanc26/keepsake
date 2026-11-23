// Unit tests for save::Bundle — the portable export envelope. The bundle
// wraps the schema-versioned save, so these also prove that importing a
// bundle runs the same migrations as any other load and refuses a newer
// character instead of downgrading it. No network and no PDS: every case is
// local JSON plus the local record store.

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include "save/bundle.hpp"
#include "sync/local_record_store.hpp"

namespace {

int g_failures = 0;

#define CHECK(expr)                                                            \
    do {                                                                       \
        if (!(expr)) {                                                         \
            std::cerr << "FAILED: " << #expr                                   \
                      << " at bundle_test.cpp:" << __LINE__ << "\n";           \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

} // namespace

int main() {
    using keepsake::entity::Character;
    using keepsake::entity::ItemStack;
    using keepsake::quest::Progress;
    using namespace keepsake::save;

    namespace fs = std::filesystem;
    fs::path tempDir = fs::temp_directory_path() / "keepsake_bundle_test_xdg";
    fs::remove_all(tempDir);
    fs::create_directories(tempDir);
    setenv("XDG_DATA_HOME", tempDir.string().c_str(), 1);

    Bundle bundle;
    bundle.exportedFrom = "0123456789abcdef";
    bundle.exportedAt = "2026-11-23T00:00:00Z";
    bundle.save.character.className = "Wanderer";
    bundle.save.character.level = 4;
    bundle.save.character.xp = 25;
    bundle.save.character.hp = 18;
    bundle.save.character.maxHp = 32;
    bundle.save.character.attack = 7;
    bundle.save.character.defense = 3;
    bundle.save.character.worldSeed = "pinnedseed";
    bundle.save.character.createdAt = "2026-01-01T00:00:00Z";
    bundle.save.character.inventory = {ItemStack{"iron_sword", 1}};
    bundle.save.progress.location = "undercroft";
    bundle.save.progress.flags = {"quest.keep_cleared.started"};

    // Envelope round trip preserves the metadata and every character field.
    {
        Json json = toJson(bundle);
        const Json *version = json.find("keepsakeBundle");
        CHECK(version != nullptr);
        CHECK(version->asInt() == kCurrentBundleVersion);

        Bundle back;
        CHECK(fromJson(json, back) == BundleStatus::Loaded);
        CHECK(back.bundleVersion == kCurrentBundleVersion);
        CHECK(back.exportedFrom == "0123456789abcdef");
        CHECK(back.exportedAt == "2026-11-23T00:00:00Z");
        CHECK(back.save.character.className == "Wanderer");
        CHECK(back.save.character.level == 4);
        CHECK(back.save.character.worldSeed == "pinnedseed");
        CHECK(back.save.character.inventory.size() == 1);
        CHECK(back.save.character.inventory[0].itemId == "iron_sword");
        CHECK(back.save.progress.location == "undercroft");
        CHECK(back.save.progress.flags.size() == 1);
        CHECK(back.save.schemaVersion == kCurrentSchemaVersion);
    }

    // File round trip, then import through the same local store seam the
    // CLI uses. The worldSeed must survive unchanged: the seeded world is
    // part of the character, not the account it now lives under.
    {
        const std::string path = (tempDir / "wanderer.keepsake-bundle.json").string();
        CHECK(writeBundle(path, bundle));

        Bundle readBack;
        CHECK(readBundle(path, readBack) == BundleStatus::Loaded);

        keepsake::sync::LocalRecordStore store("newidentityhash");
        CHECK(store.load(readBack.save) == LoadStatus::Missing);
        CHECK(store.save(readBack.save));

        keepsake::save::SaveData imported;
        CHECK(store.load(imported) == LoadStatus::Loaded);
        CHECK(imported.character.className == "Wanderer");
        CHECK(imported.character.level == 4);
        CHECK(imported.character.attack == 7);
        CHECK(imported.character.worldSeed == "pinnedseed");
        CHECK(imported.progress.location == "undercroft");
        CHECK(keepsake::quest::hasFlag(imported.progress,
                                       "quest.keep_cleared.started"));
    }

    // A bundle carrying the initial save shape migrates on import.
    {
        Json saveJson = Json::object();
        Json character = Json::object();
        character.set("className", "Wanderer");
        character.set("level", 1);
        character.set("xp", 0);
        character.set("hp", 20);
        character.set("maxHp", 20);
        character.set("attack", 4);
        character.set("defense", 1);
        saveJson.set("character", std::move(character));
        Json progress = Json::object();
        progress.set("location", "gatehouse");
        saveJson.set("progress", std::move(progress));

        Json json = Json::object();
        json.set("keepsakeBundle", kCurrentBundleVersion);
        json.set("save", std::move(saveJson));

        Bundle old;
        CHECK(fromJson(json, old) == BundleStatus::Loaded);
        CHECK(old.save.schemaVersion == kCurrentSchemaVersion);
        CHECK(old.save.progress.location == "gatehouse");
    }

    // A newer envelope or a newer embedded save is reported and never read as
    // the current shape.
    {
        Json newerBundle = toJson(bundle);
        newerBundle.set("keepsakeBundle", kCurrentBundleVersion + 1);
        Bundle out;
        CHECK(fromJson(newerBundle, out) == BundleStatus::NewerBundle);

        Json newerSave = toJson(bundle);
        Json inner = *newerSave.find("save");
        inner.set("schemaVersion", kCurrentSchemaVersion + 1);
        newerSave.set("save", std::move(inner));
        CHECK(fromJson(newerSave, out) == BundleStatus::NewerSchema);
    }

    // Malformed envelopes are invalid, not empty bundles.
    {
        Bundle out;
        CHECK(fromJson(Json::array(), out) == BundleStatus::Invalid);

        Json complete = toJson(bundle);
        Json noVersion = Json::object();
        const Json *innerSave = complete.find("save");
        CHECK(innerSave != nullptr);
        if (innerSave != nullptr) noVersion.set("save", *innerSave);
        CHECK(fromJson(noVersion, out) == BundleStatus::Invalid);

        Json noSave = Json::object();
        noSave.set("keepsakeBundle", kCurrentBundleVersion);
        CHECK(fromJson(noSave, out) == BundleStatus::Invalid);

        CHECK(readBundle((tempDir / "missing.json").string(), out) ==
              BundleStatus::Invalid);
    }

    fs::remove_all(tempDir);

    std::cout << "bundle_test " << (g_failures ? "FAILED" : "OK") << "\n";
    return g_failures ? 1 : 0;
}
