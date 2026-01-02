#include "HugoUtils/Injector.h"
#include "HugoUtils/Console.h"
#include "HugoUtils/WinUtils.h"
#include "HugoUtils/Logger.h"
using namespace WinUtils;
int wmain(int argc, wchar_t* argv[])
{
    Console console;
    console.setLocale();
    Logger::Inst().AddStrategy(std::make_unique<ConsoleLogStrategy>());
    if (argc != 3)
    {
        std::wcerr << L"用法: Injector.exe <DLL路径> <目标进程名>" << std::endl;
        std::wcerr << L"示例: Injector.exe C:\\mydll.dll myexe.exe" << std::endl;
        return 1;
    }
    std::wstring dllPath = argv[1];
    std::wstring targetProcess = argv[2];
    Injector injector;
    EnableDebugPrivilege();
    try
    {
        injector.MonitorAndInject(dllPath, targetProcess, 2000);
    }
    catch (const std::exception& e)
    {
        std::wcerr << L"程序异常: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}