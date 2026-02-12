#include"HugoUtils/WinUtils.h"
#include"HugoUtils/WinSvcMgr.h"
#include"HugoUtils/Console.h"
#include"HugoUtils/Logger.h"
using namespace std;
using namespace WinUtils;
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nShowCmd)
{
	wstring cmd = lpCmdLine;
	Console console;
	console.setLocale();
	LoggerCore::Inst().AddStrategy<ConsoleLogStrategy>();

	if (cmd == L"-start") {
		TerminateProcessesByName(L"HugoLaunchTool.exe");
		std::wstring serviceName = L"SeewoCoreService";
		bool result = WinSvcMgr(serviceName).StartWinService();
	}
	else if (cmd == L"-stop") {
		WinUtils::EnsureSingleInstance();
		return WinUtils::MonitorAndTerminateProcesses(hInstance, { L"SeewoServiceAssistant.exe", L"SeewoCore.exe", L"SeewoAbility.exe",L"SeewoLauncherGuard.exe" });
	}
	return 0;
}
