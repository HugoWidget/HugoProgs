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

namespace fs = std::filesystem;
using namespace WinUtils;

constexpr const wchar_t* DEFAULT_NAME = L"";
constexpr const char* BASE_URL = "https://e.seewo.com/download/file?code=SeewoServiceSetup";

struct PackageInfo {
    std::wstring version;
    std::wstring filePath;
};

struct DownloadResult {
    bool success = false;
    int statusCode = 0;
    std::string errorMsg;
    size_t downloaded = 0;
    size_t total = 0;
    std::string filename; // 从 Content-Disposition 或重定向URL提取的实际文件名
};

class HttpDownloader {
public:
    explicit HttpDownloader(int maxRedirects = 10) : maxRedirects_(maxRedirects) {}
    void setTimeout(int seconds) { timeout_ = seconds; }
    void setUserAgent(std::string ua) { userAgent_ = std::move(ua); }

    DownloadResult download(const std::string& url, const std::string& outDir,
        const std::string& customName = {},  // 参数保留但不再用于文件名
        std::function<void(int64_t, int64_t)> progress = nullptr,
        bool resume = true) {
        auto [host, path, isHttps] = parseUrl(url);
        if (host.empty())
            return { false, 0, "无效URL: " + url };

        // 始终使用临时文件名
        fs::path outPath = fs::path(outDir) / "SeewoServiceSetup.tmp";
        size_t existing = resume && fs::exists(outPath) ? fs::file_size(outPath) : 0;

        DownloadResult result;
        int redirects = 0;
        std::string currentHost = host, currentPath = path, currentUrl = url;
        bool currentHttps = isHttps;

        while (redirects <= maxRedirects_) {
            auto headers = makeHeaders(existing);
            std::string location;

            auto doDownload = [&](auto& client) {
                std::ofstream file(outPath, existing ? (std::ios::binary | std::ios::app) : (std::ios::binary | std::ios::trunc));
                if (!file.is_open()) {
                    result.errorMsg = "无法创建文件: " + outPath.string();
                    return;
                }

                size_t totalSize = existing, received = existing;
                auto res = client.Get(currentPath.c_str(), headers,
                    [&](const httplib::Response& r) {
                        if (r.status == 200) {
                            totalSize = std::stoull(r.get_header_value("Content-Length", "0"));
                            result.statusCode = 200;
                        }
                        else if (r.status == 206) {
                            auto range = r.get_header_value("Content-Range");
                            auto pos = range.find_last_of('/');
                            if (pos != std::string::npos)
                                totalSize = std::stoull(range.substr(pos + 1));
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
                    result.errorMsg = "连接错误: " + std::to_string(static_cast<int>(res.error()));
                }
                else if (res->status == 200 || res->status == 206) {
                    result.success = (totalSize == 0 || received == totalSize);
                    if (!result.success) result.errorMsg = "下载不完整";
                }
                else if (!location.empty()) {
                    // 将重定向交给外层循环处理
                }
                else {
                    result.errorMsg = "HTTP错误: " + std::to_string(res->status);
                }
                };

            if (currentHttps) {
                httplib::SSLClient cli(currentHost);
                cli.set_connection_timeout(timeout_);
                cli.set_read_timeout(timeout_);
                doDownload(cli);
            }
            else {
                httplib::Client cli(currentHost);
                cli.set_connection_timeout(timeout_);
                cli.set_read_timeout(timeout_);
                doDownload(cli);
            }

            if (!result.success && !location.empty() && location != currentUrl) {
                // 处理重定向
                std::wcout << L"[*] 重定向 (" << ++redirects << L"/" << maxRedirects_
                    << L") -> " << AnsiToWideString(location.c_str()) << L'\n';
                auto [newHost, newPath, newHttps] = parseUrl(location);
                currentHost = newHost;
                currentPath = newPath;
                currentHttps = newHttps;
                currentUrl = location;
                std::string newFilenameFromUrl = extractFilenameFromRedirectUrl(currentUrl);
                if (!newFilenameFromUrl.empty()) {
                    result.filename = newFilenameFromUrl;
                    std::wcout << L"[*] 从重定向URL识别到文件名: " << AnsiToWideString(newFilenameFromUrl.c_str()) << L'\n';
                }
                existing = 0; // 重定向后重新下载，不续传
                continue;
            }
            break;
        }

        // 移除原有的自动重命名代码，只返回提取到的文件名
        return result;
    }

private:
    int maxRedirects_;
    int timeout_ = 30;
    std::string userAgent_ = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36";

    struct UrlParts { std::string host, path; bool isHttps = false; };

    UrlParts parseUrl(const std::string& url) {
        std::string_view sv(url);
        bool https = sv.substr(0, 8) == "https://";
        if (https) sv.remove_prefix(8);
        else if (sv.substr(0, 7) == "http://") sv.remove_prefix(7);
        else return {};

        auto pos = sv.find('/');
        if (pos == std::string_view::npos)
            return { std::string(sv), "/", https };
        return { std::string(sv.substr(0, pos)), std::string(sv.substr(pos)), https };
    }

    std::string getFilenameFromDisposition(const std::string& hdr) {
        std::regex re(R"(filename=["']?([^"' ;]+)["']?)");
        std::smatch m;
        return std::regex_search(hdr, m, re) ? m[1].str() : WideStringToAnsi(DEFAULT_NAME);
    }

    httplib::Headers makeHeaders(size_t rangeStart) {
        httplib::Headers h{ {"User-Agent", userAgent_}, {"Accept", "*/*"}, {"Accept-Encoding", "identity"} };
        if (rangeStart > 0) h.insert({ "Range", "bytes=" + std::to_string(rangeStart) + "-" });
        return h;
    }

    std::string urlDecode(const std::string& encoded) {
        std::string decoded;
        for (size_t i = 0; i < encoded.length(); ++i) {
            if (encoded[i] == '%' && i + 2 < encoded.length()) {
                int hex;
                std::istringstream iss(encoded.substr(i + 1, 2));
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

    std::string extractFilenameFromRedirectUrl(const std::string& url) {
        const std::string paramKey = "response-content-disposition=";
        size_t pos = url.find(paramKey);
        if (pos == std::string::npos) return {};

        pos += paramKey.length();
        size_t end = url.find('&', pos);
        std::string encodedValue = url.substr(pos, end - pos);
        std::string decodedValue = urlDecode(encodedValue);
        return getFilenameFromDisposition(decodedValue);
    }
};

static std::vector<PackageInfo> scanPackages(const std::wstring& dir) {
    std::vector<PackageInfo> pkgs;
    std::wregex re(LR"(SeewoServiceSetup_(\d+\.\d+\.\d+\.\d+)\.exe)");
    try {
        if (fs::exists(dir)) {
            for (auto& entry : fs::directory_iterator(dir)) {
                if (!entry.is_regular_file()) continue;
                std::wsmatch m;
                auto fname = entry.path().filename().wstring();
                if (std::regex_match(fname, m, re))
                    pkgs.push_back({ m[1].str(), entry.path().wstring() });
            }
        }
    }
    catch (const fs::filesystem_error& e) {
        std::wcerr << L"[-] 扫描失败: " << AnsiToWideString(e.what()) << L'\n';
    }
    return pkgs;
}

static bool isValidVersion(const std::wstring& ver) {
    if (ver == L"0") return true;
    std::wregex re(LR"(\d+\.\d+\.\d+\.\d+)");
    return std::regex_match(ver, re);
}

static bool runPackage(const std::wstring& path) {
    std::wcout << L"[+] 启动安装包: " << path << L'\n';
    HINSTANCE h = ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(h) > 32) {
        std::wcout << L"[+] 启动成功\n";
        return true;
    }
    std::wcerr << L"[-] 启动失败，错误码: " << GetLastError() << L'\n';
    return false;
}

static int install() {
    std::wstring pkgDir = GetCurrentProcessDir() + L"SetupPackage";
    fs::create_directories(pkgDir);

    auto pkgs = scanPackages(pkgDir);
    std::wcout << L"\n==================== 可选版本 ====================\n";
    for (size_t i = 0; i < pkgs.size(); ++i)
        std::wcout << L"[" << (i + 1) << L"] " << pkgs[i].version << L" (" << fs::path(pkgs[i].filePath).filename().wstring() << L")\n";
    std::wcout << L"[" << pkgs.size() + 1 << L"] 下载新版本\n";
    std::wcout << L"==================================================\n";

    int choice = 0;
    std::wcout << L"请选择: ";
    while (!(std::wcin >> choice) || choice < 1 || choice > static_cast<int>(pkgs.size() + 1)) {
        std::wcin.clear(); std::wcin.ignore(std::numeric_limits<std::streamsize>::max(), L'\n');
        std::wcerr << L"[-] 无效输入，重新选择: ";
    }

    if (choice <= static_cast<int>(pkgs.size()))
        return runPackage(pkgs[choice - 1].filePath) ? 0 : 1;

    // 下载新版本
    std::wcout << L"\n==================== 下载新版本 ====================\n";
    std::wstring ver;
    std::wcout << L"输入版本号 (格式 1.X.X.XXXX, 0 表示最新): ";
    while (!(std::wcin >> ver) || !isValidVersion(ver)) {
        std::wcin.clear(); std::wcin.ignore(std::numeric_limits<std::streamsize>::max(), L'\n');
        std::wcerr << L"[-] 格式错误，重新输入: ";
    }

    std::string url = BASE_URL;
    // 不再构造 customName，下载器将使用临时文件名
    std::wcout << L"[+] URL: " << AnsiToWideString(url.c_str()) << L"\n[+] 保存到: " << pkgDir << L"\n--------------------------------------------------\n";

    try {
        HttpDownloader dl;
        dl.setTimeout(60);
        dl.setUserAgent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");

        auto start = std::chrono::steady_clock::now();
        size_t lastPrint = 0;

        // 下载时不指定自定义文件名，使用临时文件
        auto res = dl.download(url, WideStringToAnsi(pkgDir.c_str()), "",
            [&](int64_t done, int64_t total) {
                if (total <= 1 || done - lastPrint < 1024 * 1024) return;
                lastPrint = done;
                auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
                double speed = (done / 1024.0 / 1024.0) / elapsed;
                int pct = int(done * 100 / total);
                std::wcout << L'\r' << L'[' << std::wstring(pct / 2, L'=') << (pct < 100 ? L">" : L"") << std::wstring(50 - pct / 2 - (pct < 100 ? 1 : 0), L' ')
                    << L"] " << pct << L"% " << done / 1024 / 1024 << L'/' << total / 1024 / 1024 << L" MB " << speed << L" MB/s" << std::flush;
            });

        std::wcout << L"\n--------------------------------------------------\n";
        fs::path tempPath = fs::path(pkgDir) / "SeewoServiceSetup.tmp";

        if (!res.success) {
            std::wcerr << L"[-] 下载失败\n[-] " << AnsiToWideString(res.errorMsg.c_str()) << L"\n[-] HTTP " << res.statusCode << L'\n';
            if (res.errorMsg.find('7') != std::string::npos)
                std::wcerr << L"可能已经下载过了\n";
            if (res.statusCode == 404)
                std::wcerr << L"[-] 版本不存在或已下架\n";
            // 删除空的临时文件
            if (fs::exists(tempPath) && fs::file_size(tempPath) == 0)
                fs::remove(tempPath);
            return 1;
        }

        if (res.downloaded < 1'000'000) {
            fs::remove(tempPath);
            std::wcerr << L"[-] 文件太小，版本可能错误\n";
            return 1;
        }

        // 确定最终文件名
        std::wstring finalFilename;
        if (!res.filename.empty()) {
            finalFilename = AnsiToWideString(res.filename.c_str());
        }
        else {
            if (ver != L"0") {
                finalFilename = L"SeewoServiceSetup_" + ver + L".exe";
            }
            else {
                // 版本为0且未能从URL提取文件名，使用默认名（理论上不应发生）
                finalFilename = L"SeewoServiceSetup_latest.exe";
            }
        }

        fs::path finalPath = fs::path(pkgDir) / finalFilename;
        // 如果目标文件已存在，先删除
        std::error_code ec;
        if (fs::exists(finalPath)) {
            fs::remove(finalPath, ec);
        }
        fs::rename(tempPath, finalPath, ec);
        if (ec) {
            std::wcerr << L"[-] 重命名失败: " << AnsiToWideString(ec.message().c_str()) << L'\n';
            return 1;
        }

        std::wcout << L"[+] 下载完成\n[+] 大小: " << res.downloaded << L" 字节 (" << res.downloaded / 1024.0 / 1024.0 << L" MB)\n";
        std::wcout << L"[+] 保存为: " << finalFilename << L'\n';
        return runPackage(finalPath.wstring()) ? 0 : 1;
    }
    catch (const std::exception& e) {
        std::wcerr << L"[-] 异常: " << AnsiToWideString(e.what()) << L'\n';
        return 1;
    }
}

static int uninstall() {
    std::wstring target = L"C:\\Program Files (x86)\\Seewo\\SeewoService\\uninstall_verify.exe";
    std::wstring run = L"C:\\Program Files (x86)\\Seewo\\SeewoService\\Uninstall.exe";
    std::wstring fake = GetCurrentProcessDir() + L"HugoFakeVerify.exe";

    if (!fs::exists(fake)) { std::wcerr << L"[-] 模板缺失: " << fake << L'\n'; return 1; }
    if (!fs::exists(run)) { std::wcerr << L"[-] 可能未安装希沃\n"; return 1; }

    // 读取模板
    std::vector<BYTE> data;
    HANDLE hFake = CreateFileW(fake.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFake == INVALID_HANDLE_VALUE) { std::wcerr << L"[-] 无法读取模板, 错误 " << GetLastError() << L'\n'; return 1; }
    DWORD sz = GetFileSize(hFake, nullptr);
    data.resize(sz);
    DWORD read;
    if (!ReadFile(hFake, data.data(), sz, &read, nullptr) || read != sz) {
        CloseHandle(hFake); std::wcerr << L"[-] 读取模板失败\n"; return 1;
    }
    CloseHandle(hFake);

    // 备份原文件
    wchar_t temp[MAX_PATH];
    GetTempFileNameW(target.substr(0, target.find_last_of(L'\\')).c_str(), L"OLD", 0, temp);
    if (!MoveFileW(target.c_str(), temp) && GetLastError() != ERROR_FILE_NOT_FOUND) {
        std::wcerr << L"[-] 无法备份目标文件, 错误 " << GetLastError() << L'\n';
        return 1;
    }

    // 写入新文件
    HANDLE hTarget = CreateFileW(target.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hTarget == INVALID_HANDLE_VALUE) {
        std::wcerr << L"[-] 无法写入目标, 错误 " << GetLastError() << L'\n';
        MoveFileW(temp, target.c_str()); // 还原
        return 1;
    }
    DWORD written;
    if (!WriteFile(hTarget, data.data(), sz, &written, nullptr) || written != sz) {
        std::wcerr << L"[-] 写入失败\n";
        CloseHandle(hTarget);
        MoveFileW(temp, target.c_str());
        return 1;
    }
    CloseHandle(hTarget);
    std::wcout << L"[+] 验证文件替换成功\n";

    // 启动卸载
    HINSTANCE h = ShellExecuteW(nullptr, L"open", run.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(h) > 32) {
        std::wcout << L"[+] 卸载程序已启动\n";
        return 0;
    }
    std::wcerr << L"[-] 启动卸载失败, 错误 " << GetLastError() << L'\n';
    return 1;
}

int wmain(int argc, wchar_t* argv[]) {
    Console().setLocale();

    if (argc > 1) {
        std::wstring arg = argv[1];
        if (arg == L"-install") return install();
        if (arg == L"-uninstall") return uninstall();
        std::wcerr << L"[-] 参数错误，使用 -install 或 -uninstall\n";
        return 1;
    }

    std::wcout << L"1. 安装希沃\n2. 卸载希沃\n0. 退出\n输入: ";
    int mode;
    while (std::wcin >> mode) {
        if (mode == 0) break;
        if (mode == 1) return install();
        if (mode == 2) return uninstall();
        std::wcerr << L"[-] 无效选择，重新输入: ";
    }
    std::wcout << L"退出\n";
    return 0;
}