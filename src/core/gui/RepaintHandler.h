/*
 * VertexNote
 *
 * Class for render and repaint pages
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

class PageView;
class VertexNoteView;

class RepaintHandler {
public:
    RepaintHandler(VertexNoteView* xournal);
    virtual ~RepaintHandler();

public:
    /**
     * Repaint a page
     */
    void repaintPage(const PageView* view);

    /// Repaint a page area, coordinates are in pixel-coordinates, relative to the page's upper-left corner
    void repaintPageArea(const PageView* view, int x1, int y1, int x2, int y2);

    /**
     * Repaints the page border (at least)
     */
    void repaintPageBorder(const PageView* view);

private:
    VertexNoteView* xournal;
};
