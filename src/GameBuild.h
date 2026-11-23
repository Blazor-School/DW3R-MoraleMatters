#pragma once

#include <stdint.h>

namespace MoraleMatters
{
    enum class GameBuild { Legacy, Patch110 };
    inline GameBuild ActiveGameBuild = GameBuild::Legacy;

    inline uintptr_t BattleGroupStride()
    {
        return ActiveGameBuild == GameBuild::Patch110 ? 0x2E70 : 0x2E68;
    }

    constexpr uintptr_t Patch110Rva(uintptr_t legacy)
    {
        switch (legacy)
        {
        case 0x11F69B0: return 0x11F71D0;
        case 0x1377DF0: return 0x137E830;
        case 0x13978F0: return 0x139E300;
        case 0x1397DF6: return 0x139E806;
        case 0x1397E54: return 0x139E864;
        case 0x13994B5: return 0x139FEC5;
        case 0x13A1C80: return 0x13A86D0;
        case 0x13A27E0: return 0x13A9240;
        case 0x13A2B80: return 0x13A9600;
        case 0x13A5570: return 0x13AC010;
        case 0x13AB0C0: return 0x13B1A70;
        case 0x13AB470: return 0x13B1E20;
        case 0x13AE060: return 0x13B4A10;
        case 0x13AEF80: return 0x13B5930;
        case 0x13B11E0: return 0x13B7B90;
        case 0x13B1A30: return 0x13B8440;
        case 0x13B2D80: return 0x13B9790;
        case 0x13CE5E0: return 0x13D51E0;
        case 0x13CE640: return 0x13D5240;
        case 0x13CFE30: return 0x13D6A30;
        case 0x13CFED0: return 0x13D6AD0;
        case 0x13D0120: return 0x13D6D20;
        case 0x13D0200: return 0x13D6E00;
        case 0x13D43E0: return 0x13DAFE0;
        case 0x13D6B80: return 0x13DD780;
        case 0x13D6E60: return 0x13DDA60;
        case 0x13D77D0: return 0x13DE3D0;
        case 0x13DBC20: return 0x13E2820;
        case 0x13DD280: return 0x13E3E80;
        case 0x13E3386: return 0x13E9FA6;
        case 0x13E3BBA: return 0x13EA7DA;
        case 0x1463840: return 0x1464120;
        case 0x1465900: return 0x14661E0;
        case 0x14BEEB0: return 0x14BB450;
        case 0x15172D0: return 0x151B410;
        case 0x1517360: return 0x151B4A0;
        case 0x152D920: return 0x1533CD0;
        case 0x1534995: return 0x153AD65;
        case 0x153499D: return 0x153AD6D;
        case 0x1534B60: return 0x153AF30;
        case 0x1536630: return 0x153CA00;
        case 0x1536880: return 0x153CC50;
        case 0x1562680: return 0x156C8E0;
        case 0x1641D20: return 0x1649D40;
        case 0x1646040: return 0x164E190;
        case 0x16963C0: return 0x16A1B10;
        case 0x1696920: return 0x16A2070;
        case 0x1696EE0: return 0x16A2630;
        case 0x1697360: return 0x16A2AB0;
        case 0x1697770: return 0x16A2EE0;
        case 0x1697A10: return 0x16A3180;
        case 0x16985E0: return 0x16A3D50;
        case 0x16990F0: return 0x16A4870;
        case 0x1699750: return 0x16A4ED0;
        case 0x169A2D0: return 0x16A5990;
        case 0x1746710: return 0x175B370;
        case 0x17481D0: return 0x175D250;
        case 0x1748200: return 0x175D280;
        case 0x1749490: return 0x175E510;
        case 0x175AFC0: return 0x1770680;
        case 0x1E291F0: return 0x1E3E6D0;
        case 0x3E8EB00: return 0x3EA2EA0;
        case 0x50CA770: return 0x50DE370;
        case 0x7E1C290: return 0x7E2F290;
        case 0x9D3316C: return 0x9D56F24;
        case 0xA9C50E0: return 0xA9EB550;
        case 0xA9C50F0: return 0xA9EB560;
        case 0xA9C5250: return 0xA9EB6B0;
        case 0xA9C54F0: return 0xA9EB950;
        default: return 0;
        }
    }

    inline uintptr_t GameRva(uintptr_t legacy)
    {
        return ActiveGameBuild == GameBuild::Patch110 ? Patch110Rva(legacy) : legacy;
    }

    template <uintptr_t Legacy> inline uintptr_t GameRva()
    {
        constexpr auto current = Patch110Rva(Legacy);
        static_assert(current != 0, "Missing game patch 1.1.0 address");
        return ActiveGameBuild == GameBuild::Patch110 ? current : Legacy;
    }
}
