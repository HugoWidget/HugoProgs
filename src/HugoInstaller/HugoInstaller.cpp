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
#include <memory>

#include "HugoUtils/HInstaller.h"
#include <windows.h>
#include <shellapi.h>
#include <WinUtils/Console.h>
#include <WinUtils/StrConvert.h>
#include <WinUtils/WinUtils.h>
#include <WinUtils/Logger.h>
#include <WinUtils/CmdParser.h>

using namespace std;
namespace fs = filesystem;
using namespace WinUtils;

// 常量定义
const wstring kSetupPackageDirName = L"SetupPackage";
const wstring kTargetVerifyExe = L"C:\\Program Files (x86)\\Seewo\\SeewoService\\uninstall_verify.exe";
const wstring kUninstallExe = L"C:\\Program Files (x86)\\Seewo\\SeewoService\\Uninstall.exe";
const wstring kFakeVerifyFilename = L"HugoFakeVerify.exe";
const wstring kVersionRegexPattern = LR"(\d+\.\d+\.\d+\.\d+)";
const wstring kSetupExePrefix = L"SeewoServiceSetup_";
const wstring kTempFilename = L"SeewoServiceSetup.tmp";
const wstring kLatestFallbackName = L"SeewoServiceSetup_latest.exe";

// 辅助函数：清屏
void ClearScreen()
{
    system("cls");
}

// 扫描目录中的安装包
vector<PackageInfo> ScanPackages(const wstring& dir)
{
    vector<PackageInfo> packages;
    wregex re(kVersionRegexPattern);

    try {
        if (!fs::exists(dir)) {
            return packages;
        }
        for (auto& entry : fs::directory_iterator(dir)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            wstring filename = entry.path().filename().wstring();
            wsmatch match;
            if (regex_search(filename, match, re)) {
                packages.push_back({ match[0].str(), entry.path().wstring() });
            }
        }
    }
    catch (const fs::filesystem_error& e) {
        wcerr << L"扫描失败: " << ConvertString<wstring>(e.what()) << endl;
        WLog(LogLevel::Warn, L"ScanPackages failed: " + ConvertString<wstring>(e.what()));
    }
    return packages;
}

// 验证版本号格式
bool IsValidVersion(const wstring& version)
{
    if (version == L"0") {
        return true;
    }
    wregex re(kVersionRegexPattern);
    return regex_match(version, re);
}

// 运行安装包
bool RunPackage(const wstring& path)
{
    wcout << L"启动安装包: " << path << endl;
    HINSTANCE result = ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) > 32) {
        wcout << L"启动成功" << endl;
        WLog(LogLevel::Info, L"Started installer: " + path);
        return true;
    }
    DWORD err = GetLastError();
    wcerr << L"启动失败，错误码: " << err << endl;
    WLog(LogLevel::Error, L"Failed to start installer: " + path + L", error=" + to_wstring(err));
    return false;
}

// 安装功能
int Install()
{
    wstring pkgDir = GetCurrentProcessDir() + kSetupPackageDirName;
    fs::create_directories(pkgDir);

    auto packages = ScanPackages(pkgDir);

    wcout << L"\n==================== 可选版本 ====================\n";
    wcout << L"[0] 下载新版本\n";
    for (size_t i = 0; i < packages.size(); ++i) {
        wcout << L"[" << (i + 1) << L"] " << packages[i].version
            << L" (" << fs::path(packages[i].filePath).filename().wstring() << L")\n";
    }
    wcout << L"==================================================\n";

    int choice = -1;
    wcout << L"请选择: ";
    while (!(wcin >> choice) || choice < 0 || choice > static_cast<int>(packages.size())) {
        wcin.clear();
        wcin.ignore((numeric_limits<streamsize>::max)(), L'\n');
        wcerr << L"无效输入，重新选择: ";
    }

    // 选择本地已存在的包
    if (choice >= 1 && choice <= static_cast<int>(packages.size())) {
        return RunPackage(packages[choice - 1].filePath) ? 0 : 1;
    }

    // 下载新版本
    wcout << L"\n==================== 下载新版本 ====================\n";
    wstring version;
    wcout << L"输入版本号 (格式 1.X.X.XXXX, 0 表示最新): ";
    while (!(wcin >> version) || !IsValidVersion(version)) {
        wcin.clear();
        wcin.ignore((numeric_limits<streamsize>::max)(), L'\n');
        wcerr << L"格式错误，重新输入: ";
    }

    wstring url = BASE_URL;
    if (version != L"0") {
        url += L"&version=" + version;
    }
    wcout << L"URL: " << url << L"\n保存到: " << pkgDir << L"\n--------------------------------------------------\n";

    try {
        HttpDownloader downloader;
        downloader.setTimeout(60);
        downloader.setUserAgent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");

        auto startTime = chrono::steady_clock::now();
        size_t lastPrinted = 0;

        auto result = downloader.download(ConvertString<string>(url), ConvertString<string>(pkgDir), "",
            [&](int64_t done, int64_t total) {
                if (total <= 1 || (done - lastPrinted) < 1024 * 1024) {
                    return;
                }
                lastPrinted = done;
                double elapsed = chrono::duration<double>(chrono::steady_clock::now() - startTime).count();
                double speed = (done / 1024.0 / 1024.0) / elapsed;
                int percent = static_cast<int>(done * 100 / total);
                wcout << L'\r' << L'[' << wstring(percent / 2, L'=') << (percent < 100 ? L">" : L"")
                    << wstring(50 - percent / 2 - (percent < 100 ? 1 : 0), L' ')
                    << L"] " << percent << L"% " << done / 1024 / 1024 << L'/'
                    << total / 1024 / 1024 << L" MB " << speed << L" MB/s" << flush;
            });

        wcout << L"\n--------------------------------------------------\n";
        fs::path tempPath = fs::path(pkgDir) / kTempFilename;

        if (!result.success) {
            wcerr << L"下载失败\n错误: " << ConvertString<wstring>(result.errorMsg)
                << L"\nHTTP " << result.statusCode << endl;
            WLog(LogLevel::Error, L"Download failed: " + ConvertString<wstring>(result.errorMsg));
            if (result.errorMsg.find('7') != string::npos) {
                wcerr << L"可能已经下载过了" << endl;
            }
            if (result.statusCode == 404) {
                wcerr << L"版本不存在或已下架" << endl;
            }
            // 删除空的临时文件
            if (fs::exists(tempPath) && fs::file_size(tempPath) == 0) {
                fs::remove(tempPath);
            }
            return 1;
        }

        if (result.downloaded < 1000000) {
            fs::remove(tempPath);
            wcerr << L"文件太小，版本可能错误" << endl;
            WLog(LogLevel::Error, L"Downloaded file too small: " + to_wstring(result.downloaded));
            return 1;
        }

        // 确定最终文件名
        wstring finalFilename;
        if (!result.filename.empty()) {
            finalFilename = ConvertString<wstring>(result.filename);
        }
        else {
            if (version != L"0") {
                finalFilename = kSetupExePrefix + version + L".exe";
            }
            else {
                finalFilename = kLatestFallbackName;
            }
        }

        fs::path finalPath = fs::path(pkgDir) / finalFilename;
        error_code ec;
        if (fs::exists(finalPath)) {
            fs::remove(finalPath, ec);
        }
        fs::rename(tempPath, finalPath, ec);
        if (ec) {
            wcerr << L"重命名失败: " << ConvertString<wstring>(ec.message()) << endl;
            WLog(LogLevel::Error, L"Rename failed: " + ConvertString<wstring>(ec.message()));
            return 1;
        }

        wcout << L"下载完成\n大小: " << result.downloaded << L" 字节 ("
            << (result.downloaded / 1024.0 / 1024.0) << L" MB)\n";
        wcout << L"保存为: " << finalFilename << endl;
        WLog(LogLevel::Info, L"Downloaded installer: " + finalPath.wstring() + L", size=" + to_wstring(result.downloaded));

        return RunPackage(finalPath.wstring()) ? 0 : 1;
    }
    catch (const exception& e) {
        wcerr << L"异常: " << ConvertString<wstring>(e.what()) << endl;
        WLog(LogLevel::Error, L"Install exception: " + ConvertString<wstring>(e.what()));
        return 1;
    }
}

// RAII 文件句柄封装
class FileHandle
{
private:
    HANDLE handle_ = INVALID_HANDLE_VALUE;
public:
    FileHandle(const wstring& path, DWORD desiredAccess, DWORD shareMode,
        DWORD creationDisposition, DWORD flags)
    {
        handle_ = CreateFileW(path.c_str(), desiredAccess, shareMode, nullptr,
            creationDisposition, flags, nullptr);
    }
    ~FileHandle()
    {
        if (handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(handle_);
        }
    }
    bool IsValid() const { return handle_ != INVALID_HANDLE_VALUE; }
    HANDLE Get() const { return handle_; }
    // 禁止拷贝
    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;
};

// 卸载功能
int Uninstall()
{
    wstring fakeVerifyPath = GetCurrentProcessDir() + kFakeVerifyFilename;
    if (!fs::exists(fakeVerifyPath)) {
        wcerr << L"模板缺失: " << fakeVerifyPath << endl;
        WLog(LogLevel::Error, L"Fake verify template not found: " + fakeVerifyPath);
        return 1;
    }
    if (!fs::exists(kUninstallExe)) {
        wcerr << L"可能未安装希沃" << endl;
        WLog(LogLevel::Warn, L"Uninstaller not found, Seewo may not be installed");
        return 1;
    }

    // 读取模板文件
    FileHandle hFake(fakeVerifyPath, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL);
    if (!hFake.IsValid()) {
        DWORD err = GetLastError();
        wcerr << L"无法读取模板, 错误 " << err << endl;
        WLog(LogLevel::Error, L"Cannot read fake template, error=" + to_wstring(err));
        return 1;
    }

    DWORD fileSize = GetFileSize(hFake.Get(), nullptr);
    if (fileSize == INVALID_FILE_SIZE) {
        wcerr << L"获取模板大小失败" << endl;
        WLog(LogLevel::Error, L"GetFileSize failed on fake template");
        return 1;
    }

    vector<BYTE> buffer(fileSize);
    DWORD bytesRead = 0;
    if (!ReadFile(hFake.Get(), buffer.data(), fileSize, &bytesRead, nullptr) || bytesRead != fileSize) {
        wcerr << L"读取模板失败" << endl;
        WLog(LogLevel::Error, L"ReadFile failed on fake template");
        return 1;
    }

    // 备份原 uninstall_verify.exe
    wchar_t tempPath[MAX_PATH] = { 0 };
    wstring targetDir = kTargetVerifyExe.substr(0, kTargetVerifyExe.find_last_of(L'\\'));
    GetTempFileNameW(targetDir.c_str(), L"OLD", 0, tempPath);
    wstring backupPath = tempPath;

    if (fs::exists(kTargetVerifyExe)) {
        if (!MoveFileW(kTargetVerifyExe.c_str(), backupPath.c_str()) && GetLastError() != ERROR_FILE_NOT_FOUND) {
            DWORD err = GetLastError();
            wcerr << L"无法备份目标文件, 错误 " << err << endl;
            WLog(LogLevel::Error, L"Backup failed, error=" + to_wstring(err));
            return 1;
        }
        WLog(LogLevel::Info, L"Backed up original verify exe to " + backupPath);
    }

    // 写入新的验证文件（替换为假模板）
    FileHandle hTarget(kTargetVerifyExe, GENERIC_WRITE, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL);
    if (!hTarget.IsValid()) {
        DWORD err = GetLastError();
        wcerr << L"无法写入目标, 错误 " << err << endl;
        WLog(LogLevel::Error, L"Cannot write target, error=" + to_wstring(err));
        // 还原备份
        if (fs::exists(backupPath)) {
            MoveFileW(backupPath.c_str(), kTargetVerifyExe.c_str());
        }
        return 1;
    }

    DWORD bytesWritten = 0;
    if (!WriteFile(hTarget.Get(), buffer.data(), fileSize, &bytesWritten, nullptr) || bytesWritten != fileSize) {
        wcerr << L"写入失败" << endl;
        WLog(LogLevel::Error, L"WriteFile failed");
        // 还原备份
        if (fs::exists(backupPath)) {
            MoveFileW(backupPath.c_str(), kTargetVerifyExe.c_str());
        }
        return 1;
    }

    wcout << L"验证文件替换成功" << endl;
    WLog(LogLevel::Info, L"Replaced uninstall_verify.exe with fake");

    // 启动卸载程序
    HINSTANCE result = ShellExecuteW(nullptr, L"open", kUninstallExe.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) > 32) {
        wcout << L"卸载程序已启动" << endl;
        WLog(LogLevel::Info, L"Started uninstaller");
        return 0;
    }
    DWORD err = GetLastError();
    wcerr << L"启动卸载失败, 错误 " << err << endl;
    WLog(LogLevel::Error, L"Failed to start uninstaller, error=" + to_wstring(err));
    return 1;
}

// 显示帮助信息
void PrintHelp()
{
    wcout << L"用法: HugoInstaller.exe [选项]\n"
        << L"选项:\n"
        << L"  --install, -install    安装希沃服务\n"
        << L"  --uninstall, -uninstall 卸载希沃服务\n"
        << L"  --help, -h             显示此帮助\n"
        << L"无参数则进入交互菜单\n";
}

int wmain(int argc, wchar_t* argv[])
{
    try {
        Console().setLocale();

        // 日志配置：仅文件
        LoggerCore::Inst().SetDefaultStrategies(L"HugoInstaller.log");
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
                PrintHelp();
                return 0;
            }

            if (parser.hasCommand(L"install") || parser.hasCommand(L"-install")) {
                return Install();
            }
            if (parser.hasCommand(L"uninstall") || parser.hasCommand(L"-uninstall")) {
                return Uninstall();
            }

            wcerr << L"未知选项，使用 --help 查看帮助" << endl;
            return 1;
        }

        // 交互菜单模式
        int choice = -1;
        do {
            ClearScreen();
            wcout << L"\n=== 希沃安装/卸载工具 ===\n"
                << L"1. 安装希沃\n"
                << L"2. 卸载希沃\n"
                << L"0. 退出程序\n"
                << L"请输入选择: ";
            wcin >> choice;
            if (wcin.fail()) {
                wcin.clear();
                wcin.ignore((numeric_limits<streamsize>::max)(), L'\n');
                wcerr << L"输入无效，请输入数字" << endl;
                system("pause");
                continue;
            }

            int result = 0;
            switch (choice) {
            case 1:
                result = Install();
                if (result != 0) {
                    wcerr << L"安装失败，请查看日志" << endl;
                }
                break;
            case 2:
                result = Uninstall();
                if (result != 0) {
                    wcerr << L"卸载失败，请查看日志" << endl;
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