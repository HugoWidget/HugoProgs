/*
 * Copyright 2026 howdy213
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <set>
#include <string>
#include <vector>

class MemScan {
public:
    struct Options {
        int    threadCount = 0;              // 0 = auto
        SIZE_T chunkSize = 256 * 1024;
        DWORD  intervalMs = 100;
        size_t contextBytes = 64;            // 0 = no context dump
        bool   writableOnly = true;
        bool   exitWhenTargetMissing = true; // PID mode default; name mode forced false
    };

    enum class Encoding { ANSI, UTF16LE };
    enum class RoundState { WaitingProcess, OpenFailed, Scanning };
    enum class ProcessEventType { Connected, Disconnected, Restarted };

    struct Target {
        std::string patternText;
        std::string label;
        Encoding    encoding = Encoding::ANSI;
        bool        deduplicate = false;
        bool        extractString = false;
        size_t      maxStringLen = 256;
    };

    struct Context {
        uintptr_t            base = 0;
        size_t               hitOffset = 0;
        std::vector<uint8_t> bytes;
        bool                 valid = false;
    };

    struct Hit {
        size_t      targetIndex = 0;
        std::string label;
        uintptr_t   address = 0;
        std::string value;
        bool        isNewValue = false;
        bool        afterRestart = false;
        Context     context;
    };

    struct RoundStatus {
        RoundState state = RoundState::WaitingProcess;
        long long  round = 0;
        DWORD      pid = 0;
        size_t     regions = 0;
        size_t     chunks = 0;
        uint64_t   bytesRead = 0;
        uint32_t   failedReads = 0;
        size_t     totalHits = 0;
        size_t     newValues = 0;
        long long  elapsedMs = 0;
        int        zeroByteStreak = 0;
        DWORD      lastError = 0;
        std::vector<DWORD> triedPids;
    };

    struct ProcessEvent {
        ProcessEventType type = ProcessEventType::Connected;
        std::string      procName;
        DWORD            oldPid = 0;
        DWORD            newPid = 0;
    };

    using HitCallback = std::function<void(const Hit&)>;
    using StatusCallback = std::function<void(const RoundStatus&)>;
    using ProcessCallback = std::function<void(const ProcessEvent&)>;

    MemScan(std::string target, std::string pattern, Options options = {});
    MemScan(std::string target, std::vector<Target> targets, Options options = {});
    ~MemScan();

    MemScan(const MemScan&) = delete;
    MemScan& operator=(const MemScan&) = delete;

    void SetHitCallback(HitCallback cb) { m_onHit = std::move(cb); }
    void SetStatusCallback(StatusCallback cb) { m_onStatus = std::move(cb); }
    void SetProcessCallback(ProcessCallback cb) { m_onProcess = std::move(cb); }

    bool Initialize(std::string& error);
    int  Run();

    const std::set<std::string>& GetValues(size_t targetIndex) const;
    const Target& GetTarget(size_t index) const { return m_targets[index]; }
    size_t        GetTargetCount() const { return m_targets.size(); }
    size_t        GetCompiledPatternSize(size_t index) const;

    static bool IsRunAsAdmin();
    static bool EnableDebugPrivilege();
    static void InstallCtrlHandler();
    static void RequestStop();
    static bool IsRunning();

private:
    struct CompiledTarget {
        std::vector<uint8_t>  pattern;
        std::string           mask;
        size_t                patLen = 0;
        size_t                charWidth = 1;
        size_t                maxStringLen = 256;
        bool                  extractString = false;
        bool                  deduplicate = false;
        std::set<std::string> seenValues;
    };

    struct Region {
        uintptr_t base = 0;
        SIZE_T    size = 0;
        DWORD     protect = 0;
    };

    struct Chunk {
        uintptr_t base = 0;
        SIZE_T    offset = 0;
        SIZE_T    length = 0;
        SIZE_T    regionSize = 0;
    };

    struct CombinedScanResult {
        std::vector<std::vector<uintptr_t>> hitsPerTarget;
        size_t   regions = 0;
        size_t   chunks = 0;
        uint64_t bytesRead = 0;
        uint32_t failed = 0;
        long long ms = 0;
    };

    void InitTargetName();
    bool CompileTargets(std::string& error);

    static bool GetProcessCreateTime(HANDLE h, FILETIME& ct);
    static bool IsProcessAlive(HANDLE h);
    static bool EqualsIgnoreCase(const std::string& a, const std::string& b);
    static bool IsAllDigits(const std::string& s);

    std::vector<DWORD> FindAllPidsByName(const std::string& name) const;
    HANDLE OpenProcessForScan(DWORD pid, DWORD* err = nullptr) const;

    static bool IsWritableProtect(DWORD p);
    static bool IsReadableProtect(DWORD p);
    bool ShouldSkipRegion(const MEMORY_BASIC_INFORMATION& mbi) const;

    bool ReadRange(HANDLE h, uintptr_t start, size_t length,
        std::vector<uint8_t>& out) const;
    bool ReadContext(HANDLE h, uintptr_t hit, size_t patLen, Context& out) const;
    bool ExtractStringValue(HANDLE h, uintptr_t valueStart, size_t charWidth,
        size_t maxLen, std::string& out) const;

    CombinedScanResult ScanAll(HANDLE hProcess);

    static BOOL WINAPI CtrlHandler(DWORD type);
    static void InterruptibleSleep(DWORD ms);
    static bool ParsePattern(const std::string& text,
        std::vector<uint8_t>& bytes,
        std::string& mask);

    std::string                 m_target;
    std::vector<Target>         m_targets;
    std::vector<CompiledTarget> m_compiled;
    Options                     m_options;

    bool        m_isPid = false;
    DWORD       m_fixedPid = 0;
    std::string m_procName;

    bool m_isAdmin = false;
    bool m_debugPrivilege = false;

    HitCallback     m_onHit;
    StatusCallback  m_onStatus;
    ProcessCallback m_onProcess;

    static std::atomic<bool> s_running;
};