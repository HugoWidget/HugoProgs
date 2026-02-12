#include "HugoUtils/HugoFreezeDriver.h"
#include "HugoUtils/Console.h"
#include "HugoUtils/Logger.h"
#include <iostream>
#include <format>
#include <algorithm>
using namespace std;
using namespace WinUtils;
void PrintHexDump(const unsigned char* data, int len) {
	wcout << L"    偏移量 | 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F" << endl;
	wcout << L"    -------+------------------------------------------------" << endl;

	for (int i = 0; i < len; i += 16) {
		wcout << format(L"    0x{:04X} | ", i);
		for (int j = 0; j < 16; j++) {
			if (i + j < len) {
				wcout << format(L"{:02X} ", data[i + j]);
			}
			else {
				wcout << L"   ";
			}
		}
		wcout << endl;
	}
}

int main() noexcept {
	Console console;
	console.setLocale();
	HugoFreezeDriver& driver = HugoFreezeDriver::Instance();
	LoggerCore::Inst().SetDefaultStrategies(L"HugoFreezeTool.log");

	wcout << L"\n[查询驱动状态]" << endl;
	QueryResult result = driver.QueryDriverStatus();

	if (!result.driverOpenSuccess) {
		wcout << L"打开驱动失败，错误码：" << result.lastError << endl;
		return 1;
	}

	wcout << L"\n运行时状态：" << endl;
	if (result.runtimeStatus.querySuccess) {
		wcout << L"  激活状态 : " << result.runtimeStatus.activeFlag << L" ("
			<< (result.runtimeStatus.activeFlag ? L"已激活" : L"未激活") << L")" << endl;
		wcout << L"  Pointer1    : 0x" << hex << result.runtimeStatus.ptr1 << dec << endl;
		wcout << L"  最后日志 : " << result.runtimeStatus.logStr << endl;
	}
	else {
		wcout << L"  查询运行时状态失败" << endl;
	}

	wcout << L"\n启动配置：" << endl;
	if (result.bootConfig.querySuccess)
		PrintHexDump(result.bootConfig.buffer, result.bootConfig.validLen);
	else
		wcout << L"  查询启动配置失败" << endl;

	wcout << L"\n请输入要冻结的盘符（如CD），或输入 0 解除所有冻结：";
	wstring input;
	wcin >> input;

	transform(input.begin(), input.end(), input.begin(), [](wchar_t c) {
		return static_cast<wchar_t>(toupper(static_cast<wint_t>(c)));
		});

	bool opResult = false;
	if (input == L"0") {
		opResult = driver.UnfreezeAll();
		wcout << L"\n解除所有盘符冻结：" << (opResult ? L"成功" : L"失败") << endl;
	}
	else if (!input.empty()) {
		opResult = driver.FreezeDrives(input);
		wcout << L"\n冻结盘符 [" << input << L"]：" << (opResult ? L"成功" : L"失败") << endl;
	}
	else {
		wcout << L"\n输入无效" << endl;
		return 1;
	}

	if (opResult)wcout << L"\n操作成功！请重启计算机以应用更改。" << endl;

	return 0;
}