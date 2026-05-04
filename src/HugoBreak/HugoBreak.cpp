#include "WinUtils/WinPch.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <cstdint>
#include <cstdio>
#include "WinUtils/Console.h"
#include "HugoUtils/HInfo.h"
using namespace WinUtils;

namespace fs = std::filesystem;

const std::vector<uint8_t> original = { 0xE8, 0x16, 0xE9, 0xFF, 0xFF };
const std::vector<uint8_t> patched = { 0xB0, 0x01, 0x90, 0x90, 0x90 };

const std::wstring dll_filename = L"bind_zmodule.dll";
const std::wstring backup_filename = dll_filename + L".bak";

std::vector<uint8_t> read_file(const std::wstring& path) {
	FILE* file = nullptr;
	_wfopen_s(&file, path.c_str(), L"rb");
	if (!file) {
		throw std::runtime_error("无法打开文件");
	}

	// 获取文件大小
	fseek(file, 0, SEEK_END);
	long size = ftell(file);
	fseek(file, 0, SEEK_SET);

	if (size < 0) {
		fclose(file);
		throw std::runtime_error("获取文件大小失败");
	}

	std::vector<uint8_t> buffer(static_cast<size_t>(size));
	size_t read_bytes = fread(buffer.data(), 1, buffer.size(), file);
	fclose(file);

	if (read_bytes != buffer.size()) {
		throw std::runtime_error("读取文件失败");
	}

	return buffer;
}

void write_file(const std::wstring& path, const std::vector<uint8_t>& data) {
	FILE* file = nullptr;
	_wfopen_s(&file, path.c_str(), L"wb");
	if (!file) {
		throw std::runtime_error("无法创建文件");
	}

	size_t write_bytes = fwrite(data.data(), 1, data.size(), file);
	fclose(file);

	if (write_bytes != data.size()) {
		throw std::runtime_error("写入文件失败");
	}
}

bool find_sequence(const std::vector<uint8_t>& data, const std::vector<uint8_t>& seq) {
	if (data.size() < seq.size()) return false;

	for (size_t i = 0; i <= data.size() - seq.size(); ++i) {
		bool match = true;
		for (size_t j = 0; j < seq.size(); ++j) {
			if (data[i + j] != seq[j]) {
				match = false;
				break;
			}
		}
		if (match) return true;
	}
	return false;
}

std::vector<uint8_t> replace_sequence(const std::vector<uint8_t>& data,
	const std::vector<uint8_t>& old_seq,
	const std::vector<uint8_t>& new_seq) {
	std::vector<uint8_t> result;
	result.reserve(data.size());

	for (size_t i = 0; i < data.size();) {
		bool matched = false;
		if (i <= data.size() - old_seq.size()) {
			matched = true;
			for (size_t j = 0; j < old_seq.size(); ++j) {
				if (data[i + j] != old_seq[j]) {
					matched = false;
					break;
				}
			}
		}

		if (matched) {
			result.insert(result.end(), new_seq.begin(), new_seq.end());
			i += old_seq.size();
		}
		else {
			result.push_back(data[i]);
			i++;
		}
	}

	return result;
}

int main() {
	Console console;
	console.setLocale();
	HInfo info;
	auto folder = info.getHugoFolder();
	if (!folder.has_value()) {
		std::wcerr << L"这台电脑上似乎没有安装希沃管家";
		return 1;
	}
	std::wstring sample_path = *folder + L"SeewoCore\\module\\bind\\";

	try {
		if (!fs::exists(dll_filename)) {
			std::wcerr << L"未在当前目录找到文件!" << std::endl << std::endl;
			std::wcerr << L"请复制以下文件:" << std::endl;
			std::wcerr << L"    " << dll_filename << std::endl;
			std::wcerr << L"从路径:" << std::endl;
			std::wcerr << L"    " << sample_path << std::endl;
			std::wcerr << L"到当前目录。" << std::endl;
			system("pause");
			return 1;
		}

		// 读取文件内容
		std::vector<uint8_t> data = read_file(dll_filename);

		// 检查是否已经被修补
		if (!find_sequence(data, original)) {
			std::wcout << L"看起来文件已经被修补过了 :)" << std::endl;
			system("pause");
			return 0;
		}

		// 创建备份文件
		if (!fs::exists(backup_filename)) {
			write_file(backup_filename, data);
		}
		else {
			std::wcerr << backup_filename << L" 已经存在!" << std::endl << std::endl;
			std::wcerr << L"为了安全起见，我们不会覆盖它。" << std::endl;
			std::wcerr << L"请删除它或将其重命名。" << std::endl;
			system("pause");
			return 1;
		}

		// 修补文件
		std::vector<uint8_t> patched_data = replace_sequence(data, original, patched);
		write_file(dll_filename, patched_data);

		std::wcout << L"文件修补成功!" << std::endl;
		std::wcout << L"原始文件已备份为 " << backup_filename << std::endl << std::endl;
		std::wcout << L"请复制以下文件:" << std::endl;
		std::wcout << L"    " << dll_filename << std::endl;
		std::wcout << L"到路径:" << std::endl;
		std::wcout << L"    " << sample_path << std::endl;
		std::wcout << L"并替换原始文件。" << std::endl << std::endl;

	}
	catch (const std::exception& e) {
		std::cerr << "错误: " << e.what() << std::endl;
		system("pause");
		return 1;
	}
	system("pause");
	return 0;
}