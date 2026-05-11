#include "LinkDestination.h"

#include <utility>  // for move

#include "util/Util.h"  // for npos

LinkDestination::LinkDestination():
        page(npos),
        expand(false),
        left(0),
        top(0),
        zoom(0),
        changeLeft(false),
        changeZoom(false),
        changeTop(false),
        name(""),
        contents(UnknownType{}) {}

LinkDestination::~LinkDestination() = default;

auto LinkDestination::getPdfPage() const -> size_t { return this->page; }

void LinkDestination::setPdfPage(size_t page) { this->page = page; }

void LinkDestination::setExpand(bool expand) { this->expand = expand; }

auto LinkDestination::getExpand() const -> bool { return this->expand; }

auto LinkDestination::shouldChangeLeft() const -> bool { return changeLeft; }

auto LinkDestination::shouldChangeTop() const -> bool { return changeTop; }

auto LinkDestination::getZoom() const -> double { return zoom; }

auto LinkDestination::getLeft() const -> double { return left; }

auto LinkDestination::getTop() const -> double { return top; }

void LinkDestination::setChangeLeft(double left) {
    this->left = left;
    this->changeLeft = true;
}

void LinkDestination::setChangeZoom(double zoom) {
    this->zoom = zoom;
    this->changeZoom = true;
}

void LinkDestination::setChangeTop(double top) {
    this->top = top;
    this->changeTop = true;
}

void LinkDestination::setName(std::string name) { this->name = std::move(name); }
auto LinkDestination::getName() const -> const std::string& { return name; }

void LinkDestination::setURI(std::string uri) { this->contents = LinkType{std::move(uri)}; }

auto LinkDestination::isURI() const -> bool { return std::holds_alternative<LinkDestination::LinkType>(contents); }

auto LinkDestination::getURI() const -> std::optional<std::string> {
    if (auto* t = std::get_if<LinkDestination::LinkType>(&contents)) {
        return t->uri;
    } else {
        return std::nullopt;
    }
}
