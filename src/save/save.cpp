#include "save/save.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

#include "util/xdg.hpp"

namespace keepsake::save {

Json toJson(const SaveData &data) {
    Json character = Json::object();
    character.set("className", data.character.className);
    character.set("level", data.character.level);
    character.set("xp", data.character.xp);
    character.set("hp", data.character.hp);
    character.set("maxHp", data.character.maxHp);
    character.set("attack", data.character.attack);
    character.set("defense", data.character.defense);
    character.set("worldSeed", data.character.worldSeed);
    character.set("derivationVersion", data.character.derivationVersion);
    character.set("createdAt", data.character.createdAt);

    Json inventory = Json::array();
    for (const auto &stack : data.character.inventory) {
        Json item = Json::object();
        item.set("itemId", stack.itemId);
        item.set("count", stack.count);
        inventory.push(std::move(item));
    }
    character.set("inventory", std::move(inventory));

    Json progress = Json::object();
    progress.set("location", data.progress.location);
    Json flags = Json::array();
    for (const auto &flag : data.progress.flags) {
        flags.push(Json(flag));
    }
    progress.set("flags", std::move(flags));

    Json root = Json::object();
    // New records always name the shape this build writes, never a value
    // carried in from a migrated or stale SaveData.
    root.set("schemaVersion", kCurrentSchemaVersion);
    root.set("character", std::move(character));
    root.set("progress", std::move(progress));
    return root;
}

LoadStatus fromJson(const Json &json, SaveData &out) {
    if (!json.isObject()) return LoadStatus::Invalid;

    Json migrated = json;
    const SchemaResult schema = migrateToCurrent(migrated);
    if (schema.status == SchemaStatus::Newer) {
        out.schemaVersion = schema.version;
        return LoadStatus::NewerSchema;
    }
    if (schema.status == SchemaStatus::Invalid) return LoadStatus::Invalid;

    const Json *characterJson = migrated.find("character");
    const Json *progressJson = migrated.find("progress");
    if (characterJson == nullptr || !characterJson->isObject()) {
        return LoadStatus::Invalid;
    }
    if (progressJson == nullptr || !progressJson->isObject()) {
        return LoadStatus::Invalid;
    }

    entity::Character character;
    const Json *v = nullptr;
    if ((v = characterJson->find("className")) == nullptr) {
        return LoadStatus::Invalid;
    }
    character.className = v->asString();
    if ((v = characterJson->find("level")) == nullptr) return LoadStatus::Invalid;
    character.level = v->asInt();
    if ((v = characterJson->find("xp")) == nullptr) return LoadStatus::Invalid;
    character.xp = v->asInt();
    if ((v = characterJson->find("hp")) == nullptr) return LoadStatus::Invalid;
    character.hp = v->asInt();
    if ((v = characterJson->find("maxHp")) == nullptr) {
        return LoadStatus::Invalid;
    }
    character.maxHp = v->asInt();
    if ((v = characterJson->find("attack")) == nullptr) {
        return LoadStatus::Invalid;
    }
    character.attack = v->asInt();
    if ((v = characterJson->find("defense")) == nullptr) {
        return LoadStatus::Invalid;
    }
    character.defense = v->asInt();
    if ((v = characterJson->find("worldSeed")) != nullptr) {
        character.worldSeed = v->asString();
    }
    // Absent means a character written before #10: its seed is the legacy
    // FNV-1a hash, which is derivation version 0, and is never re-derived.
    if ((v = characterJson->find("derivationVersion")) != nullptr) {
        character.derivationVersion = v->asInt(0);
    }
    if ((v = characterJson->find("createdAt")) != nullptr) {
        character.createdAt = v->asString();
    }
    if ((v = characterJson->find("inventory")) != nullptr && v->isArray()) {
        for (const auto &item : v->items()) {
            if (!item.isObject()) continue;
            const Json *itemId = item.find("itemId");
            const Json *count = item.find("count");
            if (itemId == nullptr || count == nullptr) continue;
            character.inventory.push_back(
                entity::ItemStack{itemId->asString(), count->asInt(1)});
        }
    }

    quest::Progress progress;
    if ((v = progressJson->find("location")) == nullptr) {
        return LoadStatus::Invalid;
    }
    progress.location = v->asString();
    if ((v = progressJson->find("flags")) != nullptr && v->isArray()) {
        for (const auto &flag : v->items()) {
            progress.flags.push_back(flag.asString());
        }
    }

    out.character = std::move(character);
    out.progress = std::move(progress);
    out.schemaVersion = schema.version;
    return LoadStatus::Loaded;
}

std::string savePathFor(const std::string &identityHash) {
    namespace fs = std::filesystem;
    fs::path dir = fs::path(util::keepsakeDataDir()) / "saves";
    std::error_code ec;
    fs::create_directories(dir, ec);
    return (dir / (identityHash + ".json")).string();
}

bool writeSave(const std::string &path, const SaveData &data) {
    std::ofstream out(path, std::ios::trunc);
    if (!out) return false;
    out << toJson(data).dump() << "\n";
    return static_cast<bool>(out);
}

LoadStatus readSave(const std::string &path, SaveData &out) {
    std::ifstream in(path);
    if (!in) return LoadStatus::Missing;
    std::ostringstream buf;
    buf << in.rdbuf();

    Json json;
    if (!Json::parse(buf.str(), json)) return LoadStatus::Invalid;
    return fromJson(json, out);
}

} // namespace keepsake::save
