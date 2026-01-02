#include <iostream>
#include "HugoUtils/Console.h"
#include "HugoUtils/WinUtils.h" 
#include "HugoUtils/HugoArt.h"
#include "HugoUtils/ConsoleMenu.h"
#include "HugoUtils/GPL3.h"
#include "HugoUtils/WinUtils.h"
#include <conio.h>
using namespace std;
using namespace WinUtils;

int main()
{
	Console console;
	console.setLocale();
	HugoArt::PrintArtText(0);
	wcout << L"欢迎使用HugoProgs，按任意键进入\n";
	(void)_getwch();

	using namespace WinUtils;
	ConsoleMenu menu;
	menu.addCommonCommand(L"exit", L"退出", []() {
		wcout << L"程序退出\n";
		exit(0);
		});
	menu.addCommonCommand(L"license", L"许可", []() {
		wcout <<
			L"HugoProgs  Copyright (C) 2026  HugoWidget\n"
			"This program comes with ABSOLUTELY NO WARRANTY; for details type `w'.\n"
			"This is free software, and you are welcome to redistribute it\n"
			"under certain conditions; type `c' for details.\n\n";
		wchar_t res = _getwch();
		if (res == L'c')ShowLicense((GetCurrentProcessDir()+L"LICENSE").c_str());
		if (res == L'w')ShowWarranty();
		});
	menu.addCommonCommand(L"about", L"关于", []() {
		wstring url = L"https://github.com/HugoWidget/HugoProgs";
		HugoArt::PrintArtText(1);
		wcout << L"源代码仓库 " << url << L" 输入g以跳转\n";
		wchar_t ch = _getwch();
		if (ch == L'G' || ch == L'g')RunExternalProgram(url);
		});
	menu.addSubmenu(L"Example", L"示例菜单");
	menu.addCommandAtPath(L"Example", L"cmd1", L"示例命令1", []() {
		std::wcout << L"Action 1\n";
		});
	menu.addCommandAtPath(L"", L"User Manual", L"说明", []() {
		std::wcout << L"该命令行功能仍在开发中\n";
		});
	menu.run();
	return 0;
}
