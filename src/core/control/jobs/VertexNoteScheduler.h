/*
 * VertexNote
 *
 * A scheduler for Xournal background tasks
 *
 * Some code from Evince project
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include "control/jobs/Job.h"  // for JobType

#include "Scheduler.h"  // for JobPriority, Scheduler

class SidebarPreviewBaseEntry;
class PageView;

class VertexNoteScheduler: public Scheduler {
public:
    VertexNoteScheduler();
    ~VertexNoteScheduler() override;

public:
    /**
     * Remove source, e.g. if a page is removed they don't need to repaint.
     * Blocks until all jobs that could be using the source have finished.
     */
    void removeSidebar(SidebarPreviewBaseEntry* preview);
    void removePage(PageView* view);

    /**
     * Removes all PreviewJob%s / RenderJob%s scheduled to be run
     */
    void removeAllJobs();

    void addRepaintSidebar(SidebarPreviewBaseEntry* preview);
    void addRerenderPage(PageView* view);

    /**
     * Blocks until all currently running Job%s have been executed
     */
    void finishTask();

private:
    /**
     * Remove source, e.g. if a page is removed they don't need to repaint
     */
    void removeSource(void* source, JobType type, JobPriority priority, bool awaitFinishTask = true);

    bool existsSource(void* source, JobType type, JobPriority priority);

private:
};
