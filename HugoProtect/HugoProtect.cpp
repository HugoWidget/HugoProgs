#include <windows.h>
#include <iostream>
#include <array>
#include <string>
#include <vector>
#include <filesystem>
#include <conio.h>
#include <limits>
#include "HugoUtils/WinUtils.h"
#include "HugoUtils/HugoInfo.h"
#include "HugoUtils/Console.h"
#include "HugoUtils/Logger.h"
using namespace std;
namespace fs = filesystem;
using namespace WinUtils;

// 解析命令行参数
static optional<int> ParseCommandLineArgs(int argc, wchar_t* argv[])
{
    if (argc < 2) return nullopt;
    try
    {
        int op = -1;
        wstring var = argv[1];
        if (var == L"-enable")op = 1;
        if (var == L"-disable")op = 0;
        return (op == 0 || op == 1) ? optional<int>(op) : nullopt;
    }
    catch (...)
    {
        WLog(LogLevel::Error, L"参数无效！");
        return nullopt;
    }
}

// 交互获取用户选择
static int GetInteractiveUserChoice()
{
    int choice = -1;
    while (true)
    {
        wcout << L"0 - 关闭 文件保护" << endl;
        wcout << L"1 - 开启 文件保护" << endl;
        wcout << L"请输入数字：";

        wcin >> choice;
        if (wcin.fail() || (choice != 0 && choice != 1))
        {
            wcin.clear();
            wcin.ignore((numeric_limits<streamsize>::max)(), L'\n');
            WLog(LogLevel::Error, L"输入无效");
        }
        else break;
    }
    return choice;
}

// 执行 DriverService 核心操作
static bool ExecuteDriverServiceOp(int operation)
{
    HugoInfo info;
    auto driverPath = info.getHugoProtectDriverPath();

    if (!driverPath.has_value())
    {
        WLog(LogLevel::Error, L"未找到 DriverService.exe 路径");
        return false;
    }

    const wstring opDesc = operation ? L"开启" : L"关闭";
    const wstring opCmd = operation ? L"install" : L"uninstall";

    WLog(LogLevel::Info, format(L"[执行{}] DriverService 路径：{}", opDesc, *driverPath));

    bool success = RunExternalProgram(*driverPath, L"runas", opCmd, *driverPath);
    WLog(success ? LogLevel::Info : LogLevel::Error,
        format(L"{}操作{}！", opDesc, success ? L"成功" : L"失败"));

    return success;
}

int wmain(int argc, wchar_t* argv[])
{
    if (!IsCurrentProcessAdmin())
    {
        RequireAdminPrivilege(true);
    }
    Console console;
    console.setLocale();
    LoggerCore::Inst().AddStrategy<ConsoleLogStrategy>();
    LoggerCore::Inst().EnableApartment(DftLogger);

    optional<int> op = ParseCommandLineArgs(argc, argv);
    int operation = op.has_value() ? *op : GetInteractiveUserChoice();

    const bool result = ExecuteDriverServiceOp(operation);
    if (!op.has_value())
    {
        WLog(LogLevel::Info, L"\n操作完成，按任意键退出...");
        (void)_getwch();
    }

    return result ? 0 : 1;
}