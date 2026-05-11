#include "PopplerGlibPageBookmarkIterator.h"

#include <string>
#include <utility>

#include <Outline.h>
#include <UTF.h>

#include "pdf/popplerapi/PopplerGlibAction.h"

class PdfAction;

namespace {

auto outlineTitleToUtf8(const std::vector<Unicode>& title) -> std::string {
    std::string utf16;
    utf16.reserve(title.size() * 2U);
    for (const auto codepoint: title) {
        if (codepoint <= 0xffffU) {
            utf16.push_back(static_cast<char>((codepoint >> 8U) & 0xffU));
            utf16.push_back(static_cast<char>(codepoint & 0xffU));
        } else if (codepoint <= 0x10ffffU) {
            const auto value = codepoint - 0x10000U;
            const auto high = static_cast<unsigned int>(0xd800U + ((value >> 10U) & 0x3ffU));
            const auto low = static_cast<unsigned int>(0xdc00U + (value & 0x3ffU));
            utf16.push_back(static_cast<char>((high >> 8U) & 0xffU));
            utf16.push_back(static_cast<char>(high & 0xffU));
            utf16.push_back(static_cast<char>((low >> 8U) & 0xffU));
            utf16.push_back(static_cast<char>(low & 0xffU));
        }
    }
    prependUnicodeByteOrderMark(utf16);
    return TextStringToUtf8(utf16);
}

}  // namespace

PopplerGlibPageBookmarkIterator::PopplerGlibPageBookmarkIterator(const std::vector<OutlineItem*>* items,
                                                                 std::shared_ptr<PDFDoc> document):
        items(items), document(std::move(document)) {}

PopplerGlibPageBookmarkIterator::~PopplerGlibPageBookmarkIterator() = default;

auto PopplerGlibPageBookmarkIterator::current() const -> OutlineItem* {
    if (!items || index >= items->size()) {
        return nullptr;
    }
    return (*items)[index];
}

auto PopplerGlibPageBookmarkIterator::next() -> bool {
    if (!items || index + 1U >= items->size()) {
        return false;
    }
    ++index;
    return true;
}

auto PopplerGlibPageBookmarkIterator::isOpen() -> bool {
    auto* item = current();
    return item ? item->isOpen() : false;
}

auto PopplerGlibPageBookmarkIterator::getChildIter() -> PdfBookmarkIterator* {
    auto* item = current();
    if (!item || !item->hasKids()) {
        return nullptr;
    }

    const auto* children = item->getKids();
    if (!children || children->empty()) {
        return nullptr;
    }

    return new PopplerGlibPageBookmarkIterator(children, document);
}

auto PopplerGlibPageBookmarkIterator::getAction() -> PdfAction* {
    auto* item = current();
    if (!item || !item->getAction()) {
        return nullptr;
    }

    return new PopplerGlibAction(item->getAction(), document, outlineTitleToUtf8(item->getTitle()));
}
