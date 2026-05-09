/*
 * VertexNote
 *
 * Page listener
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <memory>  // for shared_ptr, weak_ptr
#include <vector>

class Element;
class PageHandler;
class Range;
namespace xoj::util {
template <class T>
class Rectangle;
}  // namespace xoj::util

namespace vn {
namespace util = xoj::util;
}

class PageListener {
public:
    PageListener();
    virtual ~PageListener();

public:
    void registerToHandler(std::shared_ptr<PageHandler> const& handler);
    void unregisterFromHandler();

    virtual void rectChanged(vn::util::Rectangle<double>& rect) {}
    virtual void rangeChanged(Range& range) {}
    virtual void elementChanged(const Element* elem) {}
    virtual void elementsChanged(const std::vector<const Element*>& elements, const Range& range) {}
    virtual void pageChanged() {}

private:
    std::weak_ptr<PageHandler> handler;
};
