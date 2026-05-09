/*
 * VertexNote
 *
 * Error handler, prints a stacktrace if VertexNote crashes
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

class Document;
void setEmergencyDocument(const Document* doc);
void installCrashHandlers();
void emergencySave();
