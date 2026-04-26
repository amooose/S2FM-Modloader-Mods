
#include "pch.h"
#include <stdio.h>
#include <Windows.h>
#include "MinHook.h"
#include <cstdint>
#include <cmath>

using MouseEventFn = void(__fastcall*)(void* self, void* qMouseEvent);
static MouseEventFn oMouseEvent = nullptr;

static constexpr int OFF_MODE = 1172;

static constexpr int OFF_WRAP_X = 1192; // anchor X
static constexpr int OFF_WRAP_Y = 1196; // anchor Y

static constexpr int OFF_VIRTUAL_X = 1200; // virtual mouse X
static constexpr int OFF_VIRTUAL_Y = 1204; // virtual mouse Y

static constexpr int OFF_WARPED_FLAG = 1208; // bool

static int startingMode4EventCount = 0;

static bool eventPosLooksStale(void* self, void* qMouseEvent)
{
    auto base = (uint8_t*)self;
    auto ev = (uint8_t*)qMouseEvent;

    int mode = *(int*)(base + OFF_MODE);

    int anchorX = *(int*)(base + OFF_WRAP_X);
    int anchorY = *(int*)(base + OFF_WRAP_Y);

    int eventX = *(int*)(ev + 32);
    int eventY = *(int*)(ev + 36);

    int dx = eventX - anchorX;
    int dy = eventY - anchorY;

    //only check/consume the first 50 calls since thats where the bug occurs. Maybe could be lowered
    if (mode == 4 && startingMode4EventCount > 50) {
        return false;
    }

    return std::abs(dx) > 120 || std::abs(dy) > 120;
}

static void rewriteEventPosToAnchor(void* self, void* qMouseEvent)
{
    auto base = (uint8_t*)self;
    auto ev = (uint8_t*)qMouseEvent;

    int anchorX = *(int*)(base + OFF_WRAP_X);
    int anchorY = *(int*)(base + OFF_WRAP_Y);

    *(int*)(ev + 32) = anchorX;
    *(int*)(ev + 36) = anchorY;

    *(int*)(base + OFF_VIRTUAL_X) = anchorX;
    *(int*)(base + OFF_VIRTUAL_Y) = anchorY;
    *(BYTE*)(base + OFF_WARPED_FLAG) = 1;
}

static void __fastcall hkMouseEvent(void* self, void* qMouseEvent)
{
    auto base = (uint8_t*)self;
    int modeBefore = *(int*)(base + OFF_MODE);
    int mode = *(int*)(base + OFF_MODE);

    if (mode == 0) {
        startingMode4EventCount = 0;
    }
    if (mode == 4) {
        startingMode4EventCount++;
    }

    if (modeBefore == 4 && eventPosLooksStale(self, qMouseEvent))
    {
        rewriteEventPosToAnchor(self, qMouseEvent);
    }
    oMouseEvent(self, qMouseEvent);
}

bool installViewportMouseHook(uintptr_t moduleBase)
{
    uintptr_t target = moduleBase + 0x160400;

    if (MH_Initialize() != MH_OK && MH_Initialize() != MH_ERROR_ALREADY_INITIALIZED)
        return false;

    if (MH_CreateHook(
        reinterpret_cast<void*>(target),
        reinterpret_cast<void*>(&hkMouseEvent),
        reinterpret_cast<void**>(&oMouseEvent)) != MH_OK)
    {
        return false;
    }

    return MH_EnableHook(reinterpret_cast<void*>(target)) == MH_OK;
}

bool installViewportMouseHook(uintptr_t moduleBase);

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
            installViewportMouseHook(moduleBase);
        }
        break;
    }

    case DLL_PROCESS_DETACH:

        break;
    }

    return TRUE;
}