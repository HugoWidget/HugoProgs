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

#include "WinUtils/WinUtils.h"
#include "WinUtils/ConsoleMenu.h"
#include "ExtensionMenu.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <iterator>
#include <string>
#include <map>
#include <filesystem>
#include <cstdlib>
#include <cwctype>
#include <conio.h>

using namespace std;
using namespace WinUtils;
namespace fs = filesystem;

// Defined in HugoProgs.cpp: run a program in the current console (blocking) and
// inherit the standard handles so its output shows in this window.
bool ExecuteProgramInCurrentConsole(const wstring& programPath, const wstring& args, bool wait, DWORD* exitCode);

namespace {

	// extension.ini is an optional file in the program directory. Each section
	// defines one custom menu item: the section name is the command name, and the
	// fields below describe how to launch the target program (similar to LaunchItem).
	const wchar_t kIniFileName[] = L"extension.ini";
	const wchar_t kSubmenuName[] = L"extension";
	const wchar_t kSubmenuDesc[] = L"扩展工具";
	const wchar_t kKeyProgram[] = L"program";
	const wchar_t kKeyDesc[] = L"desc";
	const wchar_t kKeyParams[] = L"params";
	const wchar_t kKeyRequireAdmin[] = L"requireadmin";
	const wchar_t kKeyRunAsAdmin[] = L"runasadmin";
	const wchar_t kKeyInConsole[] = L"inconsole";
	const wchar_t kKeyShowWnd[] = L"showwnd";
	const wchar_t kOperationRunAs[] = L"runas";
	const wchar_t kOperationOpen[] = L"open";
	const int kDefaultShowWindow = SW_SHOWNORMAL;
	const int kHiddenShowWindow = SW_HIDE;
	const size_t kUtf8BomLength = 3;
	const size_t kUtf16BomLength = 2;

	// Trim leading/trailing whitespace in place
	void TrimInPlace(wstring& text)
	{
		size_t start = text.find_first_not_of(L" \t\r\n");
		if (start == wstring::npos) {
			text.clear();
			return;
		}
		size_t end = text.find_last_not_of(L" \t\r\n");
		text = text.substr(start, end - start + 1);
	}

	// Parse a boolean INI value (case-insensitive); return defaultValue when unrecognized
	bool ParseIniBool(const wstring& value, bool defaultValue)
	{
		wstring normalized = value;
		transform(normalized.begin(), normalized.end(), normalized.begin(), ::towlower);
		if (normalized == L"true" || normalized == L"1" || normalized == L"yes") return true;
		if (normalized == L"false" || normalized == L"0" || normalized == L"no") return false;
		return defaultValue;
	}

	// Read the whole file and decode it into a wide string, handling UTF-8/UTF-16 BOMs.
	// Without a BOM, UTF-8 is tried first and ANSI is used as a fallback.
	bool ReadTextFileWide(const wstring& path, wstring& out)
	{
		ifstream file(path, ios::binary);
		if (!file.is_open()) return false;
		string bytes((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
		file.close();

		out.clear();
		if (bytes.empty()) return true;

		const unsigned char* data = reinterpret_cast<const unsigned char*>(bytes.data());
		size_t size = bytes.size();

		// UTF-16LE with BOM
		if (size >= kUtf16BomLength && data[0] == 0xFF && data[1] == 0xFE) {
			const wchar_t* wideData = reinterpret_cast<const wchar_t*>(bytes.data() + kUtf16BomLength);
			out.assign(wideData, (size - kUtf16BomLength) / sizeof(wchar_t));
			return true;
		}

		// UTF-8, optionally prefixed by a BOM
		size_t offset = 0;
		if (size >= kUtf8BomLength && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF)
			offset = kUtf8BomLength;

		int charCount = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
			bytes.data() + offset, static_cast<int>(size - offset), nullptr, 0);
		UINT codePage = CP_UTF8;
		if (charCount == 0) {
			// Not valid UTF-8, treat the file as ANSI
			offset = 0;
			codePage = CP_ACP;
			charCount = MultiByteToWideChar(codePage, 0,
				bytes.data(), static_cast<int>(size), nullptr, 0);
		}
		if (charCount <= 0) return false;

		out.resize(static_cast<size_t>(charCount));
		MultiByteToWideChar(codePage, (codePage == CP_UTF8) ? MB_ERR_INVALID_CHARS : 0,
			bytes.data() + offset, static_cast<int>(size - offset), out.data(), charCount);
		return true;
	}

	// Parse INI text into section -> (lowercase key -> value) pairs
	void ParseIniSections(const wstring& content,
		map<wstring, map<wstring, wstring>>& sections)
	{
		wistringstream stream(content);
		wstring line;
		wstring currentSection;
		while (getline(stream, line))
		{
			TrimInPlace(line);
			if (line.empty() || line[0] == L';' || line[0] == L'#') continue;

			if (line.front() == L'[' && line.back() == L']') {
				currentSection = line.substr(1, line.size() - 2);
				TrimInPlace(currentSection);
				continue;
			}

			size_t separator = line.find(L'=');
			if (separator == wstring::npos || currentSection.empty()) continue;

			wstring key = line.substr(0, separator);
			wstring value = line.substr(separator + 1);
			TrimInPlace(key);
			TrimInPlace(value);
			if (key.empty()) continue;

			transform(key.begin(), key.end(), key.begin(), ::towlower);
			sections[currentSection][key] = value;
		}
	}

} // namespace

void LoadExtensionMenu(ConsoleMenu& menu)
{
	wstring iniPath = GetCurrentProcessDir() + kIniFileName;
	if (!fs::exists(iniPath)) {
		std::ofstream outfile(iniPath);
		if (outfile) {
			outfile << "[MyTool]\n"
				<< "Desc = Description\n"
				<< "Program = .\\tools\\MyTool.bat\n"
				<< "Params = --foo bar\n"
				<< "RequireAdmin = false\n"
				<< "InConsole = true\n"
				<< "ShowWnd = 1\n";
			outfile.close();
		}
	}

	wstring content;
	if (!ReadTextFileWide(iniPath, content)) {
		wcerr << L"警告：无法读取 " << iniPath << endl;
		return;
	}

	map<wstring, map<wstring, wstring>> sections;
	ParseIniSections(content, sections);
	if (sections.empty()) return;

	auto& extensionMenu = menu.addSubmenu(kSubmenuName, kSubmenuDesc);

	for (const auto& [sectionName, values] : sections)
	{
		auto getValue = [&values](const wstring& key) -> wstring {
			auto it = values.find(key);
			return it != values.end() ? it->second : wstring();
			};

		if (sectionName.empty() ||
			sectionName.find(L'/') != wstring::npos ||
			sectionName.find_first_of(L" \t") != wstring::npos) {
			wcout << L"警告：extension.ini 中的节名 \"" << sectionName << L"\" 无效，已跳过\n";
			continue;
		}

		wstring program = getValue(kKeyProgram);
		if (program.empty()) {
			wcout << L"警告：extension.ini 中的节 [" << sectionName << L"] 缺少 Program，已跳过\n";
			continue;
		}

		wstring desc = getValue(kKeyDesc);
		if (desc.empty()) desc = sectionName;
		wstring params = getValue(kKeyParams);

		wstring adminValue = getValue(kKeyRequireAdmin);
		if (adminValue.empty()) adminValue = getValue(kKeyRunAsAdmin);
		bool requireAdmin = ParseIniBool(adminValue, false);
		bool inConsole = ParseIniBool(getValue(kKeyInConsole), false);

		wstring showWndValue = getValue(kKeyShowWnd);
		int showWnd = showWndValue.empty()
			? kDefaultShowWindow
			: (_wtoi(showWndValue.c_str()) == 0 ? kHiddenShowWindow : kDefaultShowWindow);

		extensionMenu.addCommand(sectionName, desc,
			[program, params, requireAdmin, inConsole, showWnd](ConsoleMenu&, Args) {
				wstring path = ResolvePath(program);
				if (requireAdmin && !IsCurrentProcessAdmin()) {
					wcout << L"以管理员身份启动后，输出不会显示在当前进程窗口，是否继续？ （Y/n）";
					wchar_t answer = static_cast<wchar_t>(_getwch());
					wcout << endl;
					if (answer != L'\r' && answer != L'\n' && towupper(answer) != L'Y') {
						wcout << L"已取消操作\n";
						return;
					}
					RunExternalProgram(path, kOperationRunAs, params, L"", showWnd);
				}
				else if (inConsole) {
					ExecuteProgramInCurrentConsole(path, params, true, nullptr);
				}
				else {
					RunExternalProgram(path, kOperationOpen, params, L"", showWnd);
				}
			});
	}
}