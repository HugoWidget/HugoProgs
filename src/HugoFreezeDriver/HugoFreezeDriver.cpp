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
#include "HugoUtils/HugoFreezeDriver.h"
#include "WinUtils/Console.h"
#include "WinUtils/Logger.h"
#include "WinUtils/WinUtils.h"

#include <iostream>
#include <format>
#include <algorithm>

using namespace std;
using namespace WinUtils;

/**
 * @brief 主程序入口：驱动状态查询与冻结设置工具
 * @return 程序退出码（0成功，非0失败）
 */
int main() noexcept
{
	RequireAdminPrivilege(true);
	Console console;
	console.setLocale();
	LoggerCore::Inst().SetDefaultStrategies(L"HugoFreezeTool.log");
	LoggerCore::Inst().EnableApartment(DftLogger);

	// 获取驱动实例并初始化
	HugoFreezeDriver& driver = HugoFreezeDriver::Instance();
	if (static_cast<int>(driver.Init().result) != 0)
	{
		wcout << L"打开驱动失败，请检查驱动是否安装或权限是否足够。\n";
		return 1;
	}

	// 查询驱动当前状态
	wcout << L"\n[查询驱动状态]" << endl;
	QueryResult result = driver.QueryDriverStatus();

	wcout << L"\n运行时状态：" << endl;
	if (result.runtimeStatus.querySuccess)
	{
		wcout << L"  激活状态 : " << result.runtimeStatus.activeFlag
			<< L" (" << (result.runtimeStatus.activeFlag ? L"已激活" : L"未激活") << L")" << endl;
		wcout << L"  Pointer1  : 0x" << hex << result.runtimeStatus.ptr1 << dec << endl;
		wcout << L"  最后日志 : " << result.runtimeStatus.logStr << endl;
	}
	else wcout << L"  查询运行时状态失败" << endl;

	wcout << L"\n启动配置：" << endl;
	if (result.bootConfig.querySuccess)
		wcout << HugoFreezeDriver::HexDump(result.bootConfig.buffer, result.bootConfig.validLen);
	else wcout << L"  查询启动配置失败" << endl;

	wcout << L"\n请输入要冻结的盘符（如 CD 表示C盘和D盘），输入 0 解除所有冻结，输入其他字符取消操作：";
	wstring input;
	wcin >> input;

	if (input == L"0")input.clear();
	int mask = CalculateVolumeMask(input);// 计算盘符掩码，若返回-1表示输入无效
	if (mask != -1)
	{
		auto opResult = driver.SetFreezeState(input, DriveFreezeState::Frozen);
		bool success = (static_cast<int>(opResult.result) == 0);
		wcout << L"\n冻结盘符 [" << input << L"]：" << (success ? L"成功" : L"失败") << endl;
		wcout << L"请重启计算机以应用更改。" << endl;
	}
	else wcout << L"\n操作已取消（无效盘符输入）。" << endl;

	driver.Cleanup();
	return 0;
}