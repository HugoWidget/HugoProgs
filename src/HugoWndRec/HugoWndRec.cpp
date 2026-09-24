/*
 * Copyright 2025-2026 howdy213, JYardX
 *
 * This file is part of HugoProgs.
 *
 * HugoProgs is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HugoProgs is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with HugoProgs. If not, see <https://www.gnu.org/licenses/>.
 */
#include "WinUtils/WinPch.h"

#include <Windows.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <cwctype>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "WinUtils/Console.h"
#include "WinUtils/Logger.h"
#include "WinUtils/StrConvert.h"
#include "WinUtils/WinUtils.h"

#include "HugoWndRecUtils.h"

using namespace std;
using namespace WinUtils;
namespace fs = std::filesystem;

const wstring TARGET_PROCESS_NAME = L"SeewoServiceAssistant";
const wstring TARGET_WINDOW_TITLE = L"希沃管家";
const wstring TARGET_WINDOW_CLASS = L"Chrome_WidgetWin_0";

const fs::path SEEN_SIZES_FILE = L"HugoWndSeenSizes.txt";
const fs::path BLOCKED_SIZES_FILE = L"HugoWndBlockedSizes.txt";
const fs::path SCREENSHOT_DIR = L"HugoWndScreenshots";
const fs::path LOG_FILE = L"HugoWndRec.log";
const wstring SCREENSHOT_EXT = L"bmp";

const int SCAN_INTERVAL_MS = 1000;

struct WindowInfo
{
	HWND hwnd = nullptr;
	wstring hwndHex;
	wstring title;
	wstring className;
	DWORD pid = 0;
	DWORD threadId = 0;
	wstring processName;
	bool isVisible = false;
	bool isIconic = false;
	RECT rect{};
	int width = 0;
	int height = 0;
	wstring sizeKey;
};

set<wstring> g_seenSizes;
set<wstring> g_blockedSizes;
vector<WindowInfo> g_latestWindows;
mutex g_stateMutex;
atomic<bool> g_running{ true };

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
	auto* out = reinterpret_cast<vector<WindowInfo>*>(lParam);

	if (GetWindowTitleString(hwnd) != TARGET_WINDOW_TITLE) return TRUE;

	wchar_t cls[256] = {};
	int cl = GetClassNameW(hwnd, cls, 255);
	if (cl <= 0) return TRUE;
	if (TARGET_WINDOW_CLASS != cls) return TRUE;

	DWORD pid = 0;
	DWORD tid = GetWindowThreadProcessId(hwnd, &pid);
	if (pid == 0) return TRUE;

	wstring pname = GetProcessNameByPid(pid);
	if (pname.empty()) return TRUE;

	wstring pl = pname, tlw = TARGET_PROCESS_NAME;
	transform(pl.begin(), pl.end(), pl.begin(),
		[](wchar_t c) { return (wchar_t)towlower(c); });
	transform(tlw.begin(), tlw.end(), tlw.begin(),
		[](wchar_t c) { return (wchar_t)towlower(c); });
	if (pl.find(tlw) == wstring::npos) return TRUE;

	WindowInfo info;
	info.hwnd = hwnd;
	info.hwndHex = format(L"0x{:X}", reinterpret_cast<uintptr_t>(hwnd));
	info.title = TARGET_WINDOW_TITLE;
	info.className = cls;
	info.pid = pid;
	info.threadId = tid;
	info.processName = pname;
	info.isVisible = IsWindowVisible(hwnd) != FALSE;
	info.isIconic = IsIconic(hwnd) != FALSE;
	info.rect = WinUtils::GetWindowRect(hwnd);
	info.width = info.rect.right - info.rect.left;
	info.height = info.rect.bottom - info.rect.top;
	info.sizeKey = format(L"{}x{}", info.width, info.height);

	out->push_back(move(info));
	return TRUE;
}

vector<WindowInfo> FindSeewoWindows()
{
	vector<WindowInfo> result;
	EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&result));
	return result;
}

bool CloseWindow2(HWND hwnd)
{
	return PostMessageW(hwnd, WM_CLOSE, 0, 0) != FALSE;
}

int CloseWindowsBySize(const wstring& sizeKey, const wstring& reason)
{
	int count = 0;
	auto windows = FindSeewoWindows();
	for (const auto& info : windows) {
		if (info.sizeKey != sizeKey) continue;
		if (CloseWindow2(info.hwnd)) {
			LogEvent(L"关闭", format(L"尺寸={} hwnd={} pid={} 原因={}",
				sizeKey, info.hwndHex, info.pid, reason));
			++count;
		}
	}
	return count;
}

// 后台扫描线程
struct CaptureTask
{
	wstring sizeKey;
	HWND hwnd = nullptr;
	fs::path path;
};

void ScanWorker()
{
	while (g_running.load()) {
		try {
			auto windows = FindSeewoWindows();

			set<wstring> blockedSnapshot;
			{
				lock_guard<mutex> lk(g_stateMutex);
				blockedSnapshot = g_blockedSizes;
			}

			vector<WindowInfo> active;
			set<wstring> sizesThisRound;
			vector<CaptureTask> toCapture;
			vector<wstring> newSizes;
			for (auto& info : windows) {
				if (info.width <= 0 || info.height <= 0) continue;

				HWND hwnd = info.hwnd;
				const wstring& sizeKey = info.sizeKey;

				// 命中拦截 -> 直接关闭
				if (blockedSnapshot.count(sizeKey)) {
					if (CloseWindow2(hwnd)) {
						LogEvent(L"关闭", format(L"尺寸={} hwnd={} pid={} 原因=拦截",
							sizeKey, info.hwndHex, info.pid));
					}
					continue;
				}

				// 记录新尺寸
				bool isNew = false;
				{
					lock_guard<mutex> lk(g_stateMutex);
					if (g_seenSizes.find(sizeKey) == g_seenSizes.end()) {
						g_seenSizes.insert(sizeKey);
						isNew = true;
					}
				}
				if (isNew) {
					newSizes.push_back(sizeKey);
					LogEvent(L"检测", format(L"尺寸={} hwnd={} pid={}",
						sizeKey, info.hwndHex, info.pid));
					PrintLine(L"");
					PrintLine(L"[扫描] 发现新尺寸：" + sizeKey);
				}

				// 排队截图
				if (sizesThisRound.insert(sizeKey).second) {
					fs::path shot = SCREENSHOT_DIR / (sizeKey + L"." + SCREENSHOT_EXT);
					error_code ec;
					if (!fs::exists(shot, ec)) {
						toCapture.push_back({ sizeKey, hwnd, shot });
					}
				}

				active.push_back(info);
			}

			// 保存新尺寸
			if (!newSizes.empty()) {
				set<wstring> snapshot;
				{
					lock_guard<mutex> lk(g_stateMutex);
					snapshot = g_seenSizes;
				}
				SaveSizeSet(SEEN_SIZES_FILE, snapshot);
			}

			// 截图（在锁外执行，避免阻塞）
			if (!toCapture.empty()) {
				error_code ec;
				fs::create_directories(SCREENSHOT_DIR, ec);
				for (auto& t : toCapture) {
					if (!IsWindow(t.hwnd)) continue;
					if (CaptureWindowToBmp(t.hwnd, t.path)) {
						PrintLine(L"[扫描] 已保存截图：" + t.path.wstring());
					}
				}
			}

			{
				lock_guard<mutex> lk(g_stateMutex);
				g_latestWindows = move(active);
			}
		}
		catch (const exception& e) {
			PrintLine(L"\n[扫描错误] " + AnsiToWide(e.what()));
		}

		// 可中断睡眠
		for (int i = 0; i < SCAN_INTERVAL_MS / 100 && g_running.load(); ++i) {
			this_thread::sleep_for(chrono::milliseconds(100));
		}
	}
}

// 菜单交互
void ShowHeader()
{
	PrintLine(L"");
	PrintLine(L"==============================================================");
	PrintLine(L"                    希沃管家窗口监视器");
	PrintLine(L"==============================================================");
}

bool CaptureSizeNow(const wstring& sizeKey, const fs::path& path)
{
	HWND hwnd = nullptr;
	{
		lock_guard<mutex> lk(g_stateMutex);
		for (const auto& w : g_latestWindows) {
			if (w.sizeKey == sizeKey) { hwnd = w.hwnd; break; }
		}
	}
	if (!hwnd) return false;
	error_code ec;
	fs::create_directories(SCREENSHOT_DIR, ec);
	return CaptureWindowToBmp(hwnd, path);
}

void AddBlock(const wstring& sizeKey)
{
	bool already = false;
	set<wstring> snapshot;
	{
		lock_guard<mutex> lk(g_stateMutex);
		already = g_blockedSizes.count(sizeKey) > 0;
		g_blockedSizes.insert(sizeKey);
		snapshot = g_blockedSizes;
	}
	SaveSizeSet(BLOCKED_SIZES_FILE, snapshot);

	if (already) PrintLine(L"  " + sizeKey + L" 已在拦截列表中。");
	else         PrintLine(L"  已加入拦截：" + sizeKey);

	int n = CloseWindowsBySize(sizeKey, L"拦截");
	if (n > 0)
		PrintLine(format(L"  已关闭 {} 个同尺寸窗口。", n));
	this_thread::sleep_for(chrono::milliseconds(1000));
}

void RemoveBlock(const wstring& sizeKey)
{
	bool changed = false;
	set<wstring> snapshot;
	{
		lock_guard<mutex> lk(g_stateMutex);
		if (g_blockedSizes.erase(sizeKey)) changed = true;
		snapshot = g_blockedSizes;
	}
	if (changed) {
		SaveSizeSet(BLOCKED_SIZES_FILE, snapshot);
		PrintLine(L"  已移除拦截：" + sizeKey + L"（允许再次显示）");
	}
	else {
		PrintLine(L"  " + sizeKey + L" 不在拦截列表中。");
	}
	this_thread::sleep_for(chrono::milliseconds(1000));
}

void ManageSizesMenu()
{
	while (g_running.load()) {
		vector<wstring> allSizes;
		set<wstring> blockedSnapshot;
		vector<WindowInfo> activeSnapshot;
		{
			lock_guard<mutex> lk(g_stateMutex);
			set<wstring> merged;
			merged.insert(g_seenSizes.begin(), g_seenSizes.end());
			merged.insert(g_blockedSizes.begin(), g_blockedSizes.end());
			allSizes.assign(merged.begin(), merged.end());
			sort(allSizes.begin(), allSizes.end(), SizeLess);
			blockedSnapshot = g_blockedSizes;
			activeSnapshot = g_latestWindows;
		}

		PrintLine(L"");
		PrintLine(L"==============================================================");
		PrintLine(L"  已有尺寸列表");
		PrintLine(L"==============================================================");

		if (allSizes.empty()) {
			PrintLine(L"  （暂无记录）");
			Print(L"  按回车返回...");
			ReadLine();
			return;
		}

		// 表头
		PrintLine(L"  编号  尺寸            状态     截图   当前窗口");
		PrintLine(L"--------------------------------------------------------------");
		for (size_t i = 0; i < allSizes.size(); ++i) {
			const auto& sk = allSizes[i];
			wstring status = blockedSnapshot.count(sk) ? L"已拦截" : L"正常";
			fs::path shot = SCREENSHOT_DIR / (sk + L"." + SCREENSHOT_EXT);
			error_code ec;
			wstring shotFlag = fs::exists(shot, ec) ? L"有" : L"无";
			int cur = 0;
			for (const auto& w : activeSnapshot)
				if (w.sizeKey == sk) ++cur;

			PrintLine(format(L"  [{}]   {}   {}   {}     {}", i + 1, sk, status, shotFlag, cur));
		}
		PrintLine(L"--------------------------------------------------------------");
		PrintLine(L"  命令（命令 + 编号，如 v1）:");
		PrintLine(L"    v<N>  查看/打开截图");
		PrintLine(L"    s<N>  立即重新截图");
		PrintLine(L"    b<N>  加入拦截（关闭并禁止再显示）");
		PrintLine(L"    u<N>  移除拦截（允许再次显示）");
		PrintLine(L"    d<N>  删除截图文件");
		PrintLine(L"    0     返回上级菜单");

		Print(L"  请输入命令: ");
		wstring raw = ReadLine();
		if (raw.empty() || raw == L"0" || raw == L"q" || raw == L"Q") return;

		wchar_t action = (wchar_t)towlower(raw[0]);
		wstring numPart = raw.substr(1);
		if (numPart.empty() ||
			!all_of(numPart.begin(), numPart.end(),
				[](wchar_t c) { return iswdigit(c) != 0; })) {
			PrintLine(L"  无效命令，请使用例如 v1 / s2 / b3 / u4 / d5 的格式。");
			this_thread::sleep_for(chrono::milliseconds(1200));
			continue;
		}
		int idx = stoi(numPart);
		if (idx < 1 || idx > (int)allSizes.size()) {
			PrintLine(L"  编号超出范围。");
			this_thread::sleep_for(chrono::milliseconds(1200));
			continue;
		}

		const wstring sk = allSizes[idx - 1];
		fs::path shot = SCREENSHOT_DIR / (sk + L"." + SCREENSHOT_EXT);
		error_code ec;
		bool hasShot = fs::exists(shot, ec);

		switch (action) {
		case L'v': {   // 查看 / 打开截图
			if (hasShot) {
				PrintLine(L"  打开：" + shot.wstring());
				OpenFile(shot);
				this_thread::sleep_for(chrono::milliseconds(400));
			}
			else {
				Print(L"  该尺寸暂无截图，是否立即截图？(y/N): ");
				wstring yn = ReadLine();
				if (!yn.empty() && (yn[0] == L'y' || yn[0] == L'Y')) {
					if (CaptureSizeNow(sk, shot)) {
						PrintLine(L"  已保存：" + shot.wstring());
						OpenFile(shot);
					}
					else {
						PrintLine(L"  当前没有该尺寸的活跃窗口，无法截图。");
					}
					this_thread::sleep_for(chrono::milliseconds(800));
				}
			}
			break;
		}
		case L's': {   // 立即截图
			if (CaptureSizeNow(sk, shot)) {
				PrintLine(L"  已保存截图：" + shot.wstring());
				OpenFile(shot);
			}
			else {
				PrintLine(L"  当前没有该尺寸的活跃窗口，无法截图。");
			}
			this_thread::sleep_for(chrono::milliseconds(800));
			break;
		}
		case L'b': AddBlock(sk); break;
		case L'u': RemoveBlock(sk); break;
		case L'd': {   // 删除截图
			if (hasShot) {
				if (fs::remove(shot, ec))
					PrintLine(L"  已删除：" + shot.wstring());
				else
					PrintLine(L"  删除失败。");
			}
			else {
				PrintLine(L"  没有可删除的截图。");
			}
			this_thread::sleep_for(chrono::milliseconds(800));
			break;
		}
		default:
			PrintLine(L"  无效命令。");
			this_thread::sleep_for(chrono::milliseconds(800));
			break;
		}
	}
}

void ShowCurrentWindows()
{
	vector<WindowInfo> windows;
	{
		lock_guard<mutex> lk(g_stateMutex);
		windows = g_latestWindows;
	}
	PrintLine(L"");
	PrintLine(L"==============================================================");
	PrintLine(format(L"  当前匹配窗口（共 {} 个）", windows.size()));
	PrintLine(L"==============================================================");
	if (windows.empty()) {
		PrintLine(L"  （暂无）");
	}
	else {
		for (size_t i = 0; i < windows.size(); ++i) {
			const auto& w = windows[i];
			PrintLine(format(L"  [{}] 尺寸={}  hwnd={}  pid={}  可见={}  最小化={}",
				i + 1, w.sizeKey, w.hwndHex, w.pid,
				w.isVisible ? 1 : 0, w.isIconic ? 1 : 0));
		}
	}
	Print(L"  按回车返回...");
	ReadLine();
}

void ShowLogTail()
{
	PrintLine(L"");
	PrintLine(L"==============================================================");
	PrintLine(L"  日志文件：" + LOG_FILE.wstring());
	PrintLine(L"==============================================================");

	error_code ec;
	if (!fs::exists(LOG_FILE, ec)) {
		PrintLine(L"  （日志文件尚不存在）");
		Print(L"  按回车返回...");
		ReadLine();
		return;
	}

	vector<wstring> lines;
	{
		ifstream f(LOG_FILE, ios::binary);
		string line;
		while (getline(f, line)) lines.push_back(AnsiToWide(line));
	}
	size_t tail = min<size_t>(30, lines.size());
	for (size_t i = lines.size() - tail; i < lines.size(); ++i)
		PrintLine(L"  " + lines[i]);
	PrintLine(L"--------------------------------------------------------------");
	PrintLine(format(L"  共 {} 行，以上显示最近 {} 行。", lines.size(), tail));
	system("pause");
}

void MainMenu()
{
	while (g_running.load()) {
		size_t nSeen = 0, nBlocked = 0, nActive = 0;
		{
			lock_guard<mutex> lk(g_stateMutex);
			set<wstring> merged;
			merged.insert(g_seenSizes.begin(), g_seenSizes.end());
			merged.insert(g_blockedSizes.begin(), g_blockedSizes.end());
			nSeen = merged.size();
			nBlocked = g_blockedSizes.size();
			nActive = g_latestWindows.size();
		}

		ShowHeader();
		PrintLine(format(L"  已记录尺寸: {}    已拦截: {}    当前匹配窗口: {}",
			nSeen, nBlocked, nActive));
		PrintLine(L"--------------------------------------------------------------");
		PrintLine(L"  [1] 管理尺寸列表");
		PrintLine(L"  [2] 查看当前匹配窗口");
		PrintLine(L"  [3] 查看日志文件");
		PrintLine(L"  [0] 退出");
		PrintLine(L"--------------------------------------------------------------");

		Print(L"  请选择: ");
		wstring choice = ReadLine();
		system("cls");
		if (choice.empty()) continue;
		if (choice == L"1")             ManageSizesMenu();
		else if (choice == L"2")        ShowCurrentWindows();
		else if (choice == L"3")        ShowLogTail();
		else if (choice == L"0")        { g_running.store(false); return; }
		system("cls");
	}
}

int wmain(int argc, wchar_t* argv[])
{
	Console().setLocale();
	SetProcessDPIAware();
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	EnsureSingleInstance(true);

	LoggerCore::Inst().SetDefaultStrategies(LOG_FILE.wstring());
	LoggerCore::Inst().EnableApartment(DftLogger);

	g_seenSizes = LoadSizeSet(SEEN_SIZES_FILE);
	g_blockedSizes = LoadSizeSet(BLOCKED_SIZES_FILE);

	ShowHeader();
	PrintLine(L"  过滤条件：");
	PrintLine(L"    · 进程名包含 : " + TARGET_PROCESS_NAME);
	PrintLine(L"    · 窗口标题   : " + TARGET_WINDOW_TITLE);
	PrintLine(L"    · 窗口类名   : " + TARGET_WINDOW_CLASS);
	PrintLine(L"  已记录尺寸文件 : " + SEEN_SIZES_FILE.wstring());
	PrintLine(L"  拦截尺寸文件   : " + BLOCKED_SIZES_FILE.wstring());
	PrintLine(L"  截图目录       : " + SCREENSHOT_DIR.wstring() + L"/<宽x高>." + SCREENSHOT_EXT);
	PrintLine(L"  日志文件       : " + LOG_FILE.wstring() + L"  （只记录新尺寸检测 + 关闭窗口）");
	PrintLine(L"  后台已启动，将持续扫描并按拦截规则自动关闭窗口。");

	// 启动后台扫描线程
	thread worker(ScanWorker);

	try {
		MainMenu();
	}
	catch (...) {
		// 忽略
	}

	g_running.store(false);
	if (worker.joinable()) worker.join();

	// 保存状态
	set<wstring> seenSnapshot, blockedSnapshot;
	{
		lock_guard<mutex> lk(g_stateMutex);
		seenSnapshot = g_seenSizes;
		blockedSnapshot = g_blockedSizes;
	}
	SaveSizeSet(SEEN_SIZES_FILE, seenSnapshot);
	SaveSizeSet(BLOCKED_SIZES_FILE, blockedSnapshot);

	PrintLine(L"");
	PrintLine(L"  已保存：" + SEEN_SIZES_FILE.wstring() + L", " + BLOCKED_SIZES_FILE.wstring());
	PrintLine(L"  程序已退出。");
	return 0;
}