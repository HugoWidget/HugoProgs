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

#include "framework.h"
#include "HugoScreenSaver.h"
#include "WinUtils/WinPch.h"

#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include "WinUtils/WinUtils.h"
#include "WinUtils/Console.h"
#include "WinUtils/Logger.h"
#include "WinUtils/CmdParser.h"
#include "HugoUtils/HLock.h"
#include "resource.h"
#include <unordered_set>

using namespace WinUtils;
using namespace std;

// 在屏幕中央模拟一次鼠标左键点击
void ClickAtScreenCenter()
{
	int centerX = 80;//直接点中间可能干扰锁屏界面密码的输入
	int centerY = GetSystemMetrics(SM_CYSCREEN) / 2;
	SetCursorPos(centerX, centerY);
	// 某些环境下可能需要短暂的延时以确保光标就位
	Sleep(10);

	INPUT inputs[2] = {};
	inputs[0].type = INPUT_MOUSE;
	inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
	inputs[1].type = INPUT_MOUSE;
	inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
	SendInput(2, inputs, sizeof(INPUT));
}

// 监控循环：持续检测屏保窗口并解除
// 需要注意的是，程序没有办法区分“屏保窗口”与锁屏窗口
// 所以Lock相关程序也会处理这个窗口
void ScreenSaverMonitorLoop(bool enableBlacklist)
{
	static std::unordered_set<HWND> ignoredWindows; // 仅在启用黑名单时使用
	int consecutiveClicks = 0;
	HWND lastClickedHwnd = nullptr;

	WuLog::Info(L"ScreenSaver monitor started. Watching for '希沃管家' topmost window...");

	while (true)
	{
		HWND hwnd = FindWindowW(nullptr, L"希沃管家");
		if (hwnd && IsWindowTopMost(hwnd))
		{
			// 如果启用了黑名单且该窗口已被记录，则跳过
			if (enableBlacklist && ignoredWindows.count(hwnd))
			{
				Sleep(500);
				continue;
			}

			Sleep(800);
			if (IsWindow(hwnd) && IsWindowTopMost(hwnd))
			{
				ClickAtScreenCenter();
				lastClickedHwnd = hwnd;

				Sleep(200);
				if (IsWindow(hwnd) && IsWindowTopMost(hwnd))
				{
					consecutiveClicks++;

					if (enableBlacklist && consecutiveClicks >= 5)
					{
						// 加入黑名单，不再处理该窗口
						ignoredWindows.insert(hwnd);
						WuLog::Info(L"Window (HWND: " +
							std::to_wstring((uintptr_t)hwnd) +
							L") has been blacklisted after 5 failed attempts.");
						consecutiveClicks = 0;
						lastClickedHwnd = nullptr;
					}
				}
				else
				{
					consecutiveClicks = 0;
					lastClickedHwnd = nullptr;
					WuLog::Info(L"Window dismissed or changed. Resetting consecutive click counter.");
				}
			}
			else
			{
				WuLog::Info(L"Window disappeared or changed state before click.");
				consecutiveClicks = 0;
				lastClickedHwnd = nullptr;
			}
		}
		else
		{
			consecutiveClicks = 0;
			lastClickedHwnd = nullptr;
		}

		Sleep(500);
	}
}

static void PrintUsage()
{
	std::wcout << L"HugoScreenSaver - 希沃屏幕保护程序自动解除工具\n"
		<< L"用法:\n"
		<< L"  HugoScreenSaver.exe              持续监测并解除屏幕保护（默认启用窗口去重黑名单）\n"
		<< L"  HugoScreenSaver.exe keep         禁用窗口去重黑名单\n"
		<< L"  HugoScreenSaver.exe --help, -h   显示此帮助信息\n";
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR lpCmdLine,
	_In_ int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	Console console;
	console.setLocale();
	LoggerCore::Inst().SetDefaultStrategies(L"HugoScreenSaver.log");
	LoggerCore::Inst().EnableApartment(DftLogger);

	CmdParser parser;
	if (!parser.parse(ExtractArguments(GetCommandLineW()))) {
		WuLog::Error(L"命令行语法错误");
		return 1;
	}
	if (parser.hasCommand(L"help") || parser.hasCommand(L"h") || parser.hasCommand(L"?")) {
		PrintUsage();
		return 0;
	}

	RequireAdminPrivilege(true);
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	EnsureSingleInstance(true);

	WuLog::Info(L"HugoScreenSaver 启动，窗口黑名单: {}", !parser.hasCommand(L"keep"));
	ScreenSaverMonitorLoop(!parser.hasCommand(L"keep"));

	return 0;
}