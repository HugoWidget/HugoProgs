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

#include <Windows.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <conio.h>
#include <shlobj.h>
#include <fcntl.h>
#include <io.h>

#include "HugoUtils/HugoPassword.h"
#include "WinUtils/Console.h"
#include "WinUtils/StrConvert.h"

using namespace WinUtils;
using namespace std;

class ManualInfoAcquirer : public InfoAcquirer {
private:
    int version;
    string ciphertext;
    PasswordType type;

    bool GetRegistryMachineId(string& machine_id);
public:
    ManualInfoAcquirer(int ver, const string& ct, PasswordType t = TYPE_ADMIN);
    vector<CrackTask> acquire() override;
};

bool ManualInfoAcquirer::GetRegistryMachineId(string& machine_id) {
    char buffer[128] = { 0 };
    DWORD buffer_size = sizeof(buffer);
    HKEY hKey;
    LONG result = RegOpenKeyExA(HKEY_LOCAL_MACHINE,
        "SOFTWARE\\Microsoft\\SQMClient",
        0, KEY_READ | KEY_WOW64_64KEY, &hKey);
    if (result != ERROR_SUCCESS) {
        result = RegOpenKeyExA(HKEY_LOCAL_MACHINE,
            "SOFTWARE\\Microsoft\\SQMClient",
            0, KEY_READ, &hKey);
    }
    if (result != ERROR_SUCCESS) return false;

    result = RegQueryValueExA(hKey, "MachineId", nullptr, nullptr,
        reinterpret_cast<LPBYTE>(buffer), &buffer_size);
    RegCloseKey(hKey);

    if (result == ERROR_SUCCESS) {
        machine_id = buffer;
        return true;
    }
    return false;
}

ManualInfoAcquirer::ManualInfoAcquirer(int ver, const string& ct, PasswordType t)
    : version(ver), ciphertext(ct), type(t) {
}

vector<CrackTask> ManualInfoAcquirer::acquire() {
    vector<CrackTask> tasks;
    string device_id, machine_id;

    char seewocore_ini_path[MAX_PATH] = { 0 };
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_COMMON_APPDATA, nullptr, 0, seewocore_ini_path))) {
        strcat_s(seewocore_ini_path, MAX_PATH, "\\Seewo\\SeewoCore\\SeewoCore.ini");
        char buf_device[256] = { 0 };
        GetPrivateProfileStringA("device", "id", "", buf_device, sizeof(buf_device), seewocore_ini_path);
        device_id = buf_device;
    }

    GetRegistryMachineId(machine_id);

    CrackMode mode;
    if (version == 1) mode = MODE_V1;
    else if (version == 2) mode = MODE_V2;
    else mode = MODE_V3;

    tasks.push_back({ mode, type, ciphertext, device_id, machine_id });
    return tasks;
}

class ConsoleOutput : public ResultOutput {
private:
    static bool compareResult(const CrackResult& a, const CrackResult& b) {
        if (a.task.mode != b.task.mode)
            return a.task.mode < b.task.mode;
        return a.task.type < b.task.type;
    }

    wstring modeToString(CrackMode mode) {
        switch (mode) {
        case MODE_V1: return L"V1";
        case MODE_V2: return L"V2";
        case MODE_V3: return L"V3";
        default: return L"未知";
        }
    }

    wstring typeToString(PasswordType type) {
        return (type == TYPE_ADMIN) ? L"管理密码" : L"锁屏密码";
    }

public:
    void output(const vector<CrackResult>& results) override {
        vector<CrackResult> sorted = results;
        sort(sorted.begin(), sorted.end(), compareResult);

        wcout << L"\n==================== 破解结果 ====================\n";
        for (const auto& res : sorted) {
            wcout << L"[" << modeToString(res.task.mode) << L"] "
                << typeToString(res.task.type) << L" : ";
            if (res.success) {
                wcout << L"成功 -> " << ConvertString<wstring>(res.plaintext) << endl;
            }
            else {
                wcout << L"失败 (" << ConvertString<wstring>(res.error_message) << L")" << endl;
            }
        }
        wcout << L"========================================================\n";
    }
};

int main() {
    Console console;
    console.setLocale();

    int choice = -1;
    wcout << L"选择模式:\n"
        << L"0 - 手动输入\n"
        << L"1 - 自动读取\n";
    wcin >> choice;

    InfoAcquirer* acquirer = nullptr;
    if (choice == 1) {
        acquirer = new AutoInfoAcquirer();
    }
    else {
        int ver;
        wstring wcipher;
        wcout << L"密文版本 (1/2/3): ";
        wcin >> ver;
        wcout << L"密文: ";
        wcin >> wcipher;
        string cipher = ConvertString<string>(wcipher);
        acquirer = new ManualInfoAcquirer(ver, cipher, TYPE_ADMIN);
    }

    vector<CrackTask> tasks = acquirer->acquire();
    delete acquirer;

    if (tasks.empty()) {
        wcerr << L"[!] 无破解任务" << endl;
        return 1;
    }

    // 打印任务信息（使用宽字符）
    wcout << L"\n==================== 破解任务 ====================\n";
    for (const auto& task : tasks) {
        wstring modeStr = (task.mode == MODE_V1) ? L"V1" :
            (task.mode == MODE_V2) ? L"V2" : L"V3";
        wstring typeStr = (task.type == TYPE_ADMIN) ? L"管理密码" : L"锁屏密码";
        wcout << L"模式: " << modeStr
            << L", 类型: " << typeStr
            << L", 密文: " << ConvertString<wstring>(task.ciphertext)
            << L", 设备ID: " << ConvertString<wstring>(task.device_id)
            << L", 机器ID: " << ConvertString<wstring>(task.machine_id)
            << endl;
    }
    wcout << L"==========================================================\n";

    // 准备解密器
    V1Decryptor d1;
    V2Decryptor d2;
    V3Decryptor d3;
    vector<Decryptor*> decryptors = { &d1, &d2, &d3 };

    // 执行破解
    CrackExecutor executor;
    vector<CrackResult> results = executor.execute(tasks, decryptors);

    // 输出结果
    ConsoleOutput output;
    output.output(results);

    wcout << L"\n按任意键继续" << endl;
    (void)_getwch();
    return 0;
}