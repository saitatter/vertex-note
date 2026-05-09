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

class InputContext;
struct KeyEvent;

class KeyboardInputHandler {
private:
public:
    explicit KeyboardInputHandler(InputContext* inputContext);
    ~KeyboardInputHandler();
    bool keyPressed(KeyEvent e) const;
    bool keyReleased(KeyEvent e) const;

private:
    InputContext* inputContext;
};
