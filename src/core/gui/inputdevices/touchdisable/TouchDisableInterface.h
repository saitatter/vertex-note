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


class TouchDisableInterface {
public:
    TouchDisableInterface();
    virtual ~TouchDisableInterface();

public:
    virtual void enableTouch() = 0;
    virtual void disableTouch() = 0;
    virtual void init();

private:
};
