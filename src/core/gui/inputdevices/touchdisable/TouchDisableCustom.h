/*
 * VertexNote
 *
 * Interface for touch disable implementations
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <string>  // for string

#include "TouchDisableInterface.h"  // for TouchDisableInterface


class TouchDisableCustom: public TouchDisableInterface {
public:
    TouchDisableCustom(std::string enableCommand, std::string disableCommand);
    ~TouchDisableCustom() override;

public:
    void enableTouch() override;
    void disableTouch() override;

private:
    std::string enableCommand;
    std::string disableCommand;
};
