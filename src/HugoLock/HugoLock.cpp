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
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include "WinUtils/WinUtils.h"
#include "WinUtils/Injector.h"
#include "WinUtils/Console.h"
#include "WinUtils/Logger.h"
#include "WinUtils/CmdParser.h"
#include "HugoUtils/HugoLock.h"
#include "resource.h"
#include "HugoLock.h"
#include "uiaccess.h"
#include <WinUtils/INI.h>

using namespace WinUtils;

static std::unique_ptr<SharedFlag> g_flag = nullptr;
static std::atomic<bool> g_monitorRunning{ true };  // 控制监控线程退出

// 设置拦截标志
void SetInterceptEnabled(bool enable) {
	if (g_flag && g_flag->Valid()) {
		g_flag->Set(enable ? TRUE : FALSE);
		WLog(LogLevel::Info, std::format(L"Intercept flag set to {}", enable));
	}
}

bool GetInterceptEnabled()
{
	if (g_flag && g_flag->Valid()) {
		return g_flag->Get();
	}
	return false;
}

void UnlockOnce()
{
	EnumWindows([](HWND hwnd, LPARAM) -> BOOL {
		WCHAR title[256] = {};
		GetWindowTextW(hwnd, title, 256);
		if (std::wstring(title) == L"希沃管家" && IsWindowTopMost(hwnd)) {
			ForceHideWindow(hwnd);
		}
		return TRUE;
		}, 0);
}

// Direct模式下的本地窗口隐藏循环
void DirectUnlockLoop() {
	while (true) {
		if(!GetInterceptEnabled())SetInterceptEnabled(true) ;
		Sleep(100);
	}
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR lpCmdLine,
	_In_ int nCmdShow) {
	RequireAdminPrivilege(true);
	DWORD dwUIAccessErr = PrepareForUIAccess();
	if (dwUIAccessErr != ERROR_SUCCESS) {
		WLog(LogLevel::Warn, std::format(L"PrepareForUIAccess failed with error: {}", dwUIAccessErr));
		WLog(LogLevel::Info, L"Continuing without UIAccess (some features may be limited)");
	}
	else {
		WLog(LogLevel::Info, L"UIAccess enabled successfully");
	}

	EnsureSingleInstance();

	Console console;
	console.setLocale();
	LoggerCore::Inst().AddStrategy<ConsoleLogStrategy>();
	LoggerCore::Inst().AddStrategy<DebugLogStrategy>();
	LoggerCore::Inst().EnableApartment(DftLogger);

	CmdParser parser(true);
	if (!parser.parse(lpCmdLine, CmdParser::ParseMode::Normal)) {
		WLog(LogLevel::Error, L"Failed to parse command line");
		return 1;
	}

	std::wstring mode = L"direct";
	if (auto param = parser.getParam(L"mode", 0)) {
		mode = *param;
	}
	WLog(LogLevel::Info, std::format(L"Running in mode: {}", mode));

	std::wstring dllPath = GetCurrentProcessDir() + L"HugoHSSA.dll";
	EnableDebugPrivilege();

	g_flag = std::make_unique<SharedFlag>(L"HugoLockFlag");
	if (!g_flag->Valid()) {
		WLog(LogLevel::Error, L"Failed to create shared flag");
		return 1;
	}

	auto injector = std::make_shared<Injector>();
	std::thread monitorThread([injector, dllPath]() {
		injector->MonitorAndInject(dllPath, L"SeewoServiceAssistant.exe", 500);
		});
	monitorThread.detach();

	if (mode == L"assist") {
		std::wstring iniPath = GetCurrentProcessDir() + L"HugoLock.ini";
		if (!std::filesystem::exists(iniPath)) {
			std::ofstream iniFile(iniPath);
			if (iniFile.is_open()) {
				iniFile << "[Config]\n";
				iniFile << "UI=dialog\n";
				iniFile.close();
				WLog(LogLevel::Info, L"Created default HugoLock.ini with UI=Dialog");
			}
			else {
				WLog(LogLevel::Error, L"Failed to create HugoLock.ini");
			}
		}

		// 读取配置
		WinUtils::INIStructure iniData;
		WinUtils::INIFile iniFile(iniPath);
		std::wstring uiMode = L"dialog";

		if (iniFile.read(iniData)) {
			auto& config = iniData[L"Config"];
			if (config.has(L"UI")) {
				uiMode = config.get(L"UI");
				WLog(LogLevel::Info, std::format(L"UI mode from config: {}", uiMode));
			}
		}
		else {
			WLog(LogLevel::Warn, L"Failed to read HugoLock.ini, using default UI mode (Dialog)");
		}

		bool useSimpleUI = (uiMode == L"button");

		g_flag->Set(FALSE);
		WLog(LogLevel::Info, L"Assist mode: flag=0, monitoring started");

		INT_PTR result = 0;
		if (useSimpleUI)result = DialogBoxW(hInstance, MAKEINTRESOURCE(IDD_CLOSE), nullptr, SimpleDialogProc);
		else result = DialogBoxW(hInstance, MAKEINTRESOURCE(IDD_MAIN), nullptr, MainDialogProc);

		WLog(LogLevel::Info, L"Panel closed, exiting.");
	}
	else {
		g_flag->Set(TRUE);
		WLog(LogLevel::Info, L"Direct mode: flag=1, monitoring started");
		DirectUnlockLoop();
	}

	g_monitorRunning = false;
	return 0;
}