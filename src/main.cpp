#include <cstdio>
#include <iostream>
#include <memory>

#include "identity/identity.hpp"
#include "save/save.hpp"
#include "sync/local_record_store.hpp"
#include "ui/terminal.hpp"
#include "util/time.hpp"
#include "world/world.hpp"

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
#if defined(KEEPSAKE_WITH_WOLFRAM)
           "  login <handle-or-did>   Sign in with your AT Protocol handle\n"
           "  logout                  Forget the saved session\n"
           "  whoami                  Show the signed-in DID, if any\n"
           "  events                  Watch the firehose for other "
           "players' world events\n"
#endif
           "  help     Show this message\n";
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
#endif
    if (command == "help" || command == "-h" || command == "--help") {
        printUsage();
        return 0;
    }
    if (!command.empty()) {
        std::cerr << "Unknown command: " << command << "\n\n";
        printUsage();
        return 1;
    }

    std::unique_ptr<keepsake::sync::RecordStore> store;
    std::string signedInDid;
#if defined(KEEPSAKE_WITH_WOLFRAM)
    {
        std::string error;
        auto wolframStore = keepsake::sync::WolframRecordStore::open(error);
        if (wolframStore) {
            std::cout << "Signed in as " << wolframStore->did() << "\n";
            signedInDid = wolframStore->did();
            store = std::move(wolframStore);
        }
    }
#endif

    // identity::hash() already prefers the signed-in DID (it reads the
    // same session file directly — see identity.cpp) so this is correct
    // whichever backend ended up selected above.
    const std::string idHash = keepsake::identity::hash();
    if (!store) {
        store = std::make_unique<keepsake::sync::LocalRecordStore>(idHash);
    }

    keepsake::save::SaveData data;
    bool isNewCharacter = !store->load(data);
    if (isNewCharacter) {
        data.character = keepsake::entity::Character{};
        data.character.worldSeed = idHash;
        data.character.createdAt = keepsake::util::isoNow();
        data.progress = keepsake::quest::Progress{};
    }

    keepsake::world::World world = keepsake::world::World::createDefault();
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
#endif

    keepsake::ui::run(world, data.character, data.progress, *store, std::cin,
                      std::cout);

    return 0;
}
