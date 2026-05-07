/*
 * VertexNote
 *
 * Process-local geometry ID generation.
 */

#include "GeometryIdGenerator.h"

#include <atomic>

namespace vn::geom {

namespace {
std::atomic<ObjectId> nextObjectIdValue{InvalidObjectId + 1};
}

auto GeometryIdGenerator::nextObjectId() -> ObjectId { return nextObjectIdValue.fetch_add(1); }

void GeometryIdGenerator::observeObjectId(ObjectId id) {
    if (id == InvalidObjectId) {
        return;
    }

    ObjectId expected = nextObjectIdValue.load();
    while (expected <= id && !nextObjectIdValue.compare_exchange_weak(expected, id + 1)) {}
}

void GeometryIdGenerator::resetForTests(ObjectId nextId) { nextObjectIdValue.store(nextId); }

}  // namespace vn::geom
