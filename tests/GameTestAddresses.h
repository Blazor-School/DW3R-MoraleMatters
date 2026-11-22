#pragma once

#include "../src/GameBuild.h"
#include <string.h>

namespace MoraleMatters
{
    inline bool SelectTestGameBuild(uint32_t imageSize)
    {
        switch (imageSize)
        {
        case 0xB39E000:
        case 0xB3D8000:
            ActiveGameBuild = GameBuild::Legacy;
            return true;
        case 0xB3C9000:
        case 0xB403000:
            ActiveGameBuild = GameBuild::Patch110;
            return true;
        default:
            return false;
        }
    }

    inline uint32_t FixtureGroupStride = 0x2E68;

    inline bool SelectBattleFixture(int argc, char** argv)
    {
        if (argc == 1)
            return true;
        if (argc != 2 || strcmp(argv[1], "patch110"))
            return false;
        ActiveGameBuild = GameBuild::Patch110;
        FixtureGroupStride = 0x2E70;
        return true;
    }

    // Read the immediate from the game's own indexed group lookup (imul rcx, rax, imm32).
    inline uint32_t NativeTestGroupStride(const uint8_t* image)
    {
        const auto* instruction = image + (ActiveGameBuild == GameBuild::Patch110 ? 0x13C409C : 0x13BD4CC);
        if (memcmp(instruction, "\x48\x69\xC8", 3))
            return 0;
        uint32_t stride;
        memcpy(&stride, instruction + 3, sizeof(stride));
        return stride;
    }

    constexpr uintptr_t TestOnlyPatch110Rva(uintptr_t legacy)
    {
        switch (legacy)
        {
        case 0x1215C60: return 0x1216480;
        case 0x1216160: return 0x1216980;
        case 0x1270980: return 0x12711A0;
        case 0x1283830: return 0x1284050;
        case 0x1290CA0: return 0x1291A30;
        case 0x1357660: return 0x135DEA0;
        case 0x1397E81: return 0x139E891;
        case 0x1398043: return 0x139EA53;
        case 0x1398044: return 0x139EA54;
        case 0x1398048: return 0x139EA58;
        case 0x1398049: return 0x139EA59;
        case 0x1399600: return 0x13A0020;
        case 0x13B2C10: return 0x13B9620;
        case 0x13B2D00: return 0x13B9710;
        case 0x13B5490: return 0x13BBEA0;
        case 0x13C8DB0: return 0x13CF980;
        case 0x13C90D0: return 0x13CFCA0;
        case 0x13D0480: return 0x13D7080;
        case 0x13DD1B0: return 0x13E3DB0;
        case 0x13DE6B0: return 0x13E52B0;
        case 0x140EE20: return 0x1415D60;
        case 0x1453710: return 0x145B420;
        case 0x146DE70: return 0x14AEA90;
        case 0x1534998: return 0x153AD68;
        case 0x1536A60: return 0x153CE30;
        case 0x153FE50: return 0x154E9A0;
        case 0x15D9070: return 0x15E6490;
        case 0x15D91B0: return 0x15E65D0;
        case 0x15D95C0: return 0x15E69E0;
        case 0x15D9B20: return 0x15E6F40;
        case 0x15D9F40: return 0x15E7360;
        case 0x15E4510: return 0x15DE940;
        case 0x1699490: return 0x16A4C10;
        case 0x169AB70: return 0x16A6230;
        case 0x169B500: return 0x16A6BD0;
        case 0x169B710: return 0x16A6E20;
        case 0x169B940: return 0x16A7050;
        case 0x169B9C0: return 0x16A70D0;
        case 0x169C090: return 0x16A77A0;
        case 0x169C520: return 0x16B6250;
        case 0x169C6F0: return 0x16B6420;
        case 0x169C7C0: return 0x16B64F0;
        case 0x169D8A0: return 0x16B7A60;
        case 0x16EF470: return 0x1703DA0;
        case 0x174E580: return 0x1763620;
        case 0x174F770: return 0x1764DD0;
        case 0x17519D0: return 0x1767020;
        case 0x1A363B0: return 0x1A4BA00;
        case 0x1A39BB0: return 0x1A4F200;
        case 0x1A5DBE0: return 0x1A731C0;
        case 0x1A9D0D0: return 0x1AB2640;
        case 0x1AC4B60: return 0x1ADA0D0;
        case 0x1AC8450: return 0x1ADD9C0;
        case 0x1ADB2F0: return 0x1AF0860;
        case 0x1E2BCF0: return 0x1E411D0;
        case 0x2B68910: return 0x2B7CA70;
        case 0x3E3E770: return 0x3E52B10;
        case 0x3E7AB20: return 0x3E8EEC0;
        case 0x3E7AFD0: return 0x3E8F370;
        case 0x3E86730: return 0x3E9AAD0;
        case 0x3E8D420: return 0x3EA17C0;
        case 0x3E8D510: return 0x3EA18B0;
        case 0x3E8D5B0: return 0x3EA1950;
        case 0x42A83E0: return 0x42BC610;
        case 0x4424AE0: return 0x4438B50;
        case 0x47EA370: return 0x47FE0B0;
        case 0x7CB8202: return 0x7CCAD32;
        case 0x7E1AB18: return 0x7E2DB18;
        case 0x7E1AB20: return 0x7E2DB20;
        case 0xA6DA768: return 0xA7004E8;
        case 0xA9C50C8: return 0xA9EB538;
        case 0xA9C50F8: return 0xA9EB568;
        case 0xA9C6418: return 0xA9EE738;
        default: return 0;
        }
    }

    template <uintptr_t Legacy> inline uintptr_t TestRva()
    {
        constexpr auto current = TestOnlyPatch110Rva(Legacy);
        if constexpr (current != 0)
            return ActiveGameBuild == GameBuild::Patch110 ? current : Legacy;
        else
            return GameRva<Legacy>();
    }
}
