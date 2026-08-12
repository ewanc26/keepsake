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
    root.set("character", std::move(character));
    root.set("progress", std::move(progress));
    return root;
}

bool fromJson(const Json &json, SaveData &out) {
    if (!json.isObject()) return false;

    const Json *characterJson = json.find("character");
    const Json *progressJson = json.find("progress");
    if (characterJson == nullptr || !characterJson->isObject()) return false;
    if (progressJson == nullptr || !progressJson->isObject()) return false;

    entity::Character character;
    const Json *v = nullptr;
    if ((v = characterJson->find("className")) == nullptr) return false;
    character.className = v->asString();
    if ((v = characterJson->find("level")) == nullptr) return false;
    character.level = v->asInt();
    if ((v = characterJson->find("xp")) == nullptr) return false;
    character.xp = v->asInt();
    if ((v = characterJson->find("hp")) == nullptr) return false;
    character.hp = v->asInt();
    if ((v = characterJson->find("maxHp")) == nullptr) return false;
    character.maxHp = v->asInt();
    if ((v = characterJson->find("attack")) == nullptr) return false;
    character.attack = v->asInt();
    if ((v = characterJson->find("defense")) == nullptr) return false;
    character.defense = v->asInt();
    if ((v = characterJson->find("worldSeed")) != nullptr) {
        character.worldSeed = v->asString();
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
    if ((v = progressJson->find("location")) == nullptr) return false;
    progress.location = v->asString();
    if ((v = progressJson->find("flags")) != nullptr && v->isArray()) {
        for (const auto &flag : v->items()) {
            progress.flags.push_back(flag.asString());
        }
    }

    out.character = std::move(character);
    out.progress = std::move(progress);
    return true;
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

bool readSave(const std::string &path, SaveData &out) {
    std::ifstream in(path);
    if (!in) return false;
    std::ostringstream buf;
    buf << in.rdbuf();

    Json json;
    if (!Json::parse(buf.str(), json)) return false;
    return fromJson(json, out);
}

} // namespace keepsake::save
