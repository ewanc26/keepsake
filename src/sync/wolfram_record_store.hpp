#ifndef KEEPSAKE_SYNC_WOLFRAM_RECORD_STORE_HPP
#define KEEPSAKE_SYNC_WOLFRAM_RECORD_STORE_HPP

#include <memory>
#include <string>

#include "oauth/oauth_flow.hpp"
#include "sync/record_store.hpp"

namespace keepsake::sync {

// RecordStore backed by click.croft.rpg.character/.progress records in the
// signed-in player's own PDS repo. Reads/writes go through generic
// com.atproto.repo.getRecord/putRecord calls with hand-built JSON bodies
// (save::Json) rather than lexgen-generated typed wrappers — see AGENTS.md
// for why that's the deliberate choice here, not an oversight.
class WolframRecordStore : public RecordStore {
  public:
    // Attempts to restore a previously persisted session (resolving the
    // signed-in DID's current PDS endpoint fresh — see
    // oauth::restoreSession). Returns nullptr and sets `error` on any
    // failure to do so — including "no session file yet", the ordinary
    // state before the player has ever run `keepsake login`, which callers
    // should treat as "fall back to LocalRecordStore", not as a fault.
    static std::unique_ptr<WolframRecordStore> open(std::string &error);

    bool load(save::SaveData &out) override;
    bool save(const save::SaveData &data) override;
    void recordAchievement(const std::string &id,
                           const std::string &name) override;
    void recordEvent(const std::string &kind, const std::string &locationId,
                     const std::string &detail) override;

    const std::string &did() const {
        return session_.did();
    }

  private:
    WolframRecordStore() = default;

    oauth::AuthSession session_;
};

} // namespace keepsake::sync

#endif
