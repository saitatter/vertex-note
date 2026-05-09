#include "FormatDefinitions.h"

#include "util/Util.h"  // for DPI_NORMALIZATION_FACTOR

const FormatUnits NOTE_UNITS[] = {{"cm", 28.346}, {"in", Util::DPI_NORMALIZATION_FACTOR}, {"points", 1.0}};

const int NOTE_UNIT_COUNT = sizeof(NOTE_UNITS) / sizeof(FormatUnits);
