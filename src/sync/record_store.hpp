#ifndef KEEPSAKE_SYNC_RECORD_STORE_HPP
#define KEEPSAKE_SYNC_RECORD_STORE_HPP

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
};

} // namespace keepsake::sync

#endif
