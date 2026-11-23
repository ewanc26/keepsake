#ifndef KEEPSAKE_SAVE_BUNDLE_HPP
#define KEEPSAKE_SAVE_BUNDLE_HPP

#include <string>

#include "save/json.hpp"
#include "save/save.hpp"

namespace keepsake::save {

// The portable bundle is a separate envelope around the schema-versioned
// save, so its format can change independently of the character records.
inline constexpr int kCurrentBundleVersion = 1;

enum class BundleStatus {
    Loaded,
    NewerBundle, // written by a build ahead of this one
    NewerSchema, // the embedded save is ahead of this build
    Invalid,
};

struct Bundle {
    int bundleVersion = kCurrentBundleVersion;
    // The identity hash the bundle came from. Informational only: the
    // importing identity is the one that owns the new character.
    std::string exportedFrom;
    std::string exportedAt;
    SaveData save;
};

Json toJson(const Bundle &bundle);

// Parses an envelope and then its save, running the save migrations on a
// copy. A newer bundle or a newer save is reported distinctly and is never
// downgraded.
BundleStatus fromJson(const Json &json, Bundle &out);

bool writeBundle(const std::string &path, const Bundle &bundle);
BundleStatus readBundle(const std::string &path, Bundle &out);

} // namespace keepsake::save

#endif
