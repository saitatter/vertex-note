#include "util/VersionInfo.h"

#include <sstream>

#include <QSysInfo>
#include <QString>

#include "config-git.h"
#include "config.h"

namespace xoj::util {
std::string getVertexNoteVersion() {
    auto str = std::string(PROJECT_NAME) + " " + PROJECT_VERSION;
    if (!std::string(GIT_COMMIT_ID).empty()) {
        str = str + " (" + GIT_COMMIT_ID + " from " + GIT_ORIGIN_OWNER + "/" + GIT_BRANCH + ")";
    }
    return str;
}

std::string getOsInfo() {
    const auto prettyName = QSysInfo::prettyProductName();
    if (!prettyName.isEmpty()) {
        return prettyName.toStdString();
    }
    return QSysInfo::productType().toStdString() + " " + QSysInfo::productVersion().toStdString();
}


std::string getVersionInfo() {
    std::stringstream str;
    str.imbue(std::locale::classic());

    str << getVertexNoteVersion() << std::endl;

    str << "├──shell: Qt" << std::endl;

    str << "└──OS info: " << getOsInfo() << std::endl;

    return str.str();
}
};  // namespace vn::util
