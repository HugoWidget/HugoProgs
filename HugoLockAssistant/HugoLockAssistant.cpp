#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <cwchar>
#include <chrono>
#include <string>
#include"HugoUtils/Injector.h"
#include"HugoUtils/WinUtils.h"
#include"HugoUtils/Console.h"
#include"HugoUtils/Logger.h"
using namespace WinUtils;
using namespace std;

const WCHAR TARGET_WINDOW_TITLE[] = L"希沃管家";     // 目标窗口标题
const WCHAR TARGET_PROCESS_NAME[] = L"HugoLock.exe"; // 要关闭的进程名
const int STABLE_DURATION = 1000;                    // 窗口稳定时间
const int CHECK_INTERVAL = 100;                      // 监测间隔

vector<HWND>wnds;
HWND deskHWND = GetDesktopWindow();
RECT deskSize = GetWindowRect(deskHWND);

BOOL CALLBACK  IsSpecifiedWnds(HWND hwnd, LPARAM cursor) {
	RECT wndSize = GetWindowRect(hwnd);

	string name;
	WCHAR lpstring[256] = {};
	GetWindowText(hwnd, lpstring, 255);
	if ((wstring)lpstring == TARGET_WINDOW_TITLE && WinUtils::IsWindowTopMost(hwnd)) {
		wnds.push_back(hwnd);
		return false;
	}
	return true;
}

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nShowCmd) {
	WinUtils::RequireAdminPrivilege();
	WinUtils::EnsureSingleInstance();
	LoggerCore::Inst().AddStrategy<ConsoleLogStrategy>();
	const auto RunLock = []() { RunExternalProgram(GetCurrentProcessDir() + TARGET_PROCESS_NAME); };
	RunLock();
	while (true) {
		wnds.clear();
		EnumWindows(IsSpecifiedWnds, 0);
		HWND hForegroundWnd = GetForegroundWindow();
		if (!hForegroundWnd) {
			Sleep(CHECK_INTERVAL);
			continue;
		}
		WCHAR windowTitle[256] = { 0 };
		if (!WinUtils::GetWindowTitle(hForegroundWnd, windowTitle, sizeof(windowTitle) / sizeof(WCHAR))) {
			Sleep(CHECK_INTERVAL);
			continue;
		}
		if (wstring(windowTitle) != TARGET_WINDOW_TITLE) {
			Sleep(CHECK_INTERVAL);
			continue;
		}
		if (!(WinUtils::IsWindowFullScreen(hForegroundWnd, 100, true)
			|| WinUtils::IsWindowTopMost(hForegroundWnd))) {
			Sleep(CHECK_INTERVAL);
			continue;
		}
		bool isWindowStable = true;
		auto startTimer = std::chrono::steady_clock::now();

		while (std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - startTimer
		).count() < STABLE_DURATION) {
			HWND hCurrentWnd = GetForegroundWindow();
			if (hCurrentWnd != hForegroundWnd) {
				isWindowStable = false;
				break;
			}
			Sleep(CHECK_INTERVAL);
		}
		if (isWindowStable) {
			WinUtils::TerminateProcessesByName(TARGET_PROCESS_NAME);
			Sleep(500);
			Injector injector;
			injector.UninjectFromAllProcesses(L"SeewoServiceAssistant.exe", WinUtils::GetCurrentProcessDir() + L"HugoHook.dll");
			RunLock();
		}
		Sleep(500);
	}
	return 0;
}