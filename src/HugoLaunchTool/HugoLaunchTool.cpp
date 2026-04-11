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

#include "WinUtils/WinUtils.h"
#include "WinUtils/WinSvcMgr.h"
#include "WinUtils/Console.h"
#include "WinUtils/Logger.h"
#include "WinUtils/CmdParser.h"

#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <stdexcept>
#include <WinUtils/StrConvert.h>

using namespace std;
using namespace WinUtils;

// 常量定义
const wstring kServiceName = L"SeewoCoreService";
const vector<wstring> kProcessesToTerminate = {
    L"SeewoServiceAssistant.exe",
    L"SeewoCore.exe",
    L"SeewoAbility.exe",
    L"SeewoLauncherGuard.exe"
};
const wstring kLaunchToolName = L"HugoLaunchTool.exe";

// 辅助函数：清屏
void ClearScreen()
{
    system("cls");
}

// 启动希沃核心服务
void StartSeewoService()
{
    TerminateProcessesByName(kLaunchToolName);
    WLog(LogLevel::Info, L"Terminated " + kLaunchToolName);

    WinSvcMgr serviceManager(kServiceName);
    bool success = serviceManager.Start();
    if (success) {
        wcout << L"希沃核心服务已启动" << endl;
        WLog(LogLevel::Info, L"Started service: " + kServiceName);
    }
    else {
        wcerr << L"启动服务失败" << endl;
        throw runtime_error("Failed to start SeewoCoreService");
    }
}

// 停止希沃核心服务并终止相关进程
void StopSeewoService()
{
    EnsureSingleInstance();
    MonitorAndTerminateProcesses(GetModuleHandle(nullptr), kProcessesToTerminate);
    wcout << L"希沃相关进程已终止" << endl;
    WLog(LogLevel::Info, L"Terminated Seewo processes");
}

// 显示帮助信息
void PrintHelp()
{
    wcout << L"用法: HugoLaunchTool.exe [选项]\n"
        << L"选项:\n"
        << L"  --start, -start    启动希沃核心服务\n"
        << L"  --stop, -stop      停止希沃相关进程\n"
        << L"  --help, -h         显示此帮助\n"
        << L"无参数则进入交互菜单\n";
}

// 执行命令
void ExecuteCommand(const wstring& command)
{
    if (command == L"start" || command == L"-start") {
        StartSeewoService();
    }
    else if (command == L"stop" || command == L"-stop") {
        StopSeewoService();
    }
    else {
        throw runtime_error("未知命令");
    }
}

int WINAPI wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nShowCmd)
{
    try {
        RequireAdminPrivilege(true);
        Console console;
        console.setLocale();

        // 日志配置：仅文件
        LoggerCore::Inst().SetDefaultStrategies(L"HugoLaunchTool.log");
        LoggerCore::Inst().EnableApartment(DftLogger);

        wstring cmdLine = lpCmdLine ? lpCmdLine : L"";

        // 命令行模式
        if (!cmdLine.empty()) {
            CmdParser parser(true);
            if (!parser.parse(cmdLine)) {
                wcerr << L"命令行解析失败" << endl;
                return 1;
            }

            if (parser.hasCommand(L"help") || parser.hasCommand(L"-h")) {
                PrintHelp();
                return 0;
            }

            if (parser.hasCommand(L"start") || parser.hasCommand(L"-start")) {
                StartSeewoService();
                return 0;
            }
            if (parser.hasCommand(L"stop") || parser.hasCommand(L"-stop")) {
                StopSeewoService();
                return 0;
            }

            wcerr << L"未知选项，使用 --help 查看帮助" << endl;
            return 1;
        }

        console.require();
        // 交互菜单模式
        int choice = -1;
        do {
            ClearScreen();
            wcout << L"\n=== 希沃服务控制工具 ===\n"
                << L"1. 启动希沃核心服务\n"
                << L"2. 停止希沃相关进程\n"
                << L"0. 退出程序\n"
                << L"请输入选择: ";
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
                StartSeewoService();
                break;
            case 2:
                StopSeewoService();
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
                system("pause");
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