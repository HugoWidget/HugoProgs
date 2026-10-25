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

#include <iostream>
#include <string>
#include <conio.h>

#include "WinUtils/Console.h"
#include "WinUtils/WinUtils.h"
#include "HugoUtils/HArt.h"
#include "WinUtils/ConsoleMenu.h"

#include "HugoProgsCommon.h"
#include "RegisterCommands.h"

using namespace std;
using namespace WinUtils;

namespace
{
	BOOL WINAPI IgnoreCtrlCHandler(DWORD ctrlType)
	{
		if (ctrlType == CTRL_C_EVENT || ctrlType == CTRL_BREAK_EVENT)
			return TRUE;
		return FALSE;
	}

	void InstallIgnoreCtrlC()
	{
		SetConsoleCtrlHandler(IgnoreCtrlCHandler, FALSE);
		SetConsoleCtrlHandler(IgnoreCtrlCHandler, TRUE);
	}
}

// 显示帮助信息
void ShowHelp()
{
	wcout << L"HugoProgs - 希沃教学设备工具箱\n"
		<< L"用法:\n"
		<< L"  HugoProgs.exe                    进入交互式菜单\n"
		<< L"  HugoProgs.exe <命令> [参数...]    直接执行指定命令\n"
		<< L"  HugoProgs.exe --help, -h, /?     显示此帮助信息\n"
		<< L"  HugoProgs.exe --version, -v      显示版本信息\n"
		<< L"  HugoProgs.exe <脚本.hps>         执行脚本文件\n\n";
}

int wmain(int argc, wchar_t* argv[])
{
	Console console;
	console.setLocale();
	InstallIgnoreCtrlC();

	wstring rawCmdLine = GetCommandLineW();
	wstring argsPart = ExtractArguments(rawCmdLine);

	if (argsPart.empty() || argsPart.find_first_not_of(L" \t") == wstring::npos)
	{
		HArt::PrintArtText(0);
		if (!IsCurrentProcessAdmin())
		{
			wcout << L"当前无管理员权限，部分功能可能无法正常运行\n";
		}
		wcout << L"欢迎使用HugoProgs，按任意键进入主界面";
		(void)_getwch();

		ConsoleMenu menu;
		registerObject(menu);
		MaybeRunGuide(menu);

		menu.run();
		return 0;
	}

	CmdParser parser(true);
	if (!parser.parse(argsPart, CmdParser::ParseMode::NoFlag))
	{
		wcerr << L"命令行语法错误" << endl;
		return 1;
	}

	const auto& parsed = parser.result();

	// 检测帮助选项
	auto hasOption = [&parsed](const wstring& name) -> bool {
		return parsed.find(name) != parsed.end();
		};
	if (hasOption(L"help") || hasOption(L"h") || hasOption(L"?"))
	{
		ShowHelp();
		return 0;
	}

	// 检测脚本文件
	wstring scriptPath;
	auto posIt = parsed.find(L"");
	if (posIt != parsed.end())
	{
		for (const auto& arg : posIt->second)
		{
			if (IsScriptFile(arg))
			{
				scriptPath = arg;
				break;
			}
		}
	}

	if (!scriptPath.empty())
	{
		return ExecuteScriptFile(scriptPath, nullptr);
	}

	// 直接执行指定命令
	ConsoleMenu directMenu;
	registerObject(directMenu);
	wcout << L"[执行] " << argsPart << endl;
	directMenu.execute(argsPart, true);
	return 0;
}