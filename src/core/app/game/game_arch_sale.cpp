// game_arch_sale.cpp (ARCH's SELL counter): trading a finished Daemon off the rack for
// Bits or for a patch on the pet being raised (part of the Game unit split,
// core/app/game.h).
//
// A Daemon is the end of a raise, and once there is nothing left for it to become a
// rack slot holding one is a trophy shelf. The counter turns a finished pet into one
// of three things:
//   BITS:     priced on its level (tunables.h's daemonSaleBits).
//   REIMAGE:  the active pet, if it is the same species, goes back to its Script with a
//              clean error log and keeps everything else it has earned. This is what a
//              duplicate is for: a second copy of the Daemon you did not want buys
//              another try at the one you did, without raising a third from an egg.
//   HOTFIX:   the active pet's care errors are cleared, whatever it is.
// Whichever it pays, the Daemon leaves a RETIRED record, the headstone a corrupted pet
// leaves too; Release (game_arch.cpp) is the only way off the rack that leaves none.
#include "core/app/game.h"

#include <cstdio>
#include <cstring>

#include "tunables.h"

namespace mal {

namespace {

bool sameId(const char* a, const char* b) { return a && b && std::strcmp(a, b) == 0; }

// Does `script`'s evolution ever reach `daemonId`, by any of the routes a Script->Daemon
// hop can take? The rows carry four of them and the Daemon pools the fifth.
bool scriptReaches(const ContentRegistry& reg, const CreatureDef& script,
                   const char* daemonId) {
    if (sameId(script.evolvesToId, daemonId) || sameId(script.evolvesToGoodId, daemonId) ||
        sameId(script.evolvesToBadId, daemonId) ||
        sameId(script.evolvesToTrojanId, daemonId) ||
        sameId(script.evolvesToTrojanBadId, daemonId))
        return true;
    for (const bool bad : {false, true})
        if (const DaemonPoolDef* pool = reg.daemonPool(script.id, bad))
            for (int i = 0; i < pool->count; ++i)
                if (sameId(pool->entries[i].daemonId, daemonId)) return true;
    return false;
}

}  // namespace

const CreatureDef* Game::reimageScript(const CreatureDef* daemon) const {
    if (!daemon || daemon->stage != Stage::Daemon) return nullptr;
    // A Daemon one family reached by a Trojan divert has a Script on another family
    // (Rootgrub's Coaxeel). Its own family's Script is preferred where both exist, and
    // the divert's otherwise: reimaging a diverted pet hands it back the line it was
    // diverted off, which re-rolls the divert along with the branch.
    const CreatureDef* other = nullptr;
    for (const CreatureDef* c : registry_.allCreatures()) {
        if (c->stage != Stage::Script || !scriptReaches(registry_, *c, daemon->id)) continue;
        if (sameId(c->line, daemon->line)) return c;
        if (!other) other = c;
    }
    return other;
}

const char* Game::archSaleBlocker(SaleOffer offer, const SaveStoredPet& sold) const {
    switch (offer) {
        case SaleOffer::Bits:
            return nullptr;
        case SaleOffer::Reimage:
            if (!pet_.creature || inEggPhase() || !sameId(pet_.creature->id, sold.id)) return "NEED TWIN ACTIVE";
            if (!reimageScript(pet_.creature)) return "NO SCRIPT FORM";
            return nullptr;
        case SaleOffer::Hotfix:
            if (!pet_.creature || inEggPhase()) return "NO ACTIVE PET";
            if (pet_.model.careMistakes() <= 0) return "ACTIVE HAS NONE";
            return nullptr;
    }
    return "";
}

bool Game::daemonOnRack() const {
    for (const SaveStoredPet& p : op_.arch.rack) {
        const CreatureDef* c = registry_.creature(p.id);
        if (c && c->stage == Stage::Daemon) return true;
    }
    return false;
}

void Game::reimageActivePet() {
    const CreatureDef* script = reimageScript(pet_.creature);
    if (!script) return;
    const char* oldLine = pet_.creature->line;
    installPet(script);
    // Back across a line (a diverted Daemon going home) is a new kit, the same re-seed
    // completeEvolution does going the other way. Within a line the pet keeps what it
    // learned, and gives up only what a Script cannot hold: the Daemon's slot, and any
    // move locked above the Script stage. The Daemon slot is unstamped too, because the
    // Daemon the retry reaches is the one that gets to type it.
    if (!sameId(oldLine, script->line)) {
        pet_.moves = MoveLoadout::startingForLine(registry_, script->line);
        for (SlotKind& k : pet_.slotKinds) k = SlotKind::Unset;
    } else {
        const int unlocked = MoveLoadout::slotsForStage(script->stage);
        for (int i = 0; i < kMaxMoveSlots; ++i) {
            if (i >= unlocked) {
                pet_.moves.unequip(i);
                pet_.slotKinds[i] = SlotKind::Unset;
                continue;
            }
            const char* id = pet_.moves.equipped(i);
            const MoveDef* m = id ? registry_.move(id) : nullptr;
            if (m && !moveUnlockedAtStage(*m, script->stage)) pet_.moves.unequip(i);
        }
    }
    stampSlotKinds();
    enforceSlotKindInvariant();
    // The clean error log is what makes the retry a retry: errors are what pick the
    // branch, and nothing else takes more than one of them away.
    pet_.model.setCareMistakes(0);
    // Whatever was steering the boundary the pet already crossed is spent with it.
    clearUsbPort();
    pet_.stageEnteredMs = nowMs_;
}

void Game::archSellStored(int storedIdx, SaleOffer offer) {
    if (storedIdx < 0 || storedIdx >= static_cast<int>(op_.arch.rack.size())) return;
    const SaveStoredPet sold = op_.arch.rack[storedIdx];
    const CreatureDef* d = registry_.creature(sold.id);
    if (!d || d->stage != Stage::Daemon || archSaleBlocker(offer, sold)) return;

    switch (offer) {
        case SaleOffer::Bits:    op_.bits += daemonSaleBits(sold.combatLevel); break;
        case SaleOffer::Reimage: reimageActivePet(); break;
        case SaleOffer::Hotfix:  pet_.model.setCareMistakes(0); break;
    }

    op_.arch.rack.erase(op_.arch.rack.begin() + storedIdx);
    SaveRecord rec;
    std::strncpy(rec.id, sold.id, kSaveIdCap - 1);
    rec.status = static_cast<uint8_t>(RecordStatus::Retired);
    rec.generation = sold.generation;
    op_.arch.records.push_back(rec);

    // A reimage changes the pet on screen, so the habitat is where to see it; the other
    // two leave the player at the (now shorter) counter for the next sale.
    nav_ = offer == SaleOffer::Reimage ? Nav::Idle : Nav::Submenu;
    dirty_ = true;
    persistSave();
}

}  // namespace mal
