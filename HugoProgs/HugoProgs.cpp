#include <iostream>
#include <string>
#include <filesystem>
#include "HugoUtils/Console.h"
#include "HugoUtils/WinUtils.h" 
#include "HugoUtils/HugoArt.h"
#include "HugoUtils/ConsoleMenu.h"
#include "HugoUtils/GPL3.h"
#include <conio.h>
#include <ShlObj.h>

using namespace std;
using namespace WinUtils;
namespace fs = filesystem;

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

wstring ReadUserInput(const wstring& prompt)
{
	wcout << prompt;
	wstring input;
	getline(wcin, input);
	return input;
}

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

	menu.addCommonCommand(L"exit", L"退出", [](ConsoleMenu&, Args) {
		wcout << L"\n程序已退出";
		exit(0);
		});

	menu.addCommonCommand(L"help", L"帮助", [&menu](ConsoleMenu& menu, Args) {
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
	auto& mountMenu = menu.addSubmenu(L"mount", L"希沃虚拟磁盘管理器");
	{
		mountMenu.addCommand(L"log", L"挂载日志盘", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoMount.exe");
			if (!progPath.empty()) ExecuteProgramInCurrentConsole(progPath, L"mount 0 1 p");
			});
		mountMenu.addCommand(L"mnt", L"挂载", [](ConsoleMenu&, Args args) {
			auto params = args.getParams(L"");
			if (params.size() <= 2) {
				wcout << L"<disk> <partition> <driveLetter>:\n";
				return;
			}
			wstring disk = params[0], partition = params[1];
			wstring driveLetter = params[2];
			wstring progPath = GetExternalProgramPath(L"HugoMount.exe");
			if (!progPath.empty()) ExecuteProgramInCurrentConsole(progPath, L"mount " + disk + L" " + partition + L" " + driveLetter);
			});
		mountMenu.addCommand(L"list", L"枚举虚拟磁盘", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoMount.exe");
			if (!progPath.empty()) ExecuteProgramInCurrentConsole(progPath, L"list");
			});
		mountMenu.addCommand(L"conf", L"挂载配置盘", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoMount.exe");
			if (!progPath.empty()) ExecuteProgramInCurrentConsole(progPath, L"mount 1 1 q");
			});
		mountMenu.addCommand(L"unmnt", L"卸载", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoMount.exe");
			wcout << L"<driveLetter>:\n";
			wchar_t diskLetter = getwchar();
			if (!progPath.empty()) ExecuteProgramInCurrentConsole(progPath, L"unmount " + wstring(1, diskLetter));
			});
	}
	auto& freezeMenu = menu.addSubmenu(L"freeze", L"希沃冰点配置工具");
	{
		freezeMenu.addCommand(L"drv", L"希沃Freeze 驱动配置工具", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoFreezeDriver.exe");
			if (!progPath.empty()) ExecuteProgramInCurrentConsole(progPath);
			});

		freezeMenu.addCommand(L"api", L"希沃Freeze API配置工具", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoFreezeApi.exe");
			if (!progPath.empty())ExecuteProgramInCurrentConsole(progPath);
			});
		freezeMenu.addCommand(L"off", L"希沃官方冰点工具", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"SeewoFreeze/SeewoFreezeUI.exe");
			if (!progPath.empty())ExecuteProgramInCurrentConsole(progPath, L"", false);
			});
	}

	auto& injectMenu = menu.addSubmenu(L"inject", L"DLL注入工具"); {
		injectMenu.addCommand(L"in", L"进程监控+注入", [](ConsoleMenu&, Args) {
			wcout << L"\nDLL注入配置\n";
			wstring dllPath = ReadUserInput(L"请输入DLL文件完整路径：");
			if (dllPath.empty() || !fs::exists(dllPath)) {
				wcout << L"错误：DLL路径无效\n";
				return;
			}
			wstring procName = ReadUserInput(L"请输入目标进程名：");
			if (procName.empty())
			{
				wcout << L"错误：进程名不能为空\n";
				return;
			}
			wstring progPath = GetExternalProgramPath(L"HugoInjector.exe");
			if (!progPath.empty())
				ExecuteProgramInCurrentConsole(progPath, (wstring)L"-inject" + dllPath + L" " + procName, false);
			});

		injectMenu.addCommand(L"un", L"卸载dll", [](ConsoleMenu&, Args) {
			wcout << L"\nDLL卸载配置\n";
			wstring dllPath = ReadUserInput(L"请输入DLL文件完整路径：");
			if (dllPath.empty() || !fs::exists(dllPath)) {
				wcout << L"错误：DLL路径无效\n";
				return;
			}
			wstring procName = ReadUserInput(L"请输入目标进程名：");
			if (procName.empty())
			{
				wcout << L"错误：进程名不能为空\n";
				return;
			}
			wstring progPath = GetExternalProgramPath(L"HugoInjector.exe");
			if (!progPath.empty())
				ExecuteProgramInCurrentConsole(progPath, (wstring)L"-uninject" + dllPath + L" " + procName);
			});

		injectMenu.addCommand(L"h", L"帮助", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoInjector.exe");
			if (!progPath.empty())
				ExecuteProgramInCurrentConsole(progPath);
			});
	}

	auto& launchMenu = menu.addSubmenu(L"launch", L"希沃启动工具");
	{
		launchMenu.addCommand(L"start", L"启动希沃", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoLaunchTool.exe");
			if (!progPath.empty())
				ExecuteProgramInCurrentConsole(progPath, L"-start");
			});

		launchMenu.addCommand(L"stop", L"终止希沃进程", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoLaunchTool.exe");
			if (!progPath.empty())
				ExecuteProgramInCurrentConsole(progPath, L"-stop", false);
			});

		launchMenu.addCommand(L"inst", L"安装希沃管家", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoInstaller.exe");
			if (!progPath.empty())
				ExecuteProgramInCurrentConsole(progPath, L"-install", true);
			});

		launchMenu.addCommand(L"uninst", L"卸载希沃管家", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoInstaller.exe");
			if (!progPath.empty())
				ExecuteProgramInCurrentConsole(progPath, L"-uninstall", true);
			});
	}

	auto& fprotectMenu = menu.addSubmenu(L"fprotect", L"希沃文件保护工具");
	{
		fprotectMenu.addCommand(L"enable", L"启用文件保护", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoProtect.exe");
			if (!progPath.empty())
				ExecuteProgramInCurrentConsole(progPath, L"-enable");
			});
		fprotectMenu.addCommand(L"disable", L"禁用文件保护", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoProtect.exe");
			if (!progPath.empty())
				ExecuteProgramInCurrentConsole(progPath, L"-disable");
			});
	}

	auto& lockMenu = menu.addSubmenu(L"lock", L"希沃锁屏工具");
	{
		wstring explainText = L"这个工具不适合在命令行运行\n";
		lockMenu.addCommand(L"HugoLock", L"说明", [=](ConsoleMenu&, Args) {
			wcout << explainText << L"直接运行HugoLock.exe即可使用解除锁屏功能\n";
			});
		lockMenu.addCommand(L"HugoLockAssistant", L"说明", [=](ConsoleMenu&, Args) {
			wcout << explainText << L"HugoLock的辅助程序，处理特殊情况\n";
			});
		lockMenu.addCommand(L"HugoPanel", L"说明", [=](ConsoleMenu&, Args) {
			wcout << explainText << L"自带UIAccess的简洁面板，运行后，锁屏时自动出现\n";
			});
	}

	menu.addCommandAtPath(L"", L"common", L"查看通用命令", [](ConsoleMenu&, Args) {});
	menu.run();

	return 0;
}