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

#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <string_view>
#include <filesystem>

#include "WinUtils/CmdParser.h"
#include "WinUtils/WinUtils.h"
#include "WinUtils/Logger.h"
using namespace WinUtils;
using namespace std;
namespace fs = std::filesystem;

static const wchar_t* kUsage =
L"用法:\n"
L"  HugoLockAssistant.exe --method=<dbg|launchtool|frontend|lock> --mode=<assist|direct|disable>\n"
L"                        [--extracmd=<可选命令>] [--hide] [--launch]\n\n"
L"参数:\n"
L"  --method   要调用的子工具\n"
L"  --mode     传给子工具的运行模式\n"
L"  --extracmd 追加到子工具命令行的额外参数\n"
L"  --hide     以隐藏窗口方式启动子工具\n"
L"  --launch   跳过确认对话框直接启动\n"
L"  --help, -h, /?  显示此帮助信息";

static void FatalError(const std::wstring& msg)
{
	WuLog::Error(msg);
	MessageBoxW(nullptr, msg.c_str(), L"HugoLockAssistant", MB_ICONERROR);
	ExitProcess(1);
}
// 复制到剪贴板
void CopyToClipboard(wstring text) {
	if (OpenClipboard(nullptr)) {
		EmptyClipboard();
		size_t len = (text.length() + 1) * sizeof(wchar_t);
		HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
		if (hMem) {
			void* pMem = GlobalLock(hMem);
			if (pMem) {
				memcpy(pMem, text.c_str(), len);
				GlobalUnlock(hMem);
			}
			SetClipboardData(CF_UNICODETEXT, hMem);
		}
		CloseClipboard();
	}
};
int APIENTRY wWinMain(
	_In_ HINSTANCE /*hInstance*/,
	_In_opt_ HINSTANCE /*hPrevInstance*/,
	_In_ LPWSTR lpCmdLine,
	_In_ int /*nCmdShow*/)
{
	CmdParser parser(true);
	string_t cmdLineStr(lpCmdLine);
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	LoggerCore::Inst().SetDefaultStrategies(GetCurrentProcessDir() + L"HugoLockAssistant.log");
	LoggerCore::Inst().EnableApartment(DftLogger);

	if (!parser.parse(ExtractArguments(GetCommandLine()))) {
		FatalError(L"Command line syntax error.");
	}

	if (parser.hasCommand(L"help") || parser.hasCommand(L"h") || parser.hasCommand(L"?")) {
		MessageBoxW(nullptr, kUsage, L"HugoLockAssistant", MB_ICONINFORMATION);
		return 0;
	}

	auto methodOpt = parser.getParam(L"method", 0);
	auto modeOpt = parser.getParam(L"mode", 0);

	if (!methodOpt || !modeOpt) {
		FatalError(kUsage);
	}

	std::wstring method = *methodOpt;
	std::wstring mode = *modeOpt;
	WuLog::Info(L"method={}, mode={}", method, mode);

	std::wstring exeName;
	if (method == L"dbg")           exeName = L"HugoDbg.exe";
	else if (method == L"launchtool") exeName = L"HugoLaunchTool.exe";
	else if (method == L"frontend")   exeName = L"HugoFrontend.exe";
	else if (method == L"lock")       exeName = L"HugoLock.exe";
	else {
		FatalError(L"Invalid method. Must be one of: dbg, launchtool, frontend, lock.");
	}

	std::wstring dir = GetCurrentProcessDir();
	std::wstring targetPath = dir + exeName;
	if (!fs::exists(targetPath)) {
		FatalError(L"Target program not found: " + targetPath);
	}

	std::wstring args;
	if (method == L"launchtool") {
		if (mode == L"assist")
			args = L"--kill";
		else if (mode == L"direct")
			args = L"--stop";
		else if (mode == L"disable")
			args = L"--stop";
	}
	else if (method == L"dbg") {
		if (mode == L"assist")
			args = L"--fso=assist ";
		else if (mode == L"direct")
			args = L"--fso=direct ";
		else if (mode == L"disable")
			args = L"--fso=disable ";
	}
	else {
		args = L"--mode=" + mode;
	}
	args += L" ";
	if (auto extra = parser.getParam(L"extracmd", 0)) {
		if (extra) args += *extra;
	}

	bool hide = parser.hasCommand(L"hide");
	bool launch = parser.hasCommand(L"launch");

	if (launch) {
		WuLog::Info(L"启动子工具: " + targetPath + L" " + args);
		int ret = reinterpret_cast<int>(RunExternalProgram(targetPath, L"open", args, L"", hide ? SW_HIDE : SW_SHOWNORMAL));
		if (ret <= 32) {
			FatalError(L"Failed to start " + exeName + L". Error: " + std::to_wstring(GetLastError()));
		}
		return 0;
	}

	// 未指定 --launch，显示预览对话框
	std::wstring fullCmd = L"\"" + targetPath + L"\" " + args;
	std::wstring msg = L"由于子工具功能不局限于解除锁屏，现不再建议使用该工具启动\n"
		"要不显示该消息，请加上--launch选项\n"
		"将要执行的命令行：\n" + fullCmd + L"\n\n[中止] 复制到剪贴板\n[重试] 直接启动\n[忽略] 取消";
	int msgRet = MessageBoxW(nullptr, msg.c_str(), L"HugoLockAssistant",
		MB_ABORTRETRYIGNORE | MB_ICONINFORMATION);
	if (msgRet == IDABORT) {
		CopyToClipboard(fullCmd);
	}
	else if (msgRet == IDRETRY) {
		int ret = reinterpret_cast<int>(RunExternalProgram(targetPath, L"open", args, L"", hide ? SW_HIDE : SW_SHOWNORMAL));
		if (ret <= 32) {
			FatalError(L"Failed to start " + exeName + L". Error: " + std::to_wstring(GetLastError()));
		}
	}
	return 0;
}