/*
 * VertexNote
 *
 * Sidebar preview layout
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

class SidebarPreviewBase;

class SidebarLayout {
public:
    SidebarLayout() = delete;
    ~SidebarLayout() = delete;

public:
    /**
     * Layouts the sidebar
     */
    static void layout(SidebarPreviewBase* sidebar);

private:
};
