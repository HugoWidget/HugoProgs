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
#include"WinUtils/WinUtils.h"
#include"WinUtils/WinSvcMgr.h"
#include"WinUtils/Console.h"
#include"WinUtils/Logger.h"
using namespace std;
using namespace WinUtils;
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nShowCmd)
{
	RequireAdminPrivilege(true);
	Console console;
	console.setLocale();
	LoggerCore::Inst().AddStrategy<ConsoleLogStrategy>();
	LoggerCore::Inst().EnableApartment(DftLogger);

	wstring cmd = lpCmdLine;
	if (cmd == L"-start") {
		TerminateProcessesByName(L"HugoLaunchTool.exe");
		std::wstring serviceName = L"SeewoCoreService";
		bool result = WinSvcMgr(serviceName).Start();
	}
	else if (cmd == L"-stop") {
		EnsureSingleInstance();
		return MonitorAndTerminateProcesses(hInstance, { L"SeewoServiceAssistant.exe", L"SeewoCore.exe", L"SeewoAbility.exe",L"SeewoLauncherGuard.exe" });
	}
	return 0;
}
