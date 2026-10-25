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
#include "WinUtils/StrConvert.h"
using namespace WinUtils;
using namespace std;
static void PrintUsage(std::wostream& os)
{
	os << L"用法:\n"
		<< L"  HugoInjector.exe <DLL路径> <目标进程名>              注入并监视目标进程\n"
		<< L"  HugoInjector.exe -inject <DLL路径> <目标进程名>      同上（显式指定 -inject）\n"
		<< L"  HugoInjector.exe -uninject <DLL路径> <目标进程名>    从所有目标进程中卸载 DLL\n"
		<< L"  HugoInjector.exe --help, -h, /?                      显示此帮助信息\n";
}

int wmain(int argc, wchar_t* argv[])
{
	Console console;
	console.setLocale();
	LoggerCore::Inst().SetDefaultStrategies(GetCurrentProcessDir() + L"HugoInjector.log");
	LoggerCore::Inst().AddStrategy<ConsoleLogStrategy>();
	LoggerCore::Inst().EnableApartment(DftLogger);
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);


	auto isHelpArg = [](const wchar_t* arg) {
		return arg && (wcscmp(arg, L"--help") == 0 || wcscmp(arg, L"-h") == 0 ||
			wcscmp(arg, L"-?") == 0 || wcscmp(arg, L"/?") == 0 || wcscmp(arg, L"help") == 0);
	};

	if (argc < 2 || isHelpArg(argv[1]))
	{
		PrintUsage(argc < 2 ? std::wcerr : std::wcout);
		if (argc < 2)
			WuLog::Error(L"缺少命令行参数");
		return argc < 2 ? 1 : 0;
	}

	try
	{
		Injector injector;
		EnableDebugPrivilege();
		if (argc == 4 && wcscmp(argv[1], L"-uninject") == 0) {
			std::wstring dllPath = ResolvePath(argv[2]);
			std::wstring targetProcess = argv[3];
			WuLog::Info(L"卸载注入: " + dllPath + L" <- " + targetProcess);
			injector.UninjectFromAllProcesses(targetProcess, dllPath);
		}
		else if ((argc == 4 && wcscmp(argv[1], L"-inject") == 0) || argc == 3) {
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
			WuLog::Info(L"开始注入: " + dllPath + L" -> " + targetProcess);
			EnsureSingleInstance(true, L"", L"", L"", dllPath + targetProcess);
			injector.MonitorAndInject(dllPath, targetProcess, 100);
		}
		else
		{
			std::wcerr << L"参数错误\n";
			PrintUsage(std::wcerr);
			WuLog::Error(L"参数错误");
			return 1;
		}
	}
	catch (const std::exception& e)
	{
		std::wcerr << L"程序异常: " << e.what() << std::endl;
		WuLog::Error(std::wstring(L"程序异常: ") + ConvertString(e.what()));
		return 1;
	}
	return 0;
}