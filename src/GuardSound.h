#pragma once

#include <windows.h>
#include <mmsystem.h>
#include <wchar.h>

namespace MoraleMatters
{
    inline bool PlayGuardFeedbackSound(bool hit)
    {
        static wchar_t paths[2][MAX_PATH] = {};
        auto& path = paths[hit ? 1 : 0];
        if (!path[0])
        {
            HMODULE module = nullptr;
            if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(&PlayGuardFeedbackSound), &module))
                return false;
            const DWORD length = GetModuleFileNameW(module, path, MAX_PATH);
            const wchar_t* suffix = hit ? L"\\ue4ss\\Mods\\MoraleMatters\\Scripts\\Assets\\GuardHit.wav" : L"\\ue4ss\\Mods\\MoraleMatters\\Scripts\\Assets\\GuardBreak.wav";
            wchar_t* slash = length && length < MAX_PATH ? wcsrchr(path, L'\\') : nullptr;
            if (!slash || static_cast<size_t>(slash - path) + wcslen(suffix) + 1 > MAX_PATH)
            {
                path[0] = 0;
                return false;
            }
            wcscpy(slash, suffix);
        }
        // A short UI cue: Windows owns asynchronous playback; no Lua worker,
        // engine UObject lifetime, or game-thread wait is introduced.
        return PlaySoundW(path, nullptr, SND_FILENAME | SND_ASYNC | SND_NODEFAULT | (hit ? SND_NOSTOP : 0)) != FALSE;
    }
}
