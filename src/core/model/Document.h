/*
 * VertexNote
 *
 * The document
 *
 * All methods are unlocked, you need to lock the document before you change something and unlock after.
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cstddef>        // for size_t
#include <memory>         // for unique_ptr
#include <shared_mutex>   // for shared_mutex
#include <string>         // for string
#include <unordered_map>  // for unordered_map
#include <vector>         // for vector

#include <cairo.h>    // for cairo_surface_t
#include <glib.h>     // for gpointer, gsize

#include "pdf/base/PdfDocument.h"  // for PdfDocument
#include "pdf/base/PdfPage.h"      // for PdfPagePtr
#include "util/PathUtil.h"            // for PathStorageMode
#include "util/raii/GObjectSPtr.h"    // for GObjectSptr

#include "PageRef.h"     // for PageRef
#include "filesystem.h"  // for path

class DocumentHandler;
class PdfBookmarkIterator;
typedef struct _GtkTreeIter GtkTreeIter;
typedef struct _GtkTreeModel GtkTreeModel;
typedef struct _GtkTreePath GtkTreePath;

class Document {
public:
    Document(DocumentHandler* handler);
    virtual ~Document();

public:
    enum DocumentType { XOPP, XOJ, PDF };

    void setPdfAttributes(const fs::path& filename, bool attachToDocument);
    bool readPdf(const fs::path& filename, bool initPages, bool attachToDocument,
                 std::unique_ptr<std::string> data = {});
    void resetPdf();

    size_t getPageCount() const;
    size_t getPdfPageCount() const;
    PdfPagePtr getPdfPage(size_t page) const;
    const PdfDocument& getPdfDocument() const;

    void insertPage(const PageRef& p, size_t position);
    void addPage(const PageRef& p);
    template <class InputIter>
    void addPages(InputIter first, InputIter last);
    PageRef getPage(size_t page) const;
    void deletePage(size_t pNr);

    static void setPageSize(PageRef p, double width, double height);
    static double getPageWidth(PageRef p);
    static double getPageHeight(PageRef p);

    size_t indexOf(const PageRef& page);

    /**
     * @return The last error message to show to the user
     */
    std::string getLastErrorMsg() const;

    size_t findPdfPage(size_t pdfPage) const;

    Document& operator=(const Document& doc);

    void setFilepath(fs::path filepath);
    fs::path getFilepath() const;
    fs::path getPdfFilepath() const;
    fs::path createSaveFoldername(const fs::path& lastSavePath) const;
    fs::path createSaveFilename(DocumentType type, std::u8string_view defaultSaveName,
                                std::u8string_view defaultPfdName = u8"") const;

    fs::path getEvMetadataFilename() const;

    GtkTreeModel* getContentsModel() const;

    void setCreateBackupOnSave(bool backup);
    bool shouldCreateBackupOnSave() const;

    void clearDocument(bool destroy = false);

    bool isAttachPdf() const;

    vn::util::CairoSurfaceSPtr getPreview() const;
    void setPreview(vn::util::CairoSurfaceSPtr preview);

    void lock();
    void unlock();
    bool try_lock();
    void lock_shared();
    void unlock_shared();
    bool try_lock_shared();

    inline Util::PathStorageMode getPathStorageMode() const { return pathStorageMode; }
    inline void setPathStorageMode(Util::PathStorageMode m) { pathStorageMode = m; }

private:
    void buildContentsModel();
    void freeTreeContentModel();
    static bool freeTreeContentEntry(GtkTreeModel* treeModel, GtkTreePath* path, GtkTreeIter* iter, Document* doc);

    void buildTreeContentsModel(GtkTreeIter* parent, PdfBookmarkIterator* iter);
    void updateIndexPageNumbers();
    static bool fillPageLabels(GtkTreeModel* treeModel, GtkTreePath* path, GtkTreeIter* iter, Document* doc);

private:
    DocumentHandler* handler = nullptr;

    PdfDocument pdfDocument;

    fs::path filepath;
    fs::path pdfFilepath;
    bool attachPdf = false;

    Util::PathStorageMode pathStorageMode = Util::PathStorageMode::AS_RELATIVE_PATH;

    /**
     *  Password: not handled yet
     */
    std::string password;

    std::string lastError;

    /**
     * The pages in the document
     */
    std::vector<PageRef> pages;

    /**
     * Index from pdf page number to document page number
     */
    using PageIndex = std::unordered_map<size_t, size_t>;

    /**
     * The cached page index
     */
    std::unique_ptr<PageIndex> pageIndex;

    /**
     * Creates an index from pdf page number to document page number
     *
     * Clears the index first in case it is already exists.
     */
    void indexPdfPages();

    /**
     * The bookmark contents model
     */
    vn::util::GObjectSPtr<GtkTreeModel> contentsModel;

    /**
     *  create a backup before save
     */
    bool createBackupOnSave = false;

    /**
     * The preview for the file
     */
    vn::util::CairoSurfaceSPtr preview;

    /**
     * The lock of the document
     */
    std::shared_mutex documentLock;
};

template <class InputIter>
void Document::addPages(InputIter first, InputIter last) {
    this->pages.insert(this->pages.end(), first, last);
    this->pageIndex.reset();
    updateIndexPageNumbers();
}
