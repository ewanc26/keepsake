// Unit tests for sync::LocalRecordStore — the local RecordStore
// implementation ui::run talks to in signed-out play. save_test covers
// save::writeSave/readSave; this covers the RecordStore seam around it:
// load() returning false for a brand-new identity, save/load round trip
// through the store's own savePathFor(identity) path, and the broadcast
// defaults being safe no-ops. $XDG_DATA_HOME is redirected to a throwaway
// temp directory like save_test, so nothing real is touched. No wolfram
// dependency — the local backend never links it.

#include <cstdlib>
#include <filesystem>
#include <iostream>

#include "entity/character.hpp"
#include "sync/local_record_store.hpp"

namespace {

int g_failures = 0;

#define CHECK(expr)                                                            \
    do {                                                                       \
        if (!(expr)) {                                                         \
            std::cerr << "FAILED: " << #expr                                   \
                      << " at local_record_store_test.cpp:" << __LINE__        \
                      << "\n";                                                 \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

} // namespace

int main() {
    using keepsake::entity::Character;
    using keepsake::entity::ItemStack;
    using keepsake::quest::Progress;
    using keepsake::save::SaveData;
    using keepsake::sync::LocalRecordStore;

    namespace fs = std::filesystem;
    fs::path tempDir =
        fs::temp_directory_path() / "keepsake_local_record_store_xdg";
    fs::remove_all(tempDir);
    setenv("XDG_DATA_HOME", tempDir.string().c_str(), 1);

    // load() on a brand-new identity returns false — "start a new
    // character", not a fault (see RecordStore's contract).
    {
        LocalRecordStore store("1234567890abcdef");
        SaveData data;
        CHECK(!store.load(data));
    }

    // save() then load() round trips through the store's own identity-
    // namespaced path — the same load/save cycle signed-out play performs
    // every session.
    {
        SaveData data;
        data.character.className = "Wanderer";
        data.character.level = 2;
        data.character.worldSeed = "1234567890abcdef";
        data.character.inventory = {ItemStack{"health_potion", 2}};
        data.progress.location = "courtyard";
        data.progress.flags = {"quest.keep_cleared.started"};

        {
            LocalRecordStore store("1234567890abcdef");
            CHECK(store.save(data));
        }

        LocalRecordStore store("1234567890abcdef");
        SaveData loaded;
        CHECK(store.load(loaded));
        CHECK(loaded.character.level == 2);
        CHECK(loaded.character.worldSeed == "1234567890abcdef");
        CHECK(loaded.character.itemCount("health_potion") == 2);
        CHECK(loaded.progress.location == "courtyard");
        CHECK(keepsake::quest::hasFlag(loaded.progress,
                                       "quest.keep_cleared.started"));

        // Polymorphic use — ui::run only ever holds a RecordStore&.
        keepsake::sync::RecordStore &asBase = store;
        CHECK(asBase.save(data));
        SaveData reloaded;
        CHECK(asBase.load(reloaded));
        CHECK(reloaded.character.level == 2);
    }

    // A different identity sees a different (absent) save — the
    // identity-namespaced path is what keeps accounts separate.
    {
        LocalRecordStore other("ffffff000000ffff");
        SaveData data;
        CHECK(!other.load(data));
    }

    // recordAchievement/recordEvent defaults are safe no-ops on the local
    // backend — no PDS to write to, and nothing to crash or block on.
    {
        LocalRecordStore store("1234567890abcdef");
        store.recordAchievement("quest.keep_cleared", "Cleared the Keep");
        store.recordEvent("enemyDefeated", "undercroft", "");
        // Still fully usable afterwards.
        SaveData data;
        CHECK(store.save(data));
    }

    std::cout << "local_record_store_test "
              << (g_failures ? "FAILED" : "OK") << "\n";
    return g_failures ? 1 : 0;
}
