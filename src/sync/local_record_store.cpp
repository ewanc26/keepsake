#include "sync/local_record_store.hpp"

namespace keepsake::sync {

LocalRecordStore::LocalRecordStore(std::string identityHash)
    : path_(save::savePathFor(identityHash)) {}

bool LocalRecordStore::load(save::SaveData &out) {
    return save::readSave(path_, out);
}

bool LocalRecordStore::save(const save::SaveData &data) {
    return save::writeSave(path_, data);
}

} // namespace keepsake::sync
