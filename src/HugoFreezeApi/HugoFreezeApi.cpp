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
#include <vector>
#include <map>
#include <cstdlib>

#include "HugoUtils/HFreezeApi.h"
#include "WinUtils/Console.h"
#include "WinUtils/Logger.h"
#include "WinUtils/CmdParser.h"
#include <WinUtils/StrConvert.h>

#pragma comment(lib, "advapi32.lib")

using namespace std;
using namespace WinUtils;

// 辅助函数：清屏
void ClearScreen()
{
    system("cls");
}

// 设置冻结卷（盘符）
void SetFreezeVolume(const wstring& volume, IHugoFreeze& api)
{
    wstring vol = (volume == L"0") ? L"" : volume;
    auto res = api.SetFreezeState(vol);
    if (res.result != FrzOR::Success) {
        wcerr << L"设置冻结状态失败：" << res.errMsg << endl;
        WLog(LogLevel::Error, L"SetFreezeState failed: " + res.errMsg);
    }
    else {
        wcout << L"设置冻结状态成功：" << res.msg << endl;
        WLog(LogLevel::Info, L"Set freeze state: " + res.msg);
    }
}

// 查询当前冻结状态
void GetFreezeStatus(IHugoFreeze& api)
{
    auto res = api.GetFreezeState();
    if (res.result != FrzOR::Success) {
        wcerr << L"获取冻结状态失败：" << res.errMsg << endl;
        WLog(LogLevel::Error, L"GetFreezeState failed: " + res.errMsg);
    }
    else {
        wcout << L"当前冻结状态：" << res.msg << endl;
        WLog(LogLevel::Info, L"Freeze state: " + res.msg);
    }
}

// 尝试冻结指定磁盘
void TryProtectDisk(const wstring& disk, IHugoFreeze& api)
{
    wstring d = (disk == L"0") ? L"" : disk;
    auto res = api.TryProtect(d);
    if (res.result != FrzOR::Success) {
        wcerr << L"尝试冻结失败：" << res.errMsg << endl;
        WLog(LogLevel::Error, L"TryProtect failed: " + res.errMsg);
    }
    else {
        wcout << L"尝试冻结成功：" << res.msg << endl;
        WLog(LogLevel::Info, L"TryProtect success: " + res.msg);
    }
}

// 显示帮助信息
void PrintHelp()
{
    wcout << L"用法: HugoFreezeApi.exe [选项]\n"
        << L"选项:\n"
        << L"  --set-vol <磁盘标识>   设置冻结磁盘（0 表示解除全部）\n"
        << L"  --get-disk            查询当前冻结状态\n"
        << L"  --try-protect <磁盘>  尝试冻结指定磁盘\n"
        << L"  --help, -h            显示此帮助\n"
        << L"无参数则进入交互菜单\n";
}

int wmain(int argc, wchar_t* argv[])
{
    try {
        Console().setLocale();

        // 日志配置：仅文件
        LoggerCore::Inst().SetDefaultStrategies(L"HugoFreezeApi.log");
        LoggerCore::Inst().EnableApartment(DftLogger);

        HFreezeApi freezeApi;
        IHugoFreeze& api = freezeApi;

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

            if (parser.hasCommand(L"set-vol")) {
                auto params = parser.getParams(L"set-vol");
                if (params.empty()) {
                    wcerr << L"错误：--set-vol 需要磁盘标识参数" << endl;
                    return 1;
                }
                SetFreezeVolume(params[0], api);
                return 0;
            }

            if (parser.hasCommand(L"get-disk")) {
                GetFreezeStatus(api);
                return 0;
            }

            if (parser.hasCommand(L"try-protect")) {
                auto params = parser.getParams(L"try-protect");
                if (params.empty()) {
                    wcerr << L"错误：--try-protect 需要磁盘标识参数" << endl;
                    return 1;
                }
                TryProtectDisk(params[0], api);
                return 0;
            }

            wcerr << L"未知选项，使用 --help 查看帮助" << endl;
            return 1;
        }

        // 交互菜单模式
        int choice = -1;
        do {
            ClearScreen();
            uint16_t port = 0;
            wstring ip;
            freezeApi.GetConfig(ip, port);
            wcout << L"\n=== 希沃远程冻结工具 ===\n"
                << L"目标 IP: " << ip << L":" << to_wstring(port) << L"\n\n"
                << L"1. 设置冻结磁盘\n"
                << L"2. 查询冻结状态\n"
                << L"3. 尝试冻结磁盘\n"
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
            case 1: {
                wstring vol;
                wcout << L"请输入磁盘标识（0 代表解除全部冻结）：";
                wcin >> vol;
                SetFreezeVolume(vol, api);
                break;
            }
            case 2:
                GetFreezeStatus(api);
                break;
            case 3: {
                wstring disk;
                wcout << L"请输入磁盘标识（0 代表解除全部冻结）：";
                wcin >> disk;
                TryProtectDisk(disk, api);
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