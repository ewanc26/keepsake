#ifndef KEEPSAKE_SYNC_RECORD_STORE_HPP
#define KEEPSAKE_SYNC_RECORD_STORE_HPP

#include <string>

#include "save/save.hpp"

namespace keepsake::sync {

// The seam Phase 2 plugs a wolfram-backed implementation into. UI code
// talks to a RecordStore&, never to a save path or a PDS directly, so
// swapping backends is contained to this interface and whatever
// constructs it in main.cpp.
class RecordStore {
  public:
    virtual ~RecordStore() = default;

    // Returns false if there is nothing to load yet (a brand-new
    // identity), not on error — callers should treat that as "start a new
    // character", not as a fault.
    virtual bool load(save::SaveData &out) = 0;
    virtual bool save(const save::SaveData &data) = 0;

    // Broadcasts a verifiable, portable milestone (click.croft.rpg.achievement)
    // or a world-altering action (click.croft.rpg.event) other players'
    // clients could someday fold into their own world state. Best-effort,
    // fire-and-forget by design — a missed broadcast never blocks or
    // corrupts the character/progress save, so neither returns a status.
    // Default is a no-op: LocalRecordStore has no PDS to write to; only a
    // signed-in backend can actually do this.
    virtual void recordAchievement(const std::string &id,
                                   const std::string &name) {
        (void)id;
        (void)name;
    }
    virtual void recordEvent(const std::string &kind,
                             const std::string &locationId,
                             const std::string &detail) {
        (void)kind;
        (void)locationId;
        (void)detail;
    }
};

} // namespace keepsake::sync

#endif
