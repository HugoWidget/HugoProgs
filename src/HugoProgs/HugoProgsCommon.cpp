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
#include "HugoProgsCommon.h"

#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>
#include <algorithm>

#include "WinUtils/WinUtils.h"
#include "WinUtils/ConsoleMenu.h"

#include "RegisterCommands.h"

using namespace std;
using namespace WinUtils;
namespace fs = filesystem;

wstring GetExternalProgramPath(const wstring& programName)
{
	wstring fullPath = GetCurrentProcessDir() + programName;
	if (!fs::exists(fullPath))
	{
		wcerr << L"错误：未找到程序文件 " << fullPath << endl;
		return L"";
	}
	return fullPath;
}

wstring ReadUserInput(const wstring& prompt)
{
	wcout << prompt;
	wstring input;
	getline(wcin, input);
	return input;
}

bool ExecuteProgramInCurrentConsole(
	const wstring& programPath,
	const wstring& args,
	bool wait,
	DWORD* exitCode)
{
	if (!wait) {
		RunExternalProgram(programPath, L"open", args);
		return true;
	}
	if (programPath.empty()) return false;

	STARTUPINFOW si = { sizeof(STARTUPINFOW) };
	PROCESS_INFORMATION pi;
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_SHOW;
	si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
	si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
	si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

	wstring cmdLine = L"\"" + programPath + L"\" " + args;
	BOOL ret = CreateProcessW(
		nullptr,
		const_cast<wchar_t*>(cmdLine.c_str()),
		nullptr, nullptr, TRUE, 0, nullptr,
		GetDirFromPath(programPath).c_str(),
		&si, &pi
	);

	if (!ret)
	{
		wcerr << L"\n错误：启动程序失败，错误码 " << GetLastError() << endl;
		return false;
	}

	WaitForSingleObject(pi.hProcess, INFINITE);

	if (exitCode)
		GetExitCodeProcess(pi.hProcess, exitCode);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	return true;
}

bool IsScriptFile(const wstring& path)
{
	if (path.size() < 4) return false;
	wstring ext = path.substr(path.size() - 4);
	transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
	return ext == L".hps" && fs::exists(path);
}

int ExecuteScriptFile(const wstring& scriptPath, ConsoleMenu* menu)
{
	wifstream scriptFile(scriptPath);
	if (!scriptFile.is_open())
	{
		wcerr << L"错误：无法打开脚本文件 " << scriptPath << endl;
		return 1;
	}
	ConsoleMenu* usedMenu = nullptr;
	ConsoleMenu dummyMenu;
	if (!menu) {
		registerObject(dummyMenu);
		usedMenu = &dummyMenu;
	}
	else usedMenu = menu;

	wstring line;
	int lineNum = 0;
	while (getline(scriptFile, line))
	{
		++lineNum;
		size_t start = line.find_first_not_of(L" \t\r\n");
		if (start == wstring::npos) continue;
		size_t end = line.find_last_not_of(L" \t\r\n");
		line = line.substr(start, end - start + 1);
		if (line.empty() || line[0] == L'#') continue;

		wcout << L"[执行] " << line << endl;
		usedMenu->execute(line, true);
	}
	scriptFile.close();
	return 0;
}