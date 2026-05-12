#include "ReleaseFetcher.h"

#include <array>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>
#else
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>
#endif

namespace vn::update {
namespace {

#ifdef _WIN32

constexpr auto GITHUB_API_HOST_W = L"api.github.com";
constexpr auto GITHUB_LATEST_RELEASE_PATH_W = L"/repos/saitatter/vertex-note/releases/latest";

class WinHttpHandle final {
public:
    explicit WinHttpHandle(HINTERNET handle = nullptr): handle(handle) {}

    WinHttpHandle(const WinHttpHandle&) = delete;
    auto operator=(const WinHttpHandle&) -> WinHttpHandle& = delete;

    WinHttpHandle(WinHttpHandle&& other) noexcept: handle(std::exchange(other.handle, nullptr)) {}

    auto operator=(WinHttpHandle&& other) noexcept -> WinHttpHandle& {
        if (this != &other) {
            reset();
            handle = std::exchange(other.handle, nullptr);
        }
        return *this;
    }

    ~WinHttpHandle() { reset(); }

    auto get() const -> HINTERNET { return handle; }

    explicit operator bool() const { return handle != nullptr; }

private:
    void reset() {
        if (handle) {
            WinHttpCloseHandle(handle);
            handle = nullptr;
        }
    }

    HINTERNET handle = nullptr;
};

auto utf8FromWide(const std::wstring& value) -> std::string {
    if (value.empty()) {
        return {};
    }

    const int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr,
                                         nullptr);
    if (size <= 0) {
        return "Unknown Windows error.";
    }

    std::string utf8(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), utf8.data(), size, nullptr, nullptr);
    return utf8;
}

auto formatWinHttpError(DWORD errorCode) -> std::string {
    LPWSTR messageBuffer = nullptr;
    const DWORD size = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                                              FORMAT_MESSAGE_IGNORE_INSERTS,
                                      nullptr, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                      reinterpret_cast<LPWSTR>(&messageBuffer), 0, nullptr);

    std::wstring message = size > 0 && messageBuffer ? std::wstring(messageBuffer, size) : L"Unknown Windows error.";
    if (messageBuffer) {
        LocalFree(messageBuffer);
    }

    while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n')) {
        message.pop_back();
    }
    return utf8FromWide(message);
}

auto checkedStatusCode(HINTERNET request) -> DWORD {
    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeSize, WINHTTP_NO_HEADER_INDEX)) {
        throw std::runtime_error("GitHub returned a response without a status code.");
    }
    return statusCode;
}

auto openWinHttpSession() -> WinHttpHandle {
    WinHttpHandle session(WinHttpOpen(L"VertexNote", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                                      WINHTTP_NO_PROXY_BYPASS, 0));
    if (session) {
        return session;
    }

    const DWORD automaticProxyError = GetLastError();
    session = WinHttpHandle(WinHttpOpen(L"VertexNote", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                                        WINHTTP_NO_PROXY_BYPASS, 0));
    if (session) {
        return session;
    }

    const DWORD defaultProxyError = GetLastError();
    throw std::runtime_error(formatWinHttpError(defaultProxyError != ERROR_SUCCESS ? defaultProxyError : automaticProxyError));
}

auto fetchWithWinHttp() -> std::string {
    WinHttpHandle session = openWinHttpSession();

    WinHttpHandle connection(WinHttpConnect(session.get(), GITHUB_API_HOST_W, INTERNET_DEFAULT_HTTPS_PORT, 0));
    if (!connection) {
        throw std::runtime_error(formatWinHttpError(GetLastError()));
    }

    WinHttpHandle request(WinHttpOpenRequest(connection.get(), L"GET", GITHUB_LATEST_RELEASE_PATH_W, nullptr,
                                             WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
    if (!request) {
        throw std::runtime_error(formatWinHttpError(GetLastError()));
    }

    DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    if (!WinHttpSetOption(request.get(), WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy))) {
        throw std::runtime_error(formatWinHttpError(GetLastError()));
    }

    constexpr auto requestHeaders =
            L"Accept: application/vnd.github+json\r\n"
            L"X-GitHub-Api-Version: 2022-11-28\r\n";
    if (!WinHttpAddRequestHeaders(request.get(), requestHeaders, static_cast<DWORD>(-1L),
                                  WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE)) {
        throw std::runtime_error(formatWinHttpError(GetLastError()));
    }

    if (!WinHttpSendRequest(request.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        throw std::runtime_error(formatWinHttpError(GetLastError()));
    }
    if (!WinHttpReceiveResponse(request.get(), nullptr)) {
        throw std::runtime_error(formatWinHttpError(GetLastError()));
    }

    const DWORD statusCode = checkedStatusCode(request.get());
    if (statusCode != 200) {
        throw std::runtime_error("GitHub returned a non-success HTTP response.");
    }

    std::string response;
    while (true) {
        DWORD availableBytes = 0;
        if (!WinHttpQueryDataAvailable(request.get(), &availableBytes)) {
            throw std::runtime_error(formatWinHttpError(GetLastError()));
        }
        if (availableBytes == 0) {
            break;
        }

        std::array<char, 8192> buffer{};
        DWORD bytesRead = 0;
        const auto bytesToRead = std::min<DWORD>(availableBytes, static_cast<DWORD>(buffer.size()));
        if (!WinHttpReadData(request.get(), buffer.data(), bytesToRead, &bytesRead)) {
            throw std::runtime_error(formatWinHttpError(GetLastError()));
        }
        response.append(buffer.data(), static_cast<std::size_t>(bytesRead));
    }

    return response;
}

#else

auto fetchWithQtNetwork() -> std::string {
    QNetworkAccessManager manager;
    QNetworkRequest request(QUrl(QStringLiteral("https://api.github.com/repos/saitatter/vertex-note/releases/latest")));
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("VertexNote"));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);

    QNetworkReply* reply = manager.get(request);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(15000);
    loop.exec();

    if (!timeout.isActive()) {
        reply->abort();
        reply->deleteLater();
        throw std::runtime_error("GitHub release request timed out.");
    }
    timeout.stop();

    const auto replyGuard = std::unique_ptr<QNetworkReply, void (*)(QNetworkReply*)>(
            reply, [](QNetworkReply* guardedReply) { guardedReply->deleteLater(); });

    if (reply->error() != QNetworkReply::NoError) {
        throw std::runtime_error(reply->errorString().toStdString());
    }

    const auto statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (statusCode != 200) {
        throw std::runtime_error("GitHub returned a non-success HTTP response.");
    }

    return reply->readAll().toStdString();
}

#endif

}  // namespace

auto fetchLatestReleaseJson() -> std::string {
#ifdef _WIN32
    return fetchWithWinHttp();
#else
    return fetchWithQtNetwork();
#endif
}

}  // namespace vn::update
