#include "util/VersionInfo.h"

#include <sstream>

#include <cairo.h>
#include <glib.h>

#include "config-git.h"
#include "config.h"
#include "util/raii/CStringWrapper.h"

namespace xoj::util {
const char* getGdkBackend() { return nullptr; }

std::string getVertexNoteVersion() {
    auto str = std::string(PROJECT_NAME) + " " + PROJECT_VERSION;
    if (!std::string(GIT_COMMIT_ID).empty()) {
        str = str + " (" + GIT_COMMIT_ID + " from " + GIT_ORIGIN_OWNER + "/" + GIT_BRANCH + ")";
    }
    return str;
}

std::string getOsInfo() {
    auto osInfo = vn::util::OwnedCString::assumeOwnership(g_get_os_info(G_OS_INFO_KEY_NAME));
    if (!osInfo) {
        osInfo = vn::util::OwnedCString::assumeOwnership(g_get_os_info(G_OS_INFO_KEY_PRETTY_NAME));
    }
    if (osInfo) {
        vn::util::OwnedCString osVersion;
        for (auto key: {G_OS_INFO_KEY_VERSION, G_OS_INFO_KEY_VERSION_ID, G_OS_INFO_KEY_VERSION_CODENAME}) {
            osVersion = vn::util::OwnedCString::assumeOwnership(g_get_os_info(key));
            if (osVersion) {
                break;
            }
        }
        if (osVersion) {
            return std::string(osInfo.get()) + " " + osVersion.get();
        } else {
            return std::string(osInfo.get());
        }
    }
    return std::string();
}


std::string getVersionInfo() {
    std::stringstream str;
    str.imbue(std::locale::classic());

    str << getVertexNoteVersion() << std::endl;

    str << "├──shell: Qt" << std::endl;
    str << "├──glib: " << glib_major_version << "." << glib_minor_version << "." << glib_micro_version << std::endl;
    str << "├──cairo:  " << cairo_version_string() << std::endl;

    str << "└──OS info: " << getOsInfo() << std::endl;

    return str.str();
}
};  // namespace vn::util
