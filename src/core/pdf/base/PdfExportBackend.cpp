#include "PdfExportBackend.h"

#include "util/i18n.h"

#include "config-features.h"

ExportBackend ExportBackend::fromString(const char* str) {
    return str != nullptr ? fromString(std::string_view(str)) : ExportBackend(ExportBackend::DEFAULT);
}

ExportBackend ExportBackend::fromString(std::string_view str) {
#ifdef ENABLE_QPDF
    if (str == "qpdf") {
        return ExportBackend::QPDF;
    }
#endif
    if (str != DEFAULT_ID_STRING && !str.empty()) {
        g_warning("%s", (_F("Unknown pdf backend: {1}. Available backends are: {2}. Using default backend.") % str %
                         listAvailableBackends())
                                .c_str());
    }
    return ExportBackend::DEFAULT;
}

const char* ExportBackend::listAvailableBackends() {
#ifdef ENABLE_QPDF
    static const char* availablePdfExportBackends = "default qpdf";
#else
    static const char* availablePdfExportBackends = "default";
#endif
    return availablePdfExportBackends;
}

std::vector<std::pair<const char*, const char*>> ExportBackend::getPrettyNamesOfAvailableBackends() {
    std::vector<std::pair<const char*, const char*>> res;
    res.emplace_back(DEFAULT_ID_STRING, _("Default"));
#ifdef ENABLE_QPDF
    res.emplace_back("qpdf", "QPDF");
#endif
    return res;
}
