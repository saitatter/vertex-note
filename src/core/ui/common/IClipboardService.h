/*
 * VertexNote
 *
 * Platform-neutral clipboard contract.
 */

#pragma once

#include <string>
#include <string_view>

namespace vn::ui::common {

class IClipboardService {
public:
    virtual ~IClipboardService() = default;

    virtual void setText(std::string_view text) = 0;
    [[nodiscard]] virtual auto text() const -> std::string = 0;
    [[nodiscard]] virtual auto hasText() const -> bool = 0;
    virtual void clear() = 0;
};

}  // namespace vn::ui::common
