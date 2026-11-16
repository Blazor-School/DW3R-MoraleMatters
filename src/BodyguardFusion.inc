using SelectEquipmentFn = bool (*)(void*, uint8_t);
static SelectEquipmentFn OriginalSelectEquipment = nullptr;
static void* BodyguardInventoryPicker = nullptr;
static constexpr uint8_t WeaponFusionEquipmentIndex = 2;
static constexpr uint8_t BodyguardFusionEquipmentIndex = 7;

static size_t EquipmentMenuOffset(size_t legacyOffset)
{
    return ActiveGameBuild == GameBuild::Patch110 ? legacyOffset + 0x10 : legacyOffset;
}

static void* EquipmentMenu(void* manager)
{
    if (!Readable(manager, 0x520)) return nullptr;
    void* common = At<void*>(manager, 0x438);
    if (!Readable(common, 0x358)) return nullptr;
    void* menu = At<void*>(common, 0x350);
    return Readable(menu, EquipmentMenuOffset(0x390) + sizeof(uint8_t)) && At<void*>(menu, EquipmentMenuOffset(0x378)) == manager ? menu : nullptr;
}

bool BodyguardFusionAvailable()
{
    if (!Config.enabled || !Config.weaponFusion) return false;
    void* manager = At<void*>(Image, GameRva<0xA9C50E0>());
    if (!Readable(manager, 0xA0)) return false;
    void* save = At<void*>(manager, 0x98);
    return Readable(save, 0x3C0) && FusionArrayReadable(At<FusionArray>(save, 0x68), sizeof(FusionWeapon), 4096);
}

bool AddBodyguardFusionRow(void* manager, void* row)
{
    void* menu = EquipmentMenu(manager);
    if (!menu || At<uint8_t>(menu, EquipmentMenuOffset(0x370)) || !Readable(row, 0x370) || !BodyguardFusionAvailable()) return false;
    auto& rows = At<FusionArray>(menu, EquipmentMenuOffset(0x380));
    if (rows.count != BodyguardFusionEquipmentIndex || rows.capacity <= rows.count || !FusionArrayReadable(rows, sizeof(void*), BodyguardFusionEquipmentIndex)) return false;
    auto** widgets = static_cast<void**>(rows.data);
    void* last = widgets[rows.count - 1];
    if (!Readable(last, 0x370) || At<void*>(row, 0) != At<void*>(last, 0)) return false;

    // Use the existing menu's OnClickedMenu binding, with independent storage
    // owned by the new row. Native focus and activation work on this row too.
    constexpr size_t delegateSize = 16;
    const auto& binding = At<FusionArray>(last, 0x360);
    auto& click = At<FusionArray>(row, 0x360);
    if (binding.count != 1 || !FusionArrayReadable(binding, delegateSize, 1) || click.data || click.count || click.capacity) return false;
    using ReserveFn = void (*)(int, int, int, void*, int, int*);
    reinterpret_cast<ReserveFn>(Image + GameRva<0x11F69B0>())(delegateSize, 8, 1, &click, 0, &click.capacity);
    memcpy(click.data, binding.data, delegateSize);
    click.count = 1;
    widgets[rows.count++] = row;
    return true;
}

static bool SelectEquipment(void* manager, uint8_t index)
{
    void* selector = At<void*>(manager, 0x470);
    void* menu = index == BodyguardFusionEquipmentIndex ? EquipmentMenu(manager) : nullptr;
    const bool bodyguards = menu && !At<uint8_t>(menu, EquipmentMenuOffset(0x370)) &&
        At<FusionArray>(menu, EquipmentMenuOffset(0x380)).count == BodyguardFusionEquipmentIndex + 1 && BodyguardFusionAvailable();
    const bool restoreOfficerList = index == WeaponFusionEquipmentIndex && selector && selector == BodyguardInventoryPicker;
    GuardFusionSelector = bodyguards ? selector : nullptr;
    GuardFusionWidget = nullptr;
    if (bodyguards)
    {
        index = WeaponFusionEquipmentIndex;
        At<uint8_t>(menu, EquipmentMenuOffset(0x390)) = index;
    }
    const bool opened = OriginalSelectEquipment(manager, index);
    if (opened && Readable(selector, 0x488) && (bodyguards || restoreOfficerList))
    {
        // A reused picker can retain the previous inventory for this officer.
        // Rebuild its map and rows before the player can select a weapon.
        RebuildFusionBase(selector);
        BodyguardInventoryPicker = GuardFusionSelector == selector ? selector : nullptr;
    }
    if (!opened) GuardFusionSelector = nullptr;
    return opened;
}
