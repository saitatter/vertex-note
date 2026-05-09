/*
 * VertexNote
 *
 * Experimental Qt update presentation service.
 */

#pragma once

#include "ui/common/IUpdatePresentationService.h"

class QStatusBar;
class QWidget;

class QtExperimentalUpdatePresentationService: public vn::ui::common::IUpdatePresentationService {
public:
    QtExperimentalUpdatePresentationService(QWidget* parent, QStatusBar* statusBar);

public:
    void showCheckingForUpdates() override;
    void showUpdateAvailable(const vn::ui::common::UpdateReleaseSummary& release) override;
    void showUpToDate(std::string_view currentVersion) override;
    void showUpdateError(std::string_view message) override;

private:
    QWidget* parent = nullptr;
    QStatusBar* statusBar = nullptr;
};
