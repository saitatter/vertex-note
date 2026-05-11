#include "util/Util.h"

#include <array>    // for array
#include <cstdlib>  // for system
#include <string>   // for allocator, string
#include <utility>  // for move

#include "util/OutputStream.h"       // for OutputStream
#include "util/PlaceholderString.h"  // for PlaceholderString
#include "util/i18n.h"               // for FS, _F

#if defined(_MSC_VER)
#include <windows.h>
#else
#include <unistd.h>  // for getpid, pid_t
#endif


auto Util::getPid() -> PID {
#if defined(_MSC_VER)
    return GetCurrentProcessId();
#else
    return ::getpid();
#endif
}

void Util::writeCoordinateString(OutputStream* out, double xVal, double yVal) {
    std::array<char, G_ASCII_DTOSTR_BUF_SIZE> coordString{};
    g_ascii_formatd(coordString.data(), G_ASCII_DTOSTR_BUF_SIZE, Util::PRECISION_FORMAT_STRING, xVal);
    out->write(coordString.data());
    out->write(" ");
    g_ascii_formatd(coordString.data(), G_ASCII_DTOSTR_BUF_SIZE, Util::PRECISION_FORMAT_STRING, yVal);
    out->write(coordString.data());
}

void Util::systemWithMessage(const char* command) {
    if (auto errc = std::system(command); errc != 0) {
        std::string msg = FS(_F("Error {1} executing system command: {2}") % errc % command);
        g_warning("%s", msg.c_str());
    }
}

bool Util::isFlatpakInstallation() { return fs::exists("/.flatpak-info"); }
