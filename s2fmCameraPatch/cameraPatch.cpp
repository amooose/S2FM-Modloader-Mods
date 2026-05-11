
#include <stdio.h>
#include <Windows.h>
#include "MinHook.h"
#include <cstdint>
#include <cmath>
#include "safetyhook.hpp"
#include <cstring>
using MouseEventFn = void(__fastcall*)(void* self, void* qMouseEvent);
static MouseEventFn oMouseEvent = nullptr;

static constexpr int OFF_MODE = 1172;

static constexpr int OFF_WRAP_X = 1192; // anchor X
static constexpr int OFF_WRAP_Y = 1196; // anchor Y

static constexpr int OFF_VIRTUAL_X = 1200; // virtual mouse X
static constexpr int OFF_VIRTUAL_Y = 1204; // virtual mouse Y

static constexpr int OFF_WARPED_FLAG = 1208; // bool

static int startingMode4EventCount = 0;
uintptr_t moduleBase = 0;


SafetyHookMid g_delta_hookx{};
SafetyHookMid g_delta_hooky{};

static LARGE_INTEGER g_qpcFreq{};
static bool g_qpcInit = false;

static uint64_t g_lastBigX_us = 0;
static int g_bigXBurst = 0;

static uint64_t g_lastBigY_us = 0;
static int g_bigYBurst = 0;

static uint64_t NowUs()
{
    if (!g_qpcInit)
    {
        QueryPerformanceFrequency(&g_qpcFreq);
        g_qpcInit = true;
    }

    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);

    return (uint64_t)((now.QuadPart * 1000000ull) / g_qpcFreq.QuadPart);
}


void DeltaDropHookX(safetyhook::Context& ctx)
{
    int dx = static_cast<int32_t>(ctx.r15 & 0xFFFFFFFFu);

    constexpr int BIG_DELTA = 35;
    constexpr uint64_t BURST_WINDOW_US = 7'000;

    uint64_t now = NowUs();

    if (std::abs(dx) >= BIG_DELTA)
    {
        if (now - g_lastBigX_us <= BURST_WINDOW_US)
            ++g_bigXBurst;
        else
            g_bigXBurst = 1;

        g_lastBigX_us = now;

        if (g_bigXBurst >= 2)
        {
            ctx.r15 = 0;
            //Dropped stale X delta
        }
    }
    else
    {
        g_bigXBurst = 0;
    }
}

void DeltaDropHookY(safetyhook::Context& ctx)
{
    int dy = static_cast<int32_t>(ctx.r12 & 0xFFFFFFFFu);

    constexpr int BIG_DELTA = 35;
    constexpr uint64_t BURST_WINDOW_US = 7'000;

    uint64_t now = NowUs();

    if (std::abs(dy) >= BIG_DELTA)
    {
        if (now - g_lastBigY_us <= BURST_WINDOW_US)
            ++g_bigYBurst;
        else
            g_bigYBurst = 1;

        g_lastBigY_us = now;

        if (g_bigYBurst >= 2)
        {
            ctx.r12 = 0;
            //Dropped stale Y delta
        }
    }
    else
    {
        g_bigYBurst = 0;
    }
}
/*
constexpr int MAX_DELTA = 35;
void DeltaClampHook(safetyhook::Context& ctx)
{
    int deltaY = static_cast<int32_t>(ctx.r15 & 0xFFFFFFFFu);

    if (deltaY > MAX_DELTA)
        deltaY = MAX_DELTA;
    else if (deltaY < -MAX_DELTA)
        deltaY = -MAX_DELTA;
    else
        return;

    // emulate writing r15d, not full r15
    ctx.r15 = static_cast<uint32_t>(deltaY);
}
void DeltaClampHook2(safetyhook::Context& ctx)
{
    int deltaX = static_cast<int32_t>(ctx.r12 & 0xFFFFFFFFu);
    if (std::abs(deltaX) > 50)
    {
        char buf[128];
        sprintf_s(buf, "BIG deltaX = %d\n", deltaX);
        OutputDebugStringA(buf);
    }

    if (deltaX > MAX_DELTA)
        deltaX = MAX_DELTA;
    else if (deltaX < -MAX_DELTA)
        deltaX = -MAX_DELTA;
    else
        return;

    // emulate writing r12d, not full r12
    ctx.r12 = static_cast<uint32_t>(deltaX);
}
*/

void InstallCameraDeltaFix()
{

    g_delta_hookx = safetyhook::create_mid(
        reinterpret_cast<void*>(moduleBase + 0x1606DF),
        DeltaDropHookX
    );

    g_delta_hooky = safetyhook::create_mid(
        reinterpret_cast<void*>(moduleBase + 0x1606F5),
        DeltaDropHookY
    );
  
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
    int errorThreshhold = 100;
    return std::abs(dx) > errorThreshhold || std::abs(dy) > errorThreshhold;
}

int mflag = 0;
static void __fastcall hkMouseEvent(void* self, void* qMouseEvent)
{
    auto base = (uint8_t*)self;

    int mode = *(int*)(base + OFF_MODE);
    int MODE_DRAG = 4;
    if (mode != MODE_DRAG) {
        startingMode4EventCount = 0;
    }
    if (mode == MODE_DRAG) {
        startingMode4EventCount++;
    }

    if (mode == 4 && startingMode4EventCount < 50) {

        if (eventPosLooksStale(self, qMouseEvent)) {
            rewriteEventPosToAnchor(self, qMouseEvent);
        }
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
             moduleBase = reinterpret_cast<uintptr_t>(hSfm);
            installViewportMouseHook(moduleBase);
            InstallCameraDeltaFix();
        }
        break;
    }

    case DLL_PROCESS_DETACH:

        break;
    }

    return TRUE;
}