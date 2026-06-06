#include "WinUtils/WinPch.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <cstdint>
#include <cstdio>
#include <algorithm>
#include <conio.h>

#include "WinUtils/Console.h"
#include "WinUtils/WinUtils.h"
#include "WinUtils/CmdParser.h"
#include "HugoUtils/HInfo.h"
#include <WinUtils/StrConvert.h>
using namespace std;
using namespace WinUtils;
namespace fs = filesystem;

const vector<uint8_t> original = { 0xE8, 0x16, 0xE9, 0xFF, 0xFF };
const vector<uint8_t> patched = { 0xB0, 0x01, 0x90, 0x90, 0x90 };

const wstring dll_filename = L"bind_zmodule.dll";
const wstring backup_suffix = L".bak";

vector<uint8_t> ReadFile(const wstring& path) {
    FILE* file = nullptr;
    _wfopen_s(&file, path.c_str(), L"rb");
    if (!file) throw runtime_error("无法打开文件: " + ConvertString<string>(path));

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size < 0) { fclose(file); throw runtime_error("获取文件大小失败"); }

    vector<uint8_t> buffer(static_cast<size_t>(size));
    if (fread(buffer.data(), 1, buffer.size(), file) != buffer.size()) {
        fclose(file);
        throw runtime_error("读取文件失败");
    }
    fclose(file);
    return buffer;
}

void WriteFile(const wstring& path, const vector<uint8_t>& data) {
    FILE* file = nullptr;
    _wfopen_s(&file, path.c_str(), L"wb");
    if (!file) throw runtime_error("无法创建文件: " + ConvertString<string>(path));

    if (fwrite(data.data(), 1, data.size(), file) != data.size()) {
        fclose(file);
        throw runtime_error("写入文件失败");
    }
    fclose(file);
}

bool FindSequence(const vector<uint8_t>& data, const vector<uint8_t>& seq) {
    if (data.size() < seq.size()) return false;
    for (size_t i = 0; i <= data.size() - seq.size(); ++i) {
        if (equal(seq.begin(), seq.end(), data.begin() + i))
            return true;
    }
    return false;
}

vector<uint8_t> ReplaceSequence(const vector<uint8_t>& data,
    const vector<uint8_t>& old_seq,
    const vector<uint8_t>& new_seq) {
    vector<uint8_t> result;
    result.reserve(data.size());
    for (size_t i = 0; i < data.size();) {
        if (i <= data.size() - old_seq.size() &&
            equal(old_seq.begin(), old_seq.end(), data.begin() + i)) {
            result.insert(result.end(), new_seq.begin(), new_seq.end());
            i += old_seq.size();
        }
        else {
            result.push_back(data[i++]);
        }
    }
    return result;
}

fs::path GetDllPath() {
    HInfo info;
    auto folder = info.getHugoFolder();
    if (!folder.has_value())
        throw runtime_error("未找到希沃管家安装路径");
    return *folder / L"SeewoCore" / L"module" / L"bind" / dll_filename;
}

fs::path GetBackupPath() {
    return fs::path(WinUtils::GetCurrentProcessDir()) / (dll_filename + backup_suffix);
}

void Patch() {
    fs::path dll_path = GetDllPath();
    fs::path backup_path = GetBackupPath();

    wcout << L"目标文件: " << dll_path << endl;
    if (!fs::exists(dll_path)) {
        wcerr << L"错误：未找到 " << dll_filename << L"\n路径：" << dll_path << endl;
        return;
    }

    auto data = ReadFile(dll_path.wstring());
    if (!FindSequence(data, original)) {
        wcout << L"文件似乎已经被修补过了 :)" << endl;
        return;
    }

    if (!fs::exists(backup_path)) {
        WriteFile(backup_path.wstring(), data);
        wcout << L"已创建备份: " << backup_path << endl;
    }
    else {
        wcout << L"备份文件已存在，跳过创建: " << backup_path << endl;
    }

    WriteFile(dll_path.wstring(), ReplaceSequence(data, original, patched));
    wcout << L"修补成功！" << endl;
}

void Restore() {
    fs::path dll_path = GetDllPath();
    fs::path backup_path = GetBackupPath();

    wcout << L"正在从备份还原..." << endl;
    wcout << L"备份文件: " << backup_path << endl;
    wcout << L"目标文件: " << dll_path << endl;

    if (!fs::exists(backup_path)) {
        wcerr << L"错误：备份文件不存在，无法还原。" << endl;
        return;
    }
    if (!fs::exists(dll_path)) {
        wcerr << L"错误：目标 DLL 不存在，无法还原。" << endl;
        return;
    }

    auto backup_data = ReadFile(backup_path.wstring());
    if (!FindSequence(backup_data, original)) {
        wcerr << L"警告：备份文件似乎不是原始版本，仍要还原吗？(y/n): ";
        char c; cin >> c;
        if (tolower(c) != 'y') {
            wcout << L"已取消还原。" << endl;
            return;
        }
    }

    WriteFile(dll_path.wstring(), backup_data);
    wcout << L"还原成功！" << endl;
}

void PrintHelp() {
    wcout << L"用法: HugoBreak.exe [选项]\n"
        << L"选项:\n"
        << L"  --patch, -p       修补 bind_zmodule.dll（默认）\n"
        << L"  --restore, -r     从备份还原 bind_zmodule.dll\n"
        << L"  --help, -h        显示此帮助\n"
        << L"无参数则进入交互菜单\n";
}

// ---- 入口 ----
int main(int argc, char* argv[]) {
    try {
        RequireAdminPrivilege(true);

        Console console;
        console.setLocale();

        wstring cmdLine = ExtractArguments(GetCommandLine());
        bool interactive = true;

        if (!cmdLine.empty()) {
            CmdParser parser(true);
            if (!parser.parse(cmdLine)) {
                wcerr << L"命令行解析失败" << endl;
                system("pause");
                return 1;
            }

            if (parser.hasCommand(L"help") || parser.hasCommand(L"h")) {
                PrintHelp();
                system("pause");
                return 0;
            }
            if (parser.hasCommand(L"patch") || parser.hasCommand(L"p")) {
                Patch();
                interactive = false;
            }
            if (parser.hasCommand(L"restore") || parser.hasCommand(L"r")) {
                Restore();
                interactive = false;
            }
            if (interactive) {
                wcerr << L"未知选项，使用 --help 查看帮助" << endl;
                system("pause");
                return 1;
            }
        }

        // 交互菜单模式
        if (interactive) {
            int choice = -1;
            do {
                system("cls");
                wcout << L"\n=== 希沃管家 DLL 补丁工具 ===\n"
                    << L"1. 修补 " << dll_filename << L"\n"
                    << L"2. 还原 " << dll_filename << L"\n"
                    << L"0. 退出\n"
                    << L"请输入选择: ";

                wcin >> choice;
                if (wcin.fail()) {
                    wcin.clear();
                    wcin.ignore((numeric_limits<streamsize>::max)(), L'\n');
                    choice = -1;
                }

                switch (choice) {
                case 1:
                    Patch();
                    break;
                case 2:
                    Restore();
                    break;
                case 0:
                    wcout << L"退出。" << endl;
                    break;
                default:
                    wcout << L"无效选择，请重新输入" << endl;
                    break;
                }
                if (choice != 0) system("pause");
            } while (choice != 0);
        }
    }
    catch (const exception& e) {
        wcerr << L"错误: " << ConvertString<wstring>(e.what()) << endl;
        system("pause");
        return 1;
    }
    return 0;
}