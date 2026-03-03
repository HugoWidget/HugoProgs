#include "HugoUtils/HugoFreezeDriver.h"
#include "HugoUtils/Console.h"
#include "HugoUtils/Logger.h"
#include <iostream>
#include <format>
#include <algorithm>
#include <HugoUtils/WinUtils.h>
using namespace std;
using namespace WinUtils;

int main() noexcept {
	RequireAdminPrivilege(true);
	Console console;
	console.setLocale();
	LoggerCore::Inst().SetDefaultStrategies(L"HugoFreezeTool.log");
	LoggerCore::Inst().EnableApartment(DftLogger);

	//查询驱动状态
	HugoFreezeDriver& driver = HugoFreezeDriver::Instance();
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
	else wcout << L"  查询运行时状态失败" << endl;
	//查询启动配置
	wcout << L"\n启动配置：" << endl;
	if (result.bootConfig.querySuccess)
		wcout << HugoFreezeDriver::HexDump(result.bootConfig.buffer, result.bootConfig.validLen);
	else wcout << L"  查询启动配置失败" << endl;
	//执行操作
	IHugoFreeze& drv = HugoFreezeDriver::Instance();
	wcout << L"\n请输入要冻结的盘符（如CD），或输入 0 解除所有冻结：";
	wstring input;
	wcin >> input;
	if (input == L"0")input = L"";
	bool opResult = !(int)drv.SetFreezeState(input, DriveFreezeState::Frozen).result;
	wcout << L"\n冻结盘符 [" << input << L"]：" << (opResult ? L"成功" : L"失败") << endl;
	if (opResult)wcout << L"\n操作成功！请重启计算机以应用更改。" << endl;

	return 0;
}