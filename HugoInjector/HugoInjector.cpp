#include "HugoUtils/Injector.h"
#include "HugoUtils/Console.h"
#include "HugoUtils/WinUtils.h"
#include "HugoUtils/Logger.h"
using namespace WinUtils;
using namespace std;
int wmain(int argc, wchar_t* argv[])
{
	Console console;
	console.setLocale();
	if (argc <= 2)
	{
		std::wcerr << L"inject1: <DLL路径> <目标进程名>" << std::endl;
		std::wcerr << L"inject2: inject <DLL路径> <目标进程名>" << std::endl;
		std::wcerr << L"uninject:  uninject <DLL路径> <目标进程名>" << std::endl;
		return 1;
	}
	Injector injector;
	try
	{
		EnableDebugPrivilege();
		if (argc == 4 && argv[1] == L"-uninject") {
			std::wstring dllPath = argv[2];
			std::wstring targetProcess = argv[3];
			injector.UninjectFromAllProcesses(targetProcess, dllPath);
		}
		else if ((argc == 4 && ((wstring)argv[1] == L"-inject")) || argc == 3) {
			std::wstring dllPath;
			std::wstring targetProcess;
			if (argc == 4) {
				dllPath = argv[2];
				targetProcess = argv[3];
			}
			else {
				dllPath = argv[1];
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