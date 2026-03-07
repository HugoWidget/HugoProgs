#include <windows.h>
#include <iostream>
#include <string>
#include <stdexcept>
#include "HugoUtils/WinReg.h"
#include "HugoUtils/Console.h"
#include "HugoUtils/StrConvert.h"
#include "HugoUtils/Logger.h"
#include <HugoUtils/WinUtils.h>

using namespace WinUtils;
using namespace std;

const HKEY REGISTRY_ROOT_KEY = HKEY_LOCAL_MACHINE;
const wstring REGISTRY_PATH = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\SeewoServiceAssistant.exe";
const wstring REGISTRY_VALUE_NAME = L"Debugger";
const wstring REGISTRY_DISABLE_VALUE = L"1";

// 禁用希沃服务
bool DisableSeewoService()
{
	RegKey key;
	key.Create(REGISTRY_ROOT_KEY, REGISTRY_PATH, KEY_READ | KEY_WRITE | KEY_WOW64_64KEY);
	key.SetStringValue(REGISTRY_VALUE_NAME, REGISTRY_DISABLE_VALUE);
	wstring readValue = key.GetStringValue(REGISTRY_VALUE_NAME);
	if (readValue != REGISTRY_DISABLE_VALUE)
		throw runtime_error("验证注册表写入结果失败");
	return true;
}

// 启用希沃服务
bool EnableSeewoService()
{
	RegKey key;
	if (auto result = key.TryOpen(REGISTRY_ROOT_KEY, REGISTRY_PATH, KEY_WRITE | KEY_WOW64_64KEY);
		result.Failed()) return true;
	key.TryDeleteValue(REGISTRY_VALUE_NAME);
	return true;
}

// 执行操作
void ExecuteOperation(int operationMode)
{
	try
	{
		if (operationMode == 0)
		{
			DisableSeewoService();
			wcout << L"操作完成：希沃服务已禁用" << endl;
		}
		else if (operationMode == 1)
		{
			EnableSeewoService();
			wcout << L"操作完成：希沃服务已启用" << endl;
		}
	}
	catch (const exception& e)
	{
		wcerr << L"错误：" << ConvertString<wstring>(e.what()) << endl;
		exit(1);
	}
}

int main(int argc, wchar_t* argv[])
{
	RequireAdminPrivilege(true);
	EnsureSingleInstance();
	Console console;
	console.setLocale();
	LoggerCore::Inst().AddStrategy<ConsoleLogStrategy>();
	LoggerCore::Inst().EnableApartment(DftLogger);

	if (argc > 1)
	{
		wstring var = argv[1];
		if (var == L"-h") {
			wcout << L"用法：程序名 [0|1]" << endl;
			wcout << L"参数说明：" << endl;
			wcout << L"  0 - 禁用希沃服务" << endl;
			wcout << L"  1 - 启用希沃服务" << endl;
			return 0;
		}
		int operationMode = -1;
		if (var == L"-enable") operationMode = 1;
		else if (var == L"-disable") operationMode = 0;
		ExecuteOperation(operationMode);
		return 0;
	}
	int operationMode = -1;
	do
	{
		wcout << L"请选择操作\n0:禁用希沃\n1:启用希沃\n";
		wcin >> operationMode;
		if (operationMode != 0 && operationMode != 1)
			wcout << L"输入无效，请重新输入" << endl;
		else break;
	} while (1);

	ExecuteOperation(operationMode);
	return 0;
}