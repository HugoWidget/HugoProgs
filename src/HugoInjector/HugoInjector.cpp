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
#include "WinUtils/Injector.h"
#include "WinUtils/Console.h"
#include "WinUtils/WinUtils.h"
#include "WinUtils/Logger.h"
using namespace WinUtils;
using namespace std;
int wmain(int argc, wchar_t* argv[])
{
	Console console;
	console.setLocale();
	LoggerCore::Inst().AddStrategy<ConsoleLogStrategy>();
	LoggerCore::Inst().EnableApartment(DftLogger);

	if (argc <= 2)
	{
		std::wcerr << L"inject1: <DLL路径> <目标进程名>" << std::endl;
		std::wcerr << L"inject2: -inject <DLL路径> <目标进程名>" << std::endl;
		std::wcerr << L"uninject:  -uninject <DLL路径> <目标进程名>" << std::endl;
		return 1;
	}
	try
	{
		Injector injector;
		EnableDebugPrivilege();
		if (argc == 4 && argv[1] == L"-uninject") {
			std::wstring dllPath = ResolvePath(argv[2]);
			std::wstring targetProcess = argv[3];
			injector.UninjectFromAllProcesses(targetProcess, dllPath);
		}
		else if ((argc == 4 && ((wstring)argv[1] == L"-inject")) || argc == 3) {
			std::wstring dllPath;
			std::wstring targetProcess;
			if (argc == 4) {
				dllPath = ResolvePath(argv[2]);
				targetProcess = argv[3];
			}
			else {
				dllPath = ResolvePath(argv[1]);
				targetProcess = argv[2];
			}
			EnsureSingleInstance(L"", L"", L"", dllPath + targetProcess);
			injector.MonitorAndInject(dllPath, targetProcess, 100);
		}
	}
	catch (const std::exception& e)
	{
		std::wcerr << L"程序异常: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}