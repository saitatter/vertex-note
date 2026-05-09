/*
 * VertexNote
 *
 * Part of the Xournal shape recognizer
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <memory>
class Stroke;
class Inertia;

class CircleRecognizer {
private:
    CircleRecognizer();
    virtual ~CircleRecognizer();

public:
    static auto recognize(Stroke* s) -> std::unique_ptr<Stroke>;

private:
    static auto makeCircleShape(Stroke* originalStroke, Inertia& inertia) -> std::unique_ptr<Stroke>;
    static auto scoreCircle(Stroke* s, Inertia& inertia) -> double;
};
