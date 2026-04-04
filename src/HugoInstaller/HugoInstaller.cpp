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
#include "HugoUtils/HugoUtilsDef.h"

#include <iostream>
#include <fstream>
#include <string>
#include <functional>
#include <filesystem>
#include <algorithm>
#include <chrono>
#include <regex>
#include <vector>
#include <utility>
#include <limits>
#include <sstream>

#include "HugoUtils/HugoInstaller.h"
#include <windows.h>
#include <shellapi.h>
#include <WinUtils/Console.h>
#include <WinUtils/StrConvert.h>
#include <WinUtils/WinUtils.h>

using namespace std;
namespace fs = filesystem;
using namespace WinUtils;

static vector<PackageInfo> scanPackages(const wstring& dir) {
	vector<PackageInfo> pkgs;
	wregex re(LR"(SeewoServiceSetup_(\d+\.\d+\.\d+\.\d+)\.exe)");
	try {
		if (fs::exists(dir)) {
			for (auto& entry : fs::directory_iterator(dir)) {
				if (!entry.is_regular_file()) continue;
				wsmatch m;
				auto fname = entry.path().filename().wstring();
				if (regex_match(fname, m, re))
					pkgs.push_back({ m[1].str(), entry.path().wstring() });
			}
		}
	}
	catch (const fs::filesystem_error& e) {
		wcerr << L"[-] 扫描失败: " << ConvertString<wstring>(e.what()) << L'\n';
	}
	return pkgs;
}

static bool isValidVersion(const wstring& ver) {
	if (ver == L"0") return true;
	wregex re(LR"(\d+\.\d+\.\d+\.\d+)");
	return regex_match(ver, re);
}

static bool runPackage(const wstring& path) {
	wcout << L"[+] 启动安装包: " << path << L'\n';
	HINSTANCE h = ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
	if (reinterpret_cast<INT_PTR>(h) > 32) {
		wcout << L"[+] 启动成功\n";
		return true;
	}
	wcerr << L"[-] 启动失败，错误码: " << GetLastError() << L'\n';
	return false;
}

static int install() {
	wstring pkgDir = GetCurrentProcessDir() + L"SetupPackage";
	fs::create_directories(pkgDir);

	auto pkgs = scanPackages(pkgDir);
	wcout << L"\n==================== 可选版本 ====================\n";
	wcout << L"[0] 下载新版本\n";
	for (size_t i = 0; i < pkgs.size(); ++i)
		wcout << L"[" << (i + 1) << L"] " << pkgs[i].version << L" (" << fs::path(pkgs[i].filePath).filename().wstring() << L")\n";
	wcout << L"==================================================\n";

	int choice = 0;
	wcout << L"请选择: ";
	while (!(wcin >> choice) || choice < 0 || choice > static_cast<int>(pkgs.size())) {
		wcin.clear(); wcin.ignore((numeric_limits<streamsize>::max)(), L'\n');
		wcerr << L"[-] 无效输入，重新选择: ";
	}

	if (1 <= choice && choice <= static_cast<int>(pkgs.size()))
		return runPackage(pkgs[choice - 1].filePath) ? 0 : 1;

	// 下载新版本
	wcout << L"\n==================== 下载新版本 ====================\n";
	wstring ver;
	wcout << L"输入版本号 (格式 1.X.X.XXXX, 0 表示最新): ";
	while (!(wcin >> ver) || !isValidVersion(ver)) {
		wcin.clear(); wcin.ignore((numeric_limits<streamsize>::max)(), L'\n');
		wcerr << L"[-] 格式错误，重新输入: ";
	}

	wstring url = BASE_URL;
	if (ver != L"0")url += L"&version=" + ver;
	wcout << L"[+] URL: " << url << L"\n[+] 保存到: " << pkgDir << L"\n--------------------------------------------------\n";

	try {
		HttpDownloader dl;
		dl.setTimeout(60);
		dl.setUserAgent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");

		auto start = chrono::steady_clock::now();
		size_t lastPrint = 0;

		auto res = dl.download(ConvertString<string>(url), ConvertString<string>(pkgDir), "",
			[&](int64_t done, int64_t total) {
				if (total <= 1 || done - lastPrint < 1024 * 1024) return;
				lastPrint = done;
				auto elapsed = chrono::duration<double>(chrono::steady_clock::now() - start).count();
				double speed = (done / 1024.0 / 1024.0) / elapsed;
				int pct = int(done * 100 / total);
				wcout << L'\r' << L'[' << wstring(pct / 2, L'=') << (pct < 100 ? L">" : L"") << wstring(50 - pct / 2 - (pct < 100 ? 1 : 0), L' ')
					<< L"] " << pct << L"% " << done / 1024 / 1024 << L'/' << total / 1024 / 1024 << L" MB " << speed << L" MB/s" << flush;
			});

		wcout << L"\n--------------------------------------------------\n";
		fs::path tempPath = fs::path(pkgDir) / "SeewoServiceSetup.tmp";

		if (!res.success) {
			wcerr << L"[-] 下载失败\n[-] " << ConvertString<wstring>(res.errorMsg) << L"\n[-] HTTP " << res.statusCode << L'\n';
			if (res.errorMsg.find('7') != string::npos)
				wcerr << L"可能已经下载过了\n";
			if (res.statusCode == 404)
				wcerr << L"[-] 版本不存在或已下架\n";
			// 删除空的临时文件
			if (fs::exists(tempPath) && fs::file_size(tempPath) == 0)
				fs::remove(tempPath);
			return 1;
		}

		if (res.downloaded < 1000000) {
			fs::remove(tempPath);
			wcerr << L"[-] 文件太小，版本可能错误\n";
			return 1;
		}

		// 确定最终文件名
		wstring finalFilename;
		if (!res.filename.empty()) {
			finalFilename = ConvertString<wstring>(res.filename);
		}
		else {
			if (ver != L"0") {
				finalFilename = L"SeewoServiceSetup_" + ver + L".exe";
			}
			else {
				// 版本为0且未能从URL提取文件名，使用默认名
				finalFilename = L"SeewoServiceSetup_latest.exe";
			}
		}

		fs::path finalPath = fs::path(pkgDir) / finalFilename;
		// 如果目标文件已存在，先删除
		error_code ec;
		if (fs::exists(finalPath)) {
			fs::remove(finalPath, ec);
		}
		fs::rename(tempPath, finalPath, ec);
		if (ec) {
			wcerr << L"[-] 重命名失败: " << ConvertString<wstring>(ec.message()) << L'\n';
			return 1;
		}

		wcout << L"[+] 下载完成\n[+] 大小: " << res.downloaded << L" 字节 (" << res.downloaded / 1024.0 / 1024.0 << L" MB)\n";
		wcout << L"[+] 保存为: " << finalFilename << L'\n';
		return runPackage(finalPath.wstring()) ? 0 : 1;
	}
	catch (const exception& e) {
		wcerr << L"[-] 异常: " << ConvertString<wstring>(e.what()) << L'\n';
		return 1;
	}
}

static int uninstall() {
	wstring target = L"C:\\Program Files (x86)\\Seewo\\SeewoService\\uninstall_verify.exe";
	wstring run = L"C:\\Program Files (x86)\\Seewo\\SeewoService\\Uninstall.exe";
	wstring fake = GetCurrentProcessDir() + L"HugoFakeVerify.exe";

	if (!fs::exists(fake)) { wcerr << L"[-] 模板缺失: " << fake << L'\n'; return 1; }
	if (!fs::exists(run)) { wcerr << L"[-] 可能未安装希沃\n"; return 1; }

	// 读取模板
	vector<BYTE> data;
	HANDLE hFake = CreateFileW(fake.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hFake == INVALID_HANDLE_VALUE) { wcerr << L"[-] 无法读取模板, 错误 " << GetLastError() << L'\n'; return 1; }
	DWORD sz = GetFileSize(hFake, nullptr);
	data.resize(sz);
	DWORD read;
	if (!ReadFile(hFake, data.data(), sz, &read, nullptr) || read != sz) {
		CloseHandle(hFake); wcerr << L"[-] 读取模板失败\n"; return 1;
	}
	CloseHandle(hFake);

	// 备份原文件
	wchar_t temp[MAX_PATH] = {};
	GetTempFileNameW(target.substr(0, target.find_last_of(L'\\')).c_str(), L"OLD", 0, temp);
	if (!MoveFileW(target.c_str(), temp) && GetLastError() != ERROR_FILE_NOT_FOUND) {
		wcerr << L"[-] 无法备份目标文件, 错误 " << GetLastError() << L'\n';
		return 1;
	}

	// 写入新文件
	HANDLE hTarget = CreateFileW(target.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hTarget == INVALID_HANDLE_VALUE) {
		wcerr << L"[-] 无法写入目标, 错误 " << GetLastError() << L'\n';
		MoveFileW(temp, target.c_str()); // 还原
		return 1;
	}
	DWORD written;
	if (!WriteFile(hTarget, data.data(), sz, &written, nullptr) || written != sz) {
		wcerr << L"[-] 写入失败\n";
		CloseHandle(hTarget);
		MoveFileW(temp, target.c_str());
		return 1;
	}
	CloseHandle(hTarget);
	wcout << L"[+] 验证文件替换成功\n";

	// 启动卸载
	HINSTANCE h = ShellExecuteW(nullptr, L"open", run.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
	if (reinterpret_cast<INT_PTR>(h) > 32) {
		wcout << L"[+] 卸载程序已启动\n";
		return 0;
	}
	wcerr << L"[-] 启动卸载失败, 错误 " << GetLastError() << L'\n';
	return 1;
}

int wmain(int argc, wchar_t* argv[]) {
	Console().setLocale();

	if (argc > 1) {
		wstring arg = argv[1];
		if (arg == L"-install") return install();
		if (arg == L"-uninstall") return uninstall();
		wcerr << L"[-] 参数错误，使用 -install 或 -uninstall\n";
		return 1;
	}

	wcout << L"1. 安装希沃\n2. 卸载希沃\n0. 退出\n输入: ";
	int mode;
	while (wcin >> mode) {
		if (mode == 0) break;
		if (mode == 1) return install();
		if (mode == 2) return uninstall();
		wcerr << L"[-] 无效选择，重新输入: ";
	}
	wcout << L"退出\n";
	return 0;
}