#include "sync/wolfram_record_store.hpp"

#include "oauth/url_encode.hpp"
#include "util/time.hpp"

namespace keepsake::sync {

namespace {

constexpr const char *kCharacterCollection = "click.croft.rpg.character";
constexpr const char *kProgressCollection = "click.croft.rpg.progress";

save::Json characterToRecord(const entity::Character &c) {
    save::Json record = save::Json::object();
    record.set("className", c.className);
    record.set("level", c.level);
    record.set("xp", c.xp);
    record.set("hp", c.hp);
    record.set("maxHp", c.maxHp);
    record.set("attack", c.attack);
    record.set("defense", c.defense);
    record.set("worldSeed", c.worldSeed);
    record.set("createdAt", c.createdAt.empty() ? util::isoNow() : c.createdAt);

    save::Json inventory = save::Json::array();
    for (const auto &stack : c.inventory) {
        save::Json item = save::Json::object();
        item.set("itemId", stack.itemId);
        item.set("count", stack.count);
        inventory.push(std::move(item));
    }
    record.set("inventory", std::move(inventory));
    return record;
}

// Returns false if `value` is missing a field the character.json lexicon
// requires — a malformed or foreign record at this rkey, not a local bug.
bool recordToCharacter(const save::Json &value, entity::Character &out) {
    if (!value.isObject()) return false;
    const save::Json *v = nullptr;
    if ((v = value.find("className")) == nullptr) return false;
    out.className = v->asString();
    if ((v = value.find("level")) == nullptr) return false;
    out.level = v->asInt(1);
    if ((v = value.find("xp")) != nullptr) out.xp = v->asInt(0);
    if ((v = value.find("hp")) == nullptr) return false;
    out.hp = v->asInt();
    if ((v = value.find("maxHp")) == nullptr) return false;
    out.maxHp = v->asInt();
    if ((v = value.find("attack")) != nullptr) out.attack = v->asInt();
    if ((v = value.find("defense")) != nullptr) out.defense = v->asInt();
    if ((v = value.find("worldSeed")) != nullptr) out.worldSeed = v->asString();
    if ((v = value.find("createdAt")) != nullptr) out.createdAt = v->asString();
    if ((v = value.find("inventory")) != nullptr && v->isArray()) {
        out.inventory.clear();
        for (const auto &item : v->items()) {
            if (!item.isObject()) continue;
            const save::Json *itemId = item.find("itemId");
            const save::Json *count = item.find("count");
            if (itemId == nullptr || count == nullptr) continue;
            out.inventory.push_back(
                entity::ItemStack{itemId->asString(), count->asInt(1)});
        }
    }
    return true;
}

save::Json progressToRecord(const quest::Progress &p) {
    save::Json record = save::Json::object();
    record.set("location", p.location);
    save::Json flags = save::Json::array();
    for (const auto &flag : p.flags) flags.push(save::Json(flag));
    record.set("flags", std::move(flags));
    record.set("updatedAt", util::isoNow());
    return record;
}

bool recordToProgress(const save::Json &value, quest::Progress &out) {
    if (!value.isObject()) return false;
    const save::Json *location = value.find("location");
    if (location == nullptr) return false;
    out.location = location->asString();
    out.flags.clear();
    if (const save::Json *flags = value.find("flags");
        flags != nullptr && flags->isArray()) {
        for (const auto &flag : flags->items())
            out.flags.push_back(flag.asString());
    }
    return true;
}

bool putRecord(wf_auth_client *auth, const std::string &did,
               const std::string &collection, save::Json recordValue) {
    save::Json body = save::Json::object();
    body.set("repo", did);
    body.set("collection", collection);
    body.set("rkey", "self");
    body.set("record", std::move(recordValue));
    std::string bodyJson = body.dump();

    wf_response resp{};
    wf_status status = wf_auth_client_procedure(
        auth, "com.atproto.repo.putRecord", bodyJson.c_str(), &resp);
    bool ok = status == WF_OK && resp.status >= 200 && resp.status < 300;
    wf_response_free(&resp);
    return ok;
}

// Returns false (and leaves `outValue` untouched) if the record doesn't
// exist yet or the fetch otherwise fails — the ordinary state for a
// brand-new DID that has never saved before.
bool getRecord(wf_auth_client *auth, const std::string &did,
               const std::string &collection, save::Json &outValue) {
    std::string query = "repo=" + oauth::urlEncode(did) +
                        "&collection=" + oauth::urlEncode(collection) +
                        "&rkey=self";
    wf_response resp{};
    wf_status status = wf_auth_client_query(auth, "com.atproto.repo.getRecord",
                                            query.c_str(), &resp);
    bool ok = status == WF_OK && resp.status >= 200 && resp.status < 300 &&
              resp.body != nullptr;
    if (ok) {
        save::Json parsed;
        if (save::Json::parse(resp.body, parsed) && parsed.isObject()) {
            if (const save::Json *value = parsed.find("value")) {
                outValue = *value;
            } else {
                ok = false;
            }
        } else {
            ok = false;
        }
    }
    wf_response_free(&resp);
    return ok;
}

} // namespace

std::unique_ptr<WolframRecordStore>
WolframRecordStore::open(std::string &error) {
    auto store = std::unique_ptr<WolframRecordStore>(new WolframRecordStore());
    if (!oauth::restoreSession(oauth::sessionFilePath(), store->session_,
                               error)) {
        return nullptr;
    }
    return store;
}

bool WolframRecordStore::load(save::SaveData &out) {
    if (!session_.valid()) return false;

    save::Json characterValue;
    save::Json progressValue;
    if (!getRecord(session_.client(), session_.did(), kCharacterCollection,
                   characterValue)) {
        return false;
    }
    if (!getRecord(session_.client(), session_.did(), kProgressCollection,
                   progressValue)) {
        return false;
    }

    entity::Character character;
    quest::Progress progress;
    if (!recordToCharacter(characterValue, character)) return false;
    if (!recordToProgress(progressValue, progress)) return false;

    out.character = std::move(character);
    out.progress = std::move(progress);
    return true;
}

bool WolframRecordStore::save(const save::SaveData &data) {
    if (!session_.valid()) return false;

    // Both writes must succeed before this reports success — see
    // AGENTS.md's note on atproto-snake's write-reporting discipline.
    bool characterOk =
        putRecord(session_.client(), session_.did(), kCharacterCollection,
                  characterToRecord(data.character));
    bool progressOk =
        putRecord(session_.client(), session_.did(), kProgressCollection,
                  progressToRecord(data.progress));
    return characterOk && progressOk;
}

} // namespace keepsake::sync
