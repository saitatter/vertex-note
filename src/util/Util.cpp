#include "util/Util.h"

#include <array>    // for array
#include <charconv>  // for chars_format, to_chars
#include <cstdlib>  // for system
#include <iostream>  // for cerr
#include <stdexcept>  // for runtime_error
#include <string>   // for allocator, string
#include <string_view>  // for string_view
#include <utility>  // for move

#include "util/OutputStream.h"       // for OutputStream
#include "util/PlaceholderString.h"  // for PlaceholderString
#include "util/i18n.h"               // for FS, _F

#if defined(_MSC_VER)
#include <windows.h>
#else
#include <unistd.h>  // for getpid, pid_t
#endif

namespace {
void writeDouble(OutputStream* out, double value) {
    std::array<char, 64> coordString{};
    const auto [end, ec] =
            std::to_chars(coordString.data(), coordString.data() + coordString.size(), value, std::chars_format::general, 8);
    if (ec != std::errc{}) {
        throw std::runtime_error("Could not format coordinate");
    }
    out->write(std::string_view(coordString.data(), static_cast<std::size_t>(end - coordString.data())));
}
}  // namespace


auto Util::getPid() -> PID {
#if defined(_MSC_VER)
    return GetCurrentProcessId();
#else
    return ::getpid();
#endif
}

void Util::writeCoordinateString(OutputStream* out, double xVal, double yVal) {
    writeDouble(out, xVal);
    out->write(" ");
    writeDouble(out, yVal);
}

void Util::systemWithMessage(const char* command) {
    if (auto errc = std::system(command); errc != 0) {
        std::string msg = FS(_F("Error {1} executing system command: {2}") % errc % command);
        std::cerr << msg << std::endl;
    }
}

bool Util::isFlatpakInstallation() { return fs::exists("/.flatpak-info"); }
