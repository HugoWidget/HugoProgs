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
#include <ShlObj.h>
#include <string>
#include <vector>
#include "WinUtils/WinUtils.h"
#include "WinUtils/Injector.h"
#include "WinUtils/Console.h"
#include "WinUtils/Logger.h"
using namespace std;
using namespace WinUtils;

vector<HWND>wnds;
HWND deskHWND = GetDesktopWindow();
RECT deskSize = WinUtils::GetWindowRect(deskHWND);
bool hiden = false;
BOOL injected = false;

BOOL CALLBACK IsSpecifiedWnds(HWND hwnd, LPARAM cursor);
void Unlock();

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	RequireAdminPrivilege(true);
	EnsureSingleInstance();
	Console console;
	console.setLocale();
	LoggerCore::Inst().AddStrategy<ConsoleLogStrategy>();
	LoggerCore::Inst().EnableApartment(DftLogger);

	Injector injector;
	EnableDebugPrivilege();
	wstring dllPath = GetCurrentProcessDir() + L"HugoHook.dll";
	vector<DWORD> injectedProcess;
	while (true) {
		Unlock();
		Sleep(100);
		if (hiden) {
			auto res = injector.InjectToAllProcesses(L"SeewoServiceAssistant.exe", dllPath, injectedProcess);
			injectedProcess.insert(injectedProcess.end(), res.begin(), res.end());
			hiden = false;
		}
	}
	return 0;
}
void Unlock() {
	deskHWND = GetDesktopWindow();
	deskSize = WinUtils::GetWindowRect(deskHWND);
	wnds.clear();
	EnumWindows(IsSpecifiedWnds, 0);
	for (auto it = wnds.begin(); it != wnds.end(); it++) {
		WinUtils::ForceHideWindow(*it);
		if (!hiden) {
			hiden = true;
		}
	}
}
BOOL CALLBACK  IsSpecifiedWnds(HWND hwnd, LPARAM cursor) {
	RECT wndSize = WinUtils::GetWindowRect(hwnd);
	WCHAR lpstring[256] = {};
	GetWindowText(hwnd, lpstring, 255);
	if ((wstring)lpstring == L"希沃管家" && WinUtils::IsWindowTopMost(hwnd)) {
		wnds.push_back(hwnd);
		return false;
	}
	return true;
}