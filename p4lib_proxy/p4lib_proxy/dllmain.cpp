// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"

#include <windows.h>
#include <cwchar>
#include <vector>
#include <string>

extern "C" IMAGE_DOS_HEADER __ImageBase;

static HMODULE g_real = nullptr;
static std::vector<HMODULE> g_loadedMods;

static std::wstring GetProxyFolder()
{
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW((HMODULE)&__ImageBase, path, MAX_PATH);

    wchar_t* slash = wcsrchr(path, L'\\');
    if (slash)
        slash[1] = L'\0';

    return path;
}

static HMODULE LoadReal()
{
    if (g_real)
        return g_real;

    std::wstring path = GetProxyFolder() + L"p4lib_real.dll";
    g_real = LoadLibraryW(path.c_str());
    return g_real;
}

static FARPROC RealProc(const char* name)
{
    HMODULE real = LoadReal();
    return real ? GetProcAddress(real, name) : nullptr;
}

static void LoadAllMods()
{
    std::wstring modsDir = GetProxyFolder() + L"mods\\";
    std::wstring search = modsDir + L"*.dll";

    WIN32_FIND_DATAW fd{};
    HANDLE hFind = FindFirstFileW(search.c_str(), &fd);

    if (hFind == INVALID_HANDLE_VALUE)
        return;

    do
    {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;

        std::wstring dllPath = modsDir + fd.cFileName;

        HMODULE h = LoadLibraryW(dllPath.c_str());
        if (h)
            g_loadedMods.push_back(h);

    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);
}

static HMODULE WaitForModule(const wchar_t* name)
{
    HMODULE h = nullptr;

    while (!h)
    {
        h = GetModuleHandleW(name);
        if (h)
            return h;

        Sleep(100);
    }

    return h;
}

DWORD WINAPI InitThread(LPVOID)
{
    // Change/remove this depending on what your mods need.
    WaitForModule(L"sfm.dll");
    // WaitForModule(L"ifm.dll");

    LoadAllMods();
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hinst);

        LoadReal();
        CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr);
    }

    return TRUE;
}

extern "C" __declspec(dllexport)
__int64 BinaryProperties_GetValue(int a1, __int64 a2)
{
    using Fn = __int64(*)(int, __int64);
    auto fn = (Fn)RealProc("BinaryProperties_GetValue");
    return fn ? fn(a1, a2) : 0;
}

extern "C" __declspec(dllexport)
void* CreateInterface(const char* name, int* ret)
{
    using Fn = void* (*)(const char*, int*);
    auto fn = (Fn)RealProc("CreateInterface");
    return fn ? fn(name, ret) : nullptr;
}