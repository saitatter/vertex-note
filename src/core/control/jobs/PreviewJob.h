/*
 * VertexNote
 *
 * A job which handles preview repaint
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cairo.h>  // for cairo_surface_t, cairo_t

#include "util/raii/CairoWrappers.h"

#include "Job.h"  // for Job, JobType

class SidebarPreviewBaseEntry;

/**
 * @brief A Job which renders a SidebarPreviewPage
 */
class PreviewJob: public Job {
public:
    PreviewJob(SidebarPreviewBaseEntry* sidebar);

protected:
    void onDelete() override;
    ~PreviewJob() override;

public:
    void* getSource() override;

    void run() override;

    JobType getType() override;

private:
    void initGraphics();
    void clipToPage();
    void finishPaint();
    void drawPage();

private:
    /**
     * Graphics buffer
     */
    vn::util::CairoSurfaceSPtr buffer;

    /**
     * Graphics drawing
     */
    vn::util::CairoSPtr cr;

    /**
     * Sidebar preview
     */
    SidebarPreviewBaseEntry* sidebarPreview = nullptr;
};
