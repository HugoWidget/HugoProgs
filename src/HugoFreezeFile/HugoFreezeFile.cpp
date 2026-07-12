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
 *
 * HugoFreezeFile – Interactive VolumeInfo.config editor.
 */
#include <iostream>
#include <iomanip>
#include <string>
#include <limits>
#include <cstring>
#include <optional>
#include <algorithm>
#include <cstdio>
#include "WinUtils/WinUtils.h"
#include "WinUtils/StrConvert.h"
#include "HugoUtils/HFreezeFile_p.h"
#include "WinUtils/Console.h"
#include "hashlib/md5.h"

using namespace std;
using namespace WinUtils;

// 将字节数组转为大写十六进制字符串
static string bytesToHex(const uint8_t* data, size_t len) {
	string hex;
	for (size_t i = 0; i < len; ++i) {
		char buf[3];
		snprintf(buf, sizeof(buf), "%02X", data[i]);
		hex += buf;
	}
	return hex;
}

// 计算 1024 字节缓冲区的 MD5
static string computeConfigMD5(const ProtectInfo& info) {
	constexpr size_t BUF_SIZE = HFreezeFilePrivate::CONFIG_SIZE;
	uint8_t raw[BUF_SIZE] = {};
	info.ToBuffer(raw, BUF_SIZE);

	MD5 md5;
	const uint8_t* start = raw + 0x10;
	size_t length = BUF_SIZE - 0x10;
	md5.add(start, length);

	unsigned char digest[16];
	md5.getHash(digest);
	return bytesToHex(digest, 16);
}

static void printProtectInfo(const ProtectInfo& info) {
	wcout << L"\n========== ProtectInfo (VolumeInfo.config) ==========\n";
	wcout << left;

	wcout << setw(30) << L"MD5 (stored): " << ConvertString(bytesToHex(info.md5, 16)) << L"\n";

	wcout << setw(30) << L"readytoProtectVolume: " << hex << L"0x" << info.readytoProtectVolume << dec << L"\n";
	wcout << setw(30) << L"alreadyProtectVolume: " << hex << L"0x" << info.alreadyProtectVolume << dec << L"\n";
	wcout << setw(30) << L"diskNum: " << (int)info.diskNum << L"\n";
	wcout << setw(30) << L"stopProtect: " << info.stopProtect << L"\n";
	wcout << setw(30) << L"needUpdate: " << info.needUpdate << L"\n";
	wcout << setw(30) << L"storageFileSize: " << info.storageFileSize << L" (0x"
		<< hex << info.storageFileSize << dec << L")\n";
	wcout << setw(30) << L"bRunSlowly: " << info.bRunSlowly << L"\n";
	wcout << setw(30) << L"bsodNum: " << info.bsodNum << L"\n";
	wcout << setw(30) << L"bsodMaxUptime: " << info.bsodMaxUptime << L"\n";
	wcout << setw(30) << L"blueHistoryReport: " << info.blueHistoryReport << L"\n";
	wcout << setw(30) << L"lastFreezeState: " << hex << L"0x" << info.lastFreezeState << dec << L"\n";
	wcout << setw(30) << L"lastbsodRuntime: " << info.lastbsodRuntime << L"\n";
	wcout << setw(30) << L"lastsendbsodtime: " << info.lastsendbsodtime << L"\n";
	wcout << setw(30) << L"coreDumpZipReport: " << info.coreDumpZipReport << L"\n";
	wcout << setw(30) << L"isLastPagefileInFreezeVol: " << info.isLastPagefileInFreezeVol << L"\n";
	wcout << setw(30) << L"isLastVolumeCorrupt: " << info.isLastVolumeCorrupt << L"\n";
	wcout << setw(30) << L"iotDeviceID: " << ConvertString((char*)info.iotDeviceID) << L"\n";
	wcout << setw(30) << L"iotSchoolID: " << ConvertString((char*)info.iotSchoolID) << L"\n";
	wcout << setw(30) << L"bNeedFreeze: " << info.bNeedFreeze << L"\n";
	wcout << setw(30) << L"bNeedUnFreeze: " << info.bNeedUnFreeze << L"\n";
	wcout << setw(30) << L"updateRebootCount: " << info.updateRebootCount << L"\n";
	wcout << setw(30) << L"startupTime: " << info.startupTime << L"\n";
	wcout << setw(30) << L"configVersion: " << (int)info.configVersion << L"\n";
	wcout << setw(30) << L"volMaskCopy: " << hex << L"0x" << info.volMaskCopy << dec << L"\n";
	wcout << setw(30) << L"updatingTimeSet: " << info.updatingTimeSet << L"\n";
	wcout << setw(30) << L"updatingTimeNotAfter: " << info.updatingTimeNotAfter << L"\n";
	wcout << L"========================================================\n";

	// MD5 校验
	string computed = computeConfigMD5(info);
	//wcout << L"Current computed MD5 (1024‑byte): ";
	cout << (computed);
	if (computed == bytesToHex(info.md5, 16))
		wcout << L" (VALID)\n";
	else
		wcout << L" (MISMATCH!)\n";
}


static void clearInputBuffer() {
	wcin.clear();
	wcin.ignore((numeric_limits<streamsize>::max)(), L'\n');
}

template<typename T>
static T inputInt(const wstring& prompt, T minVal, T maxVal) {
	T val;
	while (true) {
		wcout << prompt;
		if (wcin >> val) {
			if (val >= minVal && val <= maxVal) break;
			wcout << L"Value out of range [" << minVal << L", " << maxVal << L"].\n";
		}
		else {
			wcout << L"Invalid input. Please enter a number.\n";
			clearInputBuffer();
		}
	}
	clearInputBuffer();
	return val;
}

static uint32_t inputHex(const wstring& prompt) {
	uint32_t val;
	while (true) {
		wcout << prompt << L" (hex, e.g. 0x4): ";
		if (wcin >> hex >> val) {
			break;
		}
		else {
			wcout << L"Invalid hex input.\n";
			clearInputBuffer();
		}
	}
	clearInputBuffer();
	return val;
}

static void modifyFreezeMask(ProtectInfo& info) {
	wcout << L"\n--- Modify Freeze Volume Mask ---\n";
	wcout << L"Current readytoProtectVolume: 0x" << hex << info.readytoProtectVolume << dec << L"\n";
	uint32_t newMask = inputHex(L"New mask: ");
	info.readytoProtectVolume = newMask;
	info.volMaskCopy = newMask;
	info.bNeedFreeze = newMask ? 1 : 0;
	info.bNeedUnFreeze = newMask ? 0 : 1;
	wcout << L"Mask updated. bNeedFreeze=" << info.bNeedFreeze
		<< L", bNeedUnFreeze=" << info.bNeedUnFreeze << L" (auto-set).\n";
}

static void modifyField(ProtectInfo& info) {
	wcout << L"\n--- Modify Individual Field ---\n";
	wcout << L" 1. readytoProtectVolume\n";
	wcout << L" 2. alreadyProtectVolume\n";
	wcout << L" 3. diskNum\n";
	wcout << L" 4. stopProtect\n";
	wcout << L" 5. needUpdate\n";
	wcout << L" 6. storageFileSize\n";
	wcout << L" 7. bRunSlowly\n";
	wcout << L" 8. bsodNum\n";
	wcout << L" 9. bsodMaxUptime\n";
	wcout << L"10. blueHistoryReport\n";
	wcout << L"11. lastFreezeState\n";
	wcout << L"12. lastbsodRuntime\n";
	wcout << L"13. lastsendbsodtime\n";
	wcout << L"14. coreDumpZipReport\n";
	wcout << L"15. isLastPagefileInFreezeVol\n";
	wcout << L"16. isLastVolumeCorrupt\n";
	wcout << L"17. iotDeviceID\n";
	wcout << L"18. iotSchoolID\n";
	wcout << L"19. bNeedFreeze\n";
	wcout << L"20. bNeedUnFreeze\n";
	wcout << L"21. updateRebootCount\n";
	wcout << L"22. startupTime\n";
	wcout << L"23. configVersion\n";
	wcout << L"24. volMaskCopy\n";
	wcout << L"25. updatingTimeSet\n";
	wcout << L"26. updatingTimeNotAfter\n";
	wcout << L" 0. Cancel\n";

	int choice = inputInt(L"Select field: ", 0, 26);
	if (choice == 0) return;

	switch (choice) {
	case 1: info.readytoProtectVolume = inputHex(L"New readytoProtectVolume: "); break;
	case 2: info.alreadyProtectVolume = inputHex(L"New alreadyProtectVolume: "); break;
	case 3: info.diskNum = (uint8_t)inputInt(L"diskNum (0-255): ", 0, 255); break;
	case 4: info.stopProtect = inputInt(L"stopProtect (0/1): ", 0, 1); break;
	case 5: info.needUpdate = inputInt(L"needUpdate (0/1): ", 0, 1); break;
	case 6: {
		wcout << L"Enter storageFileSize (hex like 0x40000000 or decimal): ";
		uint64_t val;
		if (wcin >> hex >> val) info.storageFileSize = val;
		else { clearInputBuffer(); wcout << L"Invalid input.\n"; }
		break;
	}
	case 7: info.bRunSlowly = inputInt(L"bRunSlowly (0/1): ", 0, 1); break;
	case 8: info.bsodNum = inputInt<uint32_t>(L"bsodNum: ", 0, UINT32_MAX); break;
	case 9: info.bsodMaxUptime = inputInt<uint32_t>(L"bsodMaxUptime: ", 0, UINT32_MAX); break;
	case 10: info.blueHistoryReport = inputInt(L"blueHistoryReport (0/1): ", 0, 1); break;
	case 11: info.lastFreezeState = inputHex(L"lastFreezeState: "); break;
	case 12: info.lastbsodRuntime = inputInt<uint32_t>(L"lastbsodRuntime: ", 0, UINT32_MAX); break;
	case 13: {
		wcout << L"Enter lastsendbsodtime (hex uint64): ";
		uint64_t val;
		if (wcin >> hex >> val) info.lastsendbsodtime = val;
		else { clearInputBuffer(); wcout << L"Invalid input.\n"; }
		break;
	}
	case 14: info.coreDumpZipReport = inputInt(L"coreDumpZipReport (0/1): ", 0, 1); break;
	case 15: info.isLastPagefileInFreezeVol = inputInt(L"isLastPagefileInFreezeVol (0/1): ", 0, 1); break;
	case 16: info.isLastVolumeCorrupt = inputInt(L"isLastVolumeCorrupt (0/1): ", 0, 1); break;
	case 17: {
		wcout << L"Enter iotDeviceID (max 19 chars): ";
		wstring s; getline(wcin, s);
		memset(info.iotDeviceID, 0, 19);
		memcpy(info.iotDeviceID, s.c_str(), (min)(s.size(), (size_t)19));
		break;
	}
	case 18: {
		wcout << L"Enter iotSchoolID (max 5 chars): ";
		wstring s; getline(wcin, s);
		memset(info.iotSchoolID, 0, 5);
		memcpy(info.iotSchoolID, s.c_str(), (min)(s.size(), (size_t)5));
		break;
	}
	case 19: info.bNeedFreeze = inputInt(L"bNeedFreeze (0/1): ", 0, 1); break;
	case 20: info.bNeedUnFreeze = inputInt(L"bNeedUnFreeze (0/1): ", 0, 1); break;
	case 21: info.updateRebootCount = (uint16_t)inputInt(L"updateRebootCount (0-65535): ", 0, 65535); break;
	case 22: {
		wcout << L"Enter startupTime (max 20 chars): ";
		wstring s; getline(wcin, s);
		memset(info.startupTime, 0, 20);
		memcpy(info.startupTime, s.c_str(), (min)(s.size(), (size_t)20));
		break;
	}
	case 23: info.configVersion = (uint8_t)inputInt(L"configVersion (0-255): ", 0, 255); break;
	case 24: info.volMaskCopy = inputHex(L"volMaskCopy: "); break;
	case 25: info.updatingTimeSet = inputInt(L"updatingTimeSet (0/1): ", 0, 1); break;
	case 26: info.updatingTimeNotAfter = inputInt<uint32_t>(L"updatingTimeNotAfter (unix timestamp): ", 0, UINT32_MAX); break;
	default: break;
	}
	wcout << L"Field updated.\n";
}

static optional<ProtectInfo> loadConfig() {
	auto cfg = HFreezeFilePrivate::ReadConfig();
	if (!cfg) {
		wcout << L"Config file not found or invalid. Create new? (y/n): ";
		wchar_t c; wcin >> c; clearInputBuffer();
		if (c == L'y' || c == L'Y')
			return ProtectInfo{};
		return nullopt;
	}
	wcout << L"Config loaded.\n";
	return cfg;
}

static void saveConfig(const ProtectInfo& info) {
	if (HFreezeFilePrivate::WriteConfig(info)) {
		wcout << L"Config saved successfully to "
			<< HFreezeFilePrivate::GetConfigPath() << L"\n";
	}
	else {
		wcout << L"ERROR: Failed to write config file.\n";
	}
}

static void interactiveLoop() {
	// Allow custom path from the outside via HFreezeFilePrivate::SetConfigPath
	wcout << L"Current config path: " << HFreezeFilePrivate::GetConfigPath() << L"\n";

	optional<ProtectInfo> current = loadConfig();
	if (!current) {
		wcout << L"Exiting.\n";
		return;
	}
	ProtectInfo working = *current;

	while (true) {
		wcout << L"\n========== HugoFreezeFile Menu ==========\n";
		wcout << L"1. View current configuration\n";
		wcout << L"2. Modify freeze volume mask (simplified)\n";
		wcout << L"3. Modify individual field (advanced)\n";
		wcout << L"4. Save configuration to file\n";
		wcout << L"5. Reload from file (discard changes)\n";
		wcout << L"6. Select custom config path (optional)\n";
		wcout << L"7. Exit\n";
		int choice = inputInt(L"Enter your choice: ", 1, 7);

		switch (choice) {
		case 1:
			printProtectInfo(working);
			break;
		case 2:
			modifyFreezeMask(working);
			break;
		case 3:
			modifyField(working);
			break;
		case 4:
			saveConfig(working);
			// After saving, automatically reload to reflect the written MD5
			{
				auto reloaded = HFreezeFilePrivate::ReadConfig();
				if (reloaded) {
					working = *reloaded;
					wcout << L"Reloaded automatically to show the stored MD5.\n";
				}
			}
			break;
		case 5: {
			auto reloaded = loadConfig();
			if (reloaded) { working = *reloaded; wcout << L"Reloaded.\n"; }
			break;
		}
		case 6:
		{
			wcout << L"Enter new config path: \n"
				"leave 'default' for default,\n"
				"'current' for <current dir>/VolumeInfo.config),\n"
				"empty for current path\n";
			wstring newPath;
			getline(wcin, newPath);
			if (newPath == L"default") {
				newPath = HFreezeFilePrivate::DEFAULT_CONFIG_PATH;
			}
			else if (newPath == L"current") {
				newPath = GetCurrentProcessDir() + L"VolumeInfo.config";
			}
			else if (newPath.empty()) {
				break;
			}
			HFreezeFilePrivate::SetConfigPath(newPath);
			wcout << L"Config path updated to: " << HFreezeFilePrivate::GetConfigPath() << L"\n";
			{
				auto reloaded = loadConfig();
				if (reloaded) { working = *reloaded; wcout << L"Reloaded from new path.\n"; }
			}
			break;
		}
		case 7:
			wcout << L"Goodbye.\n";
			return;
		default:
			wcout << L"Invalid choice.\n";
		}
	}
}

int wmain(int argc, wchar_t* argv[]) {
	Console console;
	console.setLocale();

	wcout << L"HugoFreezeFile - SWFreeze VolumeInfo.config Manager\n";
	interactiveLoop();
	return 0;
}