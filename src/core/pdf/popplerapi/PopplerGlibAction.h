/*
 * VertexNote
 *
 * PDF Action Abstraction Interface
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <memory>  // for shared_ptr
#include <string>  // for string

#include <poppler.h>  // for PopplerAction, PopplerDocument

#include "pdf/base/PdfAction.h"  // for PdfAction
#include "util/raii/GObjectSPtr.h"  // for GObjectSPtr

class LinkDestination;


class PopplerGlibAction: public PdfAction {
public:
    PopplerGlibAction(PopplerAction* action, PopplerDocument* document);
    ~PopplerGlibAction() override;

public:
    virtual std::shared_ptr<const LinkDestination> getDestination() override;
    virtual std::string getTitle() override;

private:
    virtual std::shared_ptr<const LinkDestination> getDestination(PopplerAction* action);
    void linkFromDest(LinkDestination& link, PopplerDest* pDest);

private:
    vn::util::raii::GObjectSPtr<PopplerDocument> document;
    std::shared_ptr<const LinkDestination> destination;
    std::string title;
};
