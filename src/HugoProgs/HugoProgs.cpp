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
#include <fstream>

#include "WinUtils/Console.h"
#include "WinUtils/WinUtils.h" 
#include "HugoUtils/HArt.h"
#include "WinUtils/ConsoleMenu.h"
#include "HugoUtils/GPL3.h"
#include <WinUtils/WinReg.h>

using namespace std;
using namespace WinUtils;
namespace fs = filesystem;
void registerObject(ConsoleMenu& menu);
// 获取外部程序完整路径
wstring GetExternalProgramPath(const wstring& programName);
// 读取用户输入
wstring ReadUserInput(const wstring& prompt)
{
	wcout << prompt;
	wstring input;
	getline(wcin, input);
	return input;
}
// 在当前控制台执行程序（可选等待）
bool ExecuteProgramInCurrentConsole(const wstring& programPath, const wstring& args = L"", bool wait = true);

// 检查是否为 .hps 脚本文件
bool IsScriptFile(const wstring& path)
{
	if (path.size() < 4) return false;
	wstring ext = path.substr(path.size() - 4);
	transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
	return ext == L".hps" && fs::exists(path);
}

// 执行脚本文件
int ExecuteScriptFile(const wstring& scriptPath, ConsoleMenu* menu)
{
	wifstream scriptFile(scriptPath);
	if (!scriptFile.is_open())
	{
		wcerr << L"错误：无法打开脚本文件 " << scriptPath << endl;
		return 1;
	}
	ConsoleMenu* usedMenu = 0;
	ConsoleMenu dummyMenu;
	if (!menu) {
		registerObject(dummyMenu);
		usedMenu = &dummyMenu;
	}
	else usedMenu = &*menu;

	wstring line;
	int lineNum = 0;
	while (getline(scriptFile, line))
	{
		++lineNum;
		size_t start = line.find_first_not_of(L" \t\r\n");
		if (start == wstring::npos) continue;
		size_t end = line.find_last_not_of(L" \t\r\n");
		line = line.substr(start, end - start + 1);
		if (line.empty() || line[0] == L'#') continue;

		wcout << L"[执行] " << line << endl;
		usedMenu->execute(line, true);
	}
	scriptFile.close();
	return 0;
}

// 显示帮助信息
void ShowHelp()
{
	wcout << L"HugoProgs - 希沃教学设备工具箱\n"
		<< L"用法:\n"
		<< L"  HugoProgs.exe                    进入交互式菜单\n"
		<< L"  HugoProgs.exe <命令> [参数...]    直接执行指定命令\n"
		<< L"  HugoProgs.exe --help, -h, /?     显示此帮助信息\n"
		<< L"  HugoProgs.exe --version, -v      显示版本信息\n"
		<< L"  HugoProgs.exe <脚本.hps>         执行脚本文件\n\n";
}

int wmain(int argc, wchar_t* argv[])
{
	Console console;
	console.setLocale();

	wstring rawCmdLine = GetCommandLineW();
	wstring argsPart = ExtractArguments(rawCmdLine);

	// 没有任何参数 -> 进入交互式菜单
	if (argsPart.empty() || argsPart.find_first_not_of(L" \t") == wstring::npos)
	{
		HArt::PrintArtText(0);
		if (!IsCurrentProcessAdmin())
		{
			wcout << L"当前无管理员权限，部分功能可能无法正常运行\n";
		}
		wcout << L"欢迎使用HugoProgs，按任意键进入主界面";
		(void)_getwch();
		ConsoleMenu menu;
		registerObject(menu);
		menu.run();
		return 0;
	}

	// 使用 CmdParser 解析参数字符串
	CmdParser parser(true);
	if (!parser.parse(argsPart, CmdParser::ParseMode::NoFlag))
	{
		wcerr << L"命令行语法错误" << endl;
		return 1;
	}

	const auto& parsed = parser.result();

	// 检测帮助选项
	auto hasOption = [&parsed](const wstring& name) -> bool {
		return parsed.find(name) != parsed.end();
		};
	if (hasOption(L"help") || hasOption(L"h") || hasOption(L"?"))
	{
		ShowHelp();
		return 0;
	}

	wstring scriptPath;
	auto posIt = parsed.find(L"");
	if (posIt != parsed.end())
	{
		for (const auto& arg : posIt->second)
		{
			if (IsScriptFile(arg))
			{
				scriptPath = arg;
				break;
			}
		}
	}

	if (!scriptPath.empty())
	{
		return ExecuteScriptFile(scriptPath, nullptr);
	}

	ConsoleMenu directMenu;
	registerObject(directMenu);
	wcout << L"[执行] " << argsPart << endl;
	directMenu.execute(argsPart, true);
	return 0;
}

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

bool ExecuteProgramInCurrentConsole(const wstring& programPath, const wstring& args, bool wait)
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


void registerObject(ConsoleMenu& menu) {

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
	menu.addCommonCommand(L"privilege", L"提升至管理员权限", [](ConsoleMenu&, Args) {
		if (!IsCurrentProcessAdmin()) {
			RequireAdminPrivilege(true);
		}
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
		freezeMenu.addCommand(L"drv.get", L"查询驱动冻结状态", [](ConsoleMenu&, Args) {
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

	// ==================== 子菜单：WinPE 工具 ====================
	auto& winpeMenu = menu.addSubmenu(L"winpe", L"WinPE工具");
	{
		winpeMenu.addCommand(L"launch", L"启动 WinPE 部署工具", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoWinPE.exe");
			if (!progPath.empty())
				ExecuteProgramInCurrentConsole(progPath, L"--launch");
			});
	}

	menu.addCommandAtPath(L"", L"common", L"查看通用命令", [](ConsoleMenu&, Args) {});

	// ==================== 子菜单：.hps 文件绑定 ====================
	auto& hpsMenu = menu.addSubmenu(L"hps", L".hps 脚本文件绑定");
	{

		// 命令：关联 .hps 文件
		hpsMenu.addCommand(L"assoc", L"将 .hps 文件关联到 HugoProgs（需管理员权限）",
			[](ConsoleMenu&, Args) {
				// 检查是否以管理员身份运行
				if (!IsCurrentProcessAdmin()) {
					wcout << L"错误：关联操作需要管理员权限。请以管理员身份重新运行 HugoProgs 后重试。\n";
					return;
				}

				wstring exePath = GetCurrentProcessPath();
				try {
					WinUtils::RegKey keyExt;
					keyExt.Create(HKEY_CLASSES_ROOT, L".hps", KEY_WRITE);
					keyExt.SetStringValue(L"", L"HugoProgs.Script");


					// 2. 创建 HugoProgs.Script 主项
					WinUtils::RegKey keyMain;
					keyMain.Create(HKEY_CLASSES_ROOT, L"HugoProgs.Script", KEY_WRITE);

					// 3. 设置默认图标：使用程序自身的第一个图标资源（索引 0）
					WinUtils::RegKey keyIcon;
					keyIcon.Create(keyMain.Get(), L"DefaultIcon", KEY_WRITE);
					wstring iconPath = L"\"" + exePath + L"\",0";
					keyIcon.SetStringValue(L"", iconPath);

					// 4. 设置 open 命令：双击时用本程序打开脚本文件
					WinUtils::RegKey keyCommand;
					keyCommand.Create(keyMain.Get(), L"shell\\open\\command", KEY_WRITE);
					wstring cmdLine = L"\"" + exePath + L"\" \"%1\"";
					keyCommand.SetStringValue(L"", cmdLine);

					wcout << L".hps 文件已成功关联到 HugoProgs，图标已刷新。\n";
				}
				catch (const WinUtils::RegException& e) {
					wcout << L"关联失败：注册表操作错误，代码 " << e.code().value()
						<< L"，消息：" << e.what() << L"\n";
					wcout << L"请确保以管理员身份运行本程序。\n";
				}
			});

		// 命令：取消 .hps 文件关联
		hpsMenu.addCommand(L"disassoc", L"取消 .hps 文件关联（需管理员权限）",
			[](ConsoleMenu&, Args) {
				if (!IsCurrentProcessAdmin()) {
					wcout << L"错误：取消关联需要管理员权限。请以管理员身份重新运行 HugoProgs 后重试。\n";
					return;
				}

				try {
					WinUtils::RegKey keyExt;
					if (keyExt.TryOpen(HKEY_CLASSES_ROOT, L".hps", DELETE) ||
						keyExt.TryOpen(HKEY_CLASSES_ROOT, L".hps", KEY_WRITE)) {
						// 注意：RegDeleteTree 需要 Windows XP 以上
						LSTATUS status = RegDeleteTreeW(keyExt.Get(), nullptr);
						if (status != ERROR_SUCCESS && status != ERROR_FILE_NOT_FOUND)
							throw WinUtils::RegException(status, "RegDeleteTree failed for .hps");
					}

					WinUtils::RegKey keyMain;
					if (keyMain.TryOpen(HKEY_CLASSES_ROOT, L"HugoProgs.Script", DELETE) ||
						keyMain.TryOpen(HKEY_CLASSES_ROOT, L"HugoProgs.Script", KEY_WRITE)) {
						LSTATUS status = RegDeleteTreeW(keyMain.Get(), nullptr);
						if (status != ERROR_SUCCESS && status != ERROR_FILE_NOT_FOUND)
							throw WinUtils::RegException(status, "RegDeleteTree failed for HugoProgs.Script");
					}

					wcout << L".hps 文件关联已移除。\n";
				}
				catch (const WinUtils::RegException& e) {
					wcout << L"取消关联失败：注册表操作错误，代码 " << e.code().value()
						<< L"，消息：" << e.what() << L"\n";
					wcout << L"请确保以管理员身份运行本程序。\n";
				}
			});

		hpsMenu.addCommand(L"run", L"执行脚本文件 <path>", [&menu](ConsoleMenu&, Args args) {
			if (auto path = args.getParam(L"", 0)) {
				wstring scriptPath = ResolvePath(*path);
				if (!IsScriptFile(scriptPath)) {
					wcout << L"错误：不是有效的 .hps 文件或文件不存在。\n";
					return;
				}
				ExecuteScriptFile(scriptPath, &menu);
			}
			});
	}

	// ==================== 子菜单：执行外部程序 ====================
	auto& execMenu = menu.addSubmenu(L"execute", L"执行外部程序");

	auto join = [](vector<wstring> params) {
		wstring cmdLine;
		for (size_t i = 1; i < params.size(); ++i) {
			if (i > 1) cmdLine += L' ';
			cmdLine += params[i];
		}
		return cmdLine;
		};
	auto exec = [&](const wstring& verb, const Args& args) {
		auto params = args.getParams(L"");
		if (params.empty()) {
			wcout << L"用法: " << verb << L" <路径> [参数...]\n";
			return;
		}
		HINSTANCE ret = WinUtils::RunExternalProgram(ResolvePath(params[0]), verb, join(params));
		wcout << ((INT_PTR)ret <= 32 ? L"执行失败" : L"程序已启动") << L"\n";
		};

	execMenu.addCommand(L"open", L"普通权限打开 <路径> [参数...]",
		[exec](ConsoleMenu&, Args a) { exec(L"open", a); });
	execMenu.addCommand(L"runas", L"管理员权限运行 <路径> [参数...]",
		[exec](ConsoleMenu&, Args a) { exec(L"runas", a); });
	execMenu.addCommand(L"create", L"CreateProcess阻塞打开 <路径> [参数...]",
		[exec, join](ConsoleMenu&, Args a) {
			auto params = a.getParams(L"");
			if (!params.empty()) {
				ExecuteProgramInCurrentConsole(params[0], join(params));
			}
		});
}