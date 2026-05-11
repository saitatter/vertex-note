/*
 * VertexNote
 *
 * A link destination in a PDF Document
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cstddef>   // for size_t
#include <optional>  // for optional
#include <string>    // for string
#include <variant>   // for variant

class LinkDestination {
public:
    LinkDestination();
    virtual ~LinkDestination();

    /// A link to a URI.
    struct LinkType {
        std::string uri;
    };
    /// A link to some unknown PDF action.
    struct UnknownType {};

    /// Represents a possible PDF link action.
    using Type = std::variant<UnknownType, LinkType>;

public:
    size_t getPdfPage() const;
    void setPdfPage(size_t page);

    void setExpand(bool expand);
    bool getExpand() const;

    bool shouldChangeLeft() const;
    bool shouldChangeTop() const;

    double getZoom() const;
    [[maybe_unused]] double getLeft() const;
    double getTop() const;

    void setChangeLeft(double left);
    void setChangeZoom(double zoom);
    void setChangeTop(double top);

    void setName(std::string name);
    const std::string& getName() const;

    /// \return Whether this link refers to a URI.
    bool isURI() const;

    std::optional<std::string> getURI() const;

    /// \return Changes this link to refer to the given URI.
    void setURI(std::string uri);

private:
    size_t page;
    bool expand;

    double left;
    double top;
    double zoom;

    bool changeLeft;
    bool changeZoom;
    bool changeTop;

    std::string name;
    Type contents;
};
