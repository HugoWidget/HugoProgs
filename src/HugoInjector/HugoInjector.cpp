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
#include "WinUtils/CmdParser.h"
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

	std::wstring cmdLine;
	for (int i = 1; i < argc; ++i) {
		cmdLine += (i > 1 ? L" " : L"") + std::wstring(argv[i]);
	}
	CmdParser parser(true);
	bool parseOk = (argc <= 1) || parser.parse(cmdLine, CmdParser::ParseMode::Normal);

	if (parseOk && argc > 1 && (parser.hasCommand(L"help") || parser.hasCommand(L"h") || parser.hasCommand(L"?")))
	{
		PrintUsage(std::wcout);
		return 0;
	}

	LoggerCore::Inst().SetDefaultStrategies(GetCurrentProcessDir() + L"HugoInjector.log");
	LoggerCore::Inst().AddStrategy<ConsoleLogStrategy>();
	LoggerCore::Inst().EnableApartment(DftLogger);
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	if (argc < 2 || !parseOk)
	{
		PrintUsage(std::wcerr);
		WuLog::Error(parseOk ? L"缺少命令行参数" : L"命令行解析失败");
		return 1;
	}

	auto paramError = [](const std::wstring& msg) {
		std::wcerr << msg << std::endl;
		PrintUsage(std::wcerr);
		WuLog::Error(msg);
		return 1;
		};

	try
	{
		Injector injector;
		EnableDebugPrivilege();

		std::vector<std::wstring> killParams = parser.getParams(L"uninject");
		std::vector<std::wstring> injectParams = parser.getParams(L"inject");
		std::vector<std::wstring> posParams = parser.getParams(L"");

		if (parser.hasCommand(L"uninject") && parser.hasCommand(L"inject"))
			return paramError(L"参数错误：不能同时指定 -inject 和 -uninject");

		if (parser.hasCommand(L"uninject"))
		{
			if (killParams.size() != 2 || !posParams.empty())
				return paramError(L"参数错误");

			std::wstring dllPath = ResolvePath(killParams[0]);
			std::wstring targetProcess = killParams[1];
			WuLog::Info(L"卸载注入: " + dllPath + L" <- " + targetProcess);
			injector.UninjectFromAllProcesses(targetProcess, dllPath);
		}
		else if (parser.hasCommand(L"inject"))
		{
			if (injectParams.size() != 2 || !posParams.empty())
				return paramError(L"参数错误");

			std::wstring dllPath = ResolvePath(injectParams[0]);
			std::wstring targetProcess = injectParams[1];
			WuLog::Info(L"开始注入: " + dllPath + L" -> " + targetProcess);
			EnsureSingleInstance(true, L"", L"", L"", dllPath + targetProcess);
			injector.MonitorAndInject(dllPath, targetProcess, 100);
		}
		else if (!posParams.empty())
		{
			// Positional form: <DLL path> <target process>
			if (posParams.size() != 2)
				return paramError(L"参数错误");

			std::wstring dllPath = ResolvePath(posParams[0]);
			std::wstring targetProcess = posParams[1];
			WuLog::Info(L"开始注入: " + dllPath + L" -> " + targetProcess);
			EnsureSingleInstance(true, L"", L"", L"", dllPath + targetProcess);
			injector.MonitorAndInject(dllPath, targetProcess, 100);
		}
		else
		{
			return paramError(L"参数错误");
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