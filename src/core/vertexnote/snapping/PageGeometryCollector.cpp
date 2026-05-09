/*
 * VertexNote
 *
 * Collect geometry objects from a VertexNote page.
 */

#include "PageGeometryCollector.h"

#include "model/Element.h"
#include "model/Layer.h"
#include "model/NotePage.h"
#include "vertexnote/geometry/GeometryElement.h"

namespace vn::snap {

auto collectGeometryObjects(const PageRef& page) -> std::vector<const geom::GeometryObject*> {
    std::vector<const geom::GeometryObject*> objects;
    if (!page) {
        return objects;
    }

    for (const auto* layer: page->getLayersView()) {
        if (!layer || !layer->isVisible()) {
            continue;
        }

        for (const auto* element: layer->getElementsView()) {
            const auto* geometryElement = dynamic_cast<const geom::GeometryElement*>(element);
            if (geometryElement) {
                objects.push_back(&geometryElement->geometry());
            }
        }
    }

    return objects;
}

}  // namespace vn::snap
