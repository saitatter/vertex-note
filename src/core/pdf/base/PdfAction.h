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

#include <string>  // for string
#include <memory>
#include <vector>

#include "model/LinkDestination.h"


class PdfAction {
public:
    PdfAction();
    virtual ~PdfAction();

public:
    virtual std::shared_ptr<const LinkDestination> getDestination() = 0;
    virtual std::string getTitle() = 0;

private:
};
