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
#include "HugoUtils/HugoMount.h"
#include "WinUtils/Logger.h"
#include "WinUtils/StrConvert.h"
#include "WinUtils/WinUtils.h"
#include "WinUtils/Console.h"
#include <iostream>
#include <string>
#include <exception>
#include <cwchar>
#include <cstdlib>
#include <map>
#include <functional>
#include <optional>
#include <cctype>
#include <charconv>

using namespace std;
using namespace WinUtils;

// 命令枚举，便于分发和处理
enum class Command {
	List,
	Mount,
	Unmount,
	Conf,
	Log,
	Help,
	Unknown
};

// 将宽字符串转换为小写
void ToLowerInPlace(wstring& str) {
	for (auto& ch : str) {
		ch = towlower(ch);
	}
}

// 提取命令和可选盘符（如果参数是字母则返回，否则返回 nullopt）
optional<char> ParseDriveLetter(const wstring& arg) {
	if (!arg.empty() && iswalpha(static_cast<wint_t>(arg[0]))) {
		return static_cast<char>(towupper(arg[0]));
	}
	return nullopt;
}

// 显示帮助信息
void ShowHelp(wstring_view progName) {
	wcout << L"用法:" << endl;
	wcout << L"  " << progName << L" list                - 列出所有虚拟磁盘信息" << endl;
	wcout << L"  " << progName << L" mount <disk> <part> [drive] - 挂载虚拟磁盘分区" << endl;
	wcout << L"  " << progName << L" unmount <drive>     - 按盘符卸载（如: unmount D）" << endl;
	wcout << L"  " << progName << L" unmount <disk> <part> - 按磁盘/分区号卸载" << endl;
	wcout << L"  " << progName << L" help                - 显示此帮助信息" << endl;
	wcout << L"  " << progName << L" conf [drive]        - 挂载配置文件分区 (swvdisk.vhd)" << endl;
	wcout << L"  " << progName << L" log [drive]         - 挂载日志文件分区 (swvdisk_log.vhd)" << endl;
	wcout << L"============================================================" << endl;
}

// 执行 list 命令
int DoList(HugoMount& inst) {
	inst.PrintAllInfo();
	return 0;
}

// 执行 mount 命令
int DoMount(HugoMount& inst, const vector<wstring>& args) {
	if (args.size() < 2) { // 至少需要 disk 和 part
		wcerr << L"Error: 挂载命令缺少参数！用法: mount <disk> <part> [drive]" << endl;
		WLog(LogLevel::Error, L"挂载命令参数不足");
		return 1;
	}

	int diskId = _wtoi(args[0].c_str());
	int partId = _wtoi(args[1].c_str());

	char driveLetter = 0;
	if (args.size() >= 3) {
		auto letterOpt = ParseDriveLetter(args[2]);
		if (!letterOpt) {
			wcerr << L"Error: 盘符参数必须是单个字母！" << endl;
			WLog(LogLevel::Error, L"挂载命令盘符参数无效");
			return 1;
		}
		driveLetter = *letterOpt;
	}

	int ret = inst.Mount(diskId, partId, driveLetter);
	WLog(LogLevel::Info, format(L"挂载命令执行完成，返回码: {}", ret));
	return ret;
}

// 执行 unmount 命令
int DoUnmount(HugoMount& inst, const vector<wstring>& args) {
	if (args.empty()) {
		wcerr << L"Error: 卸载命令缺少参数！用法: unmount <drive> 或 unmount <disk> <part>" << endl;
		WLog(LogLevel::Error, L"卸载命令参数不足");
		return 1;
	}

	// 检查是否为盘符卸载
	if (args.size() == 1) {
		auto driveOpt = ParseDriveLetter(args[0]);
		if (driveOpt) {
			int ret = inst.Unmount(-1, -1, *driveOpt);
			WLog(LogLevel::Info, format(L"按盘符卸载命令执行完成，返回码: {}", ret));
			return ret;
		}
	}

	// 否则尝试解析磁盘/分区号
	if (args.size() >= 2) {
		int diskId = _wtoi(args[0].c_str());
		int partId = _wtoi(args[1].c_str());
		int ret = inst.Unmount(diskId, partId);
		WLog(LogLevel::Info, format(L"按磁盘/分区卸载命令执行完成，返回码: {}", ret));
		return ret;
	}

	wcerr << L"Error: 无效的卸载参数！" << endl;
	WLog(LogLevel::Error, L"卸载命令参数无效");
	return 1;
}

// 执行 conf/log 命令
int DoConfLog(HugoMount& inst, const wstring& cmd, const vector<wstring>& args) {
	wstring targetFile = (cmd == L"conf") ? L"swvdisk.v" : L"swvdisk_log.v";
	optional<char> driveLetter;

	if (!args.empty()) {
		driveLetter = ParseDriveLetter(args[0]);
		if (!driveLetter) {
			wcerr << L"Error: 盘符参数必须是单个字母！" << endl;
			WLog(LogLevel::Error, L"无效的盘符参数");
			return 1;
		}
	}

	// 获取所有磁盘信息
	HugoMountInfo info = inst.GetAllInfo();
	int foundDiskIndex = -1;
	int foundPartIndex = -1;

	// 将目标文件名转为小写用于不区分大小写比较
	wstring targetLower = targetFile;
	ToLowerInPlace(targetLower);

	for (const auto& disk : info.disks) {
		for (const auto& bf : disk.backingFiles) {
			wstring widePath = ConvertString<wstring>(bf.path);
			ToLowerInPlace(widePath);
			if (widePath.contains(targetFile)) {
				foundDiskIndex = disk.index;
				if (!disk.partitions.empty()) {
					foundPartIndex = disk.partitions[0].index;
				}
				break;
			}
		}
		if (foundDiskIndex != -1) break;
	}

	if (foundDiskIndex == -1) {
		wcerr << L"Error: 未找到包含 " << targetFile << L" 的磁盘！" << endl;
		WLog(LogLevel::Error, format(L"未找到包含 {} 的磁盘", targetFile));
		return 1;
	}

	if (foundPartIndex == -1) {
		wcerr << L"Error: 磁盘 " << foundDiskIndex << L" 没有有效分区可挂载！" << endl;
		WLog(LogLevel::Error, format(L"磁盘 {} 没有有效分区", foundDiskIndex));
		return 1;
	}

	int ret = inst.Mount(foundDiskIndex, foundPartIndex, driveLetter.value_or(0));
	WLog(LogLevel::Info, format(L"{} 挂载命令执行完成，返回码: {}", targetFile, ret));
	return ret;
}

// 主函数
int wmain(int argc, wchar_t* argv[]) {
	RequireAdminPrivilege(true);
	EnsureSingleInstance();
	Console console;
	console.setLocale();
	LoggerCore::Inst().SetDefaultStrategies(L"HugoMount.log");
	LoggerCore::Inst().EnableApartment(DftLogger);

	HugoMount inst;

	if (argc < 2) {
		ShowHelp(GetCurrentProcessName());
		return 0;
	}

	wstring cmdStr = argv[1];
	static const map<wstring, Command> cmdMap = {
		{L"list", Command::List},
		{L"mount", Command::Mount},
		{L"unmount", Command::Unmount},
		{L"conf", Command::Conf},
		{L"log", Command::Log},
		{L"help", Command::Help},
		{L"-h", Command::Help}
	};

	auto it = cmdMap.find(cmdStr);
	Command cmd = (it != cmdMap.end()) ? it->second : Command::Unknown;

	if (cmd == Command::Help) {
		ShowHelp(GetCurrentProcessName());
		return 0;
	}

	// 收集剩余参数到 vector<wstring>
	vector<wstring> args;
	for (int i = 2; i < argc; ++i) {
		args.emplace_back(argv[i]);
	}

	try {
		switch (cmd) {
		case Command::List:
			return DoList(inst);
		case Command::Mount:
			return DoMount(inst, args);
		case Command::Unmount:
			return DoUnmount(inst, args);
		case Command::Conf:
		case Command::Log:
			return DoConfLog(inst, cmdStr, args);
		default:
			wcerr << L"Error: 未知命令 '" << cmdStr << L"'！" << endl;
			ShowHelp(GetCurrentProcessName());
			WLog(LogLevel::Error, format(L"未知命令: {}", cmdStr));
			return 1;
		}
	}
	catch (const exception& e) {
		wcerr << L"程序异常: " << ConvertString<wstring>(e.what()) << endl;
		WLog(LogLevel::Error, format(L"程序异常: {}", ConvertString<wstring>(e.what())));
		return 1;
	}

	WLog(LogLevel::Info, L"程序正常退出");
	return 0;
}