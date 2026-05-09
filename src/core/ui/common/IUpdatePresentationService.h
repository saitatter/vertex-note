/*
 * VertexNote
 *
 * Platform-neutral updater presentation contract.
 */

#pragma once

#include <string_view>

#include "UiTypes.h"

namespace vn::ui::common {

class IUpdatePresentationService {
public:
    virtual ~IUpdatePresentationService() = default;

    virtual void showCheckingForUpdates() = 0;
    virtual void showUpdateAvailable(const UpdateReleaseSummary& release) = 0;
    virtual void showUpToDate(std::string_view currentVersion) = 0;
    virtual void showUpdateError(std::string_view message) = 0;
};

}  // namespace vn::ui::common
