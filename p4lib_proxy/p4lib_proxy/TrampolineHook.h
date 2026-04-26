#pragma once

#include <Windows.h>
#include <stdint.h>
#include <cstring>


class TrampolineHook
{
public:
    TrampolineHook()
        : m_target(0),
        m_hook(nullptr),
        m_trampoline(nullptr),
        m_stolenLen(0),
        m_installed(false)
    {
        std::memset(m_originalBytes, 0, sizeof(m_originalBytes));
    }

    ~TrampolineHook()
    {
        Remove();
    }

    bool Install(uintptr_t target, void* hook, int stolenLen)
    {
#ifdef _WIN64
        constexpr int kJumpSize = 12;
#else
        constexpr int kJumpSize = 5;
#endif

        if (m_installed)
            return true;

        if (!target || !hook || stolenLen < kJumpSize)
            return false;

        if (stolenLen > (int)sizeof(m_originalBytes))
            return false;

        m_target = target;
        m_hook = hook;
        m_stolenLen = stolenLen;

        ReadBytes((void*)m_target, m_originalBytes, m_stolenLen);

        // trampoline = stolen bytes + jump back
        m_trampoline = VirtualAlloc(
            nullptr,
            m_stolenLen + kJumpSize,
            MEM_COMMIT | MEM_RESERVE,
            PAGE_EXECUTE_READWRITE
        );

        if (!m_trampoline)
            return false;

        WriteBytes(m_trampoline, m_originalBytes, m_stolenLen);

        void* trampJmp = (unsigned char*)m_trampoline + m_stolenLen;
        void* returnAddr = (void*)(m_target + m_stolenLen);

        WriteJump(trampJmp, returnAddr, 0);

        WriteJump((void*)m_target, m_hook, 0);

        if (m_stolenLen > kJumpSize)
            WriteNoop((void*)(m_target + kJumpSize), m_stolenLen - kJumpSize);

        FlushInstructionCache(GetCurrentProcess(), (void*)m_target, m_stolenLen);
        FlushInstructionCache(GetCurrentProcess(), m_trampoline, m_stolenLen + kJumpSize);

        m_installed = true;
        return true;
    }

    bool Remove()
    {
        if (!m_installed)
            return true;

        WriteBytes((void*)m_target, m_originalBytes, m_stolenLen);
        FlushInstructionCache(GetCurrentProcess(), (void*)m_target, m_stolenLen);

        if (m_trampoline)
        {
            VirtualFree(m_trampoline, 0, MEM_RELEASE);
            m_trampoline = nullptr;
        }

        m_target = 0;
        m_hook = nullptr;
        m_stolenLen = 0;
        m_installed = false;
        std::memset(m_originalBytes, 0, sizeof(m_originalBytes));

        return true;
    }

    template <typename FnT>
    FnT GetOriginal() const
    {
        return reinterpret_cast<FnT>(m_trampoline);
    }

    void* GetTrampoline() const
    {
        return m_trampoline;
    }

    uintptr_t GetTarget() const
    {
        return m_target;
    }

    int GetStolenLen() const
    {
        return m_stolenLen;
    }

    bool IsInstalled() const
    {
        return m_installed;
    }
    private:
        static bool ReadBytes(void* src, void* dst, size_t len)
        {
            if (!src || !dst || !len)
                return false;

            std::memcpy(dst, src, len);
            return true;
        }

        static bool WriteBytes(void* dst, const void* src, size_t len)
        {
            if (!dst || !src || !len)
                return false;

            DWORD oldProtect;
            if (!VirtualProtect(dst, len, PAGE_EXECUTE_READWRITE, &oldProtect))
                return false;

            std::memcpy(dst, src, len);

            VirtualProtect(dst, len, oldProtect, &oldProtect);
            FlushInstructionCache(GetCurrentProcess(), dst, len);
            return true;
        }

        static bool WriteNoop(void* dst, size_t len)
        {
            if (!dst || !len)
                return false;

            DWORD oldProtect;
            if (!VirtualProtect(dst, len, PAGE_EXECUTE_READWRITE, &oldProtect))
                return false;

            std::memset(dst, 0x90, len);

            VirtualProtect(dst, len, oldProtect, &oldProtect);
            FlushInstructionCache(GetCurrentProcess(), dst, len);
            return true;
        }

        static bool WriteJump(void* src, void* dst, int nopCount = 0)
        {
            if (!src || !dst)
                return false;

#ifdef _WIN64

            // x64:
            // mov rax, <dst>
            // jmp rax
            //
            // 48 B8 xx xx xx xx xx xx xx xx
            // FF E0
            //
            // total = 12 bytes

            const size_t jumpSize = 12;
            const size_t totalSize = jumpSize + (size_t)nopCount;

            DWORD oldProtect;
            if (!VirtualProtect(src, totalSize, PAGE_EXECUTE_READWRITE, &oldProtect))
                return false;

            unsigned char* p = (unsigned char*)src;

            p[0] = 0x48;
            p[1] = 0xB8; // mov rax, imm64

            *(uintptr_t*)(p + 2) = (uintptr_t)dst;

            p[10] = 0xFF;
            p[11] = 0xE0; // jmp rax

            if (nopCount > 0)
                std::memset(p + jumpSize, 0x90, nopCount);

            VirtualProtect(src, totalSize, oldProtect, &oldProtect);
            FlushInstructionCache(GetCurrentProcess(), src, totalSize);

            return true;

#else

            // x86:
            // E9 rel32

            const size_t jumpSize = 5;
            const size_t totalSize = jumpSize + (size_t)nopCount;

            DWORD oldProtect;
            if (!VirtualProtect(src, totalSize, PAGE_EXECUTE_READWRITE, &oldProtect))
                return false;

            unsigned char* p = (unsigned char*)src;

            p[0] = 0xE9;

            *(int*)(p + 1) =
                (int)((uintptr_t)dst - ((uintptr_t)src + 5));

            if (nopCount > 0)
                std::memset(p + jumpSize, 0x90, nopCount);

            VirtualProtect(src, totalSize, oldProtect, &oldProtect);
            FlushInstructionCache(GetCurrentProcess(), src, totalSize);

            return true;

#endif
        }
private:
    uintptr_t     m_target;
    void* m_hook;
    void* m_trampoline;
    int           m_stolenLen;
    bool          m_installed;
    unsigned char m_originalBytes[32];
};