#include <windows.h>
#include <stdio.h>

int wmain(int argc, wchar_t** argv)
{
    if (argc != 2)
        return 1;
    HMODULE plugin = LoadLibraryW(argv[1]);
    if (!plugin)
    {
        printf("LoadLibrary failed: %lu\n", GetLastError());
        return 1;
    }
    auto status = reinterpret_cast<LONG (*)()>(GetProcAddress(plugin, "MoraleMatters_GetStatus"));
    if (!status)
        return 1;
    for (int index = 0; index < 100 && status() == 0; ++index)
        Sleep(50);
    printf("Host guard status: %ld\n", status());
    return status() == 3 ? 0 : 1;
}
