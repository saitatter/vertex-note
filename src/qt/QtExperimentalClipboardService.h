/*
 * VertexNote
 *
 * Experimental Qt clipboard service.
 */

#pragma once

#include "ui/common/IClipboardService.h"

class QtExperimentalClipboardService: public vn::ui::common::IClipboardService {
public:
    void setText(std::string_view text) override;
    [[nodiscard]] auto text() const -> std::string override;
    [[nodiscard]] auto hasText() const -> bool override;
    void clear() override;
};
