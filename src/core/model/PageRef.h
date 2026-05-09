/*
 * VertexNote
 *
 * A page reference, should only allocated on the stack
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */
#pragma once

#include <memory>

class NotePage;

using PageRef = std::shared_ptr<NotePage>;
using ConstPageRef = std::shared_ptr<const NotePage>;
