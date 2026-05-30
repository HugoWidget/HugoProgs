#include "HugoUtils/HInfo.h"

#include <iostream>
#include <string>
#include <optional>
#include <filesystem>
#include <io.h>
#include <fcntl.h>
#include <Windows.h>
#include <WinUtils/Console.h>
using namespace WinUtils;
namespace fs = std::filesystem;

void PrintPath(const wchar_t* desc, const std::optional<fs::path>& opt) {
	std::wcout << desc << L": ";
	if (opt.has_value())
		std::wcout << opt->wstring() << std::endl;
	else std::wcout << L"[未找到]" << std::endl;
}

void PrintString(const wchar_t* desc, const std::optional<std::wstring>& opt) {
	std::wcout << desc << L": ";
	if (opt.has_value())
		std::wcout << *opt << std::endl;
	else std::wcout << L"[未找到]" << std::endl;
}

int main() {
	Console console;
	console.setLocale();
	PrintString(L"SeewoService 版本", HInfo::getHugoVersion());
	PrintPath(L"SeewoService 目录", HInfo::getHugoFolder());
	PrintPath(L"SeewoDriverService 目录", HInfo::getHugoProtectDriverFolder());
	PrintPath(L"DriverService.exe 路径", HInfo::getHugoProtectDriverPath());
	auto updateDirs = HInfo::getHugoUpdateFolder();
	std::wcout << L"Easiupdate3 目录 (" << updateDirs.size() << L" 个):" << std::endl;
	if (updateDirs.empty()) {
		std::wcout << L"  [未找到]" << std::endl;
	}
	else {
		for (const auto& dir : updateDirs)
			std::wcout << L"  " << dir.wstring() << std::endl;
	}
	auto machineId = HInfo::GetMachineId();
	std::wcout << L"MachineId: ";
	if (machineId.has_value())
		std::wcout << std::wstring(machineId->begin(), machineId->end()) << std::endl;
	else std::wcout << L"[未找到]" << std::endl;
	PrintPath(L"SeewoCore.ini 路径", HInfo::GetSeewoCoreIniPath());
	PrintPath(L"SeewoLockConfig.ini 路径", HInfo::GetLockConfigIniPath());
	PrintPath(L".lock_backup 路径", HInfo::GetLockConfigIniPath2());
	PrintPath(L"school.ini 路径", HInfo::GetSeewoSchoolFilePath());
	std::wcout << L"\n按任意键退出...";
	std::wcin.get();
	return 0;
}