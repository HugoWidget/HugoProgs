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
#include <tlhelp32.h>
#include <iostream>
#include <cwchar>
#include <chrono>
#include <string>
#include "WinUtils/Injector.h"
#include "WinUtils/WinUtils.h"
#include "WinUtils/Console.h"
#include "WinUtils/Logger.h"
using namespace WinUtils;
using namespace std;

const WCHAR TARGET_WINDOW_TITLE[] = L"希沃管家";     // 目标窗口标题
const WCHAR TARGET_PROCESS_NAME[] = L"HugoLock.exe"; // 要关闭的进程名
const int STABLE_DURATION = 1000;                    // 窗口稳定时间
const int CHECK_INTERVAL = 100;                      // 监测间隔

vector<HWND>wnds;
HWND deskHWND = GetDesktopWindow();
RECT deskSize = GetWindowRect(deskHWND);

BOOL CALLBACK  IsSpecifiedWnds(HWND hwnd, LPARAM cursor) {
	RECT wndSize = GetWindowRect(hwnd);

	string name;
	WCHAR lpstring[256] = {};
	GetWindowText(hwnd, lpstring, 255);
	if ((wstring)lpstring == TARGET_WINDOW_TITLE && WinUtils::IsWindowTopMost(hwnd)) {
		wnds.push_back(hwnd);
		return false;
	}
	return true;
}

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nShowCmd) {
	RequireAdminPrivilege(true);
	EnsureSingleInstance();
	Console console;
	console.setLocale();
	LoggerCore::Inst().AddStrategy<ConsoleLogStrategy>();
	LoggerCore::Inst().EnableApartment(DftLogger);
	const auto RunLock = []() { RunExternalProgram(GetCurrentProcessDir() + TARGET_PROCESS_NAME); };
	RunLock();
	while (true) {
		wnds.clear();
		EnumWindows(IsSpecifiedWnds, 0);
		HWND hForegroundWnd = GetForegroundWindow();
		if (!hForegroundWnd) {
			Sleep(CHECK_INTERVAL);
			continue;
		}
		WCHAR windowTitle[256] = { 0 };
		if (!WinUtils::GetWindowTitle(hForegroundWnd, windowTitle, sizeof(windowTitle) / sizeof(WCHAR))) {
			Sleep(CHECK_INTERVAL);
			continue;
		}
		if (wstring(windowTitle) != TARGET_WINDOW_TITLE) {
			Sleep(CHECK_INTERVAL);
			continue;
		}
		if (!(WinUtils::IsWindowFullScreen(hForegroundWnd, 100, true)
			|| WinUtils::IsWindowTopMost(hForegroundWnd))) {
			Sleep(CHECK_INTERVAL);
			continue;
		}
		bool isWindowStable = true;
		auto startTimer = std::chrono::steady_clock::now();

		while (std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - startTimer
		).count() < STABLE_DURATION) {
			HWND hCurrentWnd = GetForegroundWindow();
			if (hCurrentWnd != hForegroundWnd) {
				isWindowStable = false;
				break;
			}
			Sleep(CHECK_INTERVAL);
		}
		if (isWindowStable) {
			WinUtils::TerminateProcessesByName(TARGET_PROCESS_NAME);
			Sleep(500);
			Injector injector;
			injector.UninjectFromAllProcesses(L"SeewoServiceAssistant.exe", WinUtils::GetCurrentProcessDir() + L"HugoHSSA.dll");
			RunLock();
		}
		Sleep(500);
	}
	return 0;
}