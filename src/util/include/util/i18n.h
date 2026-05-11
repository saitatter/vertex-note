/*
 * VertexNote
 *
 * Internationalization module
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cstring>
#include <libintl.h>
#include <string>

#include "util/PlaceholderString.h"

#undef snprintf

namespace Util::i18n {
inline const char* translateContext(const char* context, const char* msg) {
    const std::string key = std::string(context) + '\004' + msg;
    const char* translated = gettext(key.c_str());
    return std::strcmp(translated, key.c_str()) == 0 ? msg : translated;
}
}  // namespace Util::i18n

#define _(msg) gettext(msg)
#define C_(context, msg) Util::i18n::translateContext(context, msg)

/// The string is not looked for by xgettext and should be added to the .po files another way (e.g. with N_ below)
#define fetch_translation(msg) gettext(msg)
#define fetch_translation_context(context, msg) Util::i18n::translateContext(context, msg)

// Formatted Translation
#define _F(msg) PlaceholderString(_(msg))
#define C_F(context, msg) PlaceholderString(C_(context, msg))

// Formatted, not translated text
#define FORMAT_STR(msg) PlaceholderString(msg)


// No translation performed, but in the Translation string
// So translation can be loaded dynamically at other place
// in the code
#define N_(msg) (msg)
#define NC_(context, msg) (msg)

/* Some helper macros */

// PlaceholderString → std::string
#define FS(format) (format).str()
// PlaceholderString → const char*
#define FC(format) FS(format).c_str()
