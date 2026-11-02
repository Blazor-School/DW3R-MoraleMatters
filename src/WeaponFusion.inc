// Fusion keeps a UI-local copy of the material weapon at +0x3A0 and a separate
// result at +0x3D0. The native selection callback updates the result widget,
// skill array and undo map together; confirmation later commits that result.
struct WeaponFusionTile
{
    uint8_t item;
    uint8_t guardItem;
    uint16_t padding;
    int32_t value;
};
static_assert(sizeof(WeaponFusionTile) == 8, "Native FEquipItemSaveData layout");
#include "GuardWeaponFusion.inc"
using WeaponFusionSelectFn = void (*)(void*);
static WeaponFusionSelectFn OriginalWeaponFusionSelect = nullptr;

static int32_t WeaponFusionValue(const WeaponFusionTile& destination, const WeaponFusionTile& source)
{
    // IDs 0-12 are numeric equipment skills. ID 100 denotes an empty slot;
    // rare skills (13+) and the element row do not have numeric upgrade levels.
    const bool guard = source.item >= 200 && source.item < 210 && source.guardItem == source.item - 200 && destination.guardItem == source.guardItem;
    if ((!guard && source.item > 12) || source.item != destination.item || source.value < 0 || destination.value < 0 || source.value > destination.value)
        return source.value;
    const int64_t value = static_cast<int64_t>(destination.value) + Config.weaponFusionIncrement;
    return static_cast<int32_t>(value > INT32_MAX ? INT32_MAX : value);
}

static void WeaponFusionSelect(void* widget)
{
    // Guard tiles use the same native selection/undo callback, with their own
    // three-slot limit rather than the officer weapon's rank-table capacity.
    struct SlotLimitScope
    {
        int previous = GuardFusionSlotLimit;
        void* widget;
        SlotLimitScope(void* value) : widget(value) { if (value == GuardFusionWidget) GuardFusionSlotLimit = GuardFusionSlots; }
        ~SlotLimitScope() { SyncGuardFusionPreview(widget); GuardFusionSlotLimit = previous; }
    } limit(widget);
    if (!Config.enabled || !Config.weaponFusion || !Readable(widget, 0x490) || At<int>(widget, 0x404) != 1)
    {
        OriginalWeaponFusionSelect(widget);
        return;
    }
    void* materialWidget = At<void*>(widget, 0x350);
    void* resultWidget = At<void*>(widget, 0x358);
    const int sourceSlot = At<int>(widget, 0x400);
    const int sourceCount = At<int>(widget, 0x3B0);
    const int destinationCount = At<int>(widget, 0x488);
    if (!Readable(materialWidget, 0x398) || !Readable(resultWidget, 0x398) || sourceCount < 1 || sourceCount > 9 || destinationCount < 1 || destinationCount > 9 || sourceSlot < 1 || sourceSlot > sourceCount)
    {
        OriginalWeaponFusionSelect(widget);
        return;
    }
    const int sourceIndex = sourceSlot - 1;
    const int destinationSlot = At<int>(resultWidget, 0x394);
    auto* source = At<WeaponFusionTile*>(widget, 0x3A8);
    // Use the original destination snapshot, so changing a pending selection
    // cannot award multiple increments before a material is consumed.
    auto* destination = At<WeaponFusionTile*>(widget, 0x480);
    if (destinationSlot < 1 || destinationSlot > destinationCount || !Readable(source, sourceCount * sizeof(WeaponFusionTile)) || !Readable(destination, destinationCount * sizeof(WeaponFusionTile)))
    {
        OriginalWeaponFusionSelect(widget);
        return;
    }
    const int destinationIndex = destinationSlot - 1;
    const int32_t value = WeaponFusionValue(destination[destinationIndex], source[sourceIndex]);
    if (value == source[sourceIndex].value)
    {
        OriginalWeaponFusionSelect(widget);
        return;
    }
    // Redirect only the material preview's array during this synchronous native
    // callback. Neither the inventory weapon nor its original tile is modified.
    // Native undo, duplicate-skill validation and final save/consumption stay intact.
    WeaponFusionTile preview[9];
    memcpy(preview, source, sourceCount * sizeof(WeaponFusionTile));
    preview[sourceIndex].value = value;
    At<WeaponFusionTile*>(widget, 0x3A8) = preview;
    OriginalWeaponFusionSelect(widget);
    At<WeaponFusionTile*>(widget, 0x3A8) = source;
}
