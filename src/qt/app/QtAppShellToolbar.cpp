/*
 * VertexNote
 *
 * Qt app shell toolbar assembly.
 */

#include "QtAppShell.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include <QAction>
#include <QString>
#include <QToolBar>

#include "QtToolbarLayoutEngine.h"

namespace {

constexpr auto TOP_1_KEY = "toolbarTop1";
constexpr auto TOP_2_KEY = "toolbarTop2";
constexpr auto BOTTOM_1_KEY = "toolbarBottom1";
constexpr auto LEFT_1_KEY = "toolbarLeft1";
constexpr auto LEFT_2_KEY = "toolbarLeft2";
constexpr auto RIGHT_1_KEY = "toolbarRight1";

auto tokenGroupContains(const std::vector<std::vector<std::string>*>& groups, const std::string& token) -> bool {
    return std::ranges::any_of(groups, [&token](const auto* group) {
        return std::ranges::find(*group, token) != group->end();
    });
}

auto tokenGroupContainsAny(const std::vector<std::vector<std::string>*>& groups,
                           std::initializer_list<std::string_view> tokens) -> bool {
    return std::ranges::any_of(tokens, [&groups](std::string_view token) {
        return tokenGroupContains(groups, std::string(token));
    });
}

auto isDrawFamilyToken(const std::string& token) -> bool {
    return token == "DRAW" || token == "DRAW_STROKE" || token == "DRAW_VERTEX";
}

auto isFileFamilyToken(const std::string& token) -> bool {
    return token == "SAVE" || token == "NEW" || token == "OPEN" || token == "SAVEPDF" || token == "PRINT";
}

auto isEditFamilyToken(const std::string& token) -> bool {
    return token == "CUT" || token == "COPY" || token == "PASTE" || token == "UNDO" || token == "REDO" ||
           token == "DELETE" || token == "SEARCH";
}

auto isNavigationFamilyToken(const std::string& token) -> bool {
    return token == "GOTO_FIRST" || token == "GOTO_BACK" || token == "NAVIGATE_BACK" ||
           token == "NAVIGATE_FORWARD" || token == "GOTO_NEXT_ANNOTATED_PAGE" || token == "GOTO_NEXT" ||
           token == "GOTO_LAST" || token == "GOTO_PAGE";
}

auto isPageFamilyToken(const std::string& token) -> bool {
    return token == "INSERT_NEW_PAGE" || token == "DELETE_CURRENT_PAGE" || token == "PAGE_SPIN" || token == "LAYER";
}

auto isToolFamilyToken(const std::string& token) -> bool {
    return token == "PEN" || token == "ERASER" || token == "HIGHLIGHTER" || token == "HILIGHTER" ||
           token == "LASER_POINTER" || token == "PDF_TOOL" || token == "SELECT" || token == "VERTICAL_SPACE" ||
           token == "HAND" || token == "SETSQUARE" || token == "COMPASS" || token == "DEFAULT_TOOL";
}

auto isInsertFamilyToken(const std::string& token) -> bool {
    return token == "IMAGE" || token == "TEXT" || token == "MATH_TEX" || token == "SELECT_FONT";
}

auto isGeometryFamilyToken(const std::string& token) -> bool {
    return token == "GEOMETRY_SELECT_VERTEX" || token == "GEOMETRY_SELECT_EDGE" ||
           token == "GEOMETRY_SELECT_FACE" || token == "GEOMETRY_SELECT_OBJECT" ||
           token == "GEOMETRY_TRANSFORM" || token == "VERTEX_TRANSFORM" ||
           token == "GEOMETRY_TRANSLATE" || token == "GEOMETRY_ROTATE" || token == "GEOMETRY_SCALE" ||
           token == "GEOMETRY_WELD" || token == "GEOMETRY_DETACH" || token == "GEOMETRY_FILL_FACE" ||
           token == "GEOMETRY_DELETE_FACE" || token == "GEOMETRY_SPLIT_FACE" ||
           token == "GEOMETRY_TRIANGULATE_FACE" || token == "GEOMETRY_3D_VERTEX" || token == "GEOMETRY_3D_BOX" ||
           token == "GEOMETRY_PROJECT_ISO" || token == "GEOMETRY_PROJECT_FRONT" ||
           token == "GEOMETRY_PROJECT_TOP" || token == "GEOMETRY_Z_UP" || token == "GEOMETRY_Z_DOWN" ||
           token == "GEOMETRY_PANEL" || token == "GEOMETRY_VIEW_WIREFRAME" || token == "GEOMETRY_VIEW_VERTICES" ||
           token == "GEOMETRY_VIEW_LINKED" || token == "GEOMETRY_VIEW_FACES";
}

auto isSnapFamilyToken(const std::string& token) -> bool {
    return token == "ROTATION_SNAPPING" || token == "GRID_SNAPPING" ||
           token == "VERTEXNOTE_GEOMETRY_SNAPPING" || token == "VERTEXNOTE_GRID_SNAPPING";
}

auto isStyleFamilyToken(const std::string& token) -> bool {
    return token == "PLAIN" || token == "DASHED" || token == "DASH-DOTTED" || token == "DASH-/ DOTTED" ||
           token == "DOTTED" || token == "VERY_FINE" || token == "FINE" || token == "MEDIUM" ||
           token == "THICK" || token == "VERY_THICK" || token == "TOOL_FILL" || token == "FILL_OPACITY";
}

auto isColorFamilyToken(const std::string& token) -> bool {
    return token == "COLOR_SELECT" || token.rfind("COLOR(", 0) == 0;
}

template <typename Predicate>
void insertLabelBeforeFirst(std::vector<std::string>& tokens, const std::string& labelToken, Predicate predicate) {
    if (std::ranges::find(tokens, labelToken) != tokens.end()) {
        return;
    }
    const auto it = std::ranges::find_if(tokens, predicate);
    if (it == tokens.end()) {
        return;
    }
    tokens.insert(it, labelToken);
}

void insertDocumentToolbarGroupLabels(std::vector<std::string>& tokens) {
    insertLabelBeforeFirst(tokens, "GROUP_FILE", isFileFamilyToken);
    insertLabelBeforeFirst(tokens, "GROUP_EDIT", isEditFamilyToken);
    insertLabelBeforeFirst(tokens, "GROUP_NAV", isNavigationFamilyToken);
    insertLabelBeforeFirst(tokens, "GROUP_PAGE", isPageFamilyToken);
}

void insertToolsToolbarGroupLabels(std::vector<std::string>& tokens) {
    insertLabelBeforeFirst(tokens, "GROUP_TOOLS", isToolFamilyToken);
    insertLabelBeforeFirst(tokens, "GROUP_INSERT", isInsertFamilyToken);
    insertLabelBeforeFirst(tokens, "GROUP_DRAW", isDrawFamilyToken);
    insertLabelBeforeFirst(tokens, "GROUP_GEOMETRY", isGeometryFamilyToken);
    insertLabelBeforeFirst(tokens, "GROUP_SNAP", isSnapFamilyToken);
    insertLabelBeforeFirst(tokens, "GROUP_STYLE", isStyleFamilyToken);
    insertLabelBeforeFirst(tokens, "GROUP_COLOR", isColorFamilyToken);
}

}  // namespace

void QtAppShell::rebuildToolbar() {
    auto* documentToolBar = this->window.mainToolBar();
    auto* toolsToolBar = this->window.toolsToolBar();
    auto* footerToolBar = this->window.footerToolBar();
    auto* leftPrimaryToolBar = this->window.leftPrimaryToolBar();
    auto* leftSecondaryToolBar = this->window.leftSecondaryToolBar();
    auto* rightPrimaryToolBar = this->window.rightPrimaryToolBar();

    documentToolBar->clear();
    toolsToolBar->clear();
    footerToolBar->clear();
    leftPrimaryToolBar->clear();
    leftSecondaryToolBar->clear();
    rightPrimaryToolBar->clear();
    for (auto* floatingToolBar: this->window.floatingToolBars()) {
        floatingToolBar->clear();
    }

    resetToolbarWidgetState();
    loadActiveToolbarProfile();
    configureToolbarChrome();

    auto top1Tokens = toolbarItemsFor(TOP_1_KEY,
                                      {"SAVE", "NEW", "OPEN", "SEPARATOR", "SAVEPDF", "PRINT", "SEPARATOR",
                                       "CUT", "COPY", "PASTE", "SEPARATOR", "UNDO", "REDO", "SEPARATOR",
                                       "GOTO_FIRST", "GOTO_BACK", "GOTO_NEXT_ANNOTATED_PAGE", "GOTO_NEXT",
                                       "GOTO_LAST", "INSERT_NEW_PAGE", "DELETE_CURRENT_PAGE", "SEPARATOR",
                                       "FULLSCREEN", "SEPARATOR", "AUDIO_RECORDING", "AUDIO_SEEK_BACKWARDS",
                                       "AUDIO_PAUSE_PLAYBACK", "AUDIO_SEEK_FORWARDS", "AUDIO_STOP_PLAYBACK",
                                       "SEPARATOR", "SELECT_FONT"});
    auto top2Tokens = toolbarItemsFor(TOP_2_KEY,
                                      {"PEN", "ERASER", "HILIGHTER", "LASER_POINTER", "IMAGE", "TEXT",
                                       "MATH_TEX", "DRAW_STROKE", "DRAW_VERTEX", "GEOMETRY_TRANSFORM", "SEPARATOR", "ROTATION_SNAPPING",
                                       "GRID_SNAPPING", "VERTEXNOTE_GEOMETRY_SNAPPING", "VERTEXNOTE_GRID_SNAPPING",
                                       "TOGGLE_TOUCH_DRAWING", "SEPARATOR", "SELECT", "VERTICAL_SPACE", "HAND",
                                       "SEPARATOR", "DEFAULT_TOOL", "SEPARATOR", "VERY_FINE", "FINE", "MEDIUM",
                                       "THICK", "VERY_THICK", "SEPARATOR", "TOOL_FILL", "SEPARATOR", "COLOR(0)",
                                       "COLOR(1)", "COLOR(2)", "COLOR(3)", "COLOR(4)", "COLOR(5)", "COLOR(6)",
                                       "COLOR(7)", "COLOR(8)", "COLOR(9)", "COLOR(10)", "COLOR_SELECT"});
    auto bottomTokens = toolbarItemsFor(BOTTOM_1_KEY,
                                        {"PAGE_SPIN", "SEPARATOR", "LAYER", "SPACER", "PAIRED_PAGES",
                                         "PRESENTATION_MODE", "ZOOM_100", "ZOOM_FIT", "ZOOM_OUT", "ZOOM_SLIDER",
                                         "ZOOM_IN"});
    auto left1Tokens = toolbarItemsFor(LEFT_1_KEY,
                                       {"SAVEPDF", "PRINT", "SEARCH", "DELETE", "SETSQUARE", "COMPASS",
                                        "MANAGE_TOOLBAR", "CUSTOMIZE_TOOLBAR", "GOTO_PAGE",
                                        "SELECT_PDF_TEXT_LINEAR", "PDF_TOOL", "SELECT_PDF_TEXT_RECT",
                                        "SHAPE_RECOGNIZER", "DRAW_RECTANGLE", "DRAW_ELLIPSE", "DRAW_ARROW",
                                        "DRAW_DOUBLE_ARROW", "DRAW_COORDINATE_SYSTEM", "RULER", "DRAW_SPLINE"});
    auto left2Tokens = toolbarItemsFor(LEFT_2_KEY,
                                       {"SELECT_REGION", "SELECT_RECTANGLE", "SELECT_OBJECT", "PLAY_OBJECT",
                                        "GOTO_PREVIOUS_LAYER", "GOTO_NEXT_LAYER", "GOTO_TOP_LAYER", "FILL_OPACITY"});
    auto rightToolbarTokens = toolbarItemsFor(RIGHT_1_KEY, {});

    std::vector<std::vector<std::string>*> toolbarTokenGroups = {
            &top1Tokens, &top2Tokens, &bottomTokens, &left1Tokens, &left2Tokens, &rightToolbarTokens};
    std::vector<std::vector<std::string>> floatingToolbarTokens;
    floatingToolbarTokens.reserve(this->window.floatingToolBars().size());
    for (int floatingIndex = 0; floatingIndex < static_cast<int>(this->window.floatingToolBars().size()); ++floatingIndex) {
        const auto key = "toolbarfloat" + std::to_string(floatingIndex + 1);
        floatingToolbarTokens.push_back(toolbarItemsFor(key, {}));
        toolbarTokenGroups.push_back(&floatingToolbarTokens.back());
    }

    const bool hasSelectionFamily = tokenGroupContains(toolbarTokenGroups, "SELECT");
    const bool hasDrawingFamily =
            tokenGroupContainsAny(toolbarTokenGroups, {"DRAW", "DRAW_STROKE", "DRAW_VERTEX"});
    const bool hasPdfFamily = tokenGroupContains(toolbarTokenGroups, "PDF_TOOL");

    const auto pruneRedundantFamilyTokens = [&](std::vector<std::string>& tokens) {
        if (!this->activeToolbarProfile) {
            return;
        }

        auto redundant = [&](const std::string& token) {
            if (hasSelectionFamily &&
                (token == "SELECT_REGION" || token == "SELECT_RECTANGLE" || token == "SELECT_MULTILAYER_REGION" ||
                 token == "SELECT_MULTILAYER_RECTANGLE" || token == "SELECT_OBJECT")) {
                return true;
            }

            if (hasDrawingFamily &&
                (token == "DRAW_RECTANGLE" || token == "DRAW_ELLIPSE" || token == "DRAW_ARROW" ||
                 token == "DRAW_DOUBLE_ARROW" || token == "DRAW_COORDINATE_SYSTEM" || token == "DRAW_SPLINE" ||
                 token == "SHAPE_RECOGNIZER")) {
                return true;
            }

            if (hasPdfFamily && (token == "SELECT_PDF_TEXT_LINEAR" || token == "SELECT_PDF_TEXT_RECT")) {
                return true;
            }

            return false;
        };

        tokens.erase(std::remove_if(tokens.begin(), tokens.end(), redundant), tokens.end());
    };

    for (auto* group: toolbarTokenGroups) {
        *group = QtToolbarLayoutEngine::expandTokenAliases(*group);
        *group = QtToolbarLayoutEngine::normalizeQtDrawingFamilies(*group);
        pruneRedundantFamilyTokens(*group);
    }

    const auto ensureSnapButtonsNearDrawing = [&]() {
        auto drawAnchor = std::find_if(top2Tokens.rbegin(), top2Tokens.rend(), [](const std::string& token) {
            return token == "DRAW_VERTEX" || token == "DRAW_STROKE" || token == "DRAW";
        });
        auto insertIndex = drawAnchor == top2Tokens.rend()
                                   ? top2Tokens.size()
                                   : static_cast<std::size_t>(std::distance(top2Tokens.begin(), drawAnchor.base()));

        const auto insertTop2Token = [&](const std::string& token, std::size_t& index) {
            top2Tokens.insert(top2Tokens.begin() + static_cast<std::ptrdiff_t>(index), token);
            ++index;
        };

        bool inserted = false;
        if (!tokenGroupContainsAny(toolbarTokenGroups, {"GRID_SNAPPING", "VERTEXNOTE_GRID_SNAPPING"})) {
            insertTop2Token("GRID_SNAPPING", insertIndex);
            inserted = true;
        }
        if (!tokenGroupContains(toolbarTokenGroups, "VERTEXNOTE_GEOMETRY_SNAPPING")) {
            insertTop2Token("VERTEXNOTE_GEOMETRY_SNAPPING", insertIndex);
            inserted = true;
        }
        if (inserted && insertIndex < top2Tokens.size() && top2Tokens[insertIndex] != "SEPARATOR") {
            insertTop2Token("SEPARATOR", insertIndex);
        }
    };
    ensureSnapButtonsNearDrawing();

    const auto ensureGeometryTransformNearDrawing = [&]() {
        if (tokenGroupContainsAny(toolbarTokenGroups, {"GEOMETRY_TRANSFORM", "VERTEX_TRANSFORM"})) {
            return;
        }
        auto drawAnchor = std::find_if(top2Tokens.rbegin(), top2Tokens.rend(), [](const std::string& token) {
            return token == "DRAW_VERTEX" || token == "DRAW_STROKE" || token == "DRAW";
        });
        const auto insertIndex = drawAnchor == top2Tokens.rend()
                                         ? top2Tokens.size()
                                         : static_cast<std::size_t>(std::distance(top2Tokens.begin(), drawAnchor.base()));
        top2Tokens.insert(top2Tokens.begin() + static_cast<std::ptrdiff_t>(insertIndex), "GEOMETRY_TRANSFORM");
    };
    ensureGeometryTransformNearDrawing();

    const auto ensureWorkspaceSelector = [&]() {
        if (tokenGroupContains(toolbarTokenGroups, "WORKSPACE")) {
            return;
        }
        top1Tokens.insert(top1Tokens.begin(), "SEPARATOR");
        top1Tokens.insert(top1Tokens.begin(), "WORKSPACE");
    };
    ensureWorkspaceSelector();

    insertDocumentToolbarGroupLabels(top1Tokens);
    insertToolsToolbarGroupLabels(top1Tokens);
    insertDocumentToolbarGroupLabels(top2Tokens);
    insertToolsToolbarGroupLabels(top2Tokens);

    for (const auto& token: top1Tokens) {
        addToolbarToken(documentToolBar, token);
    }
    for (const auto& token: top2Tokens) {
        addToolbarToken(toolsToolBar, token);
    }
    for (const auto& token: bottomTokens) {
        addToolbarToken(footerToolBar, token);
    }
    for (const auto& token: left1Tokens) {
        addToolbarToken(leftPrimaryToolBar, token);
    }
    for (const auto& token: left2Tokens) {
        addToolbarToken(leftSecondaryToolBar, token);
    }
    for (const auto& token: rightToolbarTokens) {
        addToolbarToken(rightPrimaryToolBar, token);
    }

    for (int floatingIndex = 0; floatingIndex < static_cast<int>(this->window.floatingToolBars().size()); ++floatingIndex) {
        auto* floatingToolBar = this->window.floatingToolBars()[static_cast<std::size_t>(floatingIndex)];
        const auto& tokens = floatingToolbarTokens[static_cast<std::size_t>(floatingIndex)];
        for (const auto& token: tokens) {
            addToolbarToken(floatingToolBar, token);
        }

        if (!tokens.empty()) {
            floatingToolBar->setWindowTitle(QStringLiteral("Floating Toolbar %1").arg(floatingIndex + 1));
        }
    }

    const bool showToolbar = !this->presentationMode &&
                             this->window.commandHost()->actionForCommand("view.show-toolbar") &&
                             this->window.commandHost()->actionForCommand("view.show-toolbar")->isChecked();
    rightPrimaryToolBar->setVisible(!rightToolbarTokens.empty() && showToolbar);
    syncFloatingToolBarsVisibility(showToolbar);
    this->window.cascadeFloatingToolBars();

    syncToolbarWidgets();
    syncFooterWidgets();
}
