/*
 * VertexNote
 *
 * Zoom change listener
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

class ZoomListener {
public:
    virtual void zoomChanged() = 0;
    virtual void zoomRangeValuesChanged();

    virtual ~ZoomListener();
};
