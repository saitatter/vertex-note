/*
 * VertexNote
 *
 * [Header description]
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include "PositionInputData.h"  // for PositionInputData

class InputContext;
class PageView;
struct InputEvent;

/**
 * Abstract class for a specific input state
 */
class AbstractInputHandler {
private:
    bool blocked = false;

protected:
    InputContext* inputContext;
    bool inputRunning = false;

protected:
    PageView* getPageAtCurrentPosition(InputEvent const& event) const;
    PositionInputData getInputDataRelativeToCurrentPage(PageView* page, InputEvent const& event) const;

public:
    explicit AbstractInputHandler(InputContext* inputContext);
    virtual ~AbstractInputHandler();

    void block(bool block);
    bool isBlocked() const;
    virtual void onBlock();
    virtual void onUnblock();
    bool handle(InputEvent const& event);
    virtual bool handleImpl(InputEvent const& event) = 0;
};
