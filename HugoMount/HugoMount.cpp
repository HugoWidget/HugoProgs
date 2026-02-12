#include "HugoUtils/HugoMount.h"
#include "HugoUtils/Logger.h"
#include "HugoUtils/StrConvert.h"
#include "HugoUtils/WinUtils.h"
#include "HugoUtils/Console.h"
#include <iostream>
#include <string>
#include <exception>
#include <cwchar>
#include <cstdlib>
using namespace std;
using namespace WinUtils;

void ShowHelp(wstring progName) {
    wcout << L"用法:" << endl;
    wcout << L"  " << progName << L" list                - 列出所有虚拟磁盘信息" << endl;
    wcout << L"  " << progName << L" mount <disk> <part> [drive] - 挂载虚拟磁盘分区" << endl;
    wcout << L"  " << progName << L" unmount <drive>     - 按盘符卸载（如: unmount D）" << endl;
    wcout << L"  " << progName << L" unmount <disk> <part> - 按磁盘/分区号卸载" << endl;
    wcout << L"  " << progName << L" help                - 显示此帮助信息" << endl;
    wcout << L"============================================================" << endl;
}

int wmain(int argc, wchar_t* argv[]) {
    Console console;
    console.setLocale();
    LoggerCore::Inst().SetDefaultStrategies(L"HugoMount.log");

    VirtualDKManager vdkManager;

    if (argc == 1 || (argc == 2 && (wcscmp(argv[1], L"help") == 0 || wcscmp(argv[1], L"-h") == 0))) {
        ShowHelp(GetCurrentProcessName());
        WuDebug(LogLevel::Debug, L"显示帮助信息后退出");
        return 0;
    }

    wstring cmd = argv[1];
    try {
        if (cmd == L"list") {
            vdkManager.ListAll();
        }
        else if (cmd == L"mount") {
            if (argc < 4) {
                wcerr << L"Error: 挂载命令缺少参数！用法: mount <disk> <part> [drive]" << endl;
                ShowHelp(argv[0]);
                WuDebug(LogLevel::Error, L"挂载命令参数不足");
                return 1;
            }
            int diskId = _wtoi(argv[2]);
            int partId = _wtoi(argv[3]);
            wchar_t driveLetter = 0;

            if (argc >= 5 && iswalpha(static_cast<wint_t>(argv[4][0]))) {
                driveLetter = argv[4][0];
            }
            int ret = vdkManager.Mount(diskId, partId, static_cast<char>(towupper(driveLetter)));
            WuDebug(LogLevel::Info, format(L"挂载命令执行完成，返回码: {}", ret));
            return ret;
        }
        else if (cmd == L"unmount") {
            if (argc < 3) {
                wcerr << L"Error: 卸载命令缺少参数！用法: unmount <drive> 或 unmount <disk> <part>" << endl;
                ShowHelp(argv[0]);
                WuDebug(LogLevel::Error, L"卸载命令参数不足");
                return 1;
            }
            if (iswalpha(static_cast<wint_t>(argv[2][0]))) {
                wchar_t driveLetter = towupper(argv[2][0]);
                int ret = vdkManager.Unmount(-1, -1, static_cast<char>(driveLetter));
                WuDebug(LogLevel::Info, format(L"按盘符卸载命令执行完成，返回码: {}", ret));
                return ret;
            }
            else if (argc >= 4 && iswdigit(static_cast<wint_t>(argv[2][0])) &&
                iswdigit(static_cast<wint_t>(argv[3][0]))) {
                int diskId = _wtoi(argv[2]);
                int partId = _wtoi(argv[3]);
                int ret = vdkManager.Unmount(diskId, partId);
                WuDebug(LogLevel::Info, format(L"按磁盘/分区卸载命令执行完成，返回码: {}", ret));
                return ret;
            }
            else {
                wcerr << L"Error: 无效的卸载参数！" << endl;
                ShowHelp(argv[0]);
                WuDebug(LogLevel::Error, L"卸载命令参数无效");
                return 1;
            }
        }
        else {
            wcerr << L"Error: 未知命令 '" << cmd << L"'！" << endl;
            ShowHelp(argv[0]);
            WuDebug(LogLevel::Error, format(L"未知命令: {}", cmd));
            return 1;
        }
    }
    catch (const exception& e) {
        wcerr << L"程序异常: " << AnsiToWideString(e.what()) << endl;
        WuDebug(LogLevel::Error, format(L"程序异常: {}", AnsiToWideString(e.what())));
        return 1;
    }

    WuDebug(LogLevel::Info, L"程序正常退出");
    return 0;
}