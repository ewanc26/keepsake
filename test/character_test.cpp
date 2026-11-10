// Unit tests for entity::Character inventory/XP logic and the item
// registry (entity/ has no test of its own before this — combat_test links
// character.cpp but only exercises it via combat). Pure logic, no save file,
// no sync/ dependency.

#include <iostream>

#include "entity/character.hpp"

namespace {

int g_failures = 0;

#define CHECK(expr)                                                            \
    do {                                                                       \
        if (!(expr)) {                                                         \
            std::cerr << "FAILED: " << #expr                                   \
                      << " at character_test.cpp:" << __LINE__ << "\n";        \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

} // namespace

int main() {
    using namespace keepsake::entity;

    // addItem/itemCount: stacking onto an existing stack, not a new one.
    {
        Character c;
        CHECK(c.inventory.empty());

        c.addItem("health_potion");
        c.addItem("health_potion");
        c.addItem("iron_sword");
        CHECK(c.itemCount("health_potion") == 2);
        CHECK(c.itemCount("iron_sword") == 1);
        CHECK(c.itemCount("rusty_dagger") == 0);
        CHECK(c.inventory.size() == 2);
    }

    // removeItem: partial, full, and insufficient-removal cases.
    {
        Character c;
        c.addItem("health_potion", 3);

        CHECK(c.removeItem("health_potion", 2));
        CHECK(c.itemCount("health_potion") == 1);

        CHECK(c.removeItem("health_potion", 1));
        CHECK(c.itemCount("health_potion") == 0);
        CHECK(c.inventory.empty());

        // Removing more than held fails and leaves the inventory alone —
        // `use` relies on this ("not enough" beats applying anyway).
        c.addItem("iron_sword");
        CHECK(!c.removeItem("iron_sword", 2));
        CHECK(c.itemCount("iron_sword") == 1);
        CHECK(!c.removeItem("rusty_dagger"));
    }

    // gainXp: flat level*20 curve, multi-level-up in one award, and the
    // +5 maxHp / +1 attack / +1 defense / full heal per level.
    {
        Character c; // level 1, xp 0, maxHp 20, attack 4, defense 1
        c.gainXp(19);
        CHECK(c.level == 1);
        CHECK(c.xp == 19);

        c.gainXp(1); // exactly to 20 -> level 2
        CHECK(c.level == 2);
        CHECK(c.xp == 0);
        CHECK(c.maxHp == 25);
        CHECK(c.hp == 25);
        CHECK(c.attack == 5);
        CHECK(c.defense == 2);

        // 100 XP at level 2: 40 to level 3, 60 more to level 4 — a single
        // award crossing two thresholds.
        c.hp = 3;
        c.gainXp(100);
        CHECK(c.level == 4);
        CHECK(c.xp == 0);
        CHECK(c.maxHp == 35);
        CHECK(c.hp == 35); // full heal on leveling up
        CHECK(c.attack == 7);
        CHECK(c.defense == 4);
    }

    // Item registry: every world/registry id resolves, unknowns don't, and
    // the one-shot upgrade / consumable / quest-item trichotomy holds.
    {
        CHECK(!allItemDefs().empty());
        for (const ItemDef &def : allItemDefs()) {
            const ItemDef *found = findItemDef(def.id);
            CHECK(found != nullptr);
            if (found != nullptr) CHECK(found->name == def.name);
        }
        CHECK(findItemDef("no_such_item") == nullptr);

        const ItemDef *potion = findItemDef("health_potion");
        CHECK(potion != nullptr);
        if (potion != nullptr) {
            CHECK(potion->consumable);
            CHECK(potion->healAmount == 15);
            CHECK(potion->attackBonus == 0);
        }

        const ItemDef *sword = findItemDef("iron_sword");
        CHECK(sword != nullptr);
        if (sword != nullptr) {
            CHECK(!sword->consumable);
            CHECK(sword->attackBonus == 3);
        }

        const ItemDef *locket = findItemDef("silver_locket");
        CHECK(locket != nullptr);
        if (locket != nullptr) {
            // A quest item: neither consumable nor an upgrade nor a note —
            // `use` is a no-op.
            CHECK(!locket->consumable);
            CHECK(locket->healAmount == 0);
            CHECK(locket->attackBonus == 0);
            CHECK(locket->defenseBonus == 0);
            CHECK(!locket->readable);
        }

        // Readable lore notes: every seeded note id resolves, and all of
        // them are readable with no combat or healing effect — pure text
        // the seed decides whether your world contains (see world.cpp's
        // cacheSpots, which references these ids by string). The open
        // letter is the exception: it is premise content, placed in the
        // gatehouse by createDefault() in every world (premise must never
        // be seed-gated storytelling).
        for (const char *noteId :
             {"note.open_letter", "note.watch_log", "note.crypt_ledger",
              "note.abyssal_inscription"}) {
            const ItemDef *note = findItemDef(noteId);
            CHECK(note != nullptr);
            if (note != nullptr) {
                CHECK(note->readable);
                CHECK(!note->consumable);
                CHECK(note->healAmount == 0);
                CHECK(note->attackBonus == 0);
                CHECK(note->defenseBonus == 0);
                CHECK(!note->description.empty());
                CHECK(!note->name.empty());
            }
        }

        // The premise letter specifically: it names the mystery (the
        // locket that remembers names), the stakes (the thing below lost
        // its own name and wants yours), and the identity hook (the mark
        // over the gate) — the first minute's hook, in item form.
        const ItemDef *letter = findItemDef("note.open_letter");
        CHECK(letter != nullptr);
        if (letter != nullptr) {
            const std::string text = letter->description;
            CHECK(text.find("remembers names") != std::string::npos);
            CHECK(text.find("over the gate") != std::string::npos);
            CHECK(text.find("begin politely") != std::string::npos);
        }
    }

    std::cout << "character_test " << (g_failures ? "FAILED" : "OK") << "\n";
    return g_failures ? 1 : 0;
}
