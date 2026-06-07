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

	WLog(LogLevel::Info, L"ScreenSaver monitor started. Watching for '希沃管家' topmost window...");

	while (true)
	{
		HWND hwnd = FindWindowW(nullptr, L"希沃管家");
		if (hwnd && IsWindowTopMost(hwnd))
		{
			// 如果启用了黑名单且该窗口已被记录，则跳过
			if (enableBlacklist && ignoredWindows.count(hwnd))
			{
				WLog(LogLevel::Info, L"Ignoring blacklisted window (HWND: " +
					std::to_wstring((uintptr_t)hwnd) + L").");
				Sleep(500);
				continue;
			}

			WLog(LogLevel::Info, L"Detected fullscreen topmost window: 希沃管家. Waiting 800 milliseconds...");
			Sleep(800);
			if (IsWindow(hwnd) && IsWindowTopMost(hwnd))
			{
				WLog(LogLevel::Info, L"Clicking screen center to dismiss screensaver.");
				ClickAtScreenCenter();
				lastClickedHwnd = hwnd;

				Sleep(200);
				if (IsWindow(hwnd) && IsWindowTopMost(hwnd))
				{
					consecutiveClicks++;
					WLog(LogLevel::Info, L"Window still present after click. Consecutive clicks: " +
						std::to_wstring(consecutiveClicks));

					if (enableBlacklist && consecutiveClicks >= 5)
					{
						// 加入黑名单，不再处理该窗口
						ignoredWindows.insert(hwnd);
						WLog(LogLevel::Info, L"Window (HWND: " +
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
					WLog(LogLevel::Info, L"Window dismissed or changed. Resetting consecutive click counter.");
				}
			}
			else
			{
				WLog(LogLevel::Info, L"Window disappeared or changed state before click.");
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

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR lpCmdLine,
	_In_ int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	RequireAdminPrivilege(true);
	EnsureSingleInstance(true);

	Console console;
	console.setLocale();
	//LoggerCore::Inst().AddStrategy<FileLogStrategy>(GetCurrentProcessDir() + L"HugoScreenSaver.log");
	LoggerCore::Inst().AddStrategy<DebugLogStrategy>();
	LoggerCore::Inst().EnableApartment(DftLogger);

	CmdParser parser;
	(void)parser.parse(ExtractArguments(GetCommandLineW()));
	WLog(LogLevel::Info, L"HugoScreenSaver - Automatic Seewo screensaver dismisser");
	ScreenSaverMonitorLoop(!parser.hasCommand(L"keep"));

	return 0;
}