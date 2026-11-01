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

#include <Windows.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <conio.h>
#include <memory>
#include <cstdlib>
#include <stdexcept>
#include <iomanip>

#include "MemScan.h"

#include "WinUtils/Console.h"
#include "WinUtils/StrConvert.h"
#include "WinUtils/Logger.h"
#include "WinUtils/WinUtils.h"
#include "WinUtils/CmdParser.h"

using namespace WinUtils;
using namespace std;

// ============ 全局权限状态 ============
static bool g_isAdmin = false;
static bool g_debugPriv = false;

// ============ 辅助函数：清屏 ============
void ClearScreen()
{
	system("cls");
}

// ============ 辅助函数：hex+ascii 上下文转储 ============
static void PrintContextBlock(const MemScan::Context& ctx)
{
	const auto& b = ctx.bytes;
	for (size_t off = 0; off < b.size(); off += 16) {
		bool hitline = (off <= ctx.hitOffset && ctx.hitOffset < off + 16);
		wcout << (hitline ? L">> " : L"   ")
			<< L"0x" << uppercase << hex << setw(16) << setfill(L'0')
			<< static_cast<unsigned long long>(ctx.base + off) << L": ";
		for (size_t i = 0; i < 16; ++i) {
			if (off + i < b.size())
				wcout << hex << setw(2) << setfill(L'0')
				<< static_cast<unsigned>(b[off + i]) << L' ';
			else
				wcout << L"   ";
			if (i == 7) wcout << L' ';
		}
		wcout << L" |";
		for (size_t i = 0; i < 16 && off + i < b.size(); ++i) {
			uint8_t c = b[off + i];
			wcout << static_cast<wchar_t>((c >= 0x20 && c < 0x7F) ? c : '.');
		}
		wcout << L"|" << endl;
	}
	wcout << dec << nouppercase;
}

// ============================================================
// 功能 1：希沃密码提取
// ============================================================
static void RunExtractMode()
{
	const char* patAdmin = "adminPassword\\\":\\\"";
	const char* patLock = "lockStreamPassword\\\":\\\"";

	vector<MemScan::Target> targets = {
		{ patAdmin, "管理员密码",  MemScan::Encoding::ANSI,    true, true },
		{ patLock,  "锁屏密码",    MemScan::Encoding::ANSI,    true, true }
	};

	MemScan::Options opt;
	opt.contextBytes = 0;
	opt.writableOnly = true;

	MemScan scanner("SeewoCore.exe", targets, opt);

	string err;
	if (!scanner.Initialize(err)) {
		wcerr << L"[-] 特征码解析失败: " << ConvertString(err) << endl;
		WuLog::Log(LogLevel::Error, L"Extract initialize failed: " + ConvertString(err));
		return;
	}

	// 头部输出
	wcout << L"\n[*] 目标进程: SeewoCore.exe (可随时重启，程序自动重连)\n";
	wcout << L"[*] 提取目标数: " << scanner.GetTargetCount() << endl;
	for (size_t i = 0; i < scanner.GetTargetCount(); ++i) {
		const auto& t = scanner.GetTarget(i);
		wcout << L"    - " << ConvertString(t.label) << right
			<< L" : \"" << ConvertString(t.patternText) << L"\" ("
			<< scanner.GetCompiledPatternSize(i) << L" 字节"
			<< L")" << endl;
	}
	wcout << L"[*] 权限状态: " << (g_isAdmin ? L"管理员" : L"普通用户(建议以管理员运行)")
		<< L" | SeDebugPrivilege: " << (g_debugPriv ? L"已启用" : L"未启用") << endl;
	wcout << L"[*] 报告策略: 去重输出；进程重启后保留历史，仅输出增量；Ctrl+C 返回\n";
	wcout << L"[!] 注意: 内存中的字符串不保证就是最终运行时有效的密码，请自行验证\n" << endl;

	// 命中回调
	scanner.SetHitCallback([](const MemScan::Hit& h) {
		if (!h.isNewValue) return;
		wcout << L"\n[+] 新 " << ConvertString(h.label)
			<< (h.afterRestart ? L" (重启后)" : L"") << L": \""
			<< ConvertString(h.value) << L"\"" << endl;
		wcout << L"    地址: 0x" << uppercase << hex << setw(16) << setfill(L'0')
			<< static_cast<unsigned long long>(h.address) << dec << nouppercase << endl;
		});

	// 进程事件回调
	scanner.SetProcessCallback([](const MemScan::ProcessEvent& ev) {
		switch (ev.type) {
		case MemScan::ProcessEventType::Connected:
			wcout << L"[*] 已连接到 " << ConvertString(ev.procName)
				<< L" (PID " << ev.newPid << L")" << endl;
			break;
		case MemScan::ProcessEventType::Disconnected:
			wcout << L"\n[!] " << ConvertString(ev.procName) << L" 已退出 (原 PID "
				<< ev.oldPid << L")，等待重新启动..." << endl;
			break;
		case MemScan::ProcessEventType::Restarted:
			wcout << L"\n[!] 检测到 " << ConvertString(ev.procName) << L" 进程已重启: PID "
				<< ev.oldPid << L" -> " << ev.newPid << endl;
			break;
		}
		});

	// 状态回调（10 轮节流）
	long long lastReport = 0;
	MemScan::RoundState lastState = MemScan::RoundState::WaitingProcess;

	scanner.SetStatusCallback([&](const MemScan::RoundStatus& s) {
		const bool firstOfState = (lastState != s.state);

		switch (s.state) {
		case MemScan::RoundState::WaitingProcess:
			if (firstOfState) {
				lastReport = s.round;
			}
			else if (s.round - lastReport >= 10) {
				wcout << L"[" << s.round << L"] 等待进程 SeewoCore.exe ..." << endl;
				lastReport = s.round;
			}
			break;

		case MemScan::RoundState::OpenFailed:
			if (firstOfState || s.round - lastReport >= 10) {
				wcout << L"\n[" << s.round << L"] 进程存在但无法打开 (尝试 "
					<< s.triedPids.size() << L" 个 PID, lastError="
					<< s.lastError << L" "
					<< (s.lastError == ERROR_ACCESS_DENIED ? L"ACCESS_DENIED" :
						s.lastError == 0 ? L"（PID 可能瞬时消失）" : L"")
					<< L")" << endl;
				if (s.lastError == ERROR_ACCESS_DENIED) {
					wcout << L"    可能原因 (按概率):" << endl;
					if (!g_isAdmin)
						wcout << L"      [高] 未以管理员身份运行本程序 —— 请右键【以管理员身份运行】" << endl;
					if (!g_debugPriv)
						wcout << L"      [高] SeDebugPrivilege 未启用 —— 管理员下仍失败可能被系统策略禁用" << endl;
					wcout << L"      [中] SeewoCore.exe 是受保护进程(PPL)，普通用户态无法读写 —— 需驱动" << endl;
					wcout << L"      [低] PID 正在退出的窗口期 —— 下轮会自动跳过" << endl;
				}
				else if (s.lastError == 0) {
					wcout << L"    PID 列表: ";
					for (DWORD p : s.triedPids) wcout << p << L' ';
					wcout << L"\n    （进程打开后瞬间消失，下轮自动重试）" << endl;
				}
				lastReport = s.round;
			}
			break;

		case MemScan::RoundState::Scanning: {
			if (firstOfState || s.newValues > 0 || s.round - lastReport >= 10) {
				size_t total = 0;
				for (size_t i = 0; i < scanner.GetTargetCount(); ++i)
					total += scanner.GetValues(i).size();
				wcout << L"[" << s.round << L"] PID " << left << setw(6) << s.pid << right
					<< L" | 区 " << s.regions << L" 块 " << s.chunks
					<< L" | 读 " << static_cast<unsigned long long>(s.bytesRead)
					<< L" B | 命中 " << s.totalHits
					<< L" | 新 " << s.newValues
					<< L" | 累计唯一 " << total << endl;
				lastReport = s.round;
			}
			break;
		}
		}
		lastState = s.state;
		});

	scanner.Run();

	// 最终汇总
	wcout << L"\n[*] 提取结束。" << endl;
	for (size_t i = 0; i < scanner.GetTargetCount(); ++i) {
		const auto& vals = scanner.GetValues(i);
		wcout << L"  [" << ConvertString(scanner.GetTarget(i).label) << L"] 共发现 "
			<< vals.size() << L" 个唯一值:" << endl;
		for (auto& v : vals) wcout << L"    - \"" << ConvertString(v) << L"\"" << endl;
	}
}

// ============================================================
// 功能 2：一般内存扫描（交互式）
// ============================================================
static void RunInteractiveScan()
{
	wstring target;
	wcout << L"进程名或PID: ";
	if (!getline(wcin, target) || target.empty()) return;

	wstring wpattern;
	wcout << L"特征码 (HEX/ASCII): ";
	if (!getline(wcin, wpattern) || wpattern.empty()) return;

	string targetA = ConvertString<string>(target);
	string pat = ConvertString<string>(wpattern);

	MemScan::Target t;
	t.patternText = pat;
	t.label = "scan";
	t.extractString = false;
	t.deduplicate = false;

	MemScan::Options opt;
	opt.contextBytes = 64;
	opt.exitWhenTargetMissing = false;   // 总是等待，与原版一致

	MemScan scanner(targetA, { t }, opt);

	string err;
	if (!scanner.Initialize(err)) {
		wcerr << L"[-] 特征码解析失败: " << ConvertString(err) << endl;
		WuLog::Log(LogLevel::Error, L"Scan initialize failed: " + ConvertString(err));
		return;
	}

	// 头部输出
	wcout << L"[*] 目标: " << target << endl;
	wcout << L"[*] 特征码: " << scanner.GetCompiledPatternSize(0) << L" 字节" << endl;
	wcout << L"[*] 权限: " << (g_isAdmin ? L"管理员" : L"普通用户")
		<< L" | SeDebugPrivilege: " << (g_debugPriv ? L"已启用" : L"未启用") << endl;
	wcout << L"[*] 报告策略: 每 10 轮或命中时输出; 不去重; Ctrl+C 退出\n" << endl;

	scanner.SetHitCallback([](const MemScan::Hit& h) {
		wcout << L"\n--- 命中 @ 0x" << uppercase << hex << setw(16) << setfill(L'0')
			<< static_cast<unsigned long long>(h.address) << L" ---" << dec << nouppercase << endl;
		if (!h.context.valid) {
			wcout << L"    [无法读取上下文]" << endl;
			return;
		}
		PrintContextBlock(h.context);
		});

	scanner.SetProcessCallback([](const MemScan::ProcessEvent& ev) {
		if (ev.type == MemScan::ProcessEventType::Connected)
			wcout << L"[*] 已连接到 " << ConvertString(ev.procName)
			<< L" (PID " << ev.newPid << L")" << endl;
		});

	long long lastReport = 0;
	MemScan::RoundState lastState = MemScan::RoundState::WaitingProcess;

	scanner.SetStatusCallback([&](const MemScan::RoundStatus& s) {
		const bool firstOfState = (lastState != s.state);

		switch (s.state) {
		case MemScan::RoundState::WaitingProcess:
			if (firstOfState) {
				lastReport = s.round;
			}
			else if (s.round - lastReport >= 10) {
				wcout << L"[" << s.round << L"] 等待进程 " << target << L" ..." << endl;
				lastReport = s.round;
			}
			break;

		case MemScan::RoundState::OpenFailed:
			if (firstOfState || s.round - lastReport >= 10) {
				wcout << L"\n[" << s.round << L"] 进程存在但无法打开 (尝试 "
					<< s.triedPids.size() << L" 个 PID, lastError="
					<< s.lastError << L")" << endl;
				if (s.lastError == ERROR_ACCESS_DENIED) {
					if (!g_isAdmin)
						wcout << L"    [高] 未以管理员身份运行 —— 请右键【以管理员身份运行】" << endl;
					if (!g_debugPriv)
						wcout << L"    [高] SeDebugPrivilege 未启用" << endl;
					wcout << L"    [中] 目标可能是受保护进程(PPL)" << endl;
				}
				lastReport = s.round;
			}
			break;

		case MemScan::RoundState::Scanning:
			if (firstOfState || s.totalHits > 0 || s.round - lastReport >= 10) {
				wcout << L"[" << s.round << L"] PID " << left << setw(6) << s.pid << right
					<< L" | 区 " << s.regions << L" 块 " << s.chunks
					<< L" | 读 " << static_cast<unsigned long long>(s.bytesRead)
					<< L" B | 失败 " << s.failedReads
					<< L" | 命中 " << s.totalHits
					<< L" | " << s.elapsedMs << L" ms" << endl;
				lastReport = s.round;
			}
			break;
		}
		lastState = s.state;
		});

	scanner.Run();
	wcout << L"\n[*] 已退出。" << endl;
}

// ============================================================
// 交互菜单
// ============================================================
static void RunMenu()
{
	for (;;) {
		ClearScreen();
		wcout << L"=================================================\n";
		wcout << L"         希沃管家内存扫描器\n";
		wcout << L"=================================================\n";
		wcout << L"  1. 提取密码        (SeewoCore.exe: 管理员密码 + 锁屏密码)\n";
		wcout << L"  2. 一般内存扫描    (自定义进程 + 特征码, 不去重)\n";
		wcout << L"  0. 退出\n";
		wcout << L"-------------------------------------------------\n";
		wcout << L"  当前权限: " << (g_isAdmin ? L"管理员" : L"普通用户")
			<< L" | SeDebugPrivilege: " << (g_debugPriv ? L"已启用" : L"未启用") << endl;
		if (!g_isAdmin || !g_debugPriv) {
			wcout << L"  [!] 若无法打开 SeewoCore.exe, 请右键【以管理员身份运行】" << endl;
		}
		wcout << L"=================================================\n";
		wcout << L"请选择 [0-2]: ";

		int choice = -1;
		wcin >> choice;
		if (wcin.fail()) {
			wcin.clear();
			wcin.ignore(1024, L'\n');
			wcerr << L"输入无效，请输入数字" << endl;
			system("pause");
			continue;
		}
		wcin.ignore(1024, L'\n');

		switch (choice) {
		case 1:
			WuLog::Log(LogLevel::Info, L"Enter extract mode");
			RunExtractMode();
			system("pause");
			break;
		case 2:
			WuLog::Log(LogLevel::Info, L"Enter interactive scan mode");
			RunInteractiveScan();
			system("pause");
			break;
		case 0:
			wcout << L"退出程序。" << endl;
			WuLog::Log(LogLevel::Info, L"User exited");
			return;
		default:
			wcerr << L"无效选择，请重新输入" << endl;
			system("pause");
			break;
		}
	}
}

// ============================================================
// 帮助信息
// ============================================================
void PrintHelp(const wstring& programName)
{
	wcout << L"用法: " << programName << L" [选项]\n"
		<< L"选项:\n"
		<< L"  --extract     提取密码模式 (SeewoCore.exe)\n"
		<< L"  --scan        一般内存扫描模式\n"
		<< L"  --help, -h    显示此帮助\n"
		<< L"无参数则进入交互菜单\n";
}

// ============================================================
// 入口
// ============================================================
int wmain(int argc, wchar_t* argv[])
{
	try {
		Console().setLocale();

		// 日志配置：仅文件
		LoggerCore::Inst().SetDefaultStrategies(GetCurrentProcessDir() + L"HugoScan.log");
		LoggerCore::Inst().EnableApartment(DftLogger);

		// 安装 Ctrl+C 处理器（用于优雅结束扫描循环）
		MemScan::InstallCtrlHandler();

		g_isAdmin = MemScan::IsRunAsAdmin();
		g_debugPriv = MemScan::EnableDebugPrivilege();

		WuLog::Log(LogLevel::Info, L"HugoScan started");

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

			if (parser.hasCommand(L"extract")) {
				RunExtractMode();
				return 0;
			}
			if (parser.hasCommand(L"scan")) {
				RunInteractiveScan();
				return 0;
			}

			wcerr << L"未知选项，使用 --help 查看帮助" << endl;
			return 1;
		}

		// 交互菜单模式
		RunMenu();

		return 0;
	}
	catch (const exception& e) {
		wcerr << L"致命错误: " << ConvertString<wstring>(e.what()) << endl;
		WuLog::Log(LogLevel::Error, L"Fatal: " + ConvertString<wstring>(e.what()));
		return 1;
	}
}