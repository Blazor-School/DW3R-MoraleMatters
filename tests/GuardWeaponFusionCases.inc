// Additional cases in the executable-mapping harness. The game's base/source
// list builders, setup, selection, undo, commit and consumption are exercised;
// engine widget/delegate and TMap services are controlled fixtures.
static uint8_t GuardNativeCommit[24], GuardNativeConsume[24], GuardNativeRender[24];
alignas(16) static uint8_t GuardSave[0x3C0], GuardTeam[0x58], GuardSelector[0x488], GuardList[0x480], GuardManager[0x520];
static FusionWeapon GuardWeapons[6], OfficerWeapons[2], UniqueWeapons[1];
static WeaponFusionTile GuardSkills[6][9], OfficerSkills[2][9], UniqueSkills[9];
static int EquippedGuard[10];
static uint8_t GuardMetadataRows[10][0x30], OfficerCharacter[0x40];
struct GuardMapEntry { int key, padding; FusionWeapon value; };
static GuardMapEntry GuardMap[16];
static int GuardMapCount, GuardListed[16], GuardListedCount, GuardDetailCalls, HiddenRows;
static WeaponFusionTile GuardDetailFirst;
static int GuardRenderedEffect, GuardRenderedValue;
static bool GuardInitScoped;
static void GuardReinit(void* widget, uint8_t character, void*)
{
    GuardInitScoped = GuardFusionInventoryContext && At<void*>(GuardSave, 0x48) != OfficerWeapons;
    At<int>(widget, 0x408) = 0;
    At<uint8_t>(widget, 0x40C) = character;
    FusionBase(widget);
}
static void* GuardTextEmpty(void* output) { memset(output, 0, 16); return output; }
static void* GuardEffectText(void* output, void* effect) { GuardRenderedEffect = At<uint8_t>(effect, 0); return GuardTextEmpty(output); }
static void* GuardTextAssign(void* output, void*) { return GuardTextEmpty(output); }
static void* GuardNumberString(void* output, int value) { GuardTextEmpty(output); At<int>(output, 8) = value; return output; }
static void* GuardStringText(void* output, void* input) { memcpy(output, input, 16); return output; }
static void GuardSetText(void*, void* text) { GuardRenderedValue = At<int>(text, 8); }

static FusionWeapon GuardClone(FusionWeapon weapon)
{
    void* copy = malloc(weapon.skills.count * sizeof(WeaponFusionTile));
    if (weapon.skills.count) memcpy(copy, weapon.skills.data, weapon.skills.count * sizeof(WeaponFusionTile));
    weapon.skills = {copy, weapon.skills.count, weapon.skills.count};
    return weapon;
}
static void GuardMapClear(void*)
{
    for (int i = 0; i < GuardMapCount; ++i) free(GuardMap[i].value.skills.data);
    GuardMapCount = 0;
}
static void* GuardMapInsert(void*, int* output, void* input)
{
    const int index = GuardMapCount++;
    GuardMap[index] = {*At<int*>(input, 0), 0, GuardClone(*At<FusionWeapon*>(input, 8))};
    *output = index;
    return output;
}
static void* GuardMapValues(void*, void* output)
{
    auto* values = static_cast<GuardMapEntry*>(calloc(GuardMapCount, sizeof(GuardMapEntry)));
    for (int i = 0; i < GuardMapCount; ++i) values[i] = {GuardMap[i].key, 0, GuardClone(GuardMap[i].value)};
    At<FusionArray>(output, 0) = {values, GuardMapCount, GuardMapCount};
    return output;
}
static void GuardListClear(void*) { GuardListedCount = 0; }
static bool GuardListAdd(void*, void* weapon, int id, bool)
{
    if (At<uint8_t>(weapon, 1) != 255) GuardListed[GuardListedCount++] = id;
    return true;
}
static void GuardFocusFirst(void*)
{
    At<int>(GuardSelector, 0x410) = GuardListedCount ? GuardListed[0] : -1;
    At<int>(GuardSelector, 0x414) = GuardListedCount ? GuardListed[0] : -1;
}
static void GuardFocus(void*, int) {}
static void* GuardGetMetadata(void* id) { return GuardMetadataRows[At<uint8_t>(id, 0)]; }
static void* GuardGetCharacter(void*) { return OfficerCharacter; }
static void GuardDetail(void* detail, void* weapon, bool, void*)
{
    ++GuardDetailCalls;
    const auto& data = At<FusionWeapon>(weapon, 0);
    if (data.skills.count) GuardDetailFirst = static_cast<WeaponFusionTile*>(data.skills.data)[0];
    auto** rows = At<void**>(detail, 0x380);
    for (int i = 1; i <= 9; ++i)
    {
        const auto tile = i <= data.skills.count ? static_cast<WeaponFusionTile*>(data.skills.data)[i - 1] : WeaponFusionTile{100, 10, 0, 0};
        At<uint8_t>(rows[i], 0x4B8) = tile.item;
        At<int>(rows[i], 0x4C4) = tile.value;
    }
}
static void GuardRender(void* widget, uint8_t item, int value)
{
    At<uint8_t>(widget, 0x4B8) = item;
    At<int>(widget, 0x4C4) = value;
}
static void GuardVisibility(void*, uint8_t visibility) { if (visibility == 1) ++HiddenRows; }
static void* GuardDefaultWeapon(void* output)
{
    At<FusionWeapon>(output, 0) = {};
    auto& weapon = At<FusionWeapon>(output, 0);
    weapon.weapon = weapon.id = 255; weapon.dataId = -1;
    auto* tiles = static_cast<WeaponFusionTile*>(calloc(9, sizeof(WeaponFusionTile)));
    for (int i = 0; i < 9; ++i) tiles[i] = {100, 10, 0, 0};
    weapon.skills = {tiles, 9, 9};
    return output;
}
static void GuardInventorySetup()
{
    memset(GuardSave, 0, sizeof(GuardSave)); memset(GuardTeam, 0, sizeof(GuardTeam));
    memset(GuardSelector, 0, sizeof(GuardSelector)); memset(GuardList, 0, sizeof(GuardList)); memset(GuardManager, 0, sizeof(GuardManager));
    const uint8_t ids[] = {173, 174, 176, 173, 175, 255};
    for (int i = 0; i < 6; ++i)
    {
        for (int j = 0; j < 9; ++j) GuardSkills[i][j] = {100, 10, 0, 0};
        GuardSkills[i][0] = {100, 0, 0, i == 0 ? 20 : (i == 4 ? 30 : 10)};
        GuardWeapons[i] = {}; GuardWeapons[i].weapon = GuardWeapons[i].id = ids[i]; GuardWeapons[i].dataId = i;
        GuardWeapons[i].skills = {GuardSkills[i], 9, 9};
    }
    GuardWeapons[5].skills = {reinterpret_cast<void*>(1), 1000, 1000}; // Rejected records are sanitized before native cloning.
    for (int i = 0; i < 2; ++i)
    {
        for (int j = 0; j < 9; ++j) OfficerSkills[i][j] = {100, 10, 0, 0};
        OfficerSkills[i][0] = {4, 10, 0, 90 + i};
        OfficerWeapons[i] = {}; OfficerWeapons[i].weapon = OfficerWeapons[i].id = static_cast<uint8_t>(8 + i); OfficerWeapons[i].dataId = i;
        OfficerWeapons[i].skills = {OfficerSkills[i], 9, 9};
    }
    UniqueWeapons[0] = {}; UniqueWeapons[0].weapon = UniqueWeapons[0].id = 92; UniqueWeapons[0].dataId = 10000;
    UniqueWeapons[0].skills = {UniqueSkills, 9, 9};
    At<FusionArray>(GuardSave, 0x48) = {OfficerWeapons, 2, 2};
    At<FusionArray>(GuardSave, 0x58) = {UniqueWeapons, 1, 1};
    At<FusionArray>(GuardSave, 0x68) = {GuardWeapons, 6, 6};
    At<int>(GuardSave, 0x30) = 1; At<int>(GuardSave, 0x378) = 10000;
    At<FusionArray>(GuardSave, 0xB8) = {GuardTeam, 1, 1};
    for (int& id : EquippedGuard) id = -1;
    EquippedGuard[0] = 3; At<FusionArray>(GuardTeam, 0x28) = {EquippedGuard, 10, 10};
    At<void*>(Singleton, 0x98) = GuardSave; At<void*>(Image, 0xA9C50E0) = Singleton;
    At<void*>(GuardManager, 0x470) = GuardSelector; At<uint8_t>(GuardManager, 0x491) = 2;
    At<void*>(GuardSelector, 0) = Vtable; At<void*>(GuardSelector, 0x350) = GuardList;
    At<uint8_t>(GuardSelector, 0x430) = 1; At<int>(GuardSelector, 0x410) = -1; At<int>(GuardSelector, 0x414) = -1;
    At<uint8_t>(GuardSelector, 0x40C) = 3;
    GuardFusionSelector = GuardFusionWidget = nullptr;
}

static int GuardFusionCases()
{
    // Restore the actual save setter/remover, then detour the setter with the
    // production guard decoder. The original officer test uses capture services.
    memcpy(Image + 0x1517360, GuardNativeCommit, sizeof(GuardNativeCommit));
    memcpy(Image + 0x15172D0, GuardNativeConsume, sizeof(GuardNativeConsume));
    memcpy(GuardNativeRender, Image + 0x169C090, sizeof(GuardNativeRender));
    Jump(0x1465900, reinterpret_cast<void*>(&GuardGetMetadata));
    Jump(0x146DE70, reinterpret_cast<void*>(&GuardGetCharacter));
    Jump(0x15D9070, reinterpret_cast<void*>(&GuardListClear)); Jump(0x15D95C0, reinterpret_cast<void*>(&Noop));
    Jump(0x15D91B0, reinterpret_cast<void*>(&GuardListAdd)); Jump(0x15D9B20, reinterpret_cast<void*>(&GuardFocusFirst));
    Jump(0x15D9F40, reinterpret_cast<void*>(&GuardFocus));
    Jump(0x169C6F0, reinterpret_cast<void*>(&GuardMapClear)); Jump(0x169D8A0, reinterpret_cast<void*>(&GuardMapInsert));
    Jump(0x169C520, reinterpret_cast<void*>(&GuardMapValues));
    Jump(0x169AB70, reinterpret_cast<void*>(&GuardDetail)); Jump(0x169C090, reinterpret_cast<void*>(&GuardRender));
    Jump(0x47EA370, reinterpret_cast<void*>(&Null)); Jump(0x3E7AB20, reinterpret_cast<void*>(&Null));
    Jump(0x140EE20, reinterpret_cast<void*>(&Noop)); Jump(0x1357660, reinterpret_cast<void*>(&GuardDefaultWeapon));
    Vtable[0x2D8 / 8] = reinterpret_cast<void*>(&Null); Vtable[0x2C0 / 8] = reinterpret_cast<void*>(&GuardVisibility);
    HookSpec hooks[] = {
        {0x16963C0, reinterpret_cast<void*>(&FusionInit), reinterpret_cast<void**>(&OriginalFusionInit)},
        {0x1696920, reinterpret_cast<void*>(&FusionBase), reinterpret_cast<void**>(&OriginalFusionBase)},
        {0x1697A10, reinterpret_cast<void*>(&FusionClick), reinterpret_cast<void**>(&OriginalFusionClick)},
        {0x169A2D0, reinterpret_cast<void*>(&FusionUndo), reinterpret_cast<void**>(&OriginalFusionUndo)},
        {0x1696EE0, reinterpret_cast<void*>(&FusionSource), reinterpret_cast<void**>(&OriginalFusionSource)},
        {0x1697770, reinterpret_cast<void*>(&FusionProgressAll), reinterpret_cast<void**>(&OriginalFusionProgressAll)},
        {0x1697360, reinterpret_cast<void*>(&FusionProgress), reinterpret_cast<void**>(&OriginalFusionProgress)},
        {0x1536630, reinterpret_cast<void*>(&FusionIds), reinterpret_cast<void**>(&OriginalFusionIds)},
        {0x1536880, reinterpret_cast<void*>(&FusionCompatible), reinterpret_cast<void**>(&OriginalFusionCompatible)},
        {0x1463840, reinterpret_cast<void*>(&FusionMetadata), reinterpret_cast<void**>(&OriginalFusionMetadata)},
        {0x14BEEB0, reinterpret_cast<void*>(&FusionRank), reinterpret_cast<void**>(&OriginalFusionRank)},
        {0x16985E0, reinterpret_cast<void*>(&FusionSetup), reinterpret_cast<void**>(&OriginalFusionSetup)},
        {0x16990F0, reinterpret_cast<void*>(&FusionConfirm), reinterpret_cast<void**>(&OriginalFusionConfirm)},
        {0x1517360, reinterpret_cast<void*>(&FusionCommit), reinterpret_cast<void**>(&OriginalFusionCommit)}
    };
    for (auto& hook : hooks) CHECK(MH_CreateHook(Image + hook.rva, hook.replacement, hook.original) == MH_OK);
    CHECK(MH_EnableHook(MH_ALL_HOOKS) == MH_OK);
    At<uint8_t>(OfficerCharacter, 0x39) = 8;
    GuardInventorySetup(); Config = Settings();
    uint8_t saveBefore[sizeof(GuardSave)]; memcpy(saveBefore, GuardSave, sizeof(saveBefore));
    FusionWeapon officersBefore[2]; memcpy(officersBefore, OfficerWeapons, sizeof(officersBefore));
    CHECK(WeaponFusionMenu(GuardManager, false) == 1);
    CHECK(WeaponFusionMenu(GuardManager, true) == 2);
    CHECK(GuardListedCount == 5 && GuardListed[0] == 0 && GuardListed[4] == 4);
    CHECK(memcmp(saveBefore, GuardSave, sizeof(saveBefore)) == 0);
    CHECK(GuardMapCount == 6 && static_cast<WeaponFusionTile*>(GuardMap[0].value.skills.data)[0].item == 200);
    // Native source building filters to the base's weapon family and excludes
    // weapons assigned to any bodyguard team, even when officer IDs overlap.
    At<int>(GuardSelector, 0x410) = 0;
    CHECK(reinterpret_cast<FusionSourceFn>(Image + 0x1696EE0)(GuardSelector));
    CHECK(GuardListedCount == 2 && GuardListed[0] == 1 && GuardListed[1] == 4);
    At<int>(GuardSelector, 0x410) = 3; // Equipped base, still usable for compatibility.
    CHECK(reinterpret_cast<FusionSourceFn>(Image + 0x1696EE0)(GuardSelector));
    CHECK(GuardListedCount == 3 && GuardListed[0] == 0 && GuardListed[1] == 1 && GuardListed[2] == 4);
    At<int>(GuardSelector, 0x410) = 0;
    CHECK(reinterpret_cast<FusionSourceFn>(Image + 0x1696EE0)(GuardSelector));
    CHECK(memcmp(saveBefore, GuardSave, sizeof(saveBefore)) == 0);
    CHECK(reinterpret_cast<FusionSourceFn>(Image + 0x1697770)(GuardSelector));
    CHECK(reinterpret_cast<FusionProgressFn>(Image + 0x1697360)(GuardSelector, 0));
    CHECK(GuardListedCount == 2);
    // F6 only switches on the base picker; it cannot change inventory midway
    // through choosing a donor or after staging a fusion.
    At<int>(GuardSelector, 0x408) = 1;
    CHECK(WeaponFusionMenu(GuardManager, true) == 4 && GuardFusionSelector == GuardSelector);
    At<int>(GuardSelector, 0x408) = 0;
    // Localized guard effect metadata is adapted without changing native rows.
    for (int i = 0; i < 10; ++i)
    {
        GuardMetadataRows[i][9] = static_cast<uint8_t>(40 + i);
        GuardMetadataRows[i][0xA] = static_cast<uint8_t>(20 + i);
        At<int>(GuardMetadataRows[i], 0x28) = 100 + i;
        auto tile = GuardFusionEncode({100, static_cast<uint8_t>(i), 0, 20});
        void* metadata = reinterpret_cast<FusionMetadataFn>(Image + 0x1463840)(&tile);
        CHECK(metadata && At<uint8_t>(metadata, 0x15) == 40 + i && At<uint8_t>(metadata, 0x16) == 20 + i && At<int>(metadata, 0x18) == 100 + i);
        CHECK(GuardFusionDecode(tile).item == 100 && GuardFusionDecode(tile).guardItem == i);
    }
    // The real tile renderer resolves the guard effect label and emits its
    // numeric value through the same native UMG text path used for officers.
    memcpy(Image + 0x169C090, GuardNativeRender, sizeof(GuardNativeRender));
    Jump(0x1AC4B60, reinterpret_cast<void*>(&GuardTextEmpty)); Jump(0x15E4510, reinterpret_cast<void*>(&GuardEffectText));
    Jump(0x1AC8450, reinterpret_cast<void*>(&GuardTextAssign)); Jump(0x1A39BB0, reinterpret_cast<void*>(&GuardNumberString));
    Jump(0x1ADB2F0, reinterpret_cast<void*>(&GuardStringText)); Jump(0x3E3E770, reinterpret_cast<void*>(&Noop));
    uint8_t text[16] = {}; At<void*>(text, 0) = Vtable;
    Vtable[0x358 / 8] = reinterpret_cast<void*>(&GuardSetText);
    Setup(20, 10);
    At<void*>(Tiles[1][1], 0x3A0) = At<void*>(Tiles[1][1], 0x3A8) = text;
    for (int i = 0; i < 10; ++i)
    {
        reinterpret_cast<void (*)(void*, uint8_t, int)>(Image + 0x169C090)(Tiles[1][1], static_cast<uint8_t>(200 + i), 22 + i);
        CHECK(GuardRenderedEffect == 20 + i && GuardRenderedValue == 22 + i && At<int>(Tiles[1][1], 0x4C4) == 22 + i);
    }
    Jump(0x169C090, reinterpret_cast<void*>(&GuardRender));
    {
        GuardFusionInventory outer(false);
        CHECK(outer.ready && At<int>(GuardSave, 0x30) == 0 && GuardFusionInventoryContext == &outer);
        void* preview = At<void*>(GuardSave, 0x48);
        {
            GuardFusionInventory inner(true, 0);
            CHECK(inner.ready && inner.weapons[3].id == 255 && GuardFusionInventoryContext == &inner);
        }
        CHECK(At<void*>(GuardSave, 0x48) == preview && GuardFusionInventoryContext == &outer);
    }
    CHECK(memcmp(saveBefore, GuardSave, sizeof(saveBefore)) == 0 && GuardFusionInventoryContext == nullptr);
    // Exercise actual setup and tile assignment for every guard skill, all
    // three positions, higher/lower/equal values and configurable increments.
    auto setup = reinterpret_cast<FusionSetupFn>(Image + 0x16985E0);
    auto select = reinterpret_cast<WeaponFusionSelectFn>(Image + 0x1699750);
    for (int type = 0; type < 10; ++type)
        for (int slot = 1; slot <= 3; ++slot)
            for (int increment : {1, 2, 7, 1000})
                for (int sourceValue : {10, 20, 30})
                {
                    Config.weaponFusionIncrement = increment;
                    for (int j = 0; j < 3; ++j)
                    {
                        GuardSkills[0][j] = {100, static_cast<uint8_t>((type + j + 1) % 10), 0, 5};
                        GuardSkills[1][j] = {100, 10, 0, 0};
                    }
                    GuardSkills[0][slot - 1] = {100, static_cast<uint8_t>(type), 0, 20};
                    GuardSkills[1][slot - 1] = {100, static_cast<uint8_t>(type), 0, sourceValue};
                    Setup(20, 10);
                    HiddenRows = 0; setup(Widget, 0, 1, nullptr);
                    if (GuardFusionWidget != Widget || GuardDetailCalls == 0 || HiddenRows != 14) fprintf(stderr, "Guard setup: widget=%p expected=%p detail=%d hidden=%d sourceCount=%d resultCount=%d rows=%d/%d\n", GuardFusionWidget, Widget, GuardDetailCalls, HiddenRows, At<int>(Widget, 0x3B0), At<int>(Widget, 0x3E0), At<int>(Material, 0x388), At<int>(Result, 0x388));
                    CHECK(GuardFusionWidget == Widget && GuardDetailCalls > 0 && HiddenRows == 14);
                    CHECK(At<int>(Widget, 0x390) == 0 && At<int>(Widget, 0x3C0) == 1);
                    CHECK(memcmp(saveBefore, GuardSave, sizeof(saveBefore)) == 0 && GuardFusionSlotLimit == 0);
                    At<int>(Result, 0x394) = slot; At<int>(Widget, 0x400) = 1; At<int>(Widget, 0x404) = 1;
                    // Setup compacts the one occupied source tile into row 1.
                    select(Widget);
                    const int expected = sourceValue > 20 ? sourceValue : 20 + increment;
                    CHECK(At<WeaponFusionTile*>(Widget, 0x3D8)[slot - 1].value == expected);
                    CHECK(At<WeaponFusionTile*>(Widget, 0x3D8)[slot - 1].guardItem == type);
                    CHECK(GuardSkills[0][slot - 1].value == 20 && GuardSkills[1][slot - 1].value == sourceValue);
                }
    Config.weaponFusionIncrement = 2;
    // Different guard skill IDs still replace, and only a matching ID gains.
    auto a = GuardFusionEncode({100, 0, 0, 20}), b = GuardFusionEncode({100, 1, 0, 10});
    CHECK(WeaponFusionValue(a, b) == 10);
    b = GuardFusionEncode({100, 0, 0, 10});
    a.value = INT32_MAX - 1;
    CHECK(WeaponFusionValue(a, b) == INT32_MAX);
    // Real native saving and donor consumption affect only guard storage.
    GuardSkills[0][0] = {100, 0, 0, 20}; GuardSkills[1][0] = {100, 0, 0, 10};
    GuardSkills[0][1] = GuardSkills[0][2] = GuardSkills[1][1] = GuardSkills[1][2] = {100, 10, 0, 0};
    Setup(20, 10); setup(Widget, 0, 1, nullptr);
    // Enter pending selection through the actual native source-tile callback.
    At<int>(Material, 0x394) = 1;
    Jump(0x3E7AFD0, reinterpret_cast<void*>(&Null));
    reinterpret_cast<FusionActionFn>(Image + 0x1699490)(Widget);
    CHECK(At<int>(Widget, 0x404) == 1 && At<int>(Widget, 0x400) == 1);
    select(Widget);
    CHECK(At<WeaponFusionTile*>(Widget, 0x3D8)[0].value == 22);
    At<int>(Widget, 0x400) = 1; At<int>(Widget, 0x404) = 1; select(Widget);
    CHECK(At<WeaponFusionTile*>(Widget, 0x3D8)[0].value == 22); // No preview farming.
    int mapping[4] = {1, 1, -1, 0};
    At<void*>(Widget, 0x418) = mapping; At<int>(Widget, 0x420) = 1;
    At<int>(Widget, 0x428) = 1; At<int>(Widget, 0x440) = 1;
    CHECK(reinterpret_cast<bool (*)(void*)>(Image + 0x169A2D0)(Widget));
    CHECK(At<WeaponFusionTile*>(Widget, 0x3D8)[0].value == 20 && GuardSkills[1][0].value == 10);
    GuardSkills[1][0] = {100, 1, 0, 10};
    Setup(20, 10); setup(Widget, 0, 1, nullptr);
    At<int>(Result, 0x394) = 1; At<int>(Widget, 0x400) = 1; At<int>(Widget, 0x404) = 1; select(Widget);
    CHECK(At<WeaponFusionTile*>(Widget, 0x3D8)[0].item == 201 && At<WeaponFusionTile*>(Widget, 0x3D8)[0].guardItem == 1 && At<WeaponFusionTile*>(Widget, 0x3D8)[0].value == 10);
    At<void*>(Widget, 0x418) = mapping; At<int>(Widget, 0x420) = 1;
    At<int>(Widget, 0x428) = 1; At<int>(Widget, 0x440) = 1;
    CHECK(reinterpret_cast<bool (*)(void*)>(Image + 0x169A2D0)(Widget));
    CHECK(At<WeaponFusionTile*>(Widget, 0x3D8)[0].item == 200 && At<WeaponFusionTile*>(Widget, 0x3D8)[0].guardItem == 0);
    GuardSkills[1][0] = {100, 0, 0, 10};
    for (int count : {1, 2})
    {
        GuardWeapons[0].skills.count = count;
        Setup(20, 10); HiddenRows = 0; setup(Widget, 0, 1, nullptr);
        CHECK(GuardFusionSlots == count && HiddenRows == 14 + 3 - count);
    }
    GuardWeapons[0].skills.count = 9;
    // Revalidation prevents a staged operation from consuming an equipped donor.
    Setup(20, 10); setup(Widget, 0, 3, nullptr);
    reinterpret_cast<FusionActionFn>(Image + 0x16990F0)(Widget);
    CHECK(GuardWeapons[3].id == 173 && GuardSkills[0][0].value == 20 && GuardSkills[3][0].value == 10);
    // Let the actual save setter own its destination buffer, as it does in game.
    GuardWeapons[0] = GuardClone(GuardWeapons[0]); GuardWeapons[1] = GuardClone(GuardWeapons[1]);
    Setup(20, 10); setup(Widget, 0, 1, nullptr);
    At<int>(Result, 0x394) = 1; At<int>(Widget, 0x400) = 1; At<int>(Widget, 0x404) = 1; select(Widget);
    reinterpret_cast<FusionActionFn>(Image + 0x16990F0)(Widget);
    CHECK(GuardWeapons[0].skills.count == 9 && static_cast<WeaponFusionTile*>(GuardWeapons[0].skills.data)[0].value == 22);
    CHECK(static_cast<WeaponFusionTile*>(GuardWeapons[0].skills.data)[0].item == 100 && static_cast<WeaponFusionTile*>(GuardWeapons[0].skills.data)[0].guardItem == 0);
    CHECK(GuardWeapons[1].id == 255 && GuardWeapons[1].dataId == -1 && GuardFusionWidget == nullptr);
    CHECK(memcmp(saveBefore, GuardSave, sizeof(saveBefore)) == 0 && memcmp(officersBefore, OfficerWeapons, sizeof(officersBefore)) == 0);
    CHECK(OfficerSkills[0][0].value == 90 && OfficerSkills[1][0].value == 91 && EquippedGuard[0] == 3);
    // A different guard effect is saved with its own GuardEquipItemID.
    free(GuardWeapons[1].skills.data);
    GuardSkills[1][0] = {100, 1, 0, 10};
    FusionWeapon replacement = {}; replacement.weapon = replacement.id = 174; replacement.dataId = 1;
    replacement.skills = {GuardSkills[1], 9, 9}; GuardWeapons[1] = GuardClone(replacement);
    Setup(20, 10); setup(Widget, 0, 1, nullptr);
    At<int>(Result, 0x394) = 1; At<int>(Widget, 0x400) = 1; At<int>(Widget, 0x404) = 1; select(Widget);
    reinterpret_cast<FusionActionFn>(Image + 0x16990F0)(Widget);
    CHECK(static_cast<WeaponFusionTile*>(GuardWeapons[0].skills.data)[0].guardItem == 1 && static_cast<WeaponFusionTile*>(GuardWeapons[0].skills.data)[0].value == 10);
    CHECK(GuardWeapons[1].id == 255 && memcmp(saveBefore, GuardSave, sizeof(saveBefore)) == 0 && memcmp(officersBefore, OfficerWeapons, sizeof(officersBefore)) == 0);
    // Native confirmation initializes the same selector through its UI delegate.
    // That rebuild must remain scoped to guards and allow another fusion.
    const auto nativeInit = OriginalFusionInit;
    OriginalFusionInit = &GuardReinit;
    FusionInit(GuardSelector, 3, nullptr);
    CHECK(GuardInitScoped && GuardFusionSelector == GuardSelector && GuardFusionWidget == nullptr);
    CHECK(WeaponFusionMenu(GuardManager, false) == 2 && GuardListedCount == 4);
    CHECK(memcmp(saveBefore, GuardSave, sizeof(saveBefore)) == 0 && memcmp(officersBefore, OfficerWeapons, sizeof(officersBefore)) == 0);
    // Restore officer mode and reject toggles outside the fusion menu.
    CHECK(WeaponFusionMenu(GuardManager, true) == 1 && GuardFusionSelector == nullptr && At<uint8_t>(GuardSelector, 0x40C) == 3);
    CHECK(GuardListedCount == 3); // Two ordinary weapons and one unique.
    FusionInit(GuardSelector, 3, nullptr);
    CHECK(!GuardInitScoped && GuardFusionSelector == nullptr && GuardListedCount == 3);
    OriginalFusionInit = nativeInit;
    At<uint8_t>(GuardManager, 0x491) = 0;
    CHECK(WeaponFusionMenu(GuardManager, true) == 0 && GuardFusionSelector == nullptr);
    CHECK(reinterpret_cast<FusionCompatibleFn>(Image + 0x1536880)(89, 0));
    CHECK(!reinterpret_cast<FusionCompatibleFn>(Image + 0x1536880)(173, 0));
    GuardMapClear(nullptr);
    return 0;
}
