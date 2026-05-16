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
#include <shlobj.h>
#include <fcntl.h>
#include <io.h>
#include <memory>
#include <cstdlib>
#include <stdexcept>

#include "HugoUtils/HPassword.h"
#include "HugoUtils/HInfo.h"
#include "WinUtils/Console.h"
#include "WinUtils/StrConvert.h"
#include "WinUtils/Logger.h"
#include "WinUtils/WinUtils.h"
#include "WinUtils/CmdParser.h"

using namespace WinUtils;
using namespace std;

// 常量定义
const wstring kSeewoCoreIniRelativePath = L"\\Seewo\\SeewoCore\\SeewoCore.ini";
const char* kRegistryMachineIdPath = "SOFTWARE\\Microsoft\\SQMClient";
const char* kRegistryMachineIdValue = "MachineId";
const char* kIniSectionDevice = "device";
const char* kIniKeyId = "id";

// 辅助函数：清屏
void ClearScreen()
{
	system("cls");
}

// 手动信息获取器
class ManualInfoAcquirer : public InfoAcquirer
{
private:
	int m_version;
	string m_ciphertext;
	PasswordType m_type;
public:
	ManualInfoAcquirer(int ver, const string& ct, PasswordType t = TYPE_ADMIN);
	vector<CrackTask> acquire() override;
};

ManualInfoAcquirer::ManualInfoAcquirer(int ver, const string& ct, PasswordType t)
	: m_version(ver), m_ciphertext(ct), m_type(t)
{
}

vector<CrackTask> ManualInfoAcquirer::acquire()
{
	vector<CrackTask> tasks;
	string deviceId;
	string machineId;

	wchar_t seewocoreIniPath[MAX_PATH] = { 0 };
	if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_COMMON_APPDATA, nullptr, 0, seewocoreIniPath))) {
		wcscat_s(seewocoreIniPath, MAX_PATH, kSeewoCoreIniRelativePath.c_str());
		char bufDevice[256] = { 0 };
		GetPrivateProfileStringA(kIniSectionDevice, kIniKeyId, "",
			bufDevice, sizeof(bufDevice), ConvertString<string>(seewocoreIniPath).c_str());
		deviceId = bufDevice;
	}

	machineId = *HInfo::GetMachineId();

	CrackMode mode;
	if (m_version == 1) {
		mode = MODE_V1;
	}
	else if (m_version == 2) {
		mode = MODE_V2;
	}
	else {
		mode = MODE_V3;
	}

	tasks.push_back({ mode, m_type, m_ciphertext, deviceId, machineId , "Manual" });
	return tasks;
}

// 控制台结果输出类
class ConsoleOutput : public ResultOutput
{
private:
	static bool CompareResult(const CrackResult& a, const CrackResult& b)
	{
		if (a.task.mode != b.task.mode) {
			return a.task.mode < b.task.mode;
		}
		return a.task.type < b.task.type;
	}

	wstring ModeToString(CrackMode mode)
	{
		switch (mode) {
		case MODE_V1: return L"V1";
		case MODE_V2: return L"V2";
		case MODE_V3: return L"V3";
		default: return L"未知";
		}
	}

	wstring TypeToString(PasswordType type)
	{
		return (type == TYPE_ADMIN) ? L"管理密码" : L"锁屏密码";
	}

public:
	void output(const vector<CrackResult>& results) override
	{
		vector<CrackResult> sorted = results;
		sort(sorted.begin(), sorted.end(), CompareResult);

		wcout << L"\n==================== 破解结果 ====================\n";
		for (const auto& res : sorted) {
			wcout << L"[" << ModeToString(res.task.mode) << L"] "
				<< TypeToString(res.task.type) << L" : ";
			if (res.success) {
				wcout << L"成功 -> " << ConvertString<wstring>(res.plaintext) << endl;
			}
			else {
				wcout << L"失败 (" << ConvertString<wstring>(res.error_message) << L")" << endl;
			}
		}
		wcout << L"========================================================\n";
	}
};

// 手动输入模式
int RunManualMode()
{
	int version;
	wstring wcipher;
	wcout << L"密文版本 (1/2/3): ";
	wcin >> version;
	wcout << L"密文: ";
	wcin >> wcipher;
	string cipher = ConvertString<string>(wcipher);

	unique_ptr<InfoAcquirer> acquirer = make_unique<ManualInfoAcquirer>(version, cipher, TYPE_ADMIN);
	vector<CrackTask> tasks = acquirer->acquire();

	if (tasks.empty()) {
		wcerr << L"错误: 无破解任务" << endl;
		WLog(LogLevel::Error, L"No crack tasks generated");
		return 1;
	}

	// 打印任务信息
	wcout << L"\n==================== 破解任务 ====================\n";
	for (const auto& task : tasks) {
		wstring modeStr = (task.mode == MODE_V1) ? L"V1" :
			(task.mode == MODE_V2) ? L"V2" : L"V3";
		wstring typeStr = (task.type == TYPE_ADMIN) ? L"管理密码" : L"锁屏密码";
		wcout << L"模式: " << modeStr
			<< L", 类型: " << typeStr
			<< L", 密文: " << ConvertString<wstring>(task.ciphertext)
			<< L", 设备ID: " << ConvertString<wstring>(task.device_id)
			<< L", 机器ID: " << ConvertString<wstring>(task.machine_id)
			<< L", 路径: " << ConvertString<wstring>(task.method)
			<< endl;
	}
	wcout << L"==========================================================\n";

	// 准备解密器
	V1Decryptor d1;
	V2Decryptor d2;
	V3Decryptor d3;
	vector<Decryptor*> decryptors = { &d1, &d2, &d3 };

	// 执行破解
	CrackExecutor executor;
	vector<CrackResult> results = executor.execute(tasks, decryptors);

	// 输出结果
	ConsoleOutput output;
	output.output(results);

	return 0;
}

// 自动读取模式
int RunAutoMode()
{
	unique_ptr<InfoAcquirer> acquirer = make_unique<AutoInfoAcquirer>();
	vector<CrackTask> tasks = acquirer->acquire();

	if (tasks.empty()) {
		wcerr << L"错误: 无破解任务" << endl;
		WLog(LogLevel::Error, L"No crack tasks generated");
		return 1;
	}

	// 打印任务信息
	wcout << L"\n==================== 破解任务 ====================\n";
	for (const auto& task : tasks) {
		wstring modeStr = (task.mode == MODE_V1) ? L"V1" :
			(task.mode == MODE_V2) ? L"V2" : L"V3";
		wstring typeStr = (task.type == TYPE_ADMIN) ? L"管理密码" : L"锁屏密码";
		wcout << L"模式: " << modeStr
			<< L", 类型: " << typeStr
			<< L", 密文: " << ConvertString<wstring>(task.ciphertext)
			<< L", 设备ID: " << ConvertString<wstring>(task.device_id)
			<< L", 机器ID: " << ConvertString<wstring>(task.machine_id)
			<< L", 路径: " << ConvertString<wstring>(task.method)
			<< endl;
	}
	wcout << L"==========================================================\n";

	// 准备解密器
	V1Decryptor d1;
	V2Decryptor d2;
	V3Decryptor d3;
	vector<Decryptor*> decryptors = { &d1, &d2, &d3 };

	// 执行破解
	CrackExecutor executor;
	vector<CrackResult> results = executor.execute(tasks, decryptors);

	// 输出结果
	ConsoleOutput output;
	output.output(results);

	return 0;
}

// 显示帮助信息
void PrintHelp(const wstring& programName)
{
	wcout << L"用法: " << programName << L" [选项]\n"
		<< L"选项:\n"
		<< L"  --manual      手动输入模式\n"
		<< L"  --auto        自动读取模式\n"
		<< L"  --help, -h    显示此帮助\n"
		<< L"无参数则进入交互菜单\n";
}

int wmain(int argc, wchar_t* argv[])
{
	try {
		Console().setLocale();

		// 日志配置：仅文件
		LoggerCore::Inst().SetDefaultStrategies(L"HugoPassword.log");
		LoggerCore::Inst().EnableApartment(DftLogger);

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

			if (parser.hasCommand(L"manual")) {
				return RunManualMode();
			}
			if (parser.hasCommand(L"auto")) {
				return RunAutoMode();
			}

			wcerr << L"未知选项，使用 --help 查看帮助" << endl;
			return 1;
		}

		// 交互菜单模式
		int choice = -1;
		do {
			ClearScreen();
			wcout << L"\n=== 希沃密码破解工具 ===\n"
				<< L"1. 手动输入模式\n"
				<< L"2. 自动读取模式\n"
				<< L"0. 退出程序\n"
				<< L"请输入选择: ";
			wcin >> choice;
			if (wcin.fail()) {
				wcin.clear();
				wcin.ignore(1024, L'\n');
				wcerr << L"输入无效，请输入数字" << endl;
				system("pause");
				continue;
			}

			int result = 0;
			switch (choice) {
			case 1:
				result = RunManualMode();
				if (result != 0) {
					wcerr << L"手动模式执行失败" << endl;
				}
				break;
			case 2:
				result = RunAutoMode();
				if (result != 0) {
					wcerr << L"自动模式执行失败" << endl;
				}
				break;
			case 0:
				wcout << L"退出程序。" << endl;
				WLog(LogLevel::Info, L"User exited");
				break;
			default:
				wcerr << L"无效选择，请重新输入" << endl;
				break;
			}
			if (choice != 0) {
				system("pause");
			}
		} while (choice != 0);

		return 0;
	}
	catch (const exception& e) {
		wcerr << L"致命错误: " << ConvertString<wstring>(e.what()) << endl;
		WLog(LogLevel::Error, L"Fatal: " + ConvertString<wstring>(e.what()));
		return 1;
	}
}