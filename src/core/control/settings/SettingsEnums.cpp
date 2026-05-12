#include "SettingsEnums.h"

#include <iostream>  // for cerr

auto stylusCursorTypeFromString(const std::string& stylusCursorTypeStr) -> StylusCursorType {
    if (stylusCursorTypeStr == "none") {
        return STYLUS_CURSOR_NONE;
    }
    if (stylusCursorTypeStr == "dot") {
        return STYLUS_CURSOR_DOT;
    }
    if (stylusCursorTypeStr == "big") {
        return STYLUS_CURSOR_BIG;
    }
    if (stylusCursorTypeStr == "arrow") {
        return STYLUS_CURSOR_ARROW;
    }
    std::cerr << "Settings::Unknown stylus cursor type: " << stylusCursorTypeStr << std::endl;
    return STYLUS_CURSOR_DOT;
}

auto eraserVisibilityFromString(const std::string& eraserVisibility) -> EraserVisibility {
    if (eraserVisibility == "never") {
        return ERASER_VISIBILITY_NEVER;
    }
    if (eraserVisibility == "always") {
        return ERASER_VISIBILITY_ALWAYS;
    }
    if (eraserVisibility == "hover") {
        return ERASER_VISIBILITY_HOVER;
    }
    if (eraserVisibility == "touch") {
        return ERASER_VISIBILITY_TOUCH;
    }
    std::cerr << "Settings::Unknown eraser visibility: " << eraserVisibility << std::endl;
    return ERASER_VISIBILITY_ALWAYS;
}

auto iconThemeFromString(const std::string& iconThemeStr) -> IconTheme {
    if (iconThemeStr == "iconsColor") {
        return ICON_THEME_COLOR;
    }
    if (iconThemeStr == "iconsLucide") {
        return ICON_THEME_LUCIDE;
    }
    std::cerr << "Settings::Unknown icon theme: " << iconThemeStr << std::endl;
    return ICON_THEME_COLOR;
}

auto themeVariantFromString(const std::string& themeVariantStr) -> ThemeVariant {
    if (themeVariantStr == "useSystem") {
        return THEME_VARIANT_USE_SYSTEM;
    }
    if (themeVariantStr == "forceLight") {
        return THEME_VARIANT_FORCE_LIGHT;
    }
    if (themeVariantStr == "forceDark") {
        return THEME_VARIANT_FORCE_DARK;
    }
    std::cerr << "Settings::Unknown theme variant: " << themeVariantStr << std::endl;
    return THEME_VARIANT_USE_SYSTEM;
}

auto emptyLastPageAppendFromString(const std::string& str) -> EmptyLastPageAppendType {
    if (str == "disabled") {
        return EmptyLastPageAppendType::Disabled;
    }
    if (str == "onDrawOfLastPage") {
        return EmptyLastPageAppendType::OnDrawOfLastPage;
    }
    if (str == "onScrollOfLastPage") {
        return EmptyLastPageAppendType::OnScrollToEndOfLastPage;
    }

    std::cerr << "Settings::Unknown empty last page append type: " << str << std::endl;
    return EmptyLastPageAppendType::Disabled;
}
