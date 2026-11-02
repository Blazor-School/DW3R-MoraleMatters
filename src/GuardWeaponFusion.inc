// Bodyguard weapons live in GameStatusSaveGame.GuardWeaponDataArray (+0x68),
// use GuardEquipItemID, and have three skills. The officer fusion UI has its
// own deep-copied weapon/undo data. Adapt only its synchronous UI callbacks;
// never move bodyguard weapons into the officer inventory.
struct FusionArray { void* data; int count; int capacity; };
struct FusionWeapon
{
    uint8_t weapon, id;
    uint8_t padding[6];
    FusionArray skills;
    int64_t attribute;
    int dataId;
    int padding2;
    double time;
};
static_assert(sizeof(FusionWeapon) == 0x30, "Native FWeaponSaveData layout");
static_assert(sizeof(FusionArray) == 0x10, "Native TArray layout");
static void* GuardFusionSelector = nullptr;
static void* GuardFusionWidget = nullptr;
static int GuardFusionOfficer = 0;
static int GuardFusionSlots = 3;
static thread_local int GuardFusionSlotLimit = 0;
using FusionSetupFn = void (*)(void*, int, int, void*);
using FusionInitFn = void (*)(void*, uint8_t, void*);
using FusionActionFn = void (*)(void*);
using FusionSourceFn = bool (*)(void*);
using FusionProgressFn = bool (*)(void*, uint8_t);
using FusionIdsFn = void* (*)(void*, uint8_t);
using FusionCompatibleFn = bool (*)(uint8_t, uint8_t);
using FusionMetadataFn = void* (*)(void*);
using FusionCommitFn = void (*)(void*, int, void*);
static FusionInitFn OriginalFusionInit = nullptr;
static FusionActionFn OriginalFusionBase = nullptr;
static FusionSourceFn OriginalFusionSource = nullptr;
static FusionSourceFn OriginalFusionProgressAll = nullptr;
static FusionProgressFn OriginalFusionProgress = nullptr;
static FusionIdsFn OriginalFusionIds = nullptr;
static FusionCompatibleFn OriginalFusionCompatible = nullptr;
static FusionMetadataFn OriginalFusionMetadata = nullptr;
static FusionMetadataFn OriginalFusionRank = nullptr;
static FusionSetupFn OriginalFusionSetup = nullptr;
static FusionActionFn OriginalFusionConfirm = nullptr;
static FusionActionFn OriginalFusionClick = nullptr;
static FusionSourceFn OriginalFusionUndo = nullptr;
static FusionCommitFn OriginalFusionCommit = nullptr;

static bool FusionArrayReadable(const FusionArray& array, size_t size, int maximum)
{
    return array.count >= 0 && array.count <= maximum && array.capacity >= array.count && (!array.count || Readable(array.data, array.count * size));
}

static bool GuardWeaponEquipped(void* save, int id)
{
    const auto& teams = At<FusionArray>(save, 0xB8);
    if (!FusionArrayReadable(teams, 0x58, 8)) return true;
    for (int i = 0; i < teams.count; ++i)
    {
        auto* team = static_cast<uint8_t*>(teams.data) + i * 0x58;
        if (At<uint8_t>(team, 0x54)) continue;
        const auto& weapons = At<FusionArray>(team, 0x28);
        if (!FusionArrayReadable(weapons, sizeof(int), 10)) return true;
        for (int j = 0; j < weapons.count; ++j)
            if (static_cast<int*>(weapons.data)[j] == id) return true;
    }
    return false;
}

static WeaponFusionTile GuardFusionEncode(WeaponFusionTile tile)
{
    if (tile.guardItem < 10) tile.item = static_cast<uint8_t>(200 + tile.guardItem);
    else tile.item = 100;
    return tile;
}

static WeaponFusionTile GuardFusionDecode(WeaponFusionTile tile)
{
    tile.guardItem = tile.item >= 200 && tile.item < 210 ? static_cast<uint8_t>(tile.item - 200) : 10;
    tile.item = 100;
    return tile;
}

class GuardFusionInventory;
static thread_local GuardFusionInventory* GuardFusionInventoryContext = nullptr;
class GuardFusionInventory
{
    void* save = nullptr;
    void* allocation = nullptr;
    FusionArray officerWeapons{}, uniqueWeapons{};
    int officerCount = 0, uniqueStart = 0, previousLimit = 0;
    GuardFusionInventory* previousContext = nullptr;
public:
    FusionWeapon* weapons = nullptr;
    int count = 0, baseId = -1;
    bool material = false, committing = false, ready = false;
    GuardFusionInventory(bool source, int base = -1, bool commit = false) : baseId(base), material(source), committing(commit)
    {
        void* manager = At<void*>(Image, 0xA9C50E0);
        if (!Readable(manager, 0xA0)) return;
        save = At<void*>(manager, 0x98);
        if (!Readable(save, 0x3C0)) { save = nullptr; return; }
        const auto& guards = At<FusionArray>(save, 0x68);
        if (!FusionArrayReadable(guards, sizeof(FusionWeapon), 4096)) { save = nullptr; return; }
        count = guards.count;
        if (base >= count) { save = nullptr; return; }
        weapons = static_cast<FusionWeapon*>(guards.data);
        if (!commit && count)
        {
            allocation = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, count * (sizeof(FusionWeapon) + 9 * sizeof(WeaponFusionTile)));
            if (!allocation) { save = nullptr; return; }
            weapons = static_cast<FusionWeapon*>(allocation);
            auto* tiles = reinterpret_cast<WeaponFusionTile*>(weapons + count);
            for (int i = 0; i < count; ++i)
            {
                weapons[i] = static_cast<FusionWeapon*>(guards.data)[i];
                auto& weapon = weapons[i];
                if (weapon.id == 255 || weapon.dataId != i || !FusionArrayReadable(weapon.skills, sizeof(WeaponFusionTile), 9) || weapon.skills.count < 1 || (source && i != base && GuardWeaponEquipped(save, i)))
                {
                    // Native list builders clone every record before filtering.
                    // Rejected entries must therefore contain a safe empty array.
                    weapon.weapon = weapon.id = 255;
                    weapon.dataId = i;
                    weapon.skills = {};
                    weapon.attribute = 0;
                    continue;
                }
                const auto* original = static_cast<WeaponFusionTile*>(weapon.skills.data);
                for (int j = 0; j < weapon.skills.count; ++j) tiles[i * 9 + j] = GuardFusionEncode(original[j]);
                weapon.skills.data = tiles + i * 9;
                weapon.skills.capacity = weapon.skills.count;
            }
        }
        // These native callbacks run synchronously on the game thread. Restore
        // every inventory header before returning to the engine, including on
        // an early return. Skill arrays shown by the UI are native deep copies.
        officerWeapons = At<FusionArray>(save, 0x48);
        uniqueWeapons = At<FusionArray>(save, 0x58);
        officerCount = At<int>(save, 0x30);
        uniqueStart = At<int>(save, 0x378);
        At<FusionArray>(save, 0x48) = {weapons, count, count};
        At<FusionArray>(save, 0x58) = {};
        At<int>(save, 0x30) = 0; // Officer DataIDs share the guard inventory's index range.
        At<int>(save, 0x378) = INT32_MAX;
        previousLimit = GuardFusionSlotLimit;
        previousContext = GuardFusionInventoryContext;
        GuardFusionSlotLimit = base >= 0 && weapons[base].skills.count >= 1 && weapons[base].skills.count < 3 ? weapons[base].skills.count : 3;
        GuardFusionInventoryContext = this;
        ready = true;
    }
    ~GuardFusionInventory()
    {
        if (ready)
        {
            At<FusionArray>(save, 0x48) = officerWeapons;
            At<FusionArray>(save, 0x58) = uniqueWeapons;
            At<int>(save, 0x30) = officerCount;
            At<int>(save, 0x378) = uniqueStart;
            GuardFusionSlotLimit = previousLimit;
            GuardFusionInventoryContext = previousContext;
        }
        if (allocation) HeapFree(GetProcessHeap(), 0, allocation);
    }
};

static void* FusionMetadata(void* tile)
{
    const uint8_t item = At<uint8_t>(tile, 0);
    if (item < 200 || item >= 210) return OriginalFusionMetadata(tile);
    uint8_t id = static_cast<uint8_t>(item - 200);
    void* guard = reinterpret_cast<FusionMetadataFn>(Image + 0x1465900)(&id);
    if (!Readable(guard, 0x30)) return nullptr;
    static thread_local uint8_t metadata[10][0x20];
    auto* result = metadata[id];
    // Convert the native guard row to the officer row layout expected by the
    // existing tile widgets. Names, effect labels and icons stay localized.
    result[8] = At<uint8_t>(guard, 8);
    result[0x15] = At<uint8_t>(guard, 9);
    result[0x16] = At<uint8_t>(guard, 0xA);
    At<int>(result, 0xC) = At<int>(guard, 0x20);
    At<int>(result, 0x10) = At<int>(guard, 0x24);
    At<int>(result, 0x18) = At<int>(guard, 0x28);
    return result;
}

static void* FusionRank(void* rank)
{
    if (!GuardFusionSlotLimit) return OriginalFusionRank(rank);
    static thread_local uint8_t metadata[0x10];
    metadata[0xA] = static_cast<uint8_t>(GuardFusionSlotLimit);
    return metadata;
}

static void* FusionIds(void* output, uint8_t character)
{
    if (!GuardFusionInventoryContext) return OriginalFusionIds(output, character);
    uint8_t ids[256]; int count = 0;
    for (int i = 0; i < GuardFusionInventoryContext->count; ++i)
    {
        const uint8_t id = GuardFusionInventoryContext->weapons[i].id;
        if (id == 255) continue;
        bool found = false;
        for (int j = 0; j < count; ++j) if (ids[j] == id) found = true;
        if (!found) ids[count++] = id;
    }
    At<FusionArray>(output, 0) = {};
    using ReserveFn = void (*)(int, int, int, void*, int, int*);
    if (count) reinterpret_cast<ReserveFn>(Image + 0x11F69B0)(1, 1, count, output, 0, &At<int>(output, 12));
    auto& array = At<FusionArray>(output, 0);
    if (count) memcpy(array.data, ids, count);
    array.count = count;
    return output;
}

static bool FusionCompatible(uint8_t weapon, uint8_t character)
{
    auto* context = GuardFusionInventoryContext;
    if (!context) return OriginalFusionCompatible(weapon, character);
    if (!context->material || context->baseId < 0 || context->baseId >= context->count) return false;
    const uint8_t base = context->weapons[context->baseId].id;
    return weapon >= 173 && weapon < 203 && base >= 173 && base < 203 && (weapon - 173) / 3 == (base - 173) / 3;
}

static void FusionInit(void* widget, uint8_t character, void* controller)
{
    GuardFusionWidget = nullptr;
    // The game initializes the same picker again after a successful fusion.
    // Keep its inventory mode and scope that rebuild just like base selection.
    if (widget == GuardFusionSelector)
    {
        GuardFusionInventory inventory(false);
        if (inventory.ready)
        {
            GuardFusionOfficer = character;
            OriginalFusionInit(widget, character, controller);
            return;
        }
    }
    GuardFusionSelector = nullptr;
    OriginalFusionInit(widget, character, controller);
}

static void FusionBase(void* widget)
{
    if (widget != GuardFusionSelector) { OriginalFusionBase(widget); return; }
    {
        GuardFusionInventory inventory(false);
        if (inventory.ready) { OriginalFusionBase(widget); return; }
    }
    GuardFusionSelector = GuardFusionWidget = nullptr;
    OriginalFusionBase(widget);
}

static bool FusionSource(void* widget)
{
    if (widget != GuardFusionSelector) return OriginalFusionSource(widget);
    GuardFusionInventory inventory(true, At<int>(widget, 0x410));
    return inventory.ready && OriginalFusionSource(widget);
}

static void FusionClick(void* widget)
{
    if (widget != GuardFusionSelector) { OriginalFusionClick(widget); return; }
    // The base click checks whether an officer has this DataID equipped. Guard
    // indices overlap those IDs; hide only that officer lookup for this callback.
    GuardFusionInventory inventory(false);
    if (inventory.ready) OriginalFusionClick(widget);
}

static bool FusionProgressAll(void* widget)
{
    // Base/source initialization has already populated this guard list in full.
    return widget == GuardFusionSelector || OriginalFusionProgressAll(widget);
}

static bool FusionProgress(void* widget, uint8_t character)
{
    return widget == GuardFusionSelector || OriginalFusionProgress(widget, character);
}

static void SyncGuardFusionPreview(void* widget)
{
    if (widget != GuardFusionWidget || !Readable(widget, 0x3E8)) return;
    auto& skills = At<FusionArray>(widget, 0x3D8);
    if (!FusionArrayReadable(skills, sizeof(WeaponFusionTile), 9)) return;
    for (int i = 0; i < skills.count; ++i)
    {
        auto& tile = static_cast<WeaponFusionTile*>(skills.data)[i];
        if (tile.item >= 200 && tile.item < 210) tile.guardItem = static_cast<uint8_t>(tile.item - 200);
        else if (tile.item == 100) tile.guardItem = 10;
    }
}

static bool FusionUndo(void* widget)
{
    const bool result = OriginalFusionUndo(widget);
    SyncGuardFusionPreview(widget);
    return result;
}

static void HideGuardFusionRows(void* detail, int slots)
{
    if (!Readable(detail, 0x390)) return;
    const auto& rows = At<FusionArray>(detail, 0x380);
    if (!FusionArrayReadable(rows, sizeof(void*), 10)) return;
    for (int i = 0; i < rows.count; ++i)
    {
        if (i > 0 && i <= slots) continue;
        void* row = static_cast<void**>(rows.data)[i];
        if (!Readable(row, sizeof(void*))) continue;
        auto* vtable = At<void**>(row, 0);
        if (!Readable(vtable, 0x2C8)) continue;
        reinterpret_cast<void (*)(void*, uint8_t)>(vtable[0x2C0 / 8])(row, 1);
    }
    At<int>(detail, 0x394) = 1;
}

static void FusionSetup(void* widget, int destination, int source, void* controller)
{
    GuardFusionWidget = nullptr;
    if (!GuardFusionSelector) { OriginalFusionSetup(widget, destination, source, controller); return; }
    GuardFusionWidget = widget;
    At<int>(widget, 0x390) = At<int>(widget, 0x3C0) = -1;
    GuardFusionInventory inventory(false, destination);
    if (!inventory.ready || destination < 0 || source < 0 || source >= inventory.count || destination == source || inventory.weapons[destination].id == 255 || inventory.weapons[source].id == 255) return;
    GuardFusionSlots = inventory.weapons[destination].skills.count < 3 ? inventory.weapons[destination].skills.count : 3;
    OriginalFusionSetup(widget, destination, source, controller);
    HideGuardFusionRows(At<void*>(widget, 0x350), inventory.weapons[source].skills.count < 3 ? inventory.weapons[source].skills.count : 3);
    HideGuardFusionRows(At<void*>(widget, 0x358), GuardFusionSlots);
    void* material = At<void*>(widget, 0x350);
    if (controller && Readable(material, 0x390))
    {
        const auto& rows = At<FusionArray>(material, 0x380);
        if (FusionArrayReadable(rows, sizeof(void*), 10) && rows.count > 1)
            reinterpret_cast<void (*)(void*, void*)>(Image + 0x3E8EB00)(static_cast<void**>(rows.data)[1], controller);
    }
}

static void FusionCommit(void* manager, int id, void* weapon)
{
    const auto* context = GuardFusionInventoryContext;
    if (!context || !context->committing) { OriginalFusionCommit(manager, id, weapon); return; }
    if (id < 0 || id >= context->count || id != context->baseId) return;
    auto copy = At<FusionWeapon>(weapon, 0);
    WeaponFusionTile skills[9];
    if (!FusionArrayReadable(copy.skills, sizeof(WeaponFusionTile), 9)) return;
    const int count = context->weapons[id].skills.count;
    if (count < 1 || count > 9 || copy.skills.count < count) return;
    for (int i = 0; i < count; ++i) skills[i] = GuardFusionDecode(static_cast<WeaponFusionTile*>(copy.skills.data)[i]);
    copy.skills = {skills, count, count};
    OriginalFusionCommit(manager, id, &copy);
}

static void FusionConfirm(void* widget)
{
    if (widget != GuardFusionWidget) { OriginalFusionConfirm(widget); return; }
    const int destination = At<int>(widget, 0x390), source = At<int>(widget, 0x3C0);
    GuardFusionInventory inventory(false, destination, true);
    if (!inventory.ready || destination < 0 || source < 0 || source >= inventory.count || destination == source || inventory.weapons[destination].id == 255 || inventory.weapons[source].id == 255) return;
    void* save = At<void*>(At<void*>(Image, 0xA9C50E0), 0x98);
    const auto& base = inventory.weapons[destination];
    const auto& donor = inventory.weapons[source];
    const auto& preview = At<FusionArray>(widget, 0x3D8);
    if (base.dataId != destination || donor.dataId != source || !FusionArrayReadable(base.skills, sizeof(WeaponFusionTile), 9) || !FusionArrayReadable(donor.skills, sizeof(WeaponFusionTile), 9) || base.skills.count < 1 || donor.skills.count < 1 || !FusionArrayReadable(preview, sizeof(WeaponFusionTile), 9) || preview.count < base.skills.count) return;
    if (GuardWeaponEquipped(save, source)) return;
    OriginalFusionConfirm(widget);
    GuardFusionWidget = nullptr;
}

int WeaponFusionMenu(void* manager, bool toggle)
{
    if (!Config.enabled || !Config.weaponFusion || !Readable(manager, 0x520) || At<uint8_t>(manager, 0x491) != 2) return 0;
    void* selector = At<void*>(manager, 0x470);
    if (!Readable(selector, 0x488) || !At<uint8_t>(selector, 0x430)) return 0;
    if (toggle && At<int>(selector, 0x408) == 0)
    {
        if (GuardFusionSelector == selector)
        {
            GuardFusionSelector = GuardFusionWidget = nullptr;
            At<uint8_t>(selector, 0x40C) = static_cast<uint8_t>(GuardFusionOfficer);
        }
        else
        {
            GuardFusionOfficer = At<uint8_t>(selector, 0x40C);
            GuardFusionSelector = selector;
        }
        At<int>(selector, 0x410) = -1;
        At<int>(selector, 0x414) = -1;
        void* list = At<void*>(selector, 0x350);
        // Discard the old inventory's selected row before native rebuilding;
        // the native picker will focus the first item of the new inventory.
        if (Readable(list, 0x44C)) At<int>(list, 0x448) = INT32_MAX;
        FusionBase(selector);
    }
    return (GuardFusionSelector == selector ? 2 : 1) + (At<int>(selector, 0x408) == 1 ? 2 : 0);
}
