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
#include "HugoUtils/HugoFreezeApi.h"
#include "WinUtils/Console.h"
#include "WinUtils/Logger.h"
#include <iostream>
#include <cstdlib>
#include <windows.h>
#include <format>
#pragma comment(lib, "advapi32.lib")
using namespace std;
using namespace WinUtils;
// 清屏函数
void ClearScreen() noexcept {
	system("cls");
}

// 显示交互菜单
void ShowMenu(uint16_t port) noexcept {
	ClearScreen();
	wcout << L"目标IP: " << DEFAULT_IP << L":" << port << endl;
	wcout << L"请选择操作：" << endl;
	wcout << L"1. 设置冻结磁盘" << endl;// GET /api/v1/set?vol=<string>
	wcout << L"2. 查询冻结磁盘" << endl;// GET /api/v1/get_disk_data
	wcout << L"3. 尝试冻结磁盘" << endl;// POST /api/v1/excute_protect_try (Body: {\"selectedDisks\":[\"<string>\"]})
	wcout << L"0. 退出程序" << endl;
	wcout << L"请输入选择：";
}

// 处理磁盘设置交互
void HandleSetVolInteract(IHugoFreeze& api) noexcept {
	wstring volStr;
	wcout << L"请输入磁盘标识字符串（string）（0代表解除全部冻结）：";
	wcin >> volStr;
	if (volStr == L"0")volStr = L"";
	auto res = api.SetFreezeState(volStr, DriveFreezeState::Frozen);
	if (res.result != FrzOR::Success) {
		WLog(LogLevel::Error, L"设置冻结状态失败: " + res.errMsg);
	}
	else wcout << L"设置冻结状态成功:\n" << res.msg << endl;
}

// 处理磁盘数据获取交互
void HandleGetDiskDataInteract(IHugoFreeze& api) noexcept {
	auto res = api.GetFreezeState();
	if (res.result != FrzOR::Success) {
		WLog(LogLevel::Error, L"获取冻结状态失败: " + res.errMsg);
	}
	else wcout << L"获取冻结状态成功:\n" << res.msg << endl;
}

// 处理保护尝试交互
void HandlePostProtectTryInteract(IHugoFreeze& api) noexcept {
	wstring diskStr;
	wcout << L"请输入磁盘标识字符串（string）（0代表解除全部冻结）：";
	wcin >> diskStr;
	if (diskStr == L"0")diskStr = L"";
	auto res = api.TryProtect(diskStr);
	if (res.result != FrzOR::Success) {
		WLog(LogLevel::Error, L"获取尝试冻结结果失败: " + res.errMsg);
	}
	else wcout << L"获取尝试冻结结果成功\n" << res.msg << endl;
}

int main() {
	Console console;
	console.setLocale();
	LoggerCore::Inst().AddStrategy<ConsoleLogStrategy>();
	LoggerCore::Inst().EnableApartment(DftLogger);
	HugoFreezeApi freezeApi;
	IHugoFreeze& apiRef = freezeApi;

	int choice = -1;
	while (true) {
		uint16_t port = 0;
		wstring ip = L"";
		freezeApi.GetConfig(ip, port);
		ShowMenu(port);
		wcin >> choice;
		if (wcin.fail()) {
			wcin.clear();
			wcin.ignore(1024, L'\n');
			continue;
		}
		switch (choice) {
		case 1: HandleSetVolInteract(freezeApi); break;
		case 2: HandleGetDiskDataInteract(freezeApi); break;
		case 3: HandlePostProtectTryInteract(freezeApi); break;
		case 0:
			wcout << L"退出程序..." << endl;
			return 0;
		default:
			WLog(LogLevel::Error, L"选择无效：请输入0-3之间的数字！");
			break;
		}
		system("pause");
	}

	WSACleanup();
	return 0;
}