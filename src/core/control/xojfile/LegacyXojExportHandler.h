/*
 * VertexNote
 *
 * Export a document for as .xoj compatible for Xournal,
 * remove all additional features which break the compatibility
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include "model/PageRef.h"  // for PageRef

#include "SaveHandler.h"  // for SaveHandler

class AudioElement;
class Stroke;
class XmlAudioNode;
class XmlNode;
class XmlPointNode;


class LegacyXojExportHandler: public SaveHandler {
public:
    LegacyXojExportHandler();
    virtual ~LegacyXojExportHandler();

protected:
    /**
     * Export the fill attributes
     */
    void visitStrokeExtended(XmlPointNode* stroke, const Stroke* s) override;
    void writeHeader() override;
    void writeSolidBackground(XmlNode* background, ConstPageRef p) override;
    void writeTimestamp(XmlAudioNode* xmlAudioNode, const AudioElement* audioElement) override;
    void writeBackgroundName(XmlNode* background, ConstPageRef p) override;

private:
};
