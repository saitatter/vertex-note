/*
 * VertexNote
 *
 * Latex file generator
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <string>   // for string
#include <variant>  // for variant

#include "util/Color.h"  // for Color

#include "filesystem.h"  // for path

class LatexSettings;

class LatexGenerator {
public:
    LatexGenerator(const LatexSettings& settings);
    LatexGenerator(const LatexGenerator&) = delete;
    LatexGenerator& operator=(const LatexGenerator&) = delete;
    LatexGenerator(const LatexGenerator&&) = delete;
    LatexGenerator&& operator=(const LatexGenerator&&) = delete;
    virtual ~LatexGenerator() = default;

    struct GenError {
        std::string message;
    };
    struct GenOutput {
        std::string output;
        int exitStatus = 0;
    };
    using Result = std::variant<GenOutput, GenError>;

    /**
     * Run the LaTeX command to generate a preview for the given LaTeX file.
     * The contents of the LaTeX file will be written to "tex.tex" in the given
     * directory. Standard output and standard error are returned as one stream.
     */
    Result run(const fs::path& texDir, const std::string& texFileContents);

    /**
     * Instantiate the LaTeX template.
     */
    static std::string templateSub(const std::string& input, const std::string& templ, Color textColor);

private:
    const LatexSettings& settings;
};
