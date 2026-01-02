#include <windows.h>
#include <iostream>
#include <array>
#include <string>
#include <vector>
#include <filesystem>
#include <conio.h>
#include <limits>
#include "HugoUtils/WinUtils.h"
#include "HugoUtils/HugoInfo.h"
#include "HugoUtils/Console.h"
#include "HugoUtils/Logger.h"
using namespace std;
using namespace WinUtils;
namespace fs = filesystem;
int GetUserChoice() {
	int choice = -1;
	while (true) {
		wcout << L"=====================================" << endl;
		wcout << L"0 - 关闭 DriverService 服务" << endl;
		wcout << L"1 - 开启 DriverService 服务" << endl;
		wcout << L"=====================================" << endl;
		wcout << L"请输入数字（0/1）：";

		wcin >> choice;

		if (wcin.fail() || (choice != 0 && choice != 1)) {
			wcin.clear();
			wcin.ignore((numeric_limits<streamsize>::max)(), L'\n');
			WLog(LogLevel::Error, L"输入无效！请仅输入0或1");
		}
		else {
			break;
		}
	}
	return choice;
}

// 执行安装
bool RunDriverServiceOperation(const int operation) {
	HugoInfo info;
	auto driverServicePath = info.getHugoProtectDriverPath();
	if (!driverServicePath.has_value()) {
		WLog(LogLevel::Error, L"找不到DriverService.exe");
		return false;
	}
	WLog(LogLevel::Info, format(L"{}DriverService路径：", *driverServicePath));
	const wstring operationName = operation ? L"安装" : L"卸载";
	if (WinUtils::RunExternalProgram(*driverServicePath, L"runas",
		operation ? L"install" : L"uninstall",
		*info.getHugoProtectDriverPath()))
	{
		WLog(LogLevel::Info, format(L"{}操作执行完成！", operationName));
		return true;
	}
	else {
		WLog(LogLevel::Error, format(L"{}操作执行失败！",operationName));
		return false;
	}
}

int main() {
	Console console;
	console.setLocale();
	Logger::Inst().AddStrategy(make_unique<ConsoleLogStrategy>());
	Logger::Inst().AddFormat(LogFormat::Level);
	if (!WinUtils::IsCurrentProcessAdmin()) {
		WLog(LogLevel::Info, L"无管理员权限，按任意键以请求提权");
		(void)_getwch();
		WinUtils::RequireAdminPrivilege(true);
	}
	wcout << L"===== Seewo DriverService 文件保护开启/关闭工具 =====" << endl << endl;
	const int userChoice = GetUserChoice();
	const bool result = RunDriverServiceOperation(userChoice);
	WLog(LogLevel::Info, L"操作完成，按任意键退出");
	(void)_getwch();
	return !result;
}