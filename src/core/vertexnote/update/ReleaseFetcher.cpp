#include "ReleaseFetcher.h"

#include <stdexcept>
#include <string>
#include <string_view>
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
#include <gio/gio.h>

#include <array>
#include <charconv>
#include <cctype>
#include <optional>
#endif

namespace vn::update {
namespace {

constexpr auto GITHUB_API_HOST = "api.github.com";
constexpr auto GITHUB_LATEST_RELEASE_PATH = "/repos/saitatter/vertex-note/releases/latest";

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

auto fetchWithWinHttp() -> std::string {
    WinHttpHandle session(WinHttpOpen(L"VertexNote", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                                      WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session) {
        throw std::runtime_error(formatWinHttpError(GetLastError()));
    }

    WinHttpHandle connection(WinHttpConnect(session.get(), GITHUB_API_HOST_W, INTERNET_DEFAULT_HTTPS_PORT, 0));
    if (!connection) {
        throw std::runtime_error(formatWinHttpError(GetLastError()));
    }

    WinHttpHandle request(WinHttpOpenRequest(connection.get(), L"GET", GITHUB_LATEST_RELEASE_PATH_W, nullptr,
                                             WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
    if (!request) {
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

        std::string chunk(static_cast<std::size_t>(availableBytes), '\0');
        DWORD bytesRead = 0;
        if (!WinHttpReadData(request.get(), chunk.data(), availableBytes, &bytesRead)) {
            throw std::runtime_error(formatWinHttpError(GetLastError()));
        }
        chunk.resize(bytesRead);
        response += chunk;
    }

    return response;
}

#else

auto lower(std::string_view value) -> std::string {
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

auto decodeChunkedBody(std::string_view body) -> std::optional<std::string> {
    std::string decoded;
    std::size_t offset = 0;

    while (offset < body.size()) {
        const auto sizeEnd = body.find("\r\n", offset);
        if (sizeEnd == std::string_view::npos) {
            return std::nullopt;
        }

        auto sizeLine = body.substr(offset, sizeEnd - offset);
        if (const auto extension = sizeLine.find(';'); extension != std::string_view::npos) {
            sizeLine = sizeLine.substr(0, extension);
        }

        std::size_t chunkSize = 0;
        const auto* begin = sizeLine.data();
        const auto* end = sizeLine.data() + sizeLine.size();
        const auto [ptr, ec] = std::from_chars(begin, end, chunkSize, 16);
        if (ec != std::errc{} || ptr != end) {
            return std::nullopt;
        }

        offset = sizeEnd + 2;
        if (chunkSize == 0) {
            return decoded;
        }
        if (offset + chunkSize + 2 > body.size()) {
            return std::nullopt;
        }

        decoded.append(body.substr(offset, chunkSize));
        offset += chunkSize;
        if (body.substr(offset, 2) != "\r\n") {
            return std::nullopt;
        }
        offset += 2;
    }

    return std::nullopt;
}

auto fetchWithGioTls() -> std::string {
    GError* error = nullptr;
    auto* client = g_socket_client_new();
    g_socket_client_set_tls(client, true);
    g_socket_client_set_timeout(client, 15);

    auto* connection = g_socket_client_connect_to_host(client, GITHUB_API_HOST, 443, nullptr, &error);
    g_object_unref(client);
    if (error) {
        const std::string message = error->message;
        g_error_free(error);
        throw std::runtime_error(message);
    }

    auto* stream = G_IO_STREAM(connection);
    auto* output = g_io_stream_get_output_stream(stream);
    const std::string request = std::string{"GET "} + GITHUB_LATEST_RELEASE_PATH +
                                " HTTP/1.1\r\n"
                                "Host: " +
                                GITHUB_API_HOST +
                                "\r\n"
                                "User-Agent: VertexNote\r\n"
                                "Accept: application/vnd.github+json\r\n"
                                "Connection: close\r\n\r\n";

    gsize bytesWritten = 0;
    if (!g_output_stream_write_all(output, request.data(), request.size(), &bytesWritten, nullptr, &error)) {
        const std::string message = error ? error->message : "Could not write HTTP request.";
        if (error) {
            g_error_free(error);
        }
        g_object_unref(connection);
        throw std::runtime_error(message);
    }

    std::string response;
    auto* input = g_io_stream_get_input_stream(stream);
    std::array<char, 4096> buffer{};
    while (true) {
        const auto read = g_input_stream_read(input, buffer.data(), buffer.size(), nullptr, &error);
        if (read < 0) {
            const std::string message = error ? error->message : "Could not read HTTP response.";
            if (error) {
                g_error_free(error);
            }
            g_object_unref(connection);
            throw std::runtime_error(message);
        }
        if (read == 0) {
            break;
        }
        response.append(buffer.data(), static_cast<std::size_t>(read));
    }
    g_object_unref(connection);

    const auto headerEnd = response.find("\r\n\r\n");
    if (headerEnd == std::string::npos) {
        throw std::runtime_error("GitHub returned an invalid HTTP response.");
    }

    const auto headers = response.substr(0, headerEnd);
    auto body = response.substr(headerEnd + 4);
    if (headers.find(" 200 ") == std::string::npos) {
        throw std::runtime_error("GitHub returned a non-success HTTP response.");
    }

    if (lower(headers).find("transfer-encoding: chunked") != std::string::npos) {
        auto decoded = decodeChunkedBody(body);
        if (!decoded) {
            throw std::runtime_error("GitHub returned a chunked response VertexNote could not decode.");
        }
        body = std::move(*decoded);
    }

    return body;
}

#endif

}  // namespace

auto fetchLatestReleaseJson() -> std::string {
#ifdef _WIN32
    return fetchWithWinHttp();
#else
    return fetchWithGioTls();
#endif
}

}  // namespace vn::update
