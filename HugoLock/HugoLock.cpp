#include <iostream>
#include <ShlObj.h>
#include <string>
#include <vector>
#include "HugoUtils/WinUtils.h"
#include "HugoUtils/Injector.h"
#include "HugoUtils/Console.h"
#include "HugoUtils/Logger.h"
using namespace std;
using namespace WinUtils;

vector<HWND>wnds;
HWND deskHWND = GetDesktopWindow();
RECT deskSize = WinUtils::GetWindowRect(deskHWND);
bool hiden = false;
BOOL injected = false;

void Startup();
BOOL CALLBACK IsSpecifiedWnds(HWND hwnd, LPARAM cursor);
void Unlock();

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	Startup();
	Console console;
	console.setLocale();
	LoggerCore::Inst().AddStrategy<ConsoleLogStrategy>();
	LoggerCore::Inst().EnableApartment(DftLogger);
	Injector injector;
	EnableDebugPrivilege();
	wstring dllPath = WinUtils::GetCurrentProcessDir() + L"HugoHook.dll";
	vector<DWORD> injectedProcess;
	while (true) {
		Unlock();
		Sleep(100);
		if (hiden) {
			auto res = injector.InjectToAllProcesses(L"SeewoServiceAssistant.exe", dllPath, injectedProcess);
			injectedProcess.insert(injectedProcess.end(), res.begin(), res.end());
			hiden = false;
		}
	}
	return 0;
}
void Startup()
{
	if (!IsUserAnAdmin())WinUtils::RequireAdminPrivilege(true);
	WinUtils::EnsureSingleInstance();
}
void Unlock() {
	deskHWND = GetDesktopWindow();
	deskSize = WinUtils::GetWindowRect(deskHWND);
	wnds.clear();
	EnumWindows(IsSpecifiedWnds, 0);
	for (auto it = wnds.begin(); it != wnds.end(); it++) {
		WinUtils::ForceHideWindow(*it);
		if (!hiden) {
			hiden = true;
		}
	}
}
BOOL CALLBACK  IsSpecifiedWnds(HWND hwnd, LPARAM cursor) {
	RECT wndSize = WinUtils::GetWindowRect(hwnd);
	WCHAR lpstring[256] = {};
	GetWindowText(hwnd, lpstring, 255);
	if ((wstring)lpstring == L"希沃管家" && WinUtils::IsWindowTopMost(hwnd)) {
		wnds.push_back(hwnd);
		return false;
	}
	return true;
}