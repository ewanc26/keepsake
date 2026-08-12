// Unit tests for save::toJson/fromJson (Character/Progress <-> JSON) and
// writeSave/readSave (file round trip). $XDG_DATA_HOME is redirected to a
// throwaway temp directory so savePathFor()/writeSave()/readSave() never
// touch the real save directory.

#include <cstdlib>
#include <filesystem>
#include <iostream>

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
        data.character.createdAt = "2026-01-01T00:00:00Z";
        data.character.inventory = {ItemStack{"health_potion", 2},
                                    ItemStack{"iron_sword", 1}};
        data.progress.location = "undercroft";
        data.progress.flags = {"quest.keep_cleared.started"};

        Json json = toJson(data);
        SaveData roundTripped;
        CHECK(fromJson(json, roundTripped));

        CHECK(roundTripped.character.className == "Wanderer");
        CHECK(roundTripped.character.level == 3);
        CHECK(roundTripped.character.xp == 17);
        CHECK(roundTripped.character.hp == 12);
        CHECK(roundTripped.character.maxHp == 30);
        CHECK(roundTripped.character.attack == 6);
        CHECK(roundTripped.character.defense == 2);
        CHECK(roundTripped.character.worldSeed == "deadbeef");
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
        CHECK(!fromJson(notAnObject, out));

        Json missingProgress = Json::object();
        missingProgress.set("character", Json::object());
        CHECK(!fromJson(missingProgress, out));

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
        CHECK(!fromJson(missingRequiredField, out));
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
        CHECK(readSave(path, readBack));
        CHECK(readBack.character.className == "Wanderer");
        CHECK(readBack.progress.location == "gatehouse");

        // readSave fails cleanly on a nonexistent path.
        SaveData missing;
        CHECK(!readSave((tempDir / "does_not_exist.json").string(), missing));
    }

    fs::remove_all(tempDir);

    std::cout << "save_test " << (g_failures ? "FAILED" : "OK") << "\n";
    return g_failures ? 1 : 0;
}
