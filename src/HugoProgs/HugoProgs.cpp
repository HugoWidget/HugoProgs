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

#include <iostream>
#include <string>
#include <filesystem>
#include <conio.h>
#include <ShlObj.h>

#include "WinUtils/Console.h"
#include "WinUtils/WinUtils.h" 
#include "HugoUtils/HugoArt.h"
#include "WinUtils/ConsoleMenu.h"
#include "HugoUtils/GPL3.h"

using namespace std;
using namespace WinUtils;
namespace fs = filesystem;

// 获取外部程序完整路径
wstring GetExternalProgramPath(const wstring& programName)
{
	wstring fullPath = GetCurrentProcessDir() + programName;
	if (!fs::exists(fullPath))
	{
		wcerr << L"错误：未找到程序文件 " << fullPath << endl;
		return L"";
	}
	return fullPath;
}

// 读取用户输入
wstring ReadUserInput(const wstring& prompt)
{
	wcout << prompt;
	wstring input;
	getline(wcin, input);
	return input;
}

// 在当前控制台执行程序（可选等待）
bool ExecuteProgramInCurrentConsole(const wstring& programPath, const wstring& args = L"", bool wait = true)
{
	if (!wait) {
		RunExternalProgram(programPath, L"open", args);
		return true;
	}
	if (programPath.empty()) return false;

	STARTUPINFOW si = { sizeof(STARTUPINFOW) };
	PROCESS_INFORMATION pi;
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_SHOW;
	si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
	si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
	si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

	wstring cmdLine = L"\"" + programPath + L"\" " + args;
	BOOL ret = CreateProcessW(
		nullptr,
		const_cast<wchar_t*>(cmdLine.c_str()),
		nullptr,
		nullptr,
		TRUE,
		0,
		nullptr,
		GetCurrentProcessDir().c_str(),
		&si,
		&pi
	);

	if (ret)
	{
		WaitForSingleObject(pi.hProcess, INFINITE);
		DWORD exitCode = 0;
		GetExitCodeProcess(pi.hProcess, &exitCode);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
		return true;
	}
	else
	{
		wcerr << L"\n错误：启动程序失败，错误码 " << GetLastError() << endl;
		return false;
	}
}

int main()
{
	Console console;
	console.setLocale();
	HugoArt::PrintArtText(0);

	if (!IsCurrentProcessAdmin())
	{
		wcout << L"当前无管理员权限，部分功能可能无法正常运行\n";
	}

	wcout << L"欢迎使用HugoProgs，按任意键进入主界面";
	(void)_getwch();

	ConsoleMenu menu;

	// 通用命令
	menu.addCommonCommand(L"exit", L"退出", [](ConsoleMenu&, Args) {
		wcout << L"\n程序已退出";
		exit(0);
		});

	menu.addCommonCommand(L"help", L"帮助", [&menu](ConsoleMenu&, Args) {
		menu.setDisplayMode(MenuDisplayMode::Exclusive);
		wcout << L"\n==================================== HugoProgs 帮助 ====================================\n";
		wcout << L"命令格式1：<command> [parameters]\n";
		wcout << L"输入命令后按回车执行，部分命令可能需要额外输入参数，程序会提示相关信息\n";
		wcout << L"命令格式2：c<number>/s<number> 适用于命令/子菜单的快捷数字输入\n";
		wcout << L"例如：输入c1执行当前菜单的第一个命令，输入s2进入当前菜单的第二个子菜单\n";
		wcout << L"输入..可以返回上一级菜单\n";
		wcout << L"输入common可以查看所有菜单通用命令\n";
		wcout << L"祝您使用愉快！\n";
		wcout << L"=======================================================================================\n";
		menu.setDisplayMode(MenuDisplayMode::Normal);
		});

	menu.addCommonCommand(L"license", L"许可", [](ConsoleMenu&, Args) {
		wcout <<
			L"HugoProgs  Copyright (C) 2026  HugoWidget\n"
			L"This program comes with ABSOLUTELY NO WARRANTY; for details type `w'.\n"
			L"This is free software, and you are welcome to redistribute it\n"
			L"under certain conditions; type `c' for details.\n\n";
		wchar_t res = _getwch();
		if (res == L'c') ShowLicense((GetCurrentProcessDir() + L"LICENSE").c_str());
		if (res == L'w') ShowWarranty();
		});

	menu.addCommonCommand(L"about", L"关于", [](ConsoleMenu&, Args) {
		wstring url = L"https://github.com/HugoWidget/HugoProgs";
		wcout << L"源代码仓库 " << url << L" 输入g以跳转\n";
		wchar_t ch = _getwch();
		if (ch == L'G' || ch == L'g') RunExternalProgram(url);
		});

	// ==================== 子菜单：希沃虚拟磁盘管理器 ====================
	auto& mountMenu = menu.addSubmenu(L"mount", L"希沃虚拟磁盘管理器");
	{
		mountMenu.addCommand(L"list", L"枚举虚拟磁盘", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoMount.exe");
			if (!progPath.empty()) ExecuteProgramInCurrentConsole(progPath, L"--list");
			});
		mountMenu.addCommand(L"hlp", L"帮助", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoMount.exe");
			if (!progPath.empty()) ExecuteProgramInCurrentConsole(progPath, L"--help");
			});

		mountMenu.addCommand(L"mnt", L"挂载 <disk> <part> [drive]", [](ConsoleMenu&, Args args) {
			auto params = args.getParams(L"");
			if (params.size() < 2) {
				wcout << L"用法: mnt <disk> <partition> [driveLetter]\n";
				return;
			}
			wstring progPath = GetExternalProgramPath(L"HugoMount.exe");
			if (progPath.empty()) return;

			wstring cmdLine = L"--mount " + params[0] + L" " + params[1];
			if (params.size() >= 3) {
				cmdLine += L" " + params[2];
			}
			ExecuteProgramInCurrentConsole(progPath, cmdLine);
			});

		mountMenu.addCommand(L"unmnt", L"卸载 <disk> <part>/<drive>", [](ConsoleMenu&, Args args) {
			auto params = args.getParams(L"");
			if (params.empty()) {
				wcout << L"用法: unmnt <driveLetter>  或  unmnt <disk> <partition>\n";
				return;
			}
			wstring progPath = GetExternalProgramPath(L"HugoMount.exe");
			if (progPath.empty()) return;

			wstring cmdLine = L"--unmount ";
			if (params.size() == 1 && iswalpha(params[0][0])) {
				cmdLine += wstring(1, towupper(params[0][0]));
			}
			else if (params.size() >= 2) {
				cmdLine += params[0] + L" " + params[1];
			}
			else {
				wcout << L"用法: unmnt <driveLetter>  或  unmnt <disk> <partition>\n";
				return;
			}
			ExecuteProgramInCurrentConsole(progPath, cmdLine);
			});

		mountMenu.addCommand(L"log", L"挂载日志盘 [drive]", [](ConsoleMenu&, Args args) {
			auto params = args.getParams(L"");
			wstring progPath = GetExternalProgramPath(L"HugoMount.exe");
			if (progPath.empty()) return;

			wstring cmdLine = L"--log";
			if (!params.empty()) {
				cmdLine += L" " + params[0];
			}
			ExecuteProgramInCurrentConsole(progPath, cmdLine);
			});

		mountMenu.addCommand(L"conf", L"挂载配置盘 [drive]", [](ConsoleMenu&, Args args) {
			auto params = args.getParams(L"");
			wstring progPath = GetExternalProgramPath(L"HugoMount.exe");
			if (progPath.empty()) return;

			wstring cmdLine = L"--conf";
			if (!params.empty()) {
				cmdLine += L" " + params[0];
			}
			ExecuteProgramInCurrentConsole(progPath, cmdLine);
			});
	}

	// ==================== 子菜单：希沃冰点配置工具 ====================
	auto& freezeMenu = menu.addSubmenu(L"freeze", L"希沃冰点配置工具");
	{
		// 驱动版本
		freezeMenu.addCommand(L"drv.query", L"查询驱动冻结状态", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoFreezeDriver.exe");
			if (!progPath.empty()) ExecuteProgramInCurrentConsole(progPath, L"--query");
			});
		freezeMenu.addCommand(L"drv.set", L"设置驱动冻结盘符", [](ConsoleMenu&, Args) {
			wstring input = ReadUserInput(L"请输入要冻结的盘符（如 CD 表示C和D盘），输入 0 解除所有冻结：");
			if (input.empty()) {
				wcout << L"输入无效\n";
				return;
			}
			wstring progPath = GetExternalProgramPath(L"HugoFreezeDriver.exe");
			if (!progPath.empty()) ExecuteProgramInCurrentConsole(progPath, L"--set " + input);
			});
		freezeMenu.addCommand(L"drv.hlp", L"帮助", [](ConsoleMenu& menu, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoFreezeDriver.exe");
			if (!progPath.empty()) ExecuteProgramInCurrentConsole(progPath, L"--help");
			});
		// API版本
		freezeMenu.addCommand(L"api.get", L"查询API冻结状态", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoFreezeApi.exe");
			if (!progPath.empty()) ExecuteProgramInCurrentConsole(progPath, L"--get-disk");
			});
		freezeMenu.addCommand(L"api.set", L"设置API冻结磁盘", [](ConsoleMenu&, Args) {
			wstring vol = ReadUserInput(L"请输入磁盘标识（0代表解除全部冻结）：");
			if (vol.empty()) {
				wcout << L"输入无效\n";
				return;
			}
			wstring progPath = GetExternalProgramPath(L"HugoFreezeApi.exe");
			if (!progPath.empty()) ExecuteProgramInCurrentConsole(progPath, L"--set-vol " + vol);
			});
		freezeMenu.addCommand(L"api.try", L"尝试API冻结磁盘", [](ConsoleMenu&, Args) {
			wstring disk = ReadUserInput(L"请输入磁盘标识（0代表解除全部冻结）：");
			if (disk.empty()) {
				wcout << L"输入无效\n";
				return;
			}
			wstring progPath = GetExternalProgramPath(L"HugoFreezeApi.exe");
			if (!progPath.empty()) ExecuteProgramInCurrentConsole(progPath, L"--try-protect " + disk);
			});
		freezeMenu.addCommand(L"api.hlp", L"帮助", [](ConsoleMenu& menu, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoFreezeApi.exe");
			if (!progPath.empty()) ExecuteProgramInCurrentConsole(progPath, L"--help");
			});
		freezeMenu.addCommand(L"off", L"希沃官方工具（可能需要先关闭SeewoCore）", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"SeewoFreeze\\SeewoFreezeUI.exe");
			if (!progPath.empty()) ExecuteProgramInCurrentConsole(progPath, L"", false);
			});
		freezeMenu.addCommand(L"stop", L"关闭希沃进程", [](ConsoleMenu& menu, Args) {
			menu.execute(L"launch/stop", false);
			});
	}

	// ==================== 子菜单：希沃服务与网络控制 ====================
	auto& disableMenu = menu.addSubmenu(L"basic", L"希沃服务与网络控制");
	{
		disableMenu.addCommand(L"off", L"关闭文件保护", [](ConsoleMenu& menu, Args) {
			menu.execute(L"fprotect/disable", false);
			});
		disableMenu.addCommand(L"disable", L"禁用希沃服务", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoDisable.exe");
			if (!progPath.empty()) ExecuteProgramInCurrentConsole(progPath, L"--disable");
			});
		disableMenu.addCommand(L"enable", L"启用希沃服务", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoDisable.exe");
			if (!progPath.empty()) ExecuteProgramInCurrentConsole(progPath, L"--enable");
			});
		disableMenu.addCommand(L"update.off", L"禁用希沃更新", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoDisable.exe");
			if (!progPath.empty()) ExecuteProgramInCurrentConsole(progPath, L"--disable-update");
			});
		disableMenu.addCommand(L"update.on", L"启用希沃更新", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoDisable.exe");
			if (!progPath.empty()) ExecuteProgramInCurrentConsole(progPath, L"--enable-update");
			});
		disableMenu.addCommand(L"net.off", L"禁用希沃网络", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoDisable.exe");
			if (!progPath.empty()) ExecuteProgramInCurrentConsole(progPath, L"--disable-net");
			});
		disableMenu.addCommand(L"net.on", L"启用希沃网络", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoDisable.exe");
			if (!progPath.empty()) ExecuteProgramInCurrentConsole(progPath, L"--enable-net");
			});
		disableMenu.addCommand(L"clear", L"清除所有防火墙规则", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoDisable.exe");
			if (!progPath.empty()) ExecuteProgramInCurrentConsole(progPath, L"--clear-rules");
			});
		disableMenu.addCommand(L"hlp", L"帮助", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoDisable.exe");
			if (!progPath.empty()) ExecuteProgramInCurrentConsole(progPath, L"--help");
			});
	}

	// ==================== 子菜单：DLL注入工具 ====================
	auto& injectMenu = menu.addSubmenu(L"inject", L"DLL注入工具");
	{
		injectMenu.addCommand(L"in", L"进程监控+注入", [](ConsoleMenu&, Args) {
			wcout << L"\nDLL注入配置\n";
			wstring dllPath = ReadUserInput(L"请输入DLL文件完整路径：");
			if (dllPath.empty() || !fs::exists(dllPath)) {
				wcout << L"错误：DLL路径无效\n";
				return;
			}
			wstring procName = ReadUserInput(L"请输入目标进程名：");
			if (procName.empty()) {
				wcout << L"错误：进程名不能为空\n";
				return;
			}
			wstring progPath = GetExternalProgramPath(L"HugoInjector.exe");
			if (!progPath.empty())
				ExecuteProgramInCurrentConsole(progPath, L"--inject " + dllPath + L" " + procName, false);
			});

		injectMenu.addCommand(L"un", L"卸载DLL", [](ConsoleMenu&, Args) {
			wcout << L"\nDLL卸载配置\n";
			wstring dllPath = ReadUserInput(L"请输入DLL文件完整路径：");
			if (dllPath.empty() || !fs::exists(dllPath)) {
				wcout << L"错误：DLL路径无效\n";
				return;
			}
			wstring procName = ReadUserInput(L"请输入目标进程名：");
			if (procName.empty()) {
				wcout << L"错误：进程名不能为空\n";
				return;
			}
			wstring progPath = GetExternalProgramPath(L"HugoInjector.exe");
			if (!progPath.empty())
				ExecuteProgramInCurrentConsole(progPath, L"--uninject " + dllPath + L" " + procName);
			});

		injectMenu.addCommand(L"hlp", L"帮助", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoInjector.exe");
			if (!progPath.empty())
				ExecuteProgramInCurrentConsole(progPath, L"--help");
			});
	}

	// ==================== 子菜单：希沃启动工具 ====================
	auto& launchMenu = menu.addSubmenu(L"launch", L"希沃启动工具");
	{
		launchMenu.addCommand(L"start", L"启动希沃", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoLaunchTool.exe");
			if (!progPath.empty())
				ExecuteProgramInCurrentConsole(progPath, L"--start");
			});

		launchMenu.addCommand(L"stop", L"终止希沃进程", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoLaunchTool.exe");
			if (!progPath.empty())
				ExecuteProgramInCurrentConsole(progPath, L"--stop", false);
			});

		launchMenu.addCommand(L"inst", L"安装希沃管家", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoInstaller.exe");
			if (!progPath.empty())
				ExecuteProgramInCurrentConsole(progPath, L"--install");
			});

		launchMenu.addCommand(L"uninst", L"卸载希沃管家", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoInstaller.exe");
			if (!progPath.empty())
				ExecuteProgramInCurrentConsole(progPath, L"--uninstall");
			});
		launchMenu.addCommand(L"hlp", L"帮助", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoInstaller.exe");
			if (!progPath.empty())
				ExecuteProgramInCurrentConsole(progPath, L"--help");
			});
	}

	// ==================== 子菜单：希沃文件保护工具 ====================
	auto& fprotectMenu = menu.addSubmenu(L"fprotect", L"希沃文件保护工具");
	{
		fprotectMenu.addCommand(L"enable", L"启用文件保护", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoProtect.exe");
			if (!progPath.empty())
				ExecuteProgramInCurrentConsole(progPath, L"--enable");
			});
		fprotectMenu.addCommand(L"disable", L"禁用文件保护", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoProtect.exe");
			if (!progPath.empty())
				ExecuteProgramInCurrentConsole(progPath, L"--disable");
			});
		fprotectMenu.addCommand(L"hlp", L"帮助", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoProtect.exe");
			if (!progPath.empty())
				ExecuteProgramInCurrentConsole(progPath, L"--help");
			});
	}

	// ==================== 子菜单：希沃锁屏工具 ====================
	auto& lockMenu = menu.addSubmenu(L"lock", L"希沃锁屏工具");
	{
		lockMenu.addCommand(L"lock", L"说明", [=](ConsoleMenu&, Args) {
			wcout << L"HugoLock在method=lock时被HugoLockAssistant调用\n";
			});
		lockMenu.addCommand(L"assistant", L"说明", [=](ConsoleMenu&, Args) {
			wcout << L"命令：HugoLockAssistant.exe --mode=xxx --method=xxx\n";
			});
		lockMenu.addCommand(L"mode", L"HugoLockAssistant参数", [=](ConsoleMenu&, Args) {
			wcout << L"支持assist或direct，即辅助解锁（需要主动点击解锁）或直接解锁\n";
			});
		lockMenu.addCommand(L"method", L"HugoLockAssistant参数", [=](ConsoleMenu&, Args) {
			wcout << L"支持lock、dbg、frontend、launchtool\n";
			});
	}

	// ==================== 子菜单：希沃密码工具 ====================
	auto& pswMenu = menu.addSubmenu(L"psw", L"希沃密码工具");
	{
		pswMenu.addCommand(L"manual", L"手动", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoPassword.exe");
			if (!progPath.empty())
				ExecuteProgramInCurrentConsole(progPath, L"--manual");
			});
		pswMenu.addCommand(L"auto", L"自动", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoPassword.exe");
			if (!progPath.empty())
				ExecuteProgramInCurrentConsole(progPath, L"--auto");
			});
		pswMenu.addCommand(L"hlp", L"帮助", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoPassword.exe");
			if (!progPath.empty())
				ExecuteProgramInCurrentConsole(progPath, L"--help");
			});
	}

	// 空命令占位，确保 common 可用
	menu.addCommandAtPath(L"", L"common", L"查看通用命令", [](ConsoleMenu&, Args) {});

	menu.run();
	return 0;
}