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

#include <windows.h>
#include <iostream>
#include <array>
#include <string>
#include <vector>
#include <filesystem>
#include <conio.h>
#include <limits>
#include <cstdlib>
#include <stdexcept>

#include "WinUtils/WinUtils.h"
#include "WinUtils/Console.h"
#include "WinUtils/Logger.h"
#include "WinUtils/CmdParser.h"
#include "HugoUtils/HInfo.h"
#include <WinUtils/StrConvert.h>

using namespace std;
namespace fs = filesystem;
using namespace WinUtils;

// 常量定义
const wstring kDriverInstallCmd = L"install";
const wstring kDriverUninstallCmd = L"uninstall";
const wstring kRunAsVerb = L"runas";

// 辅助函数：清屏
void ClearScreen()
{
    system("cls");
}

// 执行驱动服务操作（install / uninstall）
bool ExecuteDriverServiceOp(bool enable)
{
    HInfo info;
    auto driverPath = info.getHugoProtectDriverPath();

    if (!driverPath.has_value()) {
        wcerr << L"未找到 DriverService.exe 路径" << endl;
        WLog(LogLevel::Error, L"DriverService.exe path not found");
        return false;
    }

    const wstring opDesc = enable ? L"开启" : L"关闭";
    const wstring opCmd = enable ? kDriverInstallCmd : kDriverUninstallCmd;

    wcout << L"正在" << opDesc << L"文件保护驱动..." << endl;
    WLog(LogLevel::Info, L"Executing " + opDesc + L" DriverService: " + *driverPath);

    bool success = RunExternalProgram(*driverPath, kRunAsVerb, opCmd, *driverPath);
    if (success) {
        wcout << L"文件保护驱动" << opDesc << L"成功" << endl;
        WLog(LogLevel::Info, L"DriverService " + opCmd + L" succeeded");
    }
    else {
        wcerr << L"文件保护驱动" << opDesc << L"失败" << endl;
        WLog(LogLevel::Error, L"DriverService " + opCmd + L" failed");
    }
    return success;
}

// 显示帮助信息
void PrintHelp(const wstring& programName)
{
    wcout << L"用法: " << programName << L" [选项]\n"
        << L"选项:\n"
        << L"  --enable, -enable    开启文件保护驱动\n"
        << L"  --disable, -disable  关闭文件保护驱动\n"
        << L"  --help, -h           显示此帮助\n"
        << L"无参数则进入交互菜单\n";
}

int wmain(int argc, wchar_t* argv[])
{
    try {
        RequireAdminPrivilege(true);
        Console().setLocale();

        // 日志配置：仅文件
        LoggerCore::Inst().SetDefaultStrategies(L"HugoProtect.log");
        LoggerCore::Inst().EnableApartment(DftLogger);

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
                PrintHelp(GetCurrentProcessName());
                return 0;
            }

            if (parser.hasCommand(L"enable") || parser.hasCommand(L"-enable")) {
                bool result = ExecuteDriverServiceOp(true);
                return result ? 0 : 1;
            }
            if (parser.hasCommand(L"disable") || parser.hasCommand(L"-disable")) {
                bool result = ExecuteDriverServiceOp(false);
                return result ? 0 : 1;
            }

            wcerr << L"未知选项，使用 --help 查看帮助" << endl;
            return 1;
        }

        // 交互菜单模式
        int choice = -1;
        do {
            ClearScreen();
            wcout << L"\n=== 希沃文件保护驱动控制工具 ===\n"
                << L"1. 开启文件保护驱动\n"
                << L"2. 关闭文件保护驱动\n"
                << L"0. 退出程序\n"
                << L"请输入选择: ";
            wcin >> choice;
            if (wcin.fail()) {
                wcin.clear();
                wcin.ignore((numeric_limits<streamsize>::max)(), L'\n');
                wcerr << L"输入无效，请输入数字" << endl;
                system("pause");
                continue;
            }

            bool result = false;
            switch (choice) {
            case 1:
                result = ExecuteDriverServiceOp(true);
                if (!result) {
                    wcerr << L"操作失败，请查看日志" << endl;
                }
                break;
            case 2:
                result = ExecuteDriverServiceOp(false);
                if (!result) {
                    wcerr << L"操作失败，请查看日志" << endl;
                }
                break;
            case 0:
                wcout << L"退出程序。" << endl;
                WLog(LogLevel::Info, L"User exited");
                break;
            default:
                wcerr << L"无效选择，请重新输入" << endl;
                break;
            }
            if (choice != 0) {
                wcout << L"\n按任意键继续..." << endl;
                (void)_getwch();
            }
        } while (choice != 0);

        return 0;
    }
    catch (const exception& e) {
        wcerr << L"致命错误: " << ConvertString<wstring>(e.what()) << endl;
        WLog(LogLevel::Error, L"Fatal: " + ConvertString<wstring>(e.what()));
        return 1;
    }
}