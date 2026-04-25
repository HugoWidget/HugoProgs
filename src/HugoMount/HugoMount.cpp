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

#include "HugoUtils/HMount.h"
#include "WinUtils/Logger.h"
#include "WinUtils/StrConvert.h"
#include "WinUtils/WinUtils.h"
#include "WinUtils/Console.h"
#include "WinUtils/CmdParser.h"

#include <iostream>
#include <string>
#include <vector>
#include <exception>
#include <cstdlib>
#include <map>
#include <functional>
#include <optional>
#include <cctype>
#include <charconv>

using namespace std;
using namespace WinUtils;

// 常量定义
const wstring kConfTargetFile = L"swvdisk.v";
const wstring kLogTargetFile = L"swvdisk_log.v";

// 辅助函数：清屏
void ClearScreen()
{
	system("cls");
}

// 将宽字符串转换为小写
void ToLowerInPlace(wstring& str)
{
	for (auto& ch : str) {
		ch = towlower(ch);
	}
}

// 提取盘符（如果参数是单个字母则返回，否则返回 nullopt）
optional<char> ParseDriveLetter(const wstring& arg)
{
	if (!arg.empty() && iswalpha(static_cast<wint_t>(arg[0]))) {
		return static_cast<char>(towupper(arg[0]));
	}
	return nullopt;
}

// 显示帮助信息
void PrintHelp(const wstring& programName)
{
	wcout << L"用法: HugoMount.exe [选项]\n"
		<< L"选项:\n"
		<< L" --list                     - 列出所有虚拟磁盘信息" << endl
		<< L" --mount <disk> <part> [drive] - 挂载虚拟磁盘分区" << endl
		<< L" --unmount <drive>          - 按盘符卸载（如: --unmount D）" << endl
		<< L" --unmount <disk> <part>    - 按磁盘/分区号卸载" << endl
		<< L" --conf [drive]             - 挂载配置文件分区 (swvdisk.vhd)" << endl
		<< L" --log [drive]              - 挂载日志文件分区 (swvdisk_log.vhd)" << endl
		<< L" --help, -h                 - 显示此帮助信息" << endl
		<< L"============================================================" << endl;
}

// 执行 list 命令
int ExecuteList(HMount& instance)
{
	instance.PrintAllInfo();
	WLog(LogLevel::Info, L"Executed list command");
	return 0;
}

// 执行 mount 命令
int ExecuteMount(HMount& instance, const vector<wstring>& args)
{
	if (args.size() < 2) {
		wcerr << L"错误: 挂载命令缺少参数！用法: --mount <disk> <part> [drive]" << endl;
		WLog(LogLevel::Error, L"Mount command missing arguments");
		return 1;
	}

	int diskId = _wtoi(args[0].c_str());
	int partId = _wtoi(args[1].c_str());

	char driveLetter = 0;
	if (args.size() >= 3) {
		auto letterOpt = ParseDriveLetter(args[2]);
		if (!letterOpt) {
			wcerr << L"错误: 盘符参数必须是单个字母！" << endl;
			WLog(LogLevel::Error, L"Invalid drive letter for mount command");
			return 1;
		}
		driveLetter = *letterOpt;
	}

	int result = instance.Mount(diskId, partId, driveLetter);
	WLog(LogLevel::Info, L"Mount command completed, return code: " + to_wstring(result));
	return result;
}

// 执行 unmount 命令
int ExecuteUnmount(HMount& instance, const vector<wstring>& args)
{
	if (args.empty()) {
		wcerr << L"错误: 卸载命令缺少参数！用法: --unmount <drive> 或 --unmount <disk> <part>" << endl;
		WLog(LogLevel::Error, L"Unmount command missing arguments");
		return 1;
	}

	// 检查是否为盘符卸载
	if (args.size() == 1) {
		auto driveOpt = ParseDriveLetter(args[0]);
		if (driveOpt) {
			int result = instance.Unmount(-1, -1, *driveOpt);
			WLog(LogLevel::Info, L"Unmount by drive letter completed, return code: " + to_wstring(result));
			return result;
		}
	}

	// 否则尝试解析磁盘/分区号
	if (args.size() >= 2) {
		int diskId = _wtoi(args[0].c_str());
		int partId = _wtoi(args[1].c_str());
		int result = instance.Unmount(diskId, partId);
		WLog(LogLevel::Info, L"Unmount by disk/partition completed, return code: " + to_wstring(result));
		return result;
	}

	wcerr << L"错误: 无效的卸载参数！" << endl;
	WLog(LogLevel::Error, L"Invalid unmount arguments");
	return 1;
}

// 执行 conf/log 命令（挂载配置或日志分区）
int ExecuteConfOrLog(HMount& instance, const wstring& command, const vector<wstring>& args)
{
	wstring targetFile = (command == L"conf") ? kConfTargetFile : kLogTargetFile;
	optional<char> driveLetter;

	if (!args.empty()) {
		driveLetter = ParseDriveLetter(args[0]);
		if (!driveLetter) {
			wcerr << L"错误: 盘符参数必须是单个字母！" << endl;
			WLog(LogLevel::Error, L"Invalid drive letter for " + command + L" command");
			return 1;
		}
	}

	// 获取所有磁盘信息
	HugoMountInfo info = instance.GetAllInfo();
	int foundDiskIndex = -1;
	int foundPartIndex = -1;

	// 将目标文件名转为小写用于不区分大小写比较
	wstring targetLower = targetFile;
	ToLowerInPlace(targetLower);

	for (const auto& disk : info.disks) {
		for (const auto& backingFile : disk.backingFiles) {
			wstring widePath = ConvertString<wstring>(backingFile.path);
			ToLowerInPlace(widePath);
			if (widePath.find(targetLower) != wstring::npos) {
				foundDiskIndex = disk.index;
				if (!disk.partitions.empty()) {
					foundPartIndex = disk.partitions[0].index;
				}
				break;
			}
		}
		if (foundDiskIndex != -1) {
			break;
		}
	}

	if (foundDiskIndex == -1) {
		wcerr << L"错误: 未找到包含 " << targetFile << L" 的磁盘！" << endl;
		WLog(LogLevel::Error, L"Disk containing " + targetFile + L" not found");
		return 1;
	}

	if (foundPartIndex == -1) {
		wcerr << L"错误: 磁盘 " << foundDiskIndex << L" 没有有效分区可挂载！" << endl;
		WLog(LogLevel::Error, L"Disk " + to_wstring(foundDiskIndex) + L" has no valid partition");
		return 1;
	}

	int result = instance.Mount(foundDiskIndex, foundPartIndex, driveLetter.value_or(0));
	WLog(LogLevel::Info, targetFile + L" mount completed, return code: " + to_wstring(result));
	return result;
}

// 交互菜单模式
void InteractiveMode(HMount& instance)
{
	int choice = -1;
	do {
		ClearScreen();
		wcout << L"\n=== 虚拟磁盘挂载工具 ===\n"
			<< L"1. 列出所有虚拟磁盘信息\n"
			<< L"2. 挂载虚拟磁盘分区\n"
			<< L"3. 卸载虚拟磁盘\n"
			<< L"4. 挂载配置文件分区\n"
			<< L"5. 挂载日志文件分区\n"
			<< L"0. 退出程序\n"
			<< L"请输入选择: ";
		wcin >> choice;
		if (wcin.fail()) {
			wcin.clear();
			wcin.ignore(1024, L'\n');
			wcerr << L"输入无效，请输入数字" << endl;
			system("pause");
			continue;
		}

		switch (choice) {
		case 1:
			ExecuteList(instance);
			break;
		case 2: {
			wstring diskStr, partStr, driveStr;
			wcout << L"请输入磁盘编号: ";
			wcin >> diskStr;
			wcout << L"请输入分区编号: ";
			wcin >> partStr;
			wcout << L"请输入盘符（可选，直接回车跳过）: ";
			wcin.ignore();
			getline(wcin, driveStr);
			vector<wstring> args = { diskStr, partStr };
			if (!driveStr.empty()) {
				args.push_back(driveStr);
			}
			ExecuteMount(instance, args);
			break;
		}
		case 3: {
			wstring mode;
			wcout << L"请选择卸载方式:\n"
				<< L"  a. 按盘符卸载\n"
				<< L"  b. 按磁盘/分区号卸载\n"
				<< L"请输入选择 (a/b): ";
			wcin >> mode;
			if (mode == L"a" || mode == L"A") {
				wstring drive;
				wcout << L"请输入盘符 (如 D): ";
				wcin >> drive;
				ExecuteUnmount(instance, { drive });
			}
			else if (mode == L"b" || mode == L"B") {
				wstring diskStr, partStr;
				wcout << L"请输入磁盘编号: ";
				wcin >> diskStr;
				wcout << L"请输入分区编号: ";
				wcin >> partStr;
				ExecuteUnmount(instance, { diskStr, partStr });
			}
			else {
				wcerr << L"无效选择" << endl;
			}
			break;
		}
		case 4:
			ExecuteConfOrLog(instance, L"conf", {});
			break;
		case 5:
			ExecuteConfOrLog(instance, L"log", {});
			break;
		case 0:
			wcout << L"退出程序。" << endl;
			WLog(LogLevel::Info, L"User exited");
			break;
		default:
			wcerr << L"无效选择，请重新输入" << endl;
			break;
		}
		if (choice != 0) {
			system("pause");
		}
	} while (choice != 0);
}

int wmain(int argc, wchar_t* argv[])
{
	try {
		RequireAdminPrivilege(true);
		EnsureSingleInstance();
		Console().setLocale();

		LoggerCore::Inst().SetDefaultStrategies(L"HugoMount.log");
		LoggerCore::Inst().EnableApartment(DftLogger);

		HMount instance;

		if (argc > 1) {
			wstring cmdLine;
			for (int i = 1; i < argc; ++i) {
				cmdLine += (i > 1 ? L" " : L"") + wstring(argv[i]);
			}

			CmdParser parser(true);
			if (!parser.parse(cmdLine)) {
				wcerr << L"命令行解析失败" << endl;
				return 1;
			}

			if (parser.hasCommand(L"help") || parser.hasCommand(L"-h")) {
				PrintHelp(GetCurrentProcessName());
				return 0;
			}

			if (parser.hasCommand(L"list")) {
				return ExecuteList(instance);
			}
			if (parser.hasCommand(L"mount")) {
				auto params = parser.getParams(L"mount");
				return ExecuteMount(instance, params);
			}
			if (parser.hasCommand(L"unmount")) {
				auto params = parser.getParams(L"unmount");
				return ExecuteUnmount(instance, params);
			}
			if (parser.hasCommand(L"conf")) {
				auto params = parser.getParams(L"conf");
				return ExecuteConfOrLog(instance, L"conf", params);
			}
			if (parser.hasCommand(L"log")) {
				auto params = parser.getParams(L"log");
				return ExecuteConfOrLog(instance, L"log", params);
			}

			wcerr << L"未知选项，使用 --help 查看帮助" << endl;
			return 1;
		}

		// 交互菜单模式
		InteractiveMode(instance);
		return 0;
	}
	catch (const exception& e) {
		wcerr << L"致命错误: " << ConvertString<wstring>(e.what()) << endl;
		WLog(LogLevel::Error, L"Fatal: " + ConvertString<wstring>(e.what()));
		return 1;
	}
}