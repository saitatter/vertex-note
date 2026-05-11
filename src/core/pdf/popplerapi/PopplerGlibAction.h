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

#include "pdf/base/PdfAction.h"  // for PdfAction

class LinkAction;
class LinkDestination;
class LinkDest;
class PDFDoc;


class PopplerGlibAction: public PdfAction {
public:
    PopplerGlibAction(const LinkAction* action, std::shared_ptr<PDFDoc> document, std::string title = {});
    ~PopplerGlibAction() override;

public:
    virtual std::shared_ptr<const LinkDestination> getDestination() override;
    virtual std::string getTitle() override;

private:
    virtual std::shared_ptr<const LinkDestination> getDestination(const LinkAction* action);
    void linkFromDest(LinkDestination& link, const LinkDest* dest);

private:
    std::shared_ptr<PDFDoc> document;
    std::shared_ptr<const LinkDestination> destination;
    std::string title;
};
