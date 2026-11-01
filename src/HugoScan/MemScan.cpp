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
#include "MemScan.h"
#include "WinUtils/AWDef.h"

#include <psapi.h>
#include <tlhelp32.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <sstream>
#include <thread>

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "advapi32.lib")

std::atomic<bool> MemScan::s_running{ true };

// ============ ctor / dtor ============
MemScan::MemScan(std::string target, std::string pattern, Options options)
    : m_target(std::move(target)), m_options(options)
{
    Target t;
    t.patternText = std::move(pattern);
    m_targets.push_back(std::move(t));
    InitTargetName();
}

MemScan::MemScan(std::string target, std::vector<Target> targets, Options options)
    : m_target(std::move(target)), m_targets(std::move(targets)), m_options(options)
{
    InitTargetName();
}

MemScan::~MemScan() = default;

void MemScan::InitTargetName()
{
    InstallCtrlHandler();

    if (IsAllDigits(m_target)) {
        m_isPid = true;
        try { m_fixedPid = static_cast<DWORD>(std::stoul(m_target)); }
        catch (...) { m_isPid = false; m_procName = m_target; }
    }
    else {
        m_procName = m_target;
    }

    if (m_options.chunkSize == 0) m_options.chunkSize = 256 * 1024;
    if (m_options.threadCount <= 0)
        m_options.threadCount = static_cast<int>(std::thread::hardware_concurrency());
    if (m_options.threadCount <= 0) m_options.threadCount = 4;
    if (m_options.threadCount > 64) m_options.threadCount = 64;

    if (!m_isPid) m_options.exitWhenTargetMissing = false;
}

// ============ console ============
void MemScan::InstallCtrlHandler()
{
    static std::atomic<bool> installed{ false };
    if (installed.exchange(true)) return;
    SetConsoleCtrlHandler(&MemScan::CtrlHandler, TRUE);
}

BOOL WINAPI MemScan::CtrlHandler(DWORD type)
{
    if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT) {
        s_running = false;
        return TRUE;
    }
    return FALSE;
}

void MemScan::RequestStop() { s_running = false; }
bool MemScan::IsRunning() { return s_running.load(); }

void MemScan::InterruptibleSleep(DWORD ms)
{
    while (ms > 0 && s_running) {
        DWORD step = (std::min)(ms, 100UL);
        Sleep(step);
        ms -= step;
    }
}

size_t MemScan::GetCompiledPatternSize(size_t index) const
{
    if (index >= m_compiled.size()) return 0;
    return m_compiled[index].patLen;
}

// ============ privileges (public) ============
bool MemScan::IsRunAsAdmin()
{
    BOOL isAdmin = FALSE;
    PSID adminGroup = nullptr;
    SID_IDENTIFIER_AUTHORITY nt = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&nt, 2,
        SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(nullptr, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin != FALSE;
}

bool MemScan::EnableDebugPrivilege()
{
    bool ok = false;
    HANDLE hToken = nullptr;
    if (OpenProcessToken(GetCurrentProcess(),
        TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        LUID luid{};
        if (LookupPrivilegeValueA(nullptr, "SeDebugPrivilege", &luid)) {
            TOKEN_PRIVILEGES tp{};
            tp.PrivilegeCount = 1;
            tp.Privileges[0].Luid = luid;
            tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
            SetLastError(ERROR_SUCCESS);
            if (AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp),
                nullptr, nullptr) && GetLastError() == ERROR_SUCCESS) {
                ok = true;
            }
        }
        CloseHandle(hToken);
    }
    return ok;
}

// ============ small helpers ============
bool MemScan::EqualsIgnoreCase(const std::string& a, const std::string& b)
{
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i]))
            return false;
    return true;
}

bool MemScan::IsAllDigits(const std::string& s)
{
    if (s.empty()) return false;
    for (char c : s) if (!std::isdigit((unsigned char)c)) return false;
    return true;
}

bool MemScan::GetProcessCreateTime(HANDLE h, FILETIME& ct)
{
    FILETIME e, k, u;
    return GetProcessTimes(h, &ct, &e, &k, &u) != 0;
}

bool MemScan::IsProcessAlive(HANDLE h)
{
    DWORD code = 0;
    if (!GetExitCodeProcess(h, &code)) return false;
    return code == STILL_ACTIVE;
}

std::vector<DWORD> MemScan::FindAllPidsByName(const std::string& name) const
{
    std::vector<DWORD> pids;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return pids;

    PROCESSENTRY32A pe{};
    pe.dwSize = sizeof(pe);
    if (Process32FirstA(snap, &pe)) {
        do {
            std::string exe = pe.szExeFile;
            bool hit = EqualsIgnoreCase(exe, name);
            if (!hit && exe.size() > 4 &&
                exe.compare(exe.size() - 4, 4, ".exe") == 0) {
                std::string stem = exe.substr(0, exe.size() - 4);
                hit = EqualsIgnoreCase(stem, name);
            }
            if (hit) pids.push_back(pe.th32ProcessID);
        } while (Process32NextA(snap, &pe));
    }
    CloseHandle(snap);
    return pids;
}

HANDLE MemScan::OpenProcessForScan(DWORD pid, DWORD* err) const
{
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
        FALSE, pid);
    if (!h && err) *err = GetLastError();
    return h;
}

// ============ memory protection ============
bool MemScan::IsWritableProtect(DWORD p)
{
    p &= 0xFF;
    return p == PAGE_READWRITE || p == PAGE_WRITECOPY
        || p == PAGE_EXECUTE_READWRITE || p == PAGE_EXECUTE_WRITECOPY;
}

bool MemScan::IsReadableProtect(DWORD p)
{
    if (p & (PAGE_GUARD | PAGE_NOACCESS)) return false;
    p &= 0xFF;
    return p == PAGE_READONLY || p == PAGE_READWRITE
        || p == PAGE_WRITECOPY || p == PAGE_EXECUTE_READ
        || p == PAGE_EXECUTE_READWRITE || p == PAGE_EXECUTE_WRITECOPY;
}

bool MemScan::ShouldSkipRegion(const MEMORY_BASIC_INFORMATION& mbi) const
{
    if (mbi.State != MEM_COMMIT || mbi.RegionSize == 0) return true;
    if (!IsReadableProtect(mbi.Protect)) return true;

    uintptr_t base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
    if (base < 0x10000) return true;
    if (base >= 0x7FFE0000ULL && base < 0x7FFF0000ULL) return true;
    if (base >= 0xFFFFF78000000000ULL && base < 0xFFFFF78000001000ULL) return true;

    if (mbi.Type == MEM_MAPPED && !IsWritableProtect(mbi.Protect)) return true;
    if (m_options.writableOnly && !IsWritableProtect(mbi.Protect)) return true;
    return false;
}

// ============ pattern ============
bool MemScan::ParsePattern(const std::string& text,
    std::vector<uint8_t>& out, std::string& mask)
{
    out.clear();
    mask.clear();
    if (text.empty()) return false;

    if (text.front() == '"' || text.front() == '\'') {
        char q = text.front();
        size_t e = text.find(q, 1);
        if (e != std::string::npos) {
            std::string body = text.substr(1, e - 1);
            out.assign(body.begin(), body.end());
            mask.assign(body.size(), 'x');
            return !out.empty();
        }
    }

    std::string lower = text;
    for (auto& c : lower) c = (char)std::tolower((unsigned char)c);
    if (lower.rfind("ascii:", 0) == 0 || lower.rfind("a:", 0) == 0) {
        std::string body = text.substr(text.find(':') + 1);
        out.assign(body.begin(), body.end());
        mask.assign(body.size(), 'x');
        return !out.empty();
    }

    std::vector<uint8_t> hex;
    std::string hexMask;
    std::istringstream iss(text);
    std::string tok;
    bool ok = true;
    while (iss >> tok) {
        if (tok == "?" || tok == "??") {
            hex.push_back(0);
            hexMask.push_back('?');
            continue;
        }
        std::string h = tok;
        if (h.size() > 2 && h[0] == '0' && (h[1] == 'x' || h[1] == 'X'))
            h = h.substr(2);
        if (h.empty() || h.size() > 2) { ok = false; break; }
        bool hexOk = true;
        for (char c : h) if (!std::isxdigit((unsigned char)c)) { hexOk = false; break; }
        if (!hexOk) { ok = false; break; }
        try {
            hex.push_back((uint8_t)std::stoul(h, nullptr, 16));
            hexMask.push_back('x');
        }
        catch (...) { ok = false; break; }
    }
    if (ok && !hex.empty()) {
        out = std::move(hex);
        mask = std::move(hexMask);
        return true;
    }

    out.assign(text.begin(), text.end());
    mask.assign(text.size(), 'x');
    return !out.empty();
}

// ============ compilation / Initialize ============
bool MemScan::CompileTargets(std::string& error)
{
    m_compiled.clear();
    m_compiled.resize(m_targets.size());

    for (size_t i = 0; i < m_targets.size(); ++i) {
        const auto& t = m_targets[i];
        auto& c = m_compiled[i];

        std::vector<uint8_t> raw;
        std::string mask;
        if (!ParsePattern(t.patternText, raw, mask) || raw.empty()) {
            error = "cannot parse pattern [" + t.label + "]: " + t.patternText;
            return false;
        }

        if (t.encoding == Encoding::UTF16LE) {
            std::vector<uint8_t> wide;
            wide.reserve(raw.size() * 2);
            for (auto b : raw) { wide.push_back(b); wide.push_back(0); }
            c.pattern = std::move(wide);
            c.mask.assign(c.pattern.size(), 'x');
            c.charWidth = 2;
        }
        else {
            c.pattern = std::move(raw);
            c.mask = std::move(mask);
            c.charWidth = 1;
        }
        c.patLen = c.pattern.size();
        c.extractString = t.extractString;
        c.deduplicate = t.deduplicate;
        c.maxStringLen = (t.maxStringLen > 0) ? t.maxStringLen : 256;
    }
    return true;
}

bool MemScan::Initialize(std::string& error)
{
    s_running = true;
    if (m_targets.empty()) { error = "no targets"; return false; }
    if (!CompileTargets(error)) return false;

    m_isAdmin = IsRunAsAdmin();
    m_debugPrivilege = EnableDebugPrivilege();
    return true;
}

const std::set<std::string>& MemScan::GetValues(size_t targetIndex) const
{
    static const std::set<std::string> empty;
    if (targetIndex >= m_compiled.size()) return empty;
    return m_compiled[targetIndex].seenValues;
}

// ============ ReadRange / ReadContext ============
bool MemScan::ReadRange(HANDLE h, uintptr_t start, size_t length,
    std::vector<uint8_t>& out) const
{
    out.clear();
    out.reserve(length);
    uintptr_t cur = start, end = start + length;
    while (cur < end) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQueryEx(h, (LPCVOID)cur, &mbi, sizeof(mbi))) return false;
        if (mbi.State != MEM_COMMIT || !IsReadableProtect(mbi.Protect)) return false;

        uintptr_t regionEnd = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
        uintptr_t chunkEnd = (std::min)(regionEnd, end);
        size_t    chunkLen = static_cast<size_t>(chunkEnd - cur);

        std::vector<uint8_t> tmp(chunkLen);
        SIZE_T got = 0;
        if (!ReadProcessMemory(h, (LPCVOID)cur, tmp.data(), chunkLen, &got)
            || got != chunkLen) return false;
        out.insert(out.end(), tmp.begin(), tmp.end());
        cur = chunkEnd;
    }
    return true;
}

bool MemScan::ReadContext(HANDLE h, uintptr_t hit, size_t patLen, Context& out) const
{
    out = {};
    if (m_options.contextBytes == 0) return false;

    const size_t ctx = m_options.contextBytes;
    uintptr_t start = (hit > ctx) ? (hit - ctx) : hit;
    uintptr_t end = hit + patLen + ctx;
    size_t    len = static_cast<size_t>(end - start);

    std::vector<uint8_t> buf;
    if (!ReadRange(h, start, len, buf) || buf.size() < patLen) {
        buf.clear();
        if (!ReadRange(h, hit, patLen, buf)) return false;
        start = hit;
    }
    out.base = start;
    out.hitOffset = static_cast<size_t>(hit - start);
    out.bytes = std::move(buf);
    out.valid = true;
    return true;
}

// ============ string extraction ============
bool MemScan::ExtractStringValue(HANDLE h, uintptr_t valueStart,
    size_t charWidth, size_t maxLen, std::string& out) const
{
    out.clear();
    if (charWidth != 1 && charWidth != 2) return false;
    if (maxLen == 0 || maxLen > 65536) maxLen = 256;

    std::vector<uint8_t> buf(maxLen * charWidth);
    SIZE_T got = 0;
    if (!ReadProcessMemory(h, (LPCVOID)valueStart, buf.data(),
        buf.size(), &got) || got < charWidth) return false;

    for (SIZE_T i = 0; i + charWidth <= got; i += charWidth) {
        if (charWidth == 1) {
            uint8_t c = buf[i];
            if (c == '"') return !out.empty();
            if (c < 0x20 || c >= 0x7F) return false;
            out.push_back((char)c);
        }
        else {
            uint8_t c0 = buf[i], c1 = buf[i + 1];
            if (c0 == '"' && c1 == 0) return !out.empty();
            if (c1 != 0) return false;
            if (c0 < 0x20 || c0 >= 0x7F) return false;
            out.push_back((char)c0);
        }
        if (out.size() >= maxLen) { out.clear(); return false; }
    }
    out.clear();
    return false;
}

// ============ combined scan (single sweep) ============
MemScan::CombinedScanResult MemScan::ScanAll(HANDLE hProcess)
{
    using clock = std::chrono::high_resolution_clock;
    auto t0 = clock::now();
    CombinedScanResult res;

    if (m_compiled.empty()) return res;
    res.hitsPerTarget.resize(m_compiled.size());

    size_t maxPatLen = 0;
    for (const auto& c : m_compiled) maxPatLen = (std::max)(maxPatLen, c.patLen);
    if (maxPatLen == 0) return res;

    // 1) Regions
    std::vector<Region> regions;
    MEMORY_BASIC_INFORMATION mbi{};
    uintptr_t addr = 0;
    while (VirtualQueryEx(hProcess, (LPCVOID)addr, &mbi, sizeof(mbi))) {
        if (!ShouldSkipRegion(mbi))
            regions.push_back({ reinterpret_cast<uintptr_t>(mbi.BaseAddress),
                                mbi.RegionSize, mbi.Protect });
        uintptr_t next = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
        if (next <= addr) break;
        addr = next;
    }
    res.regions = regions.size();

    // 2) Chunks
    std::vector<Chunk> chunks;
    for (const auto& r : regions) {
        for (SIZE_T off = 0; off < r.size; off += m_options.chunkSize) {
            SIZE_T length = (std::min)(m_options.chunkSize, r.size - off);
            chunks.push_back({ r.base, off, length, r.size });
        }
    }
    res.chunks = chunks.size();
    if (chunks.empty()) {
        res.ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            clock::now() - t0).count();
        return res;
    }

    // 3) Scan
    const int n = m_options.threadCount;
    std::atomic<size_t> nextChunk{ 0 };
    const SIZE_T maxOverlap = maxPatLen > 1 ? maxPatLen - 1 : 0;

    struct WorkerStats {
        std::vector<std::vector<uintptr_t>> hitsPerTarget;
        uint64_t bytesRead = 0;
        uint32_t failed = 0;
    };
    std::vector<WorkerStats> wstats(static_cast<size_t>(n));
    for (auto& ws : wstats) ws.hitsPerTarget.resize(m_compiled.size());

    auto worker = [&](size_t tid) {
        WorkerStats& ws = wstats[tid];
        std::vector<uint8_t> buf;

        for (;;) {
            size_t idx = nextChunk.fetch_add(1, std::memory_order_relaxed);
            if (idx >= chunks.size() || !s_running) break;

            const Chunk& c = chunks[idx];
            SIZE_T avail = c.regionSize - c.offset;
            SIZE_T toRead = (std::min)(c.length + maxOverlap, avail);
            if (toRead == 0) continue;
            if (buf.size() < toRead) buf.resize(toRead);

            SIZE_T got = 0;
            if (!ReadProcessMemory(hProcess, (LPCVOID)(c.base + c.offset),
                buf.data(), toRead, &got) && got == 0) {
                ++ws.failed;
                continue;
            }
            if (got == 0) continue;
            ws.bytesRead += got;

            for (size_t ti = 0; ti < m_compiled.size(); ++ti) {
                const auto& ct = m_compiled[ti];
                if (got < ct.patLen) continue;
                SIZE_T maxStart = (std::min)(c.length, got - ct.patLen + 1);
                if (maxStart == 0) continue;

                const uint8_t* data = buf.data();
                auto& hits = ws.hitsPerTarget[ti];

                if (ct.mask[0] != '?') {
                    const uint8_t  first = ct.pattern[0];
                    const uint8_t* cur = data;
                    const uint8_t* stop = data + maxStart;
                    while (cur < stop) {
                        const uint8_t* found = (const uint8_t*)memchr(
                            cur, first, static_cast<size_t>(stop - cur));
                        if (!found) break;
                        SIZE_T i = static_cast<SIZE_T>(found - data);
                        bool ok = true;
                        for (SIZE_T k = 1; k < ct.patLen; ++k) {
                            if (ct.mask[k] == '?') continue;
                            if (data[i + k] != ct.pattern[k]) { ok = false; break; }
                        }
                        if (ok) hits.push_back(c.base + c.offset + i);
                        cur = found + 1;
                    }
                }
                else {
                    for (SIZE_T i = 0; i < maxStart; ++i) {
                        bool ok = true;
                        for (SIZE_T k = 1; k < ct.patLen; ++k) {
                            if (ct.mask[k] == '?') continue;
                            if (data[i + k] != ct.pattern[k]) { ok = false; break; }
                        }
                        if (ok) hits.push_back(c.base + c.offset + i);
                    }
                }
            }
        }
        };

    std::vector<std::thread> threads;
    threads.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) threads.emplace_back(worker, static_cast<size_t>(i));
    for (auto& t : threads) t.join();

    // 4) Merge
    for (size_t ti = 0; ti < m_compiled.size(); ++ti) {
        size_t total = 0;
        for (const auto& ws : wstats) total += ws.hitsPerTarget[ti].size();
        res.hitsPerTarget[ti].reserve(total);
        for (auto& ws : wstats) {
            auto& src = ws.hitsPerTarget[ti];
            res.hitsPerTarget[ti].insert(res.hitsPerTarget[ti].end(),
                src.begin(), src.end());
        }
    }
    for (const auto& ws : wstats) {
        res.bytesRead += ws.bytesRead;
        res.failed += ws.failed;
    }

    res.ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        clock::now() - t0).count();
    return res;
}

// ============ Run ============
int MemScan::Run()
{
    if (m_compiled.empty()) {
        std::string err;
        if (!Initialize(err)) return 1;
    }

    long long round = 0;
    DWORD     lastPid = 0;
    bool      connected = false;
    bool      everConnected = false;
    bool      wasRestarted = false;
    int       zeroStreak = 0;

    DWORD     cachedPid = 0;
    FILETIME  cachedCreateTime{};

    while (s_running) {
        ++round;

        HANDLE hProc = nullptr;
        DWORD  pid = 0;
        bool   isNew = false;
        DWORD  lastError = 0;
        std::vector<DWORD> triedPids;
        bool   targetMissing = false;

        // ---- locate process ----
        if (m_isPid) {
            hProc = OpenProcessForScan(m_fixedPid, &lastError);
            if (hProc && !IsProcessAlive(hProc)) {
                CloseHandle(hProc);
                hProc = nullptr;
            }
            pid = m_fixedPid;
            triedPids.push_back(m_fixedPid);
        }
        else {
            // cached PID
            if (cachedPid) {
                DWORD err = 0;
                HANDLE h = OpenProcessForScan(cachedPid, &err);
                if (h) {
                    FILETIME ct;
                    if (IsProcessAlive(h) && GetProcessCreateTime(h, ct)
                        && CompareFileTime(&ct, &cachedCreateTime) == 0) {
                        hProc = h;
                        pid = cachedPid;
                    }
                    else {
                        CloseHandle(h);
                    }
                }
                if (!hProc) {
                    lastError = err;
                    cachedPid = 0;
                    cachedCreateTime = {};
                }
            }
            // enumerate
            if (!hProc) {
                triedPids = FindAllPidsByName(m_procName);
                if (triedPids.empty()) {
                    targetMissing = true;
                }
                else {
                    DWORD firstErr = 0;
                    for (DWORD p : triedPids) {
                        DWORD err = 0;
                        HANDLE h = OpenProcessForScan(p, &err);
                        if (!h) { if (!firstErr) firstErr = err; continue; }
                        if (!IsProcessAlive(h)) { CloseHandle(h); continue; }
                        FILETIME ct;
                        if (!GetProcessCreateTime(h, ct)) {
                            CloseHandle(h);
                            if (!firstErr) firstErr = GetLastError();
                            continue;
                        }
                        cachedPid = p;
                        cachedCreateTime = ct;
                        hProc = h;
                        pid = p;
                        isNew = true;
                        break;
                    }
                    lastError = firstErr;
                }
            }
        }

        // ---- failure path ----
        if (!hProc) {
            if (m_isPid && m_options.exitWhenTargetMissing) {
                RoundStatus rs;
                rs.state = RoundState::OpenFailed;
                rs.round = round;
                rs.pid = m_fixedPid;
                rs.lastError = lastError;
                rs.triedPids = triedPids;
                if (m_onStatus) m_onStatus(rs);
                return everConnected ? 0 : 1;
            }

            if (targetMissing) {
                if (connected) {
                    ProcessEvent ev;
                    ev.type = ProcessEventType::Disconnected;
                    ev.procName = m_procName;
                    ev.oldPid = lastPid;
                    if (m_onProcess) m_onProcess(ev);
                    connected = false;
                    lastPid = 0;
                }
                RoundStatus rs;
                rs.state = RoundState::WaitingProcess;
                rs.round = round;
                if (m_onStatus) m_onStatus(rs);
            }
            else {
                RoundStatus rs;
                rs.state = RoundState::OpenFailed;
                rs.round = round;
                rs.pid = m_isPid ? m_fixedPid
                    : (triedPids.empty() ? 0 : triedPids.front());
                rs.lastError = lastError;
                rs.triedPids = triedPids;
                if (m_onStatus) m_onStatus(rs);
            }

            InterruptibleSleep(m_options.intervalMs);
            continue;
        }

        // ---- connect / restart ----
        if (!connected) {
            ProcessEvent ev;
            ev.type = ProcessEventType::Connected;
            ev.procName = m_procName.empty() ? std::to_string(pid) : m_procName;
            ev.newPid = pid;
            if (m_onProcess) m_onProcess(ev);
        }
        else if (lastPid != pid) {
            ProcessEvent ev;
            ev.type = ProcessEventType::Restarted;
            ev.procName = m_procName.empty() ? std::to_string(pid) : m_procName;
            ev.oldPid = lastPid;
            ev.newPid = pid;
            if (m_onProcess) m_onProcess(ev);
            wasRestarted = true;
        }
        connected = true;
        everConnected = true;
        lastPid = pid;

        // pre-scan liveness
        if (!IsProcessAlive(hProc)) {
            cachedPid = 0;
            cachedCreateTime = {};
            CloseHandle(hProc);
            InterruptibleSleep(m_options.intervalMs);
            continue;
        }

        // ---- scan ----
        CombinedScanResult scan = ScanAll(hProc);
        size_t roundTotalHits = 0;
        size_t roundNew = 0;

        for (size_t ti = 0; ti < m_compiled.size(); ++ti) {
            auto& ct = m_compiled[ti];
            for (uintptr_t addr : scan.hitsPerTarget[ti]) {
                ++roundTotalHits;
                Hit h;
                h.targetIndex = ti;
                h.label = m_targets[ti].label;
                h.address = addr;
                h.afterRestart = wasRestarted;

                if (ct.extractString) {
                    std::string val;
                    if (!ExtractStringValue(hProc, addr + ct.patLen,
                        ct.charWidth, ct.maxStringLen, val)) continue;
                    h.value = val;
                    if (ct.deduplicate) {
                        h.isNewValue = ct.seenValues.insert(val).second;
                        if (h.isNewValue) ++roundNew;
                    }
                    else {
                        h.isNewValue = true;
                    }
                }
                else {
                    h.isNewValue = true;
                }

                if (m_options.contextBytes > 0
                    && (!ct.deduplicate || h.isNewValue))
                    ReadContext(hProc, addr, ct.patLen, h.context);

                if (m_onHit) m_onHit(h);
            }
        }
        wasRestarted = false;

        // post-scan liveness
        if (!IsProcessAlive(hProc)) {
            cachedPid = 0;
            cachedCreateTime = {};
            CloseHandle(hProc);
            InterruptibleSleep(m_options.intervalMs);
            continue;
        }

        if (scan.bytesRead == 0 && !isNew) ++zeroStreak;
        else zeroStreak = 0;

        if (m_onStatus) {
            RoundStatus rs;
            rs.state = RoundState::Scanning;
            rs.round = round;
            rs.pid = pid;
            rs.regions = scan.regions;
            rs.chunks = scan.chunks;
            rs.bytesRead = scan.bytesRead;
            rs.failedReads = scan.failed;
            rs.totalHits = roundTotalHits;
            rs.newValues = roundNew;
            rs.elapsedMs = scan.ms;
            rs.zeroByteStreak = zeroStreak;
            m_onStatus(rs);
        }

        CloseHandle(hProc);
        if (!s_running) break;
        InterruptibleSleep(m_options.intervalMs);
    }

    return 0;
}