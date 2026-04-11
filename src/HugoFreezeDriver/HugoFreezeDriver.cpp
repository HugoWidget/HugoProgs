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

#include <iostream>
#include <string>
#include <cstdlib>

#include "HugoUtils/HugoFreezeDriver.h"
#include "WinUtils/Console.h"
#include "WinUtils/Logger.h"
#include "WinUtils/WinUtils.h"
#include "WinUtils/CmdParser.h"
#include <WinUtils/StrConvert.h>

using namespace std;
using namespace WinUtils;

// 辅助函数：清屏
void ClearScreen()
{
    system("cls");
}

// 显示驱动状态（运行时 + 启动配置）
void PrintDriverStatus(const QueryResult& result)
{
    wcout << L"\n运行时状态：" << endl;
    if (result.runtimeStatus.querySuccess) {
        wcout << L"  激活状态 : " << result.runtimeStatus.activeFlag
            << L" (" << (result.runtimeStatus.activeFlag ? L"已激活" : L"未激活") << L")" << endl;
        wcout << L"  Pointer1  : 0x" << hex << result.runtimeStatus.ptr1 << dec << endl;
        wcout << L"  最后日志 : " << result.runtimeStatus.logStr << endl;
    }
    else {
        wcout << L"  查询运行时状态失败" << endl;
    }

    wcout << L"\n启动配置：" << endl;
    if (result.bootConfig.querySuccess) {
        wcout << HugoFreezeDriver::HexDump(result.bootConfig.buffer, result.bootConfig.validLen);
    }
    else {
        wcout << L"  查询启动配置失败" << endl;
    }
}

// 查询并打印驱动状态
void QueryAndPrint(HugoFreezeDriver& driver)
{
    QueryResult result = driver.QueryDriverStatus();
    PrintDriverStatus(result);
    WLog(LogLevel::Info, L"Queried driver status");
}

// 应用冻结设置（盘符列表）
void ApplyFreezeSettings(HugoFreezeDriver& driver, const wstring& disks)
{
    wstring input = disks;
    int mask = CalculateVolumeMask(input);
    if (mask == -1) {
        wcerr << L"无效盘符输入，操作取消。" << endl;
        WLog(LogLevel::Warn, L"Invalid volume input: " + input);
        return;
    }

    auto result = driver.SetFreezeState(input, DriveFreezeState::Frozen);
    bool success = (static_cast<int>(result.result) == 0);
    wcout << L"\n冻结盘符 [" << input << L"]：" << (success ? L"成功" : L"失败") << endl;
    if (success) {
        wcout << L"请重启计算机以应用更改。" << endl;
        WLog(LogLevel::Info, L"Set freeze state for volumes: " + input);
    }
    else {
        WLog(LogLevel::Error, L"Failed to set freeze state for volumes: " + input);
    }
}

// 显示帮助信息
void PrintHelp()
{
    wcout << L"用法: HugoFreezeDriver.exe [选项]\n"
        << L"选项:\n"
        << L"  --query              查询驱动状态\n"
        << L"  --set <盘符>         冻结指定盘符（如 CD 表示C和D盘，0表示解除所有）\n"
        << L"  --help, -h           显示此帮助\n"
        << L"无参数则进入交互菜单\n";
}

int wmain(int argc, wchar_t* argv[])
{
    try {
        RequireAdminPrivilege(true);
        Console().setLocale();

        // 日志配置：仅文件
        LoggerCore::Inst().SetDefaultStrategies(L"HugoFreezeDriver.log");
        LoggerCore::Inst().EnableApartment(DftLogger);

        HugoFreezeDriver& driver = HugoFreezeDriver::Instance();
        if (static_cast<int>(driver.Init().result) != 0) {
            wcerr << L"打开驱动失败，请检查驱动是否安装或权限是否足够。" << endl;
            WLog(LogLevel::Error, L"Failed to open driver");
            return 1;
        }

        // 确保退出时清理驱动句柄
        struct DriverGuard
        {
            HugoFreezeDriver& drv;
            ~DriverGuard() { drv.Cleanup(); }
        } guard{ driver };

        // 命令行模式
        if (argc > 1) {
            wstring cmdLine;
            for (int i = 1; i < argc; ++i) {
                cmdLine += (i > 1 ? L" " : L"") + wstring(argv[i]);
            }

            CmdParser parser(true);
            if (!parser.parse(cmdLine)) {
                wcerr << L"命令行解析失败" << endl;
                return 1;
            }

            if (parser.hasCommand(L"help") || parser.hasCommand(L"-h")) {
                PrintHelp();
                return 0;
            }

            if (parser.hasCommand(L"query")) {
                QueryAndPrint(driver);
                return 0;
            }

            if (parser.hasCommand(L"set")) {
                auto params = parser.getParams(L"set");
                if (params.empty()) {
                    wcerr << L"错误：--set 需要盘符参数" << endl;
                    return 1;
                }
                ApplyFreezeSettings(driver, params[0]);
                return 0;
            }

            wcerr << L"未知选项，使用 --help 查看帮助" << endl;
            return 1;
        }

        // 交互菜单模式
        int choice = -1;
        do {
            ClearScreen();
            wcout << L"\n=== 希沃驱动冻结工具 ===\n"
                << L"1. 查询驱动状态\n"
                << L"2. 设置冻结盘符\n"
                << L"0. 退出程序\n"
                << L"请输入选择：";
            wcin >> choice;
            if (wcin.fail()) {
                wcin.clear();
                wcin.ignore(1024, L'\n');
                wcerr << L"输入无效，请输入数字" << endl;
                system("pause");
                continue;
            }

            switch (choice) {
            case 1:
                QueryAndPrint(driver);
                break;
            case 2: {
                wstring input;
                wcout << L"请输入要冻结的盘符（如 CD 表示C和D盘），输入 0 解除所有冻结：";
                wcin >> input;
                ApplyFreezeSettings(driver, input);
                break;
            }
            case 0:
                wcout << L"退出程序。" << endl;
                WLog(LogLevel::Info, L"User exited");
                break;
            default:
                wcerr << L"无效选择，请重新输入" << endl;
                break;
            }
            if (choice != 0) {
                system("pause");
            }
        } while (choice != 0);

        return 0;
    }
    catch (const exception& e) {
        wcerr << L"致命错误：" << e.what() << endl;
        WLog(LogLevel::Error, L"Fatal: " + ConvertString<wstring>(e.what()));
        return 1;
    }
}