/*
 * VertexNote
 *
 * Layer Controller listener
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

class LayerController;

class LayerCtrlListener {
public:
    LayerCtrlListener();
    virtual ~LayerCtrlListener();

public:
    void registerListener(LayerController* handler);
    void unregisterListener();

    virtual void rebuildLayerMenu() = 0;
    virtual void layerVisibilityChanged() = 0;
    virtual void updateSelectedLayer() = 0;

private:
    LayerController* handler;
};
