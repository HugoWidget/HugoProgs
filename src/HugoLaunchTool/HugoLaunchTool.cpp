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
#include <conio.h>
#include <WinUtils/StrConvert.h>

using namespace std;
using namespace WinUtils;

const wstring kServiceName = L"SeewoCoreService";
const vector<wstring> kProcessesToTerminate = {
    L"SeewoServiceAssistant.exe",
    L"SeewoCore.exe",
    L"SeewoAbility.exe",
    L"SeewoLauncherGuard.exe"
};
const wstring kLaunchToolName = L"HugoLaunchTool.exe";

void ClearScreen() {
    system("cls");
}

void StartSeewoService() {
    TerminateProcessesByName(kLaunchToolName);
    WLog(LogLevel::Info, L"Terminated " + kLaunchToolName);

    WinSvcMgr serviceManager(kServiceName);
    if (serviceManager.Start()) {
        wcout << L"希沃核心服务已启动" << endl;
        WLog(LogLevel::Info, L"Started service: " + kServiceName);
    }
    else {
        wcerr << L"启动服务失败" << endl;
        throw runtime_error("Failed to start SeewoCoreService");
    }
}

void SingleStopProcesses() {
    EnsureSingleInstance(true);
    int closed = TerminateMultipleProcesses(kProcessesToTerminate);
    wcout << L"已终止 " << closed << L" 个希沃进程" << endl;
    WLog(LogLevel::Info, L"Single stop: terminated " + to_wstring(closed) + L" processes");
}

void StartMonitorAndWaitForStop() {
    EnsureSingleInstance(true);

    MonitorHandle handle = StartProcessMonitor(GetModuleHandle(nullptr),
        kProcessesToTerminate,
        1000); // 1秒检查一次
    if (!handle.hStopEvent) {
        wcerr << L"无法启动进程监控" << endl;
        return;
    }

    wcout << L"希沃进程监控已启动，按任意键停止..." << endl;
    (void)_getch();
    wcout << L"正在停止监控..." << endl;

    StopProcessMonitor(handle);
    wcout << L"监控已停止，相关进程处理完毕" << endl;
    WLog(LogLevel::Info, L"Terminated Seewo processes");
}

void PrintHelp() {
    wcout << L"用法: HugoLaunchTool.exe [选项]\n"
        << L"选项:\n"
        << L"  --start, -start    启动希沃核心服务\n"
        << L"  --stop, -stop      持续监控并停止希沃进程（按键退出）\n"
        << L"  --kill, -kill      单次停止希沃进程（立即终止）\n"
        << L"  --help, -h         显示此帮助\n"
        << L"无参数则进入交互菜单\n";
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

            if (parser.hasCommand(L"help") || parser.hasCommand(L"h")) {
                PrintHelp();
                return 0;
            }
            if (parser.hasCommand(L"start")) {
                StartSeewoService();
                return 0;
            }
            if (parser.hasCommand(L"stop")) {
                StartMonitorAndWaitForStop();
                return 0;
            }
            if (parser.hasCommand(L"kill")) {
                SingleStopProcesses();
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
                << L"2. 监控并停止希沃进程\n"
                << L"3. 单次终止希沃进程\n"
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

            wcin.ignore((numeric_limits<streamsize>::max)(), L'\n');

            switch (choice) {
            case 1:
                StartSeewoService();
                break;
            case 2:
                StartMonitorAndWaitForStop();
                break;
            case 3:
                SingleStopProcesses();
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