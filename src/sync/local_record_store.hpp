#ifndef KEEPSAKE_SYNC_LOCAL_RECORD_STORE_HPP
#define KEEPSAKE_SYNC_LOCAL_RECORD_STORE_HPP

#include <string>

#include "sync/record_store.hpp"

namespace keepsake::sync {

// Reads and writes save::SaveData as a single JSON file at
// save::savePathFor(identityHash). The only RecordStore implementation
// that exists so far — see AGENTS.md for what a WolframRecordStore would
// need to add.
class LocalRecordStore : public RecordStore {
  public:
    explicit LocalRecordStore(std::string identityHash);

    bool load(save::SaveData &out) override;
    bool save(const save::SaveData &data) override;

    const std::string &path() const {
        return path_;
    }

  private:
    std::string path_;
};

} // namespace keepsake::sync

#endif
