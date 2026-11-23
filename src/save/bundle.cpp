#include "save/bundle.hpp"

#include <fstream>
#include <sstream>

namespace keepsake::save {

Json toJson(const Bundle &bundle) {
    Json root = Json::object();
    root.set("keepsakeBundle", kCurrentBundleVersion);
    root.set("exportedFrom", bundle.exportedFrom);
    root.set("exportedAt", bundle.exportedAt);
    // The embedded save is always written at the current schema version;
    // save::toJson owns that rule, so a bundle cannot smuggle in an old one.
    root.set("save", toJson(bundle.save));
    return root;
}

BundleStatus fromJson(const Json &json, Bundle &out) {
    if (!json.isObject()) return BundleStatus::Invalid;

    const Json *version = json.find("keepsakeBundle");
    if (version == nullptr || !version->isNumber()) {
        return BundleStatus::Invalid;
    }
    const int bundleVersion = version->asInt(-1);
    if (bundleVersion > kCurrentBundleVersion) {
        return BundleStatus::NewerBundle;
    }
    if (bundleVersion != kCurrentBundleVersion) {
        return BundleStatus::Invalid;
    }

    const Json *saveJson = json.find("save");
    if (saveJson == nullptr || !saveJson->isObject()) {
        return BundleStatus::Invalid;
    }

    SaveData save;
    const LoadStatus saveStatus = fromJson(*saveJson, save);
    if (saveStatus == LoadStatus::NewerSchema) {
        return BundleStatus::NewerSchema;
    }
    if (saveStatus != LoadStatus::Loaded) {
        return BundleStatus::Invalid;
    }

    Bundle bundle;
    bundle.bundleVersion = bundleVersion;
    if (const Json *from = json.find("exportedFrom");
        from != nullptr && !from->isNull()) {
        bundle.exportedFrom = from->asString();
    }
    if (const Json *at = json.find("exportedAt");
        at != nullptr && !at->isNull()) {
        bundle.exportedAt = at->asString();
    }
    bundle.save = std::move(save);
    out = std::move(bundle);
    return BundleStatus::Loaded;
}

bool writeBundle(const std::string &path, const Bundle &bundle) {
    std::ofstream outFile(path, std::ios::trunc);
    if (!outFile) return false;
    outFile << toJson(bundle).dump() << "\n";
    return static_cast<bool>(outFile);
}

BundleStatus readBundle(const std::string &path, Bundle &out) {
    std::ifstream in(path);
    if (!in) return BundleStatus::Invalid;
    std::ostringstream buf;
    buf << in.rdbuf();

    Json json;
    if (!Json::parse(buf.str(), json)) return BundleStatus::Invalid;
    return fromJson(json, out);
}

} // namespace keepsake::save
