/*
 * VertexNote
 *
 * Input handler for the compass.
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include "GeometryToolInputHandler.h"

class Stroke;
class GeometryToolController;
struct InputEvent;

/**
 * @brief Input handler for the compass
 *
 * Only the pointer handling (mouse / stylus) is defined in this class.
 * The rest comes from the GeometryToolHandler
 * */
class CompassInputHandler: public GeometryToolInputHandler {

public:
    explicit CompassInputHandler(VertexNoteView* noteView, GeometryToolController* controller);
    ~CompassInputHandler() noexcept override;

private:
    /**
     * @brief handles input from mouse and stylus for the compass
     */
    bool handlePointer(InputEvent const& event) override;

    double lastProj = NAN;

    double getMinHeight() const override;
    double getMaxHeight() const override;
};
