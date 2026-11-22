#pragma once

#include <windows.h>
#include <mmsystem.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>

namespace MoraleMatters
{
    struct GuardWave
    {
        static constexpr DWORD Capacity = 192 * 1024;
        BYTE bytes[Capacity] = {};
        DWORD length = 0;
        DWORD sampleOffset = 0;
        DWORD sampleBytes = 0;

        bool Load(bool hit, LPCWSTR moduleAddress)
        {
            HMODULE module = nullptr;
            if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, moduleAddress, &module))
                return false;
            wchar_t path[MAX_PATH];
            const DWORD pathLength = GetModuleFileNameW(module, path, MAX_PATH);
            const wchar_t* suffix = hit ? L"\\ue4ss\\Mods\\MoraleMatters\\Scripts\\Assets\\GuardHit.wav" : L"\\ue4ss\\Mods\\MoraleMatters\\Scripts\\Assets\\GuardBreak.wav";
            wchar_t* slash = pathLength && pathLength < MAX_PATH ? wcsrchr(path, L'\\') : nullptr;
            if (!slash || static_cast<size_t>(slash - path) + wcslen(suffix) + 1 > MAX_PATH)
                return false;
            wcscpy(slash, suffix);

            const HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file == INVALID_HANDLE_VALUE)
                return false;
            DWORD size = 0;
            const bool read = ReadFile(file, bytes, Capacity, &size, nullptr) != FALSE;
            CloseHandle(file);
            if (!read || size < 12 || memcmp(bytes, "RIFF", 4) || memcmp(bytes + 8, "WAVE", 4))
                return false;
            DWORD end = 0;
            memcpy(&end, bytes + 4, sizeof(end));
            if (end < 4 || end > size - 8)
                return false;
            end += 8;
            bool pcm16 = false;
            DWORD offset = 0, count = 0;
            for (DWORD chunk = 12; chunk + 8 <= end;)
            {
                DWORD size = 0;
                memcpy(&size, bytes + chunk + 4, sizeof(size));
                const DWORD data = chunk + 8;
                if (size > end - data)
                    return false;
                if (!memcmp(bytes + chunk, "fmt ", 4) && size >= 16)
                {
                    WORD format = 0, bits = 0;
                    memcpy(&format, bytes + data, sizeof(format));
                    memcpy(&bits, bytes + data + 14, sizeof(bits));
                    pcm16 = format == WAVE_FORMAT_PCM && bits == 16;
                }
                else if (!memcmp(bytes + chunk, "data", 4))
                {
                    offset = data;
                    count = size;
                }
                chunk = data + size + (size & 1);
            }
            if (!pcm16 || !count || count % sizeof(int16_t))
                return false;
            length = end;
            sampleOffset = offset;
            sampleBytes = count;
            return true;
        }

        void CopyWithGain(BYTE* target, double gain) const
        {
            memcpy(target, bytes, length);
            for (DWORD at = sampleOffset; at < sampleOffset + sampleBytes; at += sizeof(int16_t))
            {
                int16_t sample = 0;
                memcpy(&sample, bytes + at, sizeof(sample));
                sample = static_cast<int16_t>(sample * gain);
                memcpy(target + at, &sample, sizeof(sample));
            }
        }
    };

    inline bool PlayGuardFeedbackSound(bool hit, double gain)
    {
        if (!(gain > 0 && gain <= 1))
            return false;
        static GuardWave cues[2];
        auto& cue = cues[hit ? 1 : 0];
        if (!cue.length && !cue.Load(hit, reinterpret_cast<LPCWSTR>(&PlayGuardFeedbackSound)))
            return false;

        // This runs on the game thread. Keep the playing buffer alive and only
        // reuse it after Windows has accepted playback from the other buffer.
        static BYTE buffers[2][GuardWave::Capacity];
        static int next = 0;
        cue.CopyWithGain(buffers[next], gain);
        const DWORD flags = SND_MEMORY | SND_ASYNC | SND_NODEFAULT | (hit ? SND_NOSTOP : 0);
        if (!PlaySoundW(reinterpret_cast<LPCWSTR>(buffers[next]), nullptr, flags))
            return false;
        next = 1 - next;
        return true;
    }
}
