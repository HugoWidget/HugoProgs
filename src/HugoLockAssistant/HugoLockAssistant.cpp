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
using namespace WinUtils;
using namespace std;
namespace fs = std::filesystem;

static void FatalError(const std::wstring& msg)
{
	MessageBoxW(nullptr, msg.c_str(), L"HugoLockAssistant", MB_ICONERROR);
	ExitProcess(1);
}

int APIENTRY wWinMain(
	_In_ HINSTANCE /*hInstance*/,
	_In_opt_ HINSTANCE /*hPrevInstance*/,
	_In_ LPWSTR lpCmdLine,
	_In_ int /*nCmdShow*/)
{
	CmdParser parser(true);
	string_t cmdLineStr(lpCmdLine);

	if (!parser.parse(ExtractArguments(GetCommandLine()))) {
		FatalError(L"Command line syntax error.");
	}

	auto methodOpt = parser.getParam(L"method", 0);
	auto modeOpt = parser.getParam(L"mode", 0);

	if (!methodOpt || !modeOpt) {
		FatalError(L"Usage: HugoLockAssistant --method=<dbg|launchtool|frontend|lock> --mode=<assist|direct>");
	}

	std::wstring method = *methodOpt;
	std::wstring mode = *modeOpt;

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
		else // direct
			args = L"--stop";
	}
	else if (method == L"dbg") {
		if (mode == L"assist")
			args = L"--lockfile=delete ";
		else // direct
			args = L"--lockfile=create ";
	}
	else {
		args = L"--mode=" + mode;
	}
	args += L" ";
	if (auto extra = parser.getParam(L"extracmd", 0)) {
		if (extra)args += *extra;
	}
	int ret = (int)RunExternalProgram(targetPath, L"open", args);

	if (ret <= 32) {
		FatalError(L"Failed to start " + exeName + L". Error: " + std::to_wstring(GetLastError()));
	}
	return 0;
}