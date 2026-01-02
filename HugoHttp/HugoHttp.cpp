#include "HugoUtils/HttpConnect.h"
#include "HugoUtils/Logger.h"
#include "HugoUtils/Console.h"
#include <iostream>
#include <cstdlib>
#include <windows.h>
#include <mutex>
#include <vector>
#include <memory>
#include <format>
#include <source_location>
#pragma comment(lib, "advapi32.lib")
using namespace WinUtils;
using namespace std;
#define DEFAULT_IP L"127.0.0.1"
#define DEFAULT_PORT 6082

void clearScreen() {
	system("cls");
}

bool GetRegistryConfig(unsigned short& port) {
	port = DEFAULT_PORT;

	HKEY hKey = NULL;
	LONG lResult = RegOpenKeyExW(
		HKEY_LOCAL_MACHINE,
		L"SOFTWARE\\WOW6432Node\\Zeus\\Rpc\\freeze",
		0,
		KEY_READ | KEY_WOW64_64KEY,
		&hKey
	);

	if (lResult != ERROR_SUCCESS) {
		wcerr << std::format(L"注册表键不存在或无访问权限（错误码：{}），自动使用默认端口：{}", lResult, DEFAULT_PORT);
		return true;
	}

	DWORD portDword = 0;
	DWORD portBufferSize = sizeof(portDword);
	lResult = RegQueryValueExW(
		hKey,
		L"port",
		NULL,
		NULL,
		(LPBYTE)&portDword,
		&portBufferSize
	);
	if (lResult != ERROR_SUCCESS)
		wcerr << std::format(L"注册表中未找到Port键值（错误码：{}），自动使用默认端口：{}", lResult, DEFAULT_PORT);
	else {
		port = (unsigned short)portDword;
		wcerr << std::format(L"从注册表读取到端口：{}", port);
	}
	RegCloseKey(hKey);

	wcerr << std::format(L"最终使用配置：IP={}, Port={}", DEFAULT_IP, port);
	return true;
}

void showMenu(unsigned short port) {
	clearScreen();
	std::wcout << L"=============================================" << std::endl;
	std::wcout << L"           HTTP 接口交互工具" << std::endl;
	std::wcout << L"           目标IP: " << DEFAULT_IP << L":" << port << std::endl;
	std::wcout << L"=============================================" << std::endl;
	std::wcout << L"请选择操作：" << std::endl;
	std::wcout << L"1. GET /api/v1/set?vol=<number>" << std::endl;
	std::wcout << L"2. GET /api/v1/get_disk_data" << std::endl;
	std::wcout << L"3. POST /api/v1/excute_protect_try (Body: {\"selectedDisks\":[\"<string>\"]})" << std::endl;
	std::wcout << L"0. 退出程序" << std::endl;
	std::wcout << L"=============================================" << std::endl;
	std::wcout << L"请输入选择（0-3）：";
}

// 处理 GET /api/v1/set?vol=<num>
void handleSetVol(HttpConnect& httpClient, unsigned short port) {
	std::wstring volStr;
	std::wcout << L"请输入目标磁盘（number）：";
	std::wcin >> volStr;

	for (wchar_t c : volStr) {
		if (!iswdigit(c)) {
			wcerr << L"错误：输入的磁盘号必须是数字！";
			system("pause");
			return;
		}
	}
	std::wstring path = L"/api/v1/set";
	std::wstring getContent = L"vol=" + volStr;
	wcout << std::format(L"发送 GET 请求：{}{}?vol={}\n", DEFAULT_IP, path, volStr);
	wcout << L"返回：\n" <<
		httpClient.getData(DEFAULT_IP, path.c_str(), getContent.c_str());
	system("pause");
}

// 处理 GET /api/v1/get_disk_data
void handleGetDiskData(HttpConnect& httpClient, unsigned short port) {
	std::wstring path = L"/api/v1/get_disk_data";
	std::wstring getContent = L"";
	wcout << std::format(L"发送 GET 请求：{}{}\n", DEFAULT_IP, path);
	wcout << L"返回：\n" <<
		httpClient.getData(DEFAULT_IP, path.c_str(), getContent.c_str());
	system("pause");
}

// 处理 POST /api/v1/excute_protect_try
void handlePostProtectTry(HttpConnect& httpClient, unsigned short port) {
	std::wstring diskStr;
	std::wcout << L"请输入磁盘标识字符串（string）：";
	std::wcin >> diskStr;
	// 构造JSON请求体
	std::wstring postContent = L"{\"selectedDisks\":[\"" + diskStr + L"\"]}";
	std::wstring path = L"/api/v1/excute_protect_try";
	wcout << std::format(L"发送 POST 请求：{}{}\n", DEFAULT_IP, path);
	wcout << std::format(L"POST请求体：{}\n", postContent);
	wcout << L"返回：\n" <<
		httpClient.postData(DEFAULT_IP, path.c_str(), postContent.c_str());
	system("pause");
}

int main() {
	Console console;
	console.setLocale();
	Logger::Inst().AddStrategy(std::make_unique<ConsoleLogStrategy>());
	Logger::Inst().AddFormat(LogFormat::Level);
	unsigned short targetPort = 0;
	GetRegistryConfig(targetPort);
	HttpConnect httpClient;
	httpClient.setPort(targetPort);

	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		wcerr << std::format(L"Winsock初始化失败！错误码：{}", WSAGetLastError());
		system("pause");
		return 1;
	}

	int choice = -1;
	while (true) {
		showMenu(targetPort);
		std::wcin >> choice;

		if (std::wcin.fail()) {
			std::wcin.clear();
			std::wcin.ignore(1024, L'\n');
			wcerr << L"错误：请输入有效的数字（0-3）！";
			system("pause");
			continue;
		}

		switch (choice) {
		case 1:
			handleSetVol(httpClient, targetPort);
			break;
		case 2:
			handleGetDiskData(httpClient, targetPort);
			break;
		case 3:
			handlePostProtectTry(httpClient, targetPort);
			break;
		case 0:
			std::wcout << L"退出程序..." << std::endl;
			WSACleanup();
			exit(0);
		default:
			wcerr << L"错误：无效的选择，请输入 0-3！";
			system("pause");
			break;
		}
	}

	WSACleanup();
	return 0;
}