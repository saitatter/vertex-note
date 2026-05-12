#include "PageTypeHandler.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <iostream>
#include <string_view>
#include <utility>

#include "config-paths.h"
#include "util/PathUtil.h"
#include "util/i18n.h"

static void addPageTypeInfo(const std::string& name, PageTypeFormat format, const std::string& config,
                            std::vector<std::unique_ptr<PageTypeInfo>>& types) {
    auto pt = std::make_unique<PageTypeInfo>();
    pt->name = std::move(name);
    pt->page.format = format;
    pt->page.config = std::move(config);

    types.emplace_back(std::move(pt));
}

static void addFallbackPageTypes(std::vector<std::unique_ptr<PageTypeInfo>>& types) {
    addPageTypeInfo(_("Plain"), PageTypeFormat::Plain, "", types);
    addPageTypeInfo(_("Ruled"), PageTypeFormat::Ruled, "", types);
    addPageTypeInfo(_("Ruled with vertical line"), PageTypeFormat::Lined, "", types);
    addPageTypeInfo(_("Staves"), PageTypeFormat::Staves, "", types);
    addPageTypeInfo(_("Graph"), PageTypeFormat::Graph, "", types);
    addPageTypeInfo(_("Dotted"), PageTypeFormat::Dotted, "", types);
    addPageTypeInfo(_("Isometric Dotted"), PageTypeFormat::IsoDotted, "", types);
    addPageTypeInfo(_("Isometric Graph"), PageTypeFormat::IsoGraph, "", types);
}

static auto trim(std::string_view value) -> std::string_view {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0) {
        value.remove_prefix(1);
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) {
        value.remove_suffix(1);
    }
    return value;
}

static auto resolvePageTemplatesFile() -> fs::path {
    const std::array candidates{
            Util::getDataPath() / "resources-templates" / "pagetemplates.ini",
            Util::getDataPath() / "resources-templates" / "pagetemplates.ini.in",
            Util::getDataPath() / "pagetemplates.ini",
            fs::path(PROJECT_SOURCE_DIR) / "resources-templates" / "pagetemplates.ini",
            fs::path(PROJECT_SOURCE_DIR) / "resources-templates" / "pagetemplates.ini.in",
    };
    for (const auto& candidate: candidates) {
        if (fs::exists(candidate)) {
            return candidate;
        }
    }
    return candidates.back();
}

PageTypeHandler::PageTypeHandler(GladeSearchpath* gladeSearchPath) {
    (void) gladeSearchPath;
    if (!parseIni(resolvePageTemplatesFile()) || this->types.size() < 5) {
        this->types.clear();
        addFallbackPageTypes(types);
    }

    // Special types
    addPageTypeInfo(_("With PDF background"), PageTypeFormat::Pdf, "", specialTypes);
    addPageTypeInfo(_("Image"), PageTypeFormat::Image, "", specialTypes);
}

PageTypeHandler::~PageTypeHandler() = default;

auto PageTypeHandler::parseIni(fs::path const& filepath) -> bool {
    if (!fs::exists(filepath)) {
        return false;
    }

    std::ifstream input(filepath);
    if (!input) {
        return false;
    }

    bool loadedAny = false;
    std::string name;
    std::string format;
    std::string config;
    auto flushGroup = [&]() {
        if (!name.empty() && !format.empty()) {
            loadFormat(name, format, config);
            loadedAny = true;
        }
        name.clear();
        format.clear();
        config.clear();
    };

    std::string line;
    while (std::getline(input, line)) {
        const auto trimmedLine = trim(line);
        if (trimmedLine.empty() || trimmedLine.front() == '#') {
            continue;
        }
        if (trimmedLine.front() == '[' && trimmedLine.back() == ']') {
            flushGroup();
            continue;
        }

        const auto separator = trimmedLine.find('=');
        if (separator == std::string_view::npos) {
            continue;
        }
        const auto key = trim(trimmedLine.substr(0, separator));
        const auto value = trim(trimmedLine.substr(separator + 1));
        if (key == "name") {
            name = std::string(value);
        } else if (key == "format") {
            format = std::string(value);
        } else if (key == "config") {
            config = std::string(value);
        }
    }
    flushGroup();
    return loadedAny;
}

void PageTypeHandler::loadFormat(const std::string& name, const std::string& format, const std::string& config) {
    addPageTypeInfo(name, getPageTypeFormatForString(format), config, types);
}

auto PageTypeHandler::getPageTypes() -> const std::vector<std::unique_ptr<PageTypeInfo>>& { return this->types; }
auto PageTypeHandler::getSpecialPageTypes() -> const std::vector<std::unique_ptr<PageTypeInfo>>& {
    return this->specialTypes;
}

auto PageTypeHandler::getInfoOn(const PageType& pt) const -> const PageTypeInfo* {
    const auto& vector = pt.isSpecial() ? specialTypes : types;
    auto it = std::find_if(vector.begin(), vector.end(), [&](const auto& info) { return info->page == pt; });
    return it == vector.end() ? nullptr : it->get();
}

auto PageTypeHandler::getPageTypeFormatForString(std::string_view format) -> PageTypeFormat {
    if (format == "plain") {
        return PageTypeFormat::Plain;
    }
    if (format == "ruled") {
        return PageTypeFormat::Ruled;
    }
    if (format == "lined") {
        return PageTypeFormat::Lined;
    }
    if (format == "staves") {
        return PageTypeFormat::Staves;
    }
    if (format == "graph") {
        return PageTypeFormat::Graph;
    }
    if (format == "dotted") {
        return PageTypeFormat::Dotted;
    }
    if (format == "isodotted") {
        return PageTypeFormat::IsoDotted;
    }
    if (format == "isograph") {
        return PageTypeFormat::IsoGraph;
    }
    if (format == ":pdf") {
        return PageTypeFormat::Pdf;
    }
    if (format == ":image") {
        return PageTypeFormat::Image;
    }
    std::cerr << "PageTypeHandler::getPageTypeFormatForString: unknown PageType: \"" << format
              << "\". Replacing with PageTypeFormat::Plain\n";
    return PageTypeFormat::Plain;
}

auto PageTypeHandler::getStringForPageTypeFormat(const PageTypeFormat& format) -> std::string {
    switch (format) {
        case PageTypeFormat::Plain:
            return "plain";
        case PageTypeFormat::Ruled:
            return "ruled";
        case PageTypeFormat::Lined:
            return "lined";
        case PageTypeFormat::Staves:
            return "staves";
        case PageTypeFormat::Graph:
            return "graph";
        case PageTypeFormat::Dotted:
            return "dotted";
        case PageTypeFormat::IsoDotted:
            return "isodotted";
        case PageTypeFormat::IsoGraph:
            return "isograph";
        case PageTypeFormat::Pdf:
            return ":pdf";
        case PageTypeFormat::Image:
            return ":image";
    }
    std::cerr << "PageTypeHandler::getStringForPageTypeFormat: unknown PageType: " << static_cast<int>(format)
              << ". Replacing with PageTypeFormat::Ruled\n";
    return "ruled";
}
