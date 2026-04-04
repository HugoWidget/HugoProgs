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
#include <iostream>
#include <string>
#include <stdexcept>
#include <vector>
#include <optional>
#include "TaggedFirewallController.h"
#include "WinUtils/WinReg.h"
#include "WinUtils/Console.h"
#include "WinUtils/StrConvert.h"
#include "WinUtils/Logger.h"
#include "HugoUtils/HugoInfo.h"
#include <WinUtils/WinUtils.h>
#include "WinUtils/CmdParser.h"
#include <windows.h>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib,"rpcrt4.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

using namespace WinUtils;
using namespace std;

const HKEY REGISTRY_ROOT_KEY = HKEY_LOCAL_MACHINE;
const wstring REGISTRY_PATH = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\SeewoServiceAssistant.exe";
const wstring REGISTRY_VALUE_NAME = L"Debugger";
const wstring REGISTRY_DISABLE_VALUE = L"1";
TaggedFirewallController* g_fw = nullptr;
enum class Operation {
	DisableSeewo, EnableSeewo,
	DisableUpdate, EnableUpdate,
	DisableNet, EnableNet,
	ClearRules, Help
};

void CreateFirewallController() {
	if (g_fw) return;
	TaggedFirewallController::ControllerIdentity id;
	id.appName = L"Hugo";
	id.version = L"1.0";
	g_fw = new TaggedFirewallController(id);
	g_fw->Initialize();
}

// 注册表操作
bool DisableSeewoService() {
	RegKey key;
	key.Create(REGISTRY_ROOT_KEY, REGISTRY_PATH, KEY_READ | KEY_WRITE | KEY_WOW64_64KEY);
	key.SetStringValue(REGISTRY_VALUE_NAME, REGISTRY_DISABLE_VALUE);
	wstring readValue = key.GetStringValue(REGISTRY_VALUE_NAME);
	if (readValue != REGISTRY_DISABLE_VALUE)
		throw runtime_error("验证注册表写入结果失败");
	return true;
}

bool EnableSeewoService() {
	RegKey key;
	if (auto result = key.TryOpen(REGISTRY_ROOT_KEY, REGISTRY_PATH, KEY_WRITE | KEY_WOW64_64KEY);
		result.Failed()) return true;
	(void)key.TryDeleteValue(REGISTRY_VALUE_NAME);
	return true;
}

// 更新程序防火墙规则
void DisableUpdate() {
	HugoInfo inf;
	auto& fw = *g_fw;
	for (auto& prog : inf.getHugoUpdateFolder())
		fw.BlockAllAccess(prog + L"EasiUpdate3.exe");
	fw.BlockAllAccess(L"C:\\ProgramData\\Seewo\\Easiupdate\\easiupdate\\EasiUpdate.exe");
}

void EnableUpdate() {
	HugoInfo inf;
	auto& fw = *g_fw;
	for (auto& prog : inf.getHugoUpdateFolder())
		fw.DeleteRulesForProgram(prog + L"EasiUpdate3.exe");
	fw.DeleteRulesForProgram(L"C:\\ProgramData\\Seewo\\Easiupdate\\easiupdate\\EasiUpdate.exe");
}

// 主程序网络规则
void DisableNet() {
	HugoInfo inf;
	auto& fw = *g_fw;
	if (auto folder = inf.getHugoFolder(); folder) {
		fw.BlockAllAccess(*folder + L"SeewoCore\\SeewoCore.exe");
		fw.BlockAllAccess(*folder + L"SeewoServiceAssistant\\SeewoServiceAssistant.exe");
	}
	else {
		wcout << L"未找到希沃安装目录，无法启用网络访问" << endl;
	}
}

void EnableNet() {
	HugoInfo inf;
	auto& fw = *g_fw;
	if (auto folder = inf.getHugoFolder(); folder) {
		fw.DeleteRulesForProgram(*folder + L"SeewoCore\\SeewoCore.exe");
		fw.DeleteRulesForProgram(*folder + L"SeewoServiceAssistant\\SeewoServiceAssistant.exe");
	}
	else {
		wcout << L"未找到希沃安装目录，无法启用网络访问" << endl;
	}
}

bool ClearRules() {
	auto& fw = *g_fw;
	return fw.CleanupResidualRules();
}

void Execute(Operation op) {
	try {
		CreateFirewallController();
		switch (op) {
		case Operation::DisableSeewo:
			if (DisableSeewoService()) wcout << L"希沃已禁用" << endl;
			break;
		case Operation::EnableSeewo:
			if (EnableSeewoService()) wcout << L"希沃已启用" << endl;
			break;
		case Operation::DisableUpdate:
			DisableUpdate();
			wcout << L"已禁用希沃更新" << endl;
			break;
		case Operation::EnableUpdate:
			EnableUpdate();
			wcout << L"已启用希沃更新" << endl;
			break;
		case Operation::DisableNet:
			DisableNet();
			wcout << L"已禁用希沃网络" << endl;
			break;
		case Operation::EnableNet:
			EnableNet();
			wcout << L"已启用希沃网络" << endl;
			break;
		case Operation::ClearRules:
			if (ClearRules()) wcout << L"已清除所有希沃网络禁用规则" << endl;
			break;
		default: break;
		}
	}
	catch (const exception& e) {
		wcerr << L"错误：" << ConvertString<wstring>(e.what()) << endl;
		exit(1);
	}
}

void PrintHelp() {
	wcout << L"用法: 程序名 [选项]\n"
		<< L"选项:\n"
		<< L"  --disable          禁用希沃服务\n"
		<< L"  --enable           启用希沃服务\n"
		<< L"  --disable-update   禁用希沃更新\n"
		<< L"  --enable-update    启用希沃更新\n"
		<< L"  --disable-net      禁用希沃网络\n"
		<< L"  --enable-net       启用希沃网络\n"
		<< L"  --clear-rules      清除所有希沃网络规则\n"
		<< L"  --help, -h         显示此帮助\n"
		<< L"若无参数则进入交互菜单\n";
}

int wmain(int argc, wchar_t* argv[]) {
	RequireAdminPrivilege(true);
	EnsureSingleInstance();
	Console console;
	console.setLocale();
	LoggerCore::Inst().AddStrategy<ConsoleLogStrategy>();
	LoggerCore::Inst().EnableApartment(DftLogger);

	CmdParser parser(true);
	if (argc > 1) {
		wstring cmdLine;
		for (int i = 1; i < argc; ++i) {
			if (i > 1) cmdLine += L' ';
			cmdLine += argv[i];
		}
		if (!parser.parse(cmdLine)) {
			wcerr << L"命令行解析失败" << endl;
			return 1;
		}
		if (parser.hasCommand(L"help") || parser.hasCommand(L"-h")) {
			PrintHelp();
			return 0;
		}
		if (parser.hasCommand(L"disable")) Execute(Operation::DisableSeewo);
		else if (parser.hasCommand(L"enable")) Execute(Operation::EnableSeewo);
		else if (parser.hasCommand(L"disable-update")) Execute(Operation::DisableUpdate);
		else if (parser.hasCommand(L"enable-update")) Execute(Operation::EnableUpdate);
		else if (parser.hasCommand(L"disable-net")) Execute(Operation::DisableNet);
		else if (parser.hasCommand(L"enable-net")) Execute(Operation::EnableNet);
		else if (parser.hasCommand(L"clear-rules")) Execute(Operation::ClearRules);
		else {
			wcerr << L"未知选项，使用 help 查看帮助" << endl;
			return 1;
		}
		return 0;
	}

	int choice = -1;
	do {
		wcout << L"请选择操作：\n"
			<< L"0: 禁用希沃\n1: 启用希沃\n2: 禁用希沃更新\n3: 启用希沃更新\n"
			<< L"4: 禁用希沃网络\n5: 启用希沃网络\n6: 清除所有希沃网络规则\n";
		wcin >> choice;
		if (choice < 0 || choice > 6)
			wcout << L"输入无效，请重新输入\n";
		else break;
	} while (true);
	Execute(static_cast<Operation>(choice));
	return 0;
}