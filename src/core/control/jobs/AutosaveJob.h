/*
 * VertexNote
 *
 * Autosave job
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <string>  // for string

#include "Job.h"  // for Job, JobType

class Control;

class AutosaveJob: public Job {
public:
    AutosaveJob(Control* control);

protected:
    ~AutosaveJob() override;

public:
    void run() override;
    void afterRun() override;

    JobType getType() override;

private:
    Control* control = nullptr;
    std::string error;
};
