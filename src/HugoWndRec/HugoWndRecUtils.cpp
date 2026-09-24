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
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "WinUtils/Logger.h"
#include "WinUtils/StrConvert.h"
#include "WinUtils/WinUtils.h"

#include "HugoWndRecUtils.h"

using namespace std;
using namespace WinUtils;
namespace fs = std::filesystem;

// 打印（后台扫描线程与菜单线程共用，需互斥）
namespace {
	mutex g_printMutex;
}

void Print(const wstring& s)
{
	lock_guard<mutex> lk(g_printMutex);
	if (s.empty()) return;
	wcout << s;
	wcout.flush();
}

void PrintLine(const wstring& s) { Print(s + L"\n"); }

wstring Trim(const wstring& s)
{
	size_t start = s.find_first_not_of(L" \t\r\n");
	if (start == wstring::npos) return L"";
	size_t end = s.find_last_not_of(L" \t\r\n");
	return s.substr(start, end - start + 1);
}

wstring ReadLine()
{
	wstring line;
	if (!getline(wcin, line)) return L"";
	return Trim(line);
}

// 日志只记录 [检测] / [关闭] 两类事件，格式由 Logger 的默认 LogFormatter 生成
void LogEvent(const wstring& event, const wstring& detail)
{
	WuLog::Info(event + L" " + detail);
}

void OpenFile(const fs::path& path)
{
	RunExternalProgram(path.wstring());
}

// 尺寸解析 / 存取
optional<pair<int, int>> ParseSize(const wstring& s)
{
	wstring t = Trim(s);
	auto pos = t.find_first_of(L"xX*");
	if (pos == wstring::npos || pos == 0 || pos == t.size() - 1)
		return nullopt;
	try {
		int w = stoi(t.substr(0, pos));
		int h = stoi(t.substr(pos + 1));
		if (w <= 0 || h <= 0) return nullopt;
		return make_pair(w, h);
	}
	catch (...) {
		return nullopt;
	}
}

bool SizeLess(const wstring& a, const wstring& b)
{
	auto pa = ParseSize(a);
	auto pb = ParseSize(b);
	if (pa && pb) return *pa < *pb;
	return a < b;
}

set<wstring> LoadSizeSet(const fs::path& path)
{
	set<wstring> sizes;
	ifstream f(path, ios::binary);
	if (!f) return sizes;
	string line;
	while (getline(f, line)) {
		if (line.empty()) continue;
		wstring w = Utf8ToWide(line);
		if (w.empty()) continue;
		auto sz = ParseSize(w);
		if (sz) sizes.insert(format(L"{}x{}", sz->first, sz->second));
	}
	return sizes;
}

void SaveSizeSet(const fs::path& path, const set<wstring>& sizes)
{
	vector<wstring> sorted(sizes.begin(), sizes.end());
	sort(sorted.begin(), sorted.end(), SizeLess);

	fs::path tmp = path;
	tmp += L".tmp";
	{
		ofstream f(tmp, ios::binary);
		if (!f) return;
		for (const auto& s : sorted) {
			string u8 = WideToUtf8(s);
			f.write(u8.data(), (streamsize)u8.size());
			f.write("\n", 1);
		}
	}
	error_code ec;
	fs::rename(tmp, path, ec);
	if (ec) fs::remove(path, ec), fs::rename(tmp, path, ec);
}

// 进程 / 窗口
wstring GetProcessNameByPid(DWORD pid)
{
	if (pid == 0) return L"";
	HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
	if (!h) return L"";
	wchar_t buf[MAX_PATH * 2] = {};
	DWORD size = (DWORD)(sizeof(buf) / sizeof(wchar_t));
	wstring name;
	if (QueryFullProcessImageNameW(h, 0, buf, &size))
		name = GetFileNameFromPath(wstring(buf, size));
	CloseHandle(h);
	return name;
}

// 纯 Win32 截图
