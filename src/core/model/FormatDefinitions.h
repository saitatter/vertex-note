/*
 * VertexNote
 *
 * Abstract gui class, which loads the glade objcts
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

typedef struct {
    const char* name;
    const double scale;
} FormatUnits;

extern const FormatUnits XOJ_UNITS[];
extern const int XOJ_UNIT_COUNT;
