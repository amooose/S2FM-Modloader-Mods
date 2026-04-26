
#include "pch.h"
#include <Windows.h>




bool WriteBytes(void* dst, const void* src, size_t size)
{
    DWORD oldProtect;
    if (!VirtualProtect(dst, size, PAGE_EXECUTE_READWRITE, &oldProtect))
        return false;

    memcpy(dst, src, size);

    FlushInstructionCache(GetCurrentProcess(), dst, size);

    DWORD temp;
    VirtualProtect(dst, size, oldProtect, &temp);
    return true;
}

static HMODULE g_hModule = nullptr;

BOOL APIENTRY DllMain(
    HMODULE hModule,
    DWORD ul_reason_for_call,
    LPVOID lpReserved
)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {
        g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);

        HMODULE hSfm = GetModuleHandleW(L"sfm.dll");
        if (hSfm)
        {
            uintptr_t moduleBase = reinterpret_cast<uintptr_t>(hSfm);
            uintptr_t camToM1 = moduleBase + 0x160516;
            uintptr_t dragToM2 = moduleBase + 0x1610A9;
            uintptr_t deselectFix = moduleBase + 0x15FC32;

            BYTE v1 = 0x01;
            WriteBytes(reinterpret_cast<void*>(camToM1), &v1, 1);
            BYTE v2 = 0x02;
            WriteBytes(reinterpret_cast<void*>(dragToM2), &v2, 1);
            BYTE v3[] = {
                0x29, 0xC9, 0x90
            };
            WriteBytes(reinterpret_cast<void*>(deselectFix), v3, sizeof(v3));
        }
        break;
    }

    case DLL_PROCESS_DETACH:

        break;
    }

    return TRUE;
}