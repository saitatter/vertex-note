#include "PageTypeHandler.h"

#include <algorithm>
#include <iostream>
#include <string_view>
#include <utility>

#include <QSettings>

#include "util/PathUtil.h"
#include "util/StringUtils.h"
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

PageTypeHandler::PageTypeHandler(GladeSearchpath* gladeSearchPath) {
    (void) gladeSearchPath;
    addFallbackPageTypes(types);

    // Special types
    addPageTypeInfo(_("With PDF background"), PageTypeFormat::Pdf, "", specialTypes);
    addPageTypeInfo(_("Image"), PageTypeFormat::Image, "", specialTypes);
}

PageTypeHandler::~PageTypeHandler() = default;

auto PageTypeHandler::parseIni(fs::path const& filepath) -> bool {
    if (!fs::exists(filepath)) {
        return false;
    }

    QSettings config(QString::fromUtf8(Util::toGFilename(filepath).c_str()), QSettings::IniFormat);
    if (config.status() != QSettings::NoError) {
        return false;
    }
    for (const auto& group: config.childGroups()) { loadFormat(config, group); }
    return true;
}

void PageTypeHandler::loadFormat(QSettings& config, const QString& group) {
    config.beginGroup(group);
    std::string strName = config.value("name").toString().toStdString();
    std::string strFormat = config.value("format").toString().toStdString();
    std::string strConfig = config.value("config").toString().toStdString();
    config.endGroup();
    addPageTypeInfo(strName, getPageTypeFormatForString(strFormat), strConfig, types);
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
