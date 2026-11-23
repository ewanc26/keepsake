#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

#include "identity/identity.hpp"
#include "save/bundle.hpp"
#include "save/save.hpp"
#include "sync/local_record_store.hpp"
#include "ui/terminal.hpp"
#include "util/time.hpp"
#include "util/xdg.hpp"
#include "world/world.hpp"

// Generated at configure time (see CMakeLists.txt) — not in src/ tree.
#include "keepsake/version.hpp"

#if defined(KEEPSAKE_WITH_WOLFRAM)
#include "oauth/oauth_flow.hpp"
#include "oauth/profile_lookup.hpp"
#include "sync/firehose_watch.hpp"
#include "sync/wolfram_record_store.hpp"
#endif

namespace {

void printUsage() {
    std::cout
        << "Usage: keepsake [command]\n\n"
           "Commands:\n"
           "  (none)   Play — synced to your PDS if signed in, local "
           "otherwise\n"
           "  play [--offline]        Play; --offline ignores a signed-in "
           "session\n"
           "  offline                 Play from this device's local save "
           "only\n"
           "  export <path> [--offline]  Write a portable character "
           "bundle\n"
           "  import <path> [--force] [--offline]  Load a bundle into this "
           "account\n"
#if defined(KEEPSAKE_WITH_WOLFRAM)
           "  login <handle-or-did>   Sign in with your AT Protocol handle\n"
           "  logout                  Forget the saved session\n"
           "  whoami                  Show the signed-in DID, if any\n"
           "  events                  Watch the firehose for other "
           "players' world events\n"
           "  npcs on|off             Turn accounts you follow into "
           "in-world mentions\n"
#endif
           "  version                 Show the version and build id\n"
           "  help     Show this message\n";
}

bool hasArg(int argc, char **argv, const char *flag) {
    for (int i = 2; i < argc; ++i) {
        if (std::string(argv[i]) == flag) return true;
    }
    return false;
}

// One backend selection path for play, export and import. Offline forces the
// local store even when a session exists, so no XRPC read is attempted.
std::unique_ptr<keepsake::sync::RecordStore>
openRecordStore(bool offline, std::string &signedInDid) {
    signedInDid.clear();
    std::unique_ptr<keepsake::sync::RecordStore> store;
#if defined(KEEPSAKE_WITH_WOLFRAM)
    if (!offline) {
        std::string error;
        auto wolframStore = keepsake::sync::WolframRecordStore::open(error);
        if (wolframStore) {
            std::cout << "Signed in as " << wolframStore->did() << "\n";
            signedInDid = wolframStore->did();
            store = std::move(wolframStore);
        }
    }
#endif
    if (!store) {
        store = std::make_unique<keepsake::sync::LocalRecordStore>(
            keepsake::identity::hash());
    }
    return store;
}

void printNewerSave(int foundSchema) {
    std::cerr << "This character was written by a newer Keepsake (save schema "
              << foundSchema << "; this build understands "
              << keepsake::save::kCurrentSchemaVersion
              << "). Update Keepsake to use it; nothing has been changed.\n";
}

} // namespace

int main(int argc, char **argv) {
    std::string command = argc > 1 ? argv[1] : "";

#if defined(KEEPSAKE_WITH_WOLFRAM)
    if (command == "login") {
        if (argc < 3) {
            std::cerr << "Usage: keepsake login <handle-or-did>\n";
            return 1;
        }
        keepsake::oauth::AuthSession session;
        std::string error;
        if (!keepsake::oauth::signIn(
                argv[2], keepsake::oauth::sessionFilePath(), session, error)) {
            std::cerr << "Sign-in failed: " << error << "\n";
            return 1;
        }
        std::cout << "Signed in as " << session.did() << "\n";
        return 0;
    }
    if (command == "logout") {
        std::string path = keepsake::oauth::sessionFilePath();
        if (std::remove(path.c_str()) == 0) {
            std::cout << "Signed out.\n";
        } else {
            std::cout << "Not signed in.\n";
        }
        return 0;
    }
    if (command == "whoami") {
        keepsake::oauth::AuthSession session;
        std::string error;
        if (keepsake::oauth::restoreSession(keepsake::oauth::sessionFilePath(),
                                            session, error)) {
            std::cout << session.did() << "\n";
        } else {
            std::cout << "Not signed in (" << error << ")\n";
        }
        return 0;
    }
    if (command == "events") {
        keepsake::sync::watchEvents(std::cout);
        return 0;
    }
    if (command == "npcs") {
        std::string path = keepsake::util::socialNpcsOptInFilePath();
        if (argc > 2 && std::string(argv[2]) == "on") {
            std::ofstream(path) << "1\n";
            std::cout
                << "On. Signed-in play will show accounts you follow as "
                   "in-world mentions (public handle and display name only "
                   "— nothing private, no DMs).\n";
        } else if (argc > 2 && std::string(argv[2]) == "off") {
            std::remove(path.c_str());
            std::cout << "Off.\n";
        } else {
            std::cerr << "Usage: keepsake npcs on|off\n";
            return 1;
        }
        return 0;
    }
#endif
    if (command == "version" || command == "--version" || command == "-V") {
        std::cout << "keepsake " << KEEPSAKE_VERSION << " (build "
                  << KEEPSAKE_BUILD_ID << ")\n";
        return 0;
    }
    if (command == "help" || command == "-h" || command == "--help") {
        printUsage();
        return 0;
    }

    // A global modifier rather than a separate flag parser: the commands are
    // few and every one of them takes at most one path argument.
    const bool forceOffline =
        hasArg(argc, argv, "--offline") || command == "offline";

    if (command == "export") {
        if (argc < 3 || std::string(argv[2]).rfind("--", 0) == 0) {
            std::cerr << "Usage: keepsake export <path> [--offline]\n";
            return 1;
        }
        std::string signedInDid;
        auto store = openRecordStore(forceOffline, signedInDid);

        keepsake::save::SaveData data;
        const keepsake::save::LoadStatus status = store->load(data);
        if (status == keepsake::save::LoadStatus::NewerSchema) {
            printNewerSave(data.schemaVersion);
            return 1;
        }
        if (status != keepsake::save::LoadStatus::Loaded) {
            std::cerr << "No character to export here. Play a session first, "
                         "or pass --offline to export the local save.\n";
            return 1;
        }

        keepsake::save::Bundle bundle;
        bundle.exportedFrom = keepsake::identity::hash();
        bundle.exportedAt = keepsake::util::isoNow();
        bundle.save = data;
        if (!keepsake::save::writeBundle(argv[2], bundle)) {
            std::cerr << "Could not write the bundle to " << argv[2] << ".\n";
            return 1;
        }
        std::cout << "Exported " << data.character.className << " level "
                  << data.character.level << " (save schema "
                  << data.schemaVersion << ") to " << argv[2]
                  << ". The character's worldSeed stays pinned on import.\n";
        return 0;
    }

    if (command == "import") {
        if (argc < 3 || std::string(argv[2]).rfind("--", 0) == 0) {
            std::cerr << "Usage: keepsake import <path> [--force] "
                         "[--offline]\n";
            return 1;
        }
        keepsake::save::Bundle bundle;
        const keepsake::save::BundleStatus bundleStatus =
            keepsake::save::readBundle(argv[2], bundle);
        if (bundleStatus == keepsake::save::BundleStatus::NewerBundle) {
            std::cerr << "This bundle was written by a newer Keepsake. "
                         "Update Keepsake to import it; the file has not been "
                         "changed.\n";
            return 1;
        }
        if (bundleStatus == keepsake::save::BundleStatus::NewerSchema) {
            std::cerr << "This bundle carries a character written by a newer "
                         "Keepsake. Update Keepsake instead of downgrading "
                         "the character.\n";
            return 1;
        }
        if (bundleStatus != keepsake::save::BundleStatus::Loaded) {
            std::cerr << "Could not read a Keepsake bundle from " << argv[2]
                      << ".\n";
            return 1;
        }

        std::string signedInDid;
        auto store = openRecordStore(forceOffline, signedInDid);
        keepsake::save::SaveData existing;
        const keepsake::save::LoadStatus current = store->load(existing);
        if (current == keepsake::save::LoadStatus::NewerSchema) {
            printNewerSave(existing.schemaVersion);
            return 1;
        }
        // Cross-device conflict rules arrive with #8. Until then import is an
        // explicit, last-write-wins replacement and must be asked for.
        if (current == keepsake::save::LoadStatus::Loaded &&
            !hasArg(argc, argv, "--force")) {
            std::cerr << "This account already has a character. Pass --force "
                         "to replace it with the bundle.\n";
            return 1;
        }
        if (!store->save(bundle.save)) {
            std::cerr << "Could not write the imported character to this "
                         "account.\n";
            return 1;
        }

        std::cout << "Imported " << bundle.save.character.className
                  << " level " << bundle.save.character.level << " (save "
                  << "schema " << bundle.save.schemaVersion << ")";
        if (!bundle.exportedFrom.empty()) {
            std::cout << " from " << bundle.exportedFrom;
        }
        std::cout << ". The worldSeed and its seeded world stay pinned to the "
                     "character; gate marks, practiced echoes and "
                     "followed-account mentions follow this identity.\n";
        return 0;
    }

    if (command == "play" || command == "offline") command.clear();
    if (!command.empty()) {
        std::cerr << "Unknown command: " << command << "\n\n";
        printUsage();
        return 1;
    }

    std::string signedInDid;
    std::unique_ptr<keepsake::sync::RecordStore> store =
        openRecordStore(forceOffline, signedInDid);

    // identity::hash() already prefers the signed-in DID (it reads the
    // same session file directly — see identity.cpp) so this is correct
    // whichever backend ended up selected above.
    const std::string idHash = keepsake::identity::hash();

    keepsake::save::SaveData data;
    const keepsake::save::LoadStatus loadStatus = store->load(data);
    if (loadStatus == keepsake::save::LoadStatus::NewerSchema) {
        std::cerr << "This character was written by a newer Keepsake (save "
                     "schema "
                  << data.schemaVersion << "; this build understands "
                  << keepsake::save::kCurrentSchemaVersion
                  << "). Update Keepsake to play it; the save has not been "
                     "changed.\n";
        return 1;
    }
    const bool isNewCharacter =
        loadStatus != keepsake::save::LoadStatus::Loaded;
    if (isNewCharacter) {
        data.character = keepsake::entity::Character{};
        data.character.worldSeed = idHash;
        data.character.createdAt = keepsake::util::isoNow();
        data.progress = keepsake::quest::Progress{};
    }

    keepsake::world::World world = keepsake::world::World::createDefault();
    // World-generation variance (design roadmap §3): the seeded caches and
    // flourishes apply to local play too — the worldSeed is the local
    // identity hash there — not just to signed-in accounts.
    world.applyWorldSeed(data.character.worldSeed);
    world.reconcile(data.progress);

#if defined(KEEPSAKE_WITH_WOLFRAM)
    // Identity-seeded flavor (design roadmap §3): a brand-new DID sees a
    // freshly-forced gate; an established one sees the same worn, rusted
    // gate the local-only game always described. Best-effort — a lookup
    // failure (offline, actor not found) just keeps the default text.
    if (isNewCharacter && !signedInDid.empty()) {
        std::string createdAtIso;
        if (keepsake::oauth::fetchAccountCreatedAt(signedInDid, createdAtIso) &&
            createdAtIso.size() >= 4) {
            std::string accountYear = createdAtIso.substr(0, 4);
            std::string currentYear = keepsake::util::isoNow().substr(0, 4);
            if (accountYear == currentYear) {
                if (auto *gatehouse = world.find("gatehouse")) {
                    gatehouse->description =
                        "The gate hangs open on one hinge — freshly forced, "
                        "by the look of the splintered wood. Whatever came "
                        "through here didn't wait around.";
                }
            }
        }
    }

    // Opt-in social-graph NPCs (design roadmap §4): mentions only, not
    // interactive — see world::Location::flavorNpcNames and AGENTS.md.
    // Re-checked every run (not just on character creation), so toggling
    // `keepsake npcs on/off` takes effect on the next play session.
    if (!signedInDid.empty()) {
        std::ifstream optIn(keepsake::util::socialNpcsOptInFilePath());
        if (optIn) {
            std::vector<keepsake::oauth::FollowedActor> follows;
            if (keepsake::oauth::fetchFollows(signedInDid, 5, follows) &&
                !follows.empty()) {
                if (auto *courtyard = world.find("courtyard")) {
                    for (const auto &actor : follows) {
                        courtyard->flavorNpcNames.push_back("@" + actor.handle);
                    }
                }
            }
        }
    }
#endif

    // The premise's identity hook: the open letter (see the gatehouse and
    // entity::Character's note.open_letter) tells the player a line of
    // characters is cut over the gate that belongs to them alone. This is
    // that line — identity::hash() is the same value that anchors the save
    // file (a DID-derived hash signed in, the local stand-in otherwise),
    // so the mark on the gate is, quite literally, the name of the save.
    // Applied here — after the flavor block above, which reassigns rather
    // than appends and would otherwise write over the mark for brand-new
    // accounts — and deliberately outside every KEEPSAKE_WITH_WOLFRAM
    // block: every build, signed in or not, arrives to find the gate
    // already knows it. Every run, new character or loaded — the mark was
    // waiting here before the player was.
    if (auto *gatehouse = world.find("gatehouse")) {
        gatehouse->description +=
            " And over the gate, cut cleanly into the lintel as if by "
            "someone who knew you were coming, is a line of characters "
            "you have never seen before and have always known:\n  " +
            idHash;
    }

    // The premise's other shoe (see applyMarkEchoes): the thing below has
    // been learning the mark too — its practice is cut into the walls,
    // wrong at first and almost right by the abyss. Deliberately beside
    // the lintel block and outside every KEEPSAKE_WITH_WOLFRAM guard for
    // the same reason: every build, signed in or not, finds the same
    // copies of its own save's name on the way down.
    world.applyMarkEchoes(idHash);

    // And once the thing below is done with (either branch of the
    // last quest sets the same flag), the practice is finished
    // properly — see World::resolveMarkEchoes.
    if (keepsake::quest::hasFlag(data.progress,
                                 "quest.the_nameless.complete")) {
        world.resolveMarkEchoes(idHash);
    }

#if defined(KEEPSAKE_WITH_WOLFRAM)
    // Live world events (design roadmap §4): only for signed-in players —
    // signed-out play stays fully offline with zero network activity, and
    // this is the same "shared world" opt-in signing in already implies
    // (see the npcs/identity-flavor blocks above). EventBridge's
    // background thread only ever feeds ui::run text to print; see
    // ui/terminal.hpp's RemoteEventPoll comment for why that keeps this
    // safe without touching World/Progress from another thread.
    keepsake::sync::EventBridge eventBridge;
    keepsake::ui::RemoteEventPoll pollRemoteEvents;
    if (!signedInDid.empty()) {
        eventBridge.start();
        pollRemoteEvents = [&eventBridge]() {
            std::vector<std::string> lines;
            for (auto &remote : eventBridge.drain()) {
                lines.push_back(keepsake::sync::formatRemoteEvent(remote));
            }
            return lines;
        };
    }

    keepsake::ui::run(world, data.character, data.progress, *store, std::cin,
                      std::cout, pollRemoteEvents);

    eventBridge.stop();
#else
    keepsake::ui::run(world, data.character, data.progress, *store, std::cin,
                      std::cout);
#endif

    return 0;
}
