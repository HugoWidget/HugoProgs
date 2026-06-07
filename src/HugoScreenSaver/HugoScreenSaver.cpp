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
// 如果没有配置解除锁屏，程序会自动退出
// 但是Lock相关程序也会处理这个窗口，所以这个程序不知道有什么用
void ScreenSaverMonitorLoop(bool autoExit)
{
	WLog(LogLevel::Info, L"ScreenSaver monitor started. Watching for '希沃管家' topmost window...");
	int consecutiveClicks = 0;
	while (true)
	{
		HWND hwnd = FindWindowW(nullptr, L"希沃管家");
		if (hwnd && IsWindowTopMost(hwnd))
		{
			WLog(LogLevel::Info, L"Detected fullscreen topmost window: 希沃管家. Waiting 800 milliseconds...");
			Sleep(800);
			// 再次确认窗口依旧存在
			if (IsWindow(hwnd) && IsWindowTopMost(hwnd))
			{
				WLog(LogLevel::Info, L"Clicking screen center to dismiss screensaver.");
				ClickAtScreenCenter();

				// 等待点击生效后检查窗口状态
				Sleep(200);
				if (autoExit) {
					if (IsWindow(hwnd) && IsWindowTopMost(hwnd))
					{
						consecutiveClicks++;
						WLog(LogLevel::Info, L"Window still present after click. Consecutive clicks: " + std::to_wstring(consecutiveClicks));
						if (consecutiveClicks >= 5)
						{
							WLog(LogLevel::Info, L"5 consecutive clicks with window still present. Exiting monitor.");
							return; // 退出循环（函数返回）
						}
					}
					else
					{
						consecutiveClicks = 0;
						WLog(LogLevel::Info, L"Window dismissed or changed. Resetting consecutive click counter.");
					}
				}
			}
			else WLog(LogLevel::Info, L"Window disappeared or changed state before click.");
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
	LoggerCore::Inst().AddStrategy<ConsoleLogStrategy>();
	LoggerCore::Inst().AddStrategy<DebugLogStrategy>();
	LoggerCore::Inst().EnableApartment(DftLogger);

	CmdParser parser;
	(void)parser.parse(ExtractArguments(GetCommandLineW()));
	WLog(LogLevel::Info, L"HugoScreenSaver - Automatic Seewo screensaver dismisser");
	ScreenSaverMonitorLoop(!parser.hasCommand(L"keep"));

	return 0;
}