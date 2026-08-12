#include "util/xdg.hpp"

#include <cstdlib>
#include <filesystem>

namespace keepsake::util {

std::string keepsakeDataDir() {
    namespace fs = std::filesystem;

    fs::path base;
    if (const char *xdg = std::getenv("XDG_DATA_HOME");
        xdg != nullptr && *xdg != '\0') {
        base = xdg;
    } else if (const char *home = std::getenv("HOME");
               home != nullptr && *home != '\0') {
        base = fs::path(home) / ".local" / "share";
    } else {
        base = fs::current_path();
    }

    fs::path dir = base / "keepsake";
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir.string();
}

std::string oauthSessionFilePath() {
    return keepsakeDataDir() + "/oauth-session.json";
}

std::string socialNpcsOptInFilePath() {
    return keepsakeDataDir() + "/social-npcs-enabled";
}

} // namespace keepsake::util
