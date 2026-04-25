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
#include <memory>
#include <map>
#include <stdexcept>

#include "TaggedFirewallController.h"
#include "WinUtils/WinReg.h"
#include "WinUtils/Console.h"
#include "WinUtils/StrConvert.h"
#include "WinUtils/Logger.h"
#include "HugoUtils/HInfo.h"
#include "WinUtils/CmdParser.h"
#include "WinUtils/WinUtils.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "rpcrt4.lib")

using namespace std;
using namespace WinUtils;

// 常量定义
const HKEY kRegRoot = HKEY_LOCAL_MACHINE;
const wstring kRegPath = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\SeewoServiceAssistant.exe";
const wstring kRegValue = L"Debugger";
const wstring kRegDisableValue = L"1";

const wstring kUpdateExe1 = L"EasiUpdate3.exe";
const wstring kUpdateExe2 = L"C:\\ProgramData\\Seewo\\Easiupdate\\easiupdate\\EasiUpdate.exe";

const vector<wstring> kCorePrograms = {
    L"SeewoCore\\SeewoCore.exe",
    L"SeewoServiceAssistant\\SeewoServiceAssistant.exe"
};

// 全局防火墙控制器
struct FirewallGuard
{
    TaggedFirewallController* fw = nullptr;

    FirewallGuard()
    {
        TaggedFirewallController::ControllerIdentity id;
        id.appName = L"Hugo";
        id.version = L"1.0";
        fw = new TaggedFirewallController(id);
        fw->Initialize();
    }

    ~FirewallGuard()
    {
        if (fw) {
            delete fw;
            fw = nullptr;
        }
    }

    TaggedFirewallController* operator->() const { return fw; }
    TaggedFirewallController& operator*() const { return *fw; }
};

// 辅助函数：清屏
void ClearScreen()
{
    system("cls");
}

// 设置希沃服务（通过注册表 Image File Execution Options）
void SetSeewoService(bool disable)
{
    RegKey key;
    if (disable) {
        key.Create(kRegRoot, kRegPath, KEY_READ | KEY_WRITE | KEY_WOW64_64KEY);
        key.SetStringValue(kRegValue, kRegDisableValue);
        if (key.GetStringValue(kRegValue) != kRegDisableValue) {
            throw runtime_error("注册表写入验证失败");
        }
        wcout << L"希沃服务已禁用" << endl;
        WLog(LogLevel::Info, L"Disabled Seewo service via IFEO");
    }
    else {
        auto res = key.TryOpen(kRegRoot, kRegPath, KEY_WRITE | KEY_WOW64_64KEY);
        if (!res.Failed()) {
            key.TryDeleteValue(kRegValue);
        }
        wcout << L"希沃服务已启用" << endl;
        WLog(LogLevel::Info, L"Enabled Seewo service (removed IFEO)");
    }
}

// 阻止/允许希沃更新程序
void SetUpdateBlock(bool block, FirewallGuard& fw)
{
    HInfo info;
    wstring exe = kUpdateExe1;
    for (auto& folder : info.getHugoUpdateFolder()) {
        wstring fullPath = folder + exe;
        if (block) {
            fw->BlockAllAccess(fullPath);
        }
        else {
            fw->DeleteRulesForProgram(fullPath);
        }
    }

    if (block) {
        fw->BlockAllAccess(kUpdateExe2);
        wcout << L"希沃更新已禁用" << endl;
        WLog(LogLevel::Info, L"Blocked Seewo update executables");
    }
    else {
        fw->DeleteRulesForProgram(kUpdateExe2);
        wcout << L"希沃更新已启用" << endl;
        WLog(LogLevel::Info, L"Unblocked Seewo update executables");
    }
}

// 阻止/允许希沃核心进程网络访问
void SetNetworkBlock(bool block, FirewallGuard& fw)
{
    HInfo info;
    auto optFolder = info.getHugoFolder();
    if (!optFolder) {
        wcerr << L"未找到希沃安装目录，网络规则操作跳过" << endl;
        WLog(LogLevel::Warn, L"Hugo installation folder not found, network rules skipped");
        return;
    }

    wstring baseFolder = *optFolder;
    for (auto& relPath : kCorePrograms) {
        wstring fullPath = baseFolder + relPath;
        if (block) {
            fw->BlockAllAccess(fullPath);
        }
        else {
            fw->DeleteRulesForProgram(fullPath);
        }
    }

    if (block) {
        wcout << L"希沃核心进程网络已禁用" << endl;
        WLog(LogLevel::Info, L"Blocked network for Seewo core processes");
    }
    else {
        wcout << L"希沃核心进程网络已启用" << endl;
        WLog(LogLevel::Info, L"Unblocked network for Seewo core processes");
    }
}

// 清除所有希沃相关的防火墙规则
void ClearAllRules(FirewallGuard& fw)
{
    if (fw->CleanupResidualRules()) {
        wcout << L"已清除所有希沃网络规则" << endl;
        WLog(LogLevel::Info, L"Cleared all Seewo firewall rules");
    }
    else {
        wcerr << L"清除规则失败" << endl;
        WLog(LogLevel::Error, L"Failed to clear firewall rules");
    }
}

// 显示帮助信息
void PrintHelp()
{
    wcout << L"用法: HugoDisable.exe [选项]\n"
        << L"选项:\n"
        << L"  --disable           禁用希沃服务\n"
        << L"  --enable            启用希沃服务\n"
        << L"  --disable-update    禁用希沃更新\n"
        << L"  --enable-update     启用希沃更新\n"
        << L"  --disable-net       禁用希沃网络\n"
        << L"  --enable-net        启用希沃网络\n"
        << L"  --clear-rules       清除所有希沃网络规则\n"
        << L"  --help, -h          显示此帮助\n"
        << L"无参数则进入交互菜单\n";
}

// 命令行执行入口
void ExecuteCommand(const wstring& cmd, FirewallGuard& fw)
{
    if (cmd == L"disable") {
        SetSeewoService(true);
    }
    else if (cmd == L"enable") {
        SetSeewoService(false);
    }
    else if (cmd == L"disable-update") {
        SetUpdateBlock(true, fw);
    }
    else if (cmd == L"enable-update") {
        SetUpdateBlock(false, fw);
    }
    else if (cmd == L"disable-net") {
        SetNetworkBlock(true, fw);
    }
    else if (cmd == L"enable-net") {
        SetNetworkBlock(false, fw);
    }
    else if (cmd == L"clear-rules") {
        ClearAllRules(fw);
    }
    else {
        throw runtime_error("未知命令");
    }
}

int wmain(int argc, wchar_t* argv[])
{
    try {
        RequireAdminPrivilege(true);
        EnsureSingleInstance();
        Console().setLocale();

        LoggerCore::Inst().SetDefaultStrategies(L"HugoDisable.log");
        LoggerCore::Inst().EnableApartment(DftLogger);

        FirewallGuard fwGuard;

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

            // 支持的命令列表
            vector<wstring> commands = {
                L"disable", L"enable", L"disable-update", L"enable-update",
                L"disable-net", L"enable-net", L"clear-rules"
            };
            bool executed = false;
            for (auto& cmd : commands) {
                if (parser.hasCommand(cmd)) {
                    ExecuteCommand(cmd, fwGuard);
                    executed = true;
                    break;
                }
            }
            if (!executed) {
                wcerr << L"未知选项，使用 --help 查看帮助" << endl;
                return 1;
            }
            return 0;
        }

        // 交互菜单模式
        int choice = -1;
        do {
            ClearScreen();
            wcout << L"\n=== 希沃控制工具 ===\n"
                << L"1. 禁用希沃服务\n"
                << L"2. 启用希沃服务\n"
                << L"3. 禁用希沃更新\n"
                << L"4. 启用希沃更新\n"
                << L"5. 禁用希沃网络\n"
                << L"6. 启用希沃网络\n"
                << L"7. 清除所有网络规则\n"
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
                SetSeewoService(true);
                break;
            case 2:
                SetSeewoService(false);
                break;
            case 3:
                SetUpdateBlock(true, fwGuard);
                break;
            case 4:
                SetUpdateBlock(false, fwGuard);
                break;
            case 5:
                SetNetworkBlock(true, fwGuard);
                break;
            case 6:
                SetNetworkBlock(false, fwGuard);
                break;
            case 7:
                ClearAllRules(fwGuard);
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
        wcerr << L"致命错误：" << ConvertString<wstring>(e.what()) << endl;
        WLog(LogLevel::Error, L"Fatal: " + ConvertString<wstring>(e.what()));
        return 1;
    }
}