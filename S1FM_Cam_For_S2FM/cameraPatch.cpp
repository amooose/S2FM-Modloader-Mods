
#include "pch.h"
#include <Windows.h>
#include <intrin.h>
#include <cstdint>
#include <MinHook.h>
#include <stdio.h>

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

static uintptr_t moduleBase = 0;

static constexpr uintptr_t RVA_GetOrCreateSceneElement = 0x1646a0;
static constexpr uintptr_t RVA_CallsiteReturnAfterCall = 0x15fcf1;

using GetOrCreateSceneElement_t = void* (__fastcall*)(void* thisptr);
static GetOrCreateSceneElement_t o_GetOrCreateSceneElement = nullptr;

//Read pointer to see if we're in the Clip Editor or not
static uint8_t ReadModeByte()
{
    uintptr_t p1 = *reinterpret_cast<uintptr_t*>(moduleBase + 0x008EC690);
    if (!p1) return 0xFF;

    uintptr_t p2 = *reinterpret_cast<uintptr_t*>(p1 + 0x80);
    if (!p2) return 0xFF;

    uintptr_t p3 = *reinterpret_cast<uintptr_t*>(p2 + 0x8);
    if (!p3) return 0xFF;

    uint8_t value = *reinterpret_cast<uint8_t*>(p3 + 0xC8);

    return value;
}

static bool clipEditorActive()
{
    uint8_t mode = ReadModeByte();
    return mode == 0;
}

void* __fastcall hk_GetOrCreateSceneElement(void* thisptr)
{
    void* result = o_GetOrCreateSceneElement(thisptr);

    uintptr_t ret = reinterpret_cast<uintptr_t>(_ReturnAddress());
    
    if (ret == moduleBase + RVA_CallsiteReturnAfterCall)
    {
        if (clipEditorActive())
            return nullptr;
    }
    return result;
}

bool InstallSceneElementHook()
{
    HMODULE hSfm = GetModuleHandleW(L"sfm.dll");
    if (!hSfm)
        return false;

    moduleBase = reinterpret_cast<uintptr_t>(hSfm);

    void* target = reinterpret_cast<void*>(moduleBase + RVA_GetOrCreateSceneElement);

    if (MH_Initialize() != MH_OK && MH_Initialize() != MH_ERROR_ALREADY_INITIALIZED)
        return false;

    MH_CreateHook(
        target,
        reinterpret_cast<void*>(&hk_GetOrCreateSceneElement),
        reinterpret_cast<void**>(&o_GetOrCreateSceneElement)
    );

    MH_EnableHook(target);

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

            //Patch to make camera mouse1 instead of mouse2
            BYTE v1 = 0x01;
            WriteBytes(reinterpret_cast<void*>(camToM1), &v1, 1);

            //Patch to make drag select/lasso mouse2
            BYTE v2 = 0x02;
            WriteBytes(reinterpret_cast<void*>(dragToM2), &v2, 1);

            //Patch to fix introduced side-effect bug of not being able to deselect bones via clicking empty space with mouse1
            BYTE v3[] = {
                0x29, 0xC9, 0x90
            };
            WriteBytes(reinterpret_cast<void*>(deselectFix), v3, sizeof(v3));

            //Hook to check for camera movement on the Clip Editor
            //If camera movement takes place where a model/element exists in our crosshair, it lags the movement some
            //This skips the check (bones/model/etc selection doesnt need to occur via the Clip Editor viewport)
            //And removes the lag
            InstallSceneElementHook();
        }

        break;
    }

    case DLL_PROCESS_DETACH:

        break;
    }

    return TRUE;
}