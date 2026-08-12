#include <iostream>

#include "identity/identity.hpp"
#include "save/save.hpp"
#include "sync/local_record_store.hpp"
#include "ui/terminal.hpp"
#include "world/world.hpp"

int main() {
    const std::string idHash = keepsake::identity::hash();

    keepsake::sync::LocalRecordStore store(idHash);

    keepsake::save::SaveData data;
    if (!store.load(data)) {
        data.character = keepsake::entity::Character{};
        data.character.worldSeed = idHash;
        data.progress = keepsake::quest::Progress{};
    }

    keepsake::world::World world = keepsake::world::World::createDefault();
    world.reconcile(data.progress);

    keepsake::ui::run(world, data.character, data.progress, store, std::cin,
                      std::cout);

    return 0;
}
