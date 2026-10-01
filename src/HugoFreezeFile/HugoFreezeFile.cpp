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
 * Uses the HugoUtils freeze backends (HFreezeFileBackend / HConfigFile).
 */
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>

#include "WinUtils/Console.h"
#include "WinUtils/StrConvert.h"
#include "WinUtils/WinUtils.h"
#include "HugoUtils/HugoFreeze/HFreezeConfig.h"
#include "HugoUtils/HugoFreeze/HFreezeDef.h"
#include "HugoUtils/HugoFreeze/HFreezeFileBackend.h"
#include "hashlib/md5.h"

using namespace std;
using namespace WinUtils;

// 将字节数组转为大写十六进制字符串
static string bytesToHex(const unsigned char* data, size_t len) {
	string hex;
	for (size_t i = 0; i < len; ++i) {
		char buf[3];
		snprintf(buf, sizeof(buf), "%02X", data[i]);
		hex += buf;
	}
	return hex;
}

// 读取定长字符串字段（按字段长度截断，避免越界）
static string fixedString(const char* field, size_t size) {
	size_t len = 0;
	while (len < size && field[len] != '\0') {
		++len;
	}
	return string(field, len);
}

// 计算配置应有的 MD5：覆盖 MD5 字段之后的全部字节
static void computeConfigMD5(const HConfigFile& cfg, unsigned char out[FRZ_CONFIG_MD5_SIZE]) {
	unsigned char buffer[FRZ_CONFIG_SIZE] = {};
	cfg.toBuffer(buffer);

	MD5 md5;
	md5.add(buffer + FRZ_CONFIG_MD5_SIZE, FRZ_CONFIG_SIZE - FRZ_CONFIG_MD5_SIZE);
	md5.getHash(out);
}

// HConfigFile 只能由文件/驱动缓冲区构造，新建配置时用已废弃的 fromBuffer 桥接
#pragma warning(push)
#pragma warning(disable : 4996)
static HConfigFile makeConfigFile(const unsigned char* data) {
	return HConfigFile::fromBuffer(data);
}
#pragma warning(pop)

// 构造一个全零的新配置（HConfigFile 默认构造为私有，必须经 fromBuffer 桥接）
// 新建时立即写入正确的 MD5，保证内存中的配置自洽（保存时仍会重算一次）
static HConfigFile makeEmptyConfig() {
	unsigned char empty[FRZ_CONFIG_SIZE] = {};
	HConfigFile cfg = makeConfigFile(empty);

	unsigned char digest[FRZ_CONFIG_MD5_SIZE] = {};
	computeConfigMD5(cfg, digest);
	memcpy(cfg.md5, digest, FRZ_CONFIG_MD5_SIZE);
	return cfg;
}

// 以十六进制转储展示完整配置（带偏移行标与列标），ProtectInfo 区域用 [ ] 括起
static void printBinaryDump(const HConfigFile& cfg) {
	unsigned char buffer[FRZ_CONFIG_SIZE] = {};
	cfg.toBuffer(buffer);

	static const char hexDigits[] = "0123456789ABCDEF";

	wcout << L"\n========== Binary dump (" << FRZ_CONFIG_SIZE
		<< L" bytes, [ ] = ProtectInfo) ==========\n";
	wcout << L"    Offset | 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\n";
	wcout << L"    -------+------------------------------------------------\n";

	for (size_t i = 0; i < FRZ_CONFIG_SIZE; ++i) {
		if (i % 16 == 0) {
			// 行首：4 位十六进制偏移
			wcout << L"    0x"
				<< wchar_t(hexDigits[(i >> 12) & 0x0F])
				<< wchar_t(hexDigits[(i >> 8) & 0x0F])
				<< wchar_t(hexDigits[(i >> 4) & 0x0F])
				<< wchar_t(hexDigits[i & 0x0F])
				<< L" | ";
		}
		else {
			wcout << L" ";
		}

		// 括号直接贴住首/末字节，不额外占位
		if (i == FRZ_CONFIG_MD5_SIZE) {
			wcout << L"[";
		}
		wcout << wchar_t(hexDigits[buffer[i] >> 4]) << wchar_t(hexDigits[buffer[i] & 0x0F]);
		if (i + 1 == FRZ_CONFIG_INFO_END) {
			wcout << L"]";
		}

		if (i % 16 == 15) {
			wcout << L"\n";
		}
	}
}

static void printConfig(const HConfigFile& cfg) {
	const ProtectInfo& info = cfg.info;

	wcout << L"\n========== ProtectInfo (VolumeInfo.config) ==========\n";
	wcout << left;

	wcout << setw(30) << L"MD5 (stored): " << ConvertString(bytesToHex(cfg.md5, FRZ_CONFIG_MD5_SIZE)) << L"\n";

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
	wcout << setw(30) << L"iotDeviceID: "
		<< ConvertString(fixedString(reinterpret_cast<const char*>(info.iotDeviceID), FRZ_IOT_DEVICE_ID_LEN)) << L"\n";
	wcout << setw(30) << L"iotSchoolID: "
		<< ConvertString(fixedString(reinterpret_cast<const char*>(info.iotSchoolID), FRZ_IOT_SCHOOL_ID_LEN)) << L"\n";
	wcout << setw(30) << L"bNeedFreeze: " << info.bNeedFreeze << L"\n";
	wcout << setw(30) << L"bNeedUnFreeze: " << info.bNeedUnFreeze << L"\n";
	wcout << setw(30) << L"updateRebootCount: " << info.updateRebootCount << L"\n";
	wcout << setw(30) << L"startupTime: "
		<< ConvertString(fixedString(info.startupTime, FRZ_STARTUP_TIME_LEN)) << L"\n";
	wcout << setw(30) << L"configVersion: " << (int)info.configVersion << L"\n";
	wcout << setw(30) << L"volMaskCopy: " << hex << L"0x" << info.volMaskCopy << dec << L"\n";
	wcout << setw(30) << L"updatingTimeSet: " << info.updatingTimeSet << L"\n";
	wcout << setw(30) << L"updatingTimeNotAfter: " << info.updatingTimeNotAfter << L"\n";
	wcout << L"========================================================\n";

	// MD5 校验
	unsigned char digest[FRZ_CONFIG_MD5_SIZE] = {};
	computeConfigMD5(cfg, digest);
	wcout << setw(30) << L"MD5 (computed): " << ConvertString(bytesToHex(digest, FRZ_CONFIG_MD5_SIZE));
	wcout << (memcmp(digest, cfg.md5, FRZ_CONFIG_MD5_SIZE) == 0 ? L" (VALID)\n" : L" (MISMATCH!)\n");
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

// 读取按行的字符串输入（回车返回空串）
static wstring inputLine(const wstring& prompt) {
	wcout << prompt;
	wstring line;
	getline(wcin, line);
	if (wcin.fail()) {
		wcin.clear();
		return wstring();
	}
	while (!line.empty() && (line.back() == L'\r' || line.back() == L'\n')) {
		line.pop_back();
	}
	return line;
}

// 将宽字符串写入定长 ASCII 字段（超出部分截断）
static void writeFixedString(unsigned char* field, size_t fieldSize, const wstring& value) {
	string narrow = ConvertString<string>(value);
	size_t length = (min)(narrow.size(), fieldSize - 1);
	memset(field, 0, fieldSize);
	memcpy(field, narrow.data(), length);
}

// 修改冻结卷掩码，返回 true 表示配置已改动
static bool modifyFreezeMask(HConfigFile& cfg) {
	wcout << L"\n--- Modify Freeze Volume Mask ---\n";
	wcout << L"Current readytoProtectVolume: 0x" << hex << cfg.info.readytoProtectVolume << dec << L"\n";
	uint32_t newMask = inputHex(L"New mask: ");

	// 统一走库里的构建逻辑：写入目标掩码、状态字与写标记，并重算 MD5
	bool enable = newMask != 0;
	cfg = HFreezeConfig::BuildFreezeConfig(cfg, newMask, enable);
	wcout << L"Mask updated (" << (enable ? L"frozen" : L"unfrozen")
		<< L"), MD5 recalculated.\n";
	return true;
}

// 修改单个字段，返回 true 表示配置已改动
static bool modifyField(HConfigFile& cfg) {
	ProtectInfo& info = cfg.info;

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
	if (choice == 0) return false;

	switch (choice) {
	case 1: info.readytoProtectVolume = inputHex(L"New readytoProtectVolume: "); break;
	case 2: info.alreadyProtectVolume = inputHex(L"New alreadyProtectVolume: "); break;
	case 3: info.diskNum = (uint8_t)inputInt<int>(L"diskNum (0-255): ", 0, 255); break;
	case 4: info.stopProtect = inputInt<int>(L"stopProtect (0/1): ", 0, 1); break;
	case 5: info.needUpdate = inputInt<int>(L"needUpdate (0/1): ", 0, 1); break;
	case 6: {
		wcout << L"Enter storageFileSize (hex like 0x40000000 or decimal): ";
		uint64_t val;
		if (wcin >> hex >> val) info.storageFileSize = val;
		else { clearInputBuffer(); wcout << L"Invalid input.\n"; }
		break;
	}
	case 7: info.bRunSlowly = inputInt<int>(L"bRunSlowly (0/1): ", 0, 1); break;
	case 8: info.bsodNum = inputInt<uint32_t>(L"bsodNum: ", 0, UINT32_MAX); break;
	case 9: info.bsodMaxUptime = inputInt<uint32_t>(L"bsodMaxUptime: ", 0, UINT32_MAX); break;
	case 10: info.blueHistoryReport = inputInt<int>(L"blueHistoryReport (0/1): ", 0, 1); break;
	case 11: info.lastFreezeState = inputHex(L"lastFreezeState: "); break;
	case 12: info.lastbsodRuntime = inputInt<uint32_t>(L"lastbsodRuntime: ", 0, UINT32_MAX); break;
	case 13: {
		wcout << L"Enter lastsendbsodtime (hex uint64): ";
		uint64_t val;
		if (wcin >> hex >> val) info.lastsendbsodtime = val;
		else { clearInputBuffer(); wcout << L"Invalid input.\n"; }
		break;
	}
	case 14: info.coreDumpZipReport = inputInt<int>(L"coreDumpZipReport (0/1): ", 0, 1); break;
	case 15: info.isLastPagefileInFreezeVol = inputInt<int>(L"isLastPagefileInFreezeVol (0/1): ", 0, 1); break;
	case 16: info.isLastVolumeCorrupt = inputInt<int>(L"isLastVolumeCorrupt (0/1): ", 0, 1); break;
	case 17: {
		wstring s = inputLine(L"Enter iotDeviceID (max 19 chars): ");
		writeFixedString(info.iotDeviceID, FRZ_IOT_DEVICE_ID_LEN, s);
		break;
	}
	case 18: {
		wstring s = inputLine(L"Enter iotSchoolID (max 5 chars): ");
		writeFixedString(info.iotSchoolID, FRZ_IOT_SCHOOL_ID_LEN, s);
		break;
	}
	case 19: info.bNeedFreeze = inputInt<int>(L"bNeedFreeze (0/1): ", 0, 1); break;
	case 20: info.bNeedUnFreeze = inputInt<int>(L"bNeedUnFreeze (0/1): ", 0, 1); break;
	case 21: info.updateRebootCount = (uint16_t)inputInt<int>(L"updateRebootCount (0-65535): ", 0, 65535); break;
	case 22: {
		wstring s = inputLine(L"Enter startupTime (max 20 chars): ");
		writeFixedString(reinterpret_cast<unsigned char*>(info.startupTime), FRZ_STARTUP_TIME_LEN, s);
		break;
	}
	case 23: info.configVersion = (uint8_t)inputInt<int>(L"configVersion (0-255): ", 0, 255); break;
	case 24: info.volMaskCopy = inputHex(L"volMaskCopy: "); break;
	case 25: info.updatingTimeSet = inputInt<int>(L"updatingTimeSet (0/1): ", 0, 1); break;
	case 26: info.updatingTimeNotAfter = inputInt<uint32_t>(L"updatingTimeNotAfter (unix timestamp): ", 0, UINT32_MAX); break;
	default: break;
	}

	// 立即重算工作副本的 MD5，保证内存中的配置始终自洽
	unsigned char digest[FRZ_CONFIG_MD5_SIZE] = {};
	computeConfigMD5(cfg, digest);
	memcpy(cfg.md5, digest, FRZ_CONFIG_MD5_SIZE);
	wcout << L"Field updated. (MD5 recalculated.)\n";
	return true;
}

// 从指定文件读取配置，返回是否成功
static bool readConfigFile(const wstring& path, HConfigFile& out) {
	HFreezeFileBackend file;
	file.setConfigPath(path);
	return file.getConfig(out);
}

// 重算 MD5 后写入指定文件（文件不存在时会被创建）
static bool writeConfigFile(const wstring& path, HConfigFile& cfg) {
	// 写回前重算 MD5，保证管家/驱动读取时校验通过
	unsigned char digest[FRZ_CONFIG_MD5_SIZE] = {};
	computeConfigMD5(cfg, digest);
	memcpy(cfg.md5, digest, FRZ_CONFIG_MD5_SIZE);

	unsigned char buffer[FRZ_CONFIG_SIZE] = {};
	cfg.toBuffer(buffer);

	HFreezeFileBackend file;
	file.setConfigPath(path);
	return file.writeBlob(buffer, FRZ_CONFIG_SIZE);
}

// 未保存更改的丢弃确认，返回 true 表示可以继续
static bool confirmDiscard() {
	wcout << L"There are unsaved changes, they will be lost. Continue? (y/n): ";
	wchar_t c = 0;
	wcin >> c;
	clearInputBuffer();
	return c == L'y' || c == L'Y';
}

static void printTitle(const wstring& path, bool dirty) {
	wcout << L"\n--- " << (path.empty() ? L"<untitled>" : path)
		<< (dirty ? L" *" : L"") << L" ---\n";
}

static void interactiveLoop() {
	// 启动时尝试打开系统默认配置，失败则进入未命名空配置
	wstring currentPath = FRZ_CONFIG_PATH;
	HConfigFile working = makeEmptyConfig();
	if (readConfigFile(currentPath, working)) {
		wcout << L"Loaded: " << currentPath << L"\n";
	}
	else {
		wcout << L"Cannot read " << currentPath << L", starting with an empty config.\n";
		currentPath.clear();
		working = makeEmptyConfig();
	}
	bool dirty = false;

	while (true) {
		printTitle(currentPath, dirty);
		wcout << L"1. View current configuration\n";
		wcout << L"2. View binary dump\n";
		wcout << L"3. Modify freeze volume mask (simplified)\n";
		wcout << L"4. Modify individual field (advanced)\n";
		wcout << L"5. Save\n";
		wcout << L"6. Save as\n";
		wcout << L"7. Open\n";
		wcout << L"8. New\n";
		wcout << L"9. Exit\n";
		int choice = inputInt<int>(L"Enter your choice: ", 1, 9);

		switch (choice) {
		case 1:
			printConfig(working);
			break;
		case 2:
			printBinaryDump(working);
			break;
		case 3:
			if (modifyFreezeMask(working)) dirty = true;
			break;
		case 4:
			if (modifyField(working)) dirty = true;
			break;
		case 5: { // Save
			if (currentPath.empty()) {
				wstring path = inputLine(L"File name to save as (empty = cancel): ");
				if (path.empty()) { wcout << L"Save cancelled.\n"; break; }
				currentPath = path;
			}
			if (writeConfigFile(currentPath, working)) {
				dirty = false;
				wcout << L"Saved to " << currentPath << L"\n";
			}
			else {
				wcout << L"ERROR: Failed to write " << currentPath << L"\n";
			}
			break;
		}
		case 6: { // Save as
			wstring path = inputLine(L"Save as (empty = cancel): ");
			if (path.empty()) { wcout << L"Save cancelled.\n"; break; }
			if (writeConfigFile(path, working)) {
				currentPath = path;
				dirty = false;
				wcout << L"Saved to " << currentPath << L"\n";
			}
			else {
				wcout << L"ERROR: Failed to write " << path << L"\n";
			}
			break;
		}
		case 7: { // Open
			if (dirty && !confirmDiscard()) break;
			wstring path = inputLine(L"File to open (empty = cancel): ");
			if (path.empty()) { wcout << L"Open cancelled.\n"; break; }
			HConfigFile loaded = makeEmptyConfig();
			if (readConfigFile(path, loaded)) {
				working = loaded;
				currentPath = path;
				dirty = false;
				wcout << L"Opened " << currentPath << L"\n";
			}
			else {
				wcout << L"ERROR: Failed to read " << path
					<< L" (needs " << FRZ_CONFIG_SIZE << L" bytes).\n";
			}
			break;
		}
		case 8: { // New
			if (dirty && !confirmDiscard()) break;
			working = makeEmptyConfig();
			currentPath.clear();
			dirty = false;
			wcout << L"New empty config created.\n";
			break;
		}
		case 9: // Exit
			if (dirty && !confirmDiscard()) break;
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
