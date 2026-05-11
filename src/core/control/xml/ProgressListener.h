/*
 * VertexNote
 *
 * Interface for progress state
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

class ProgressListener {
public:
    virtual void setMaximumState(size_t max) = 0;
    virtual void setCurrentState(size_t state) = 0;

    virtual ~ProgressListener(){};
};

class DummyProgressListener: public ProgressListener {
public:
    void setMaximumState(size_t max) override{};
    void setCurrentState(size_t state) override{};

    ~DummyProgressListener() override{};
};
