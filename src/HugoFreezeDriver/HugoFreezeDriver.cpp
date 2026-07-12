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
#include <iomanip>
#include <string>
#include <cstdlib>
#include <algorithm>

#include "HugoUtils/HFreezeDriver.h"
#include "WinUtils/Console.h"
#include "WinUtils/Logger.h"
#include "WinUtils/WinUtils.h"
#include "WinUtils/CmdParser.h"
#include "WinUtils/StrConvert.h"

using namespace std;
using namespace WinUtils;

std::wstring HexDump(const unsigned char* data, size_t len) {
    std::wstring output;
    output += L"    Offset | 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\n";
    output += L"    -------+------------------------------------------------\n";

    for (size_t i = 0; i < len; i += 16) {
        wchar_t line[100];
        swprintf(line, 100, L"    0x%04zX | ", i);
        output += line;
        for (size_t j = 0; j < 16; ++j) {
            if (i + j < len) {
                swprintf(line, 10, L"%02X ", data[i + j]);
                output += line;
            }
            else {
                output += L"   ";
            }
        }
        output += L"\n";
    }
    return output;
}

// 清屏
void ClearScreen() {
    system("cls");
}

// 将卷掩码转换为盘符列表（例如 0x4 -> "C:"）
wstring VolumeMaskToDrives(uint32_t mask) {
    wstring drives;
    for (int i = 0; i < 26; ++i) {
        if (mask & (1 << i)) {
            if (!drives.empty()) drives += L",";
            drives += wchar_t(L'A' + i);
        }
    }
    return drives.empty() ? L"无" : drives;
}

// 格式化并输出冻结状态
void PrintFreezeStatus(const HFreezeDriver& manager) {
    // 获取综合状态
    auto status = manager.GetFullStatus();
    const auto& disks = status.disks;

    wcout << L"\n========== 希沃驱动冻结状态 ==========" << endl;

    // 盘符状态
    wcout << L"当前冻结状态 (盘符):" << endl;
    if (disks.empty()) {
        wcout << L"  未检测到受保护的盘符" << endl;
    }
    else {
        for (const auto& [letter, info] : disks) {
            wstring stateStr;
            switch (info.state) {
            case DriveFreezeState::Frozen:          stateStr = L"已冻结"; break;
            case DriveFreezeState::Unfrozen:        stateStr = L"未冻结"; break;
            case DriveFreezeState::PendingFreeze:   stateStr = L"待冻结(需重启)"; break;
            case DriveFreezeState::PendingUnfreeze: stateStr = L"待解冻(需重启)"; break;
            default:                                stateStr = L"未知"; break;
            }
            wcout << L"  " << letter << L": " << stateStr << endl;
        }
    }

    // 配置文件信息
    wcout << L"\n配置文件状态:" << endl;
    if (status.fileConfig) {
        const auto& cfg = *status.fileConfig;
        wcout << L"  目标冻结掩码 : 0x" << hex << cfg.readytoProtectVolume << dec
            << L" (" << VolumeMaskToDrives(cfg.readytoProtectVolume) << L")" << endl;
        wcout << L"  当前冻结掩码 : 0x" << hex << cfg.alreadyProtectVolume << dec
            << L" (" << VolumeMaskToDrives(cfg.alreadyProtectVolume) << L")" << endl;
        wcout << L"  冻结请求     : " << (cfg.bNeedFreeze ? L"是" : L"否") << endl;
        wcout << L"  解冻请求     : " << (cfg.bNeedUnFreeze ? L"是" : L"否") << endl;
        wcout << L"  配置版本     : " << (int)cfg.configVersion << endl;
    }
    else {
        wcout << L"  无法读取配置文件" << endl;
    }

    // 启动配置（如果驱动已打开）
    wcout << L"\n驱动启动配置:" << endl;
    if (status.bootConfig) {
        const auto& cfg = *status.bootConfig;
        wcout << L"  启动掩码     : 0x" << hex << cfg.readytoProtectVolume << dec
            << L" (" << VolumeMaskToDrives(cfg.readytoProtectVolume) << L")" << endl;

        // HexDump 原始数据
        uint8_t raw[sizeof(ProtectInfo)];
        cfg.ToBuffer(raw, sizeof(raw));
        wcout << L"  原始数据 (" << sizeof(raw) << L" 字节):\n";
        wcout << HexDump(raw, sizeof(raw));
    }
    else {
        wcout << L"  未获取到驱动启动配置（驱动可能未运行）" << endl;
    }

    // 运行时信息（如果驱动返回了）
    if (status.bootSystem) {
        const auto& bs = *status.bootSystem;
        wcout << L"\n运行时状态:" << endl;
        wcout << L"  驱动状态标志 : 0x" << hex << bs.freezeDriverState << dec << endl;
        wcout << L"  原始IRP计数  : " << bs.originalIrpCount << endl;
        wcout << L"  重定向IRP计数: " << bs.redirectIrpCount << endl;
        wcout << L"  读取字节     : " << bs.readBytes << endl;
        wcout << L"  写入字节     : " << bs.writeBytes << endl;
    }

    wcout << L"==========================================" << endl;
    WuLog::Info(L"查询了冻结状态");
}

// 设置冻结盘符
void ApplyFreezeSettings(HFreezeDriver& manager, const wstring& drives) {
    wstring target = (drives == L"0" ? L"" : drives);

    int mask = CalculateVolumeMask(target);
    if (mask == -1) {
        wcerr << L"无效盘符输入，操作取消。" << endl;
        WuLog::Warn(L"无效的盘符输入: " + target);
        return;
    }

    FreezeResult result = manager.SetFreezeState(target);
    bool success = (result.result == FreezeOperationResult::Success);

    wcout << L"\n设置冻结盘符 [" << (target.empty() ? L"全部解除" : target) << L"] : "
        << (success ? L"成功" : L"失败") << endl;
    if (!result.msg.empty()) {
        wcout << L"消息: " << result.msg << endl;
    }
    if (!result.errMsg.empty()) {
        wcout << L"错误详情: " << result.errMsg << endl;
    }

    if (success) {
        wcout << L"配置已更新，需要重启计算机才能生效。" << endl;
        WuLog::Info(L"冻结设置成功: " + target);
    }
    else {
        WuLog::Error(L"冻结设置失败: " + target);
    }
}

// 显示帮助
void PrintHelp() {
    wcout << L"用法: HugoFreezeDriver.exe [选项]\n"
        << L"选项:\n"
        << L"  --query              查询当前冻结状态\n"
        << L"  --set <盘符>         设置冻结目标盘符（例如 CD 表示C和D盘，0 表示解除所有）\n"
        << L"  --help, -h           显示本帮助信息\n"
        << L"无参数运行则进入交互菜单\n";
}

int wmain(int argc, wchar_t* argv[]) {
    try {
        RequireAdminPrivilege(true);
        Console().setLocale();

        LoggerCore::Inst().SetDefaultStrategies(L"HugoFreezeDriver.log");
        LoggerCore::Inst().EnableApartment(DftLogger);
        HFreezeDriver manager;
        FreezeResult initRes = manager.Init();
        if (initRes.result != FreezeOperationResult::Success) {
            wcerr << L"打开驱动失败，请检查驱动是否安装或权限是否足够。" << endl;
            wcerr << L"错误信息: " << initRes.errMsg << endl;
            WuLog::Error(L"驱动初始化失败: " + initRes.errMsg);
            return 1;
        }
        struct ManagerGuard {
            HFreezeDriver& mgr;
            ~ManagerGuard() { mgr.Cleanup(); }
        } guard{ manager };
        if (argc > 1) {

            wstring cmdLine;
            for (int i = 1; i < argc; ++i) {
                if (i > 1) cmdLine += L" ";
                cmdLine += argv[i];
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
                PrintFreezeStatus(manager);
                return 0;
            }

            if (parser.hasCommand(L"set")) {
                auto params = parser.getParams(L"set");
                if (params.empty()) {
                    wcerr << L"错误：--set 需要盘符参数" << endl;
                    return 1;
                }
                ApplyFreezeSettings(manager, params[0]);
                return 0;
            }

            wcerr << L"未知选项，请使用 --help 查看帮助。" << endl;
            return 1;
        }

        int choice = -1;
        do {
            ClearScreen();
            wcout << L"\n=== 希沃驱动冻结工具 ===" << endl;
            wcout << L"1. 查询冻结状态" << endl;
            wcout << L"2. 设置冻结盘符" << endl;
            wcout << L"0. 退出程序" << endl;
            wcout << L"请输入选择: ";
            wcin >> choice;

            if (wcin.fail()) {
                wcin.clear();
                wcin.ignore(1024, L'\n');
                wcerr << L"输入无效，请输入数字。" << endl;
                system("pause");
                continue;
            }

            switch (choice) {
            case 1:
                PrintFreezeStatus(manager);
                break;
            case 2: {
                wstring input;
                wcout << L"请输入要冻结的盘符（如 CD 表示C和D盘），输入 0 解除所有冻结: ";
                wcin >> input;
                ApplyFreezeSettings(manager, input);
                break;
            }
            case 0:
                wcout << L"正在退出程序..." << endl;
                WuLog::Info(L"用户退出程序");
                break;
            default:
                wcerr << L"无效选择，请重新输入。" << endl;
                break;
            }

            if (choice != 0) {
                system("pause");
            }
        } while (choice != 0);

        return 0;
    }
    catch (const exception& e) {
        wcerr << L"发生致命错误: " << ConvertString<wstring>(e.what()) << endl;
        WuLog::Error(L"致命异常: " + ConvertString<wstring>(e.what()));
        return 1;
    }
}