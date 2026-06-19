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

#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <conio.h>
#include <limits>
#include <cstdlib>
#include <algorithm>

#include "WinUtils/WinUtils.h"
#include "WinUtils/Console.h"
#include "WinUtils/Logger.h"
#include "WinUtils/CmdParser.h"
#include "WinUtils/StrConvert.h"

using namespace std;
namespace fs = std::filesystem;
using namespace WinUtils;

Console console;
vector<fs::path> GetMatchingFiles()
{
	vector<fs::path> result;
	fs::path currentDir = GetCurrentProcessDir();
	if (fs::exists(currentDir) && fs::is_directory(currentDir))
	{
		for (const auto& entry : fs::directory_iterator(currentDir))
		{
			if (!entry.is_regular_file())
				continue;

			fs::path filename = entry.path().filename();
			string_t name = filename.wstring();
			// 以 "Hugo" 开头且以 ".log" 结尾
			if (name.length() >= 8 && name.substr(0, 4) == TS("Hugo") && name.substr(name.length() - 4) == TS(".log"))
				result.push_back(entry.path());
		}
	}

	string_t userName = WinUtils::GetCurrentUserName();
	if (!userName.empty())
	{
		fs::path userDbgLog = fs::path(L"C:\\Users") / userName / L"HugoDbg.log";
		if (fs::exists(userDbgLog) && fs::is_regular_file(userDbgLog))
		{
			// 避免与已记录文件重复
			if (std::find(result.begin(), result.end(), userDbgLog) == result.end())
				result.push_back(userDbgLog);
		}
	}
	return result;
}

// 列出文件，若 interactive 为 true，则允许用户选择用记事本打开某个文件
void ListFiles(const vector<fs::path>& files, bool interactive = false)
{
	if (files.empty())
	{
		tcout << TS("没有找到匹配的文件 (Hugo*.log)。") << endl;
		return;
	}

	tcout << TS("找到以下文件:") << endl;
	for (size_t i = 0; i < files.size(); ++i)
	{
		tcout << TS("  ") << (i + 1) << TS(". ") << files[i].filename().wstring() << endl;
	}
	tcout << TS("共 ") << files.size() << TS(" 个文件。") << endl;

	if (interactive && !files.empty())
	{
		tcout << TS("\n输入文件序号 (1-") << files.size() << TS(") 用记事本打开，输入 0 取消: ");
		int choice;
		if (wcin >> choice)
		{
			if (choice >= 1 && choice <= static_cast<int>(files.size()))
			{
				const auto& path = files[choice - 1];
				tcout << TS("正在用记事本打开: ") << path.filename().wstring() << TS(" ...") << endl;
				// 使用 ShellExecute 打开 notepad
				HINSTANCE result = ShellExecuteW(nullptr, L"open", L"notepad.exe", path.c_str(), nullptr, SW_SHOWNORMAL);
				if (reinterpret_cast<INT_PTR>(result) <= 32)
				{
					tcerr << TS("打开失败，错误码: ") << GetLastError() << endl;
				}
			}
			else if (choice != 0)
			{
				tcerr << TS("无效序号。") << endl;
			}
		}
		else
		{
			wcin.clear();
			wcin.ignore((numeric_limits<streamsize>::max)(), L'\n');
			tcerr << TS("输入无效。") << endl;
		}
	}
}

// 执行删除操作，返回成功删除的文件数
int DeleteFiles(const vector<fs::path>& files, bool confirm = true)
{
	if (files.empty())
	{
		tcout << TS("没有需要删除的文件。") << endl;
		return 0;
	}

	if (confirm)
	{
		tcout << TS("将要删除以下文件:") << endl;
		ListFiles(files, false);  // 只列出，不交互
		tcout << TS("确认删除? (y/n): ");
		wint_t ch = _getwch();
		tcout << endl;
		if (ch != L'y' && ch != L'Y')
		{
			tcout << TS("操作已取消。") << endl;
			return 0;
		}
	}

	int deletedCount = 0;
	for (const auto& path : files)
	{
		error_code ec;
		if (fs::remove(path, ec))
		{
			++deletedCount;
		}
	}

	tcout << TS("成功删除 ") << deletedCount << TS(" 个文件。") << endl;
	return deletedCount;
}

// 显示帮助信息
void PrintHelp(const string_t& programName)
{
	tcout << TS("用法: ") << programName << TS(" [选项]\n")
		<< TS("选项:\n")
		<< TS("  --delete, -d     直接删除所有匹配的文件 (不询问确认)\n")
		<< TS("  --list, -l       列出匹配的文件，但不删除\n")
		<< TS("  --help, -h       显示此帮助信息\n")
		<< TS("无参数则进入交互式菜单\n");
}

// 交互菜单
void InteractiveMenu()
{
	int choice = -1;
	do
	{
		system("cls");

		tcout << TS("\n=== Hugo 日志文件清理工具 ===\n")
			<< TS("当前目录: ") << GetCurrentProcessDir() << TS("\n")
			<< TS("1. 列出匹配的文件 (Hugo*.log) 并可选择用记事本打开\n")
			<< TS("2. 删除所有匹配的文件\n")
			<< TS("0. 退出程序\n")
			<< TS("请输入选择: ");

		if (!(wcin >> choice))
		{
			wcin.clear();
			wcin.ignore((numeric_limits<streamsize>::max)(), L'\n');
			tcerr << TS("输入无效，请输入数字。") << endl;
			console.get();
			continue;
		}

		switch (choice)
		{
		case 1:
		{
			auto files = GetMatchingFiles();
			ListFiles(files, true);   // 交互式列表，允许打开
			break;
		}
		case 2:
		{
			auto files = GetMatchingFiles();
			DeleteFiles(files, true);
			break;
		}
		case 0:
			tcout << TS("退出程序。") << endl;
			break;
		default:
			tcerr << TS("无效选择，请重新输入。") << endl;
			break;
		}

		if (choice != 0)
		{
			tcout << TS("\n按任意键继续...") << endl;
			console.get();
		}
	} while (choice != 0);
}

int wmain(int argc, wchar_t* argv[])
{
	console.setLocale();
	LoggerCore::Inst().ClearStrategies();
	LoggerCore::Inst().AddStrategy<ConsoleLogStrategy>();

	// 命令行模式
	if (argc > 1)
	{
		string_t cmdLine;
		for (int i = 1; i < argc; ++i)
		{
			if (i > 1) cmdLine += TS(" ");
			cmdLine += argv[i];
		}

		CmdParser parser(true);
		if (!parser.parse(cmdLine))
		{
			tcerr << TS("命令行解析失败。") << endl;
			return 1;
		}

		if (parser.hasCommand(TS("help")) || parser.hasCommand(TS("-h")))
		{
			PrintHelp(TS("HugoLogs"));
			return 0;
		}

		if (parser.hasCommand(TS("list")) || parser.hasCommand(TS("-l")))
		{
			auto files = GetMatchingFiles();
			ListFiles(files, false);
			return 0;
		}

		if (parser.hasCommand(TS("delete")) || parser.hasCommand(TS("-d")))
		{
			auto files = GetMatchingFiles();
			int deleted = DeleteFiles(files, false);
			return (deleted > 0) ? 0 : 1;
		}

		tcerr << TS("未知选项，使用 --help 查看帮助。") << endl;
		return 1;
	}

	// 交互菜单模式
	InteractiveMenu();
	return 0;

}