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

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "HugoUtils/httplib/httplib.h"
#include <windows.h>
#include <shellapi.h>
#include <HugoUtils/Console.h>
#include <HugoUtils/StrConvert.h>
#include <HugoUtils/WinUtils.h>

using namespace std;
namespace fs = filesystem;
using namespace WinUtils;

constexpr const wchar_t* DEFAULT_NAME = L"";
constexpr const wchar_t BASE_URL[] = L"https://e.seewo.com/download/file?code=SeewoServiceSetup";

struct PackageInfo {
	wstring version;
	wstring filePath;
};

struct DownloadResult {
	bool success = false;
	int statusCode = 0;
	string errorMsg;
	size_t downloaded = 0;
	size_t total = 0;
	string filename;
};

class HttpDownloader {
public:
	explicit HttpDownloader(int maxRedirects = 10) : m_maxRedirects(maxRedirects) {}
	void setTimeout(int seconds) { m_timeout = seconds; }
	void setUserAgent(string ua) { m_userAgent = move(ua); }

	DownloadResult download(const string& url, const string& outDir,
		const string& customName = {},  // 参数保留但不再用于文件名
		function<void(int64_t, int64_t)> progress = nullptr,
		bool resume = true) {
		auto [host, path, isHttps] = parseUrl(url);
		if (host.empty())
			return { false, 0, "无效URL: " + url };

		fs::path outPath = fs::path(outDir) / "SeewoServiceSetup.tmp";
		size_t existing = resume && fs::exists(outPath) ? fs::file_size(outPath) : 0;

		DownloadResult result;
		int redirects = 0;
		string currentHost = host, currentPath = path, currentUrl = url;
		bool currentHttps = isHttps;

		while (redirects <= m_maxRedirects) {
			auto headers = makeHeaders(existing);
			string location;

			auto doDownload = [&](auto& client) {
				ofstream file(outPath, existing ? (ios::binary | ios::app) : (ios::binary | ios::trunc));
				if (!file.is_open()) {
					result.errorMsg = "无法创建文件: " + outPath.string();
					return;
				}

				size_t totalSize = existing, received = existing;
				auto res = client.Get(currentPath.c_str(), headers,
					[&](const httplib::Response& r) {
						if (r.status == 200) {
							totalSize = stoull(r.get_header_value("Content-Length", "0"));
							result.statusCode = 200;
						}
						else if (r.status == 206) {
							auto range = r.get_header_value("Content-Range");
							auto pos = range.find_last_of('/');
							if (pos != string::npos)
								totalSize = stoull(range.substr(pos + 1));
							result.statusCode = 206;
						}
						else if (r.status >= 300 && r.status < 400) {
							location = r.get_header_value("Location");
							result.statusCode = r.status;
							return true; // 停止接收正文
						}
						else {
							result.statusCode = r.status;
							return false;
						}
						result.total = totalSize;
						return true;
					},
					[&](const char* data, size_t len) {
						file.write(data, len);
						if (!file) return false;
						received += len;
						result.downloaded = received;
						if (progress) progress(received, totalSize);
						return true;
					});
				file.close();

				if (!res) {
					result.errorMsg = "连接错误: " + to_string(static_cast<int>(res.error()));
				}
				else if (res->status == 200 || res->status == 206) {
					result.success = (totalSize == 0 || received == totalSize);
					if (!result.success) result.errorMsg = "下载不完整";
				}
				else if (!location.empty()) {
					// 将重定向交给外层循环处理
				}
				else {
					result.errorMsg = "HTTP错误: " + to_string(res->status);
				}
				};

			if (currentHttps) {
				httplib::SSLClient cli(currentHost);
				cli.set_connection_timeout(m_timeout);
				cli.set_read_timeout(m_timeout);
				doDownload(cli);
			}
			else {
				httplib::Client cli(currentHost);
				cli.set_connection_timeout(m_timeout);
				cli.set_read_timeout(m_timeout);
				doDownload(cli);
			}

			if (!result.success && !location.empty() && location != currentUrl) {
				// 处理重定向
				wcout << L"[*] 重定向 (" << ++redirects << L"/" << m_maxRedirects
					<< L") -> " << ConvertString<wstring>(location) << L'\n';
				auto [newHost, newPath, newHttps] = parseUrl(location);
				currentHost = newHost;
				currentPath = newPath;
				currentHttps = newHttps;
				currentUrl = location;
				string newFilenameFromUrl = extractFilenameFromRedirectUrl(currentUrl);
				if (!newFilenameFromUrl.empty()) {
					result.filename = newFilenameFromUrl;
					wcout << L"[*] 从重定向URL识别到文件名: " << ConvertString<wstring>(newFilenameFromUrl) << L'\n';
				}
				existing = 0; // 重定向后重新下载，不续传
				continue;
			}
			break;
		}

		return result;
	}

private:
	int m_maxRedirects;
	int m_timeout = 30;
	string m_userAgent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36";

	struct UrlParts { string host, path; bool isHttps = false; };

	UrlParts parseUrl(const string& url) {
		string_view sv(url);
		bool https = sv.substr(0, 8) == "https://";
		if (https) sv.remove_prefix(8);
		else if (sv.substr(0, 7) == "http://") sv.remove_prefix(7);
		else return {};

		auto pos = sv.find('/');
		if (pos == string_view::npos)
			return { string(sv), "/", https };
		return { string(sv.substr(0, pos)), string(sv.substr(pos)), https };
	}

	string getFilenameFromDisposition(const string& hdr) {
		regex re(R"(filename=["']?([^"' ;]+)["']?)");
		smatch m;
		return regex_search(hdr, m, re) ? m[1].str() : ConvertString<string>(DEFAULT_NAME);
	}

	httplib::Headers makeHeaders(size_t rangeStart) {
		httplib::Headers h{ {"User-Agent", m_userAgent}, {"Accept", "*/*"}, {"Accept-Encoding", "identity"} };
		if (rangeStart > 0) h.insert({ "Range", "bytes=" + to_string(rangeStart) + "-" });
		return h;
	}

	string urlDecode(const string& encoded) {
		string decoded;
		for (size_t i = 0; i < encoded.length(); ++i) {
			if (encoded[i] == '%' && i + 2 < encoded.length()) {
				int hex;
				istringstream iss(encoded.substr(i + 1, 2));
				iss >> std::hex >> hex;
				decoded += static_cast<char>(hex);
				i += 2;
			}
			else {
				decoded += encoded[i];
			}
		}
		return decoded;
	}

	string extractFilenameFromRedirectUrl(const string& url) {
		const string paramKey = "response-content-disposition=";
		size_t pos = url.find(paramKey);
		if (pos == string::npos) return {};

		pos += paramKey.length();
		size_t end = url.find('&', pos);
		string encodedValue = url.substr(pos, end - pos);
		string decodedValue = urlDecode(encodedValue);
		return getFilenameFromDisposition(decodedValue);
	}
};

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
		wcin.clear(); wcin.ignore(numeric_limits<streamsize>::max(), L'\n');
		wcerr << L"[-] 无效输入，重新选择: ";
	}

	if (1 <= choice && choice <= static_cast<int>(pkgs.size()))
		return runPackage(pkgs[choice - 1].filePath) ? 0 : 1;

	// 下载新版本
	wcout << L"\n==================== 下载新版本 ====================\n";
	wstring ver;
	wcout << L"输入版本号 (格式 1.X.X.XXXX, 0 表示最新): ";
	while (!(wcin >> ver) || !isValidVersion(ver)) {
		wcin.clear(); wcin.ignore(numeric_limits<streamsize>::max(), L'\n');
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