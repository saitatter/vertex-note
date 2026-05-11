#include "PopplerPdfAction.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <utility>

#include <Link.h>
#include <PDFDoc.h>
#include <goo/GooString.h>

#include "model/LinkDestination.h"
#include "util/Util.h"

PopplerPdfAction::PopplerPdfAction(const LinkAction* action, std::shared_ptr<PDFDoc> document, std::string title):
        document(std::move(document)), title(std::move(title)) {
    destination = getDestination(action);
}

PopplerPdfAction::~PopplerPdfAction() = default;

auto PopplerPdfAction::getDestination(const LinkAction* action) -> std::shared_ptr<const LinkDestination> {
    auto dest = std::make_shared<LinkDestination>();
    dest->setName(getTitle());

    if (!action || !action->isOk()) {
        return dest;
    }

    switch (action->getKind()) {
        case actionURI: {
            const auto* uriAction = static_cast<const LinkURI*>(action);
            dest->setURI(uriAction->getURI());
            break;
        }
        case actionGoTo: {
            const auto* goToAction = static_cast<const LinkGoTo*>(action);
            if (const auto* target = goToAction->getDest()) {
                linkFromDest(*dest, target);
            } else if (document && goToAction->getNamedDest()) {
                const auto namedDest = document->findDest(goToAction->getNamedDest());
                linkFromDest(*dest, namedDest.get());
            }
            break;
        }
        default:
            break;
    }

    return dest;
}

auto PopplerPdfAction::getDestination() -> std::shared_ptr<const LinkDestination> { return destination; }

void PopplerPdfAction::linkFromDest(LinkDestination& link, const LinkDest* dest) {
    if (!document || !dest || !dest->isOk()) {
        return;
    }

    const int pageNumber = dest->isPageRef() ? document->findPage(dest->getPageRef()) : dest->getPageNum();
    if (pageNumber <= 0) {
        link.setPdfPage(npos);
        return;
    }

    const double pageWidth = document->getPageCropWidth(pageNumber);
    const double pageHeight = document->getPageCropHeight(pageNumber);

    if (dest->getChangeLeft()) {
        link.setChangeLeft(dest->getLeft());
    } else if (dest->getRight() != 0.0) {
        link.setChangeLeft(pageWidth - dest->getRight());
    }

    if (dest->getChangeTop()) {
        link.setChangeTop(pageHeight - std::min(pageHeight, dest->getTop()));
    } else if (dest->getBottom() != 0.0) {
        link.setChangeTop(pageHeight - std::min(pageHeight, pageHeight - dest->getBottom()));
    }

    if (dest->getChangeZoom()) {
        link.setChangeZoom(dest->getZoom());
    }

    link.setPdfPage(static_cast<std::size_t>(pageNumber - 1));
}

auto PopplerPdfAction::getTitle() -> std::string { return title; }
