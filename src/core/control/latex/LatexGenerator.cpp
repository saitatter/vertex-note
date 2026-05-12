#include "LatexGenerator.h"

#include <algorithm>    // for transform
#include <cstddef>      // for size_t
#include <fstream>
#include <map>          // for map
#include <regex>        // for smatch, sregex_iterator
#include <sstream>      // for ostringstream
#include <string_view>  // for string_view

#include "control/settings/LatexSettings.h"  // for LatexSettings
#include "util/PathUtil.h"                   // for getLongPath
#include "util/PlaceholderString.h"          // for PlaceholderString
#include "util/StringUtils.h"                // for char_cast
#include "util/Util.h"                       // for Util
#include "util/i18n.h"                       // for FS, _F

#include <QProcess>
#include <QByteArray>
#include <QStandardPaths>
#include <QString>
#include <QStringList>


using namespace vn::util;

namespace {

constexpr int LATEX_PROCESS_TIMEOUT_MS = 30000;

}

LatexGenerator::LatexGenerator(const LatexSettings& settings): settings(settings) {}

auto LatexGenerator::templateSub(const std::string& input, const std::string& templ, const Color textColor)
        -> std::string {
    std::map<std::string, std::string> vars;
    std::vector<std::string> bodyLines;

    const static std::regex directiveRe(R"(^%xpp:([A-Za-z_][A-Za-z0-9_]*)=(.*)$)");

    std::istringstream inputStream(input);
    std::string line;
    while (std::getline(inputStream, line)) {
        std::smatch match;
        if (std::regex_match(line, match, directiveRe)) {
            std::string key = match[1].str();
            std::string value = match[2].str();

            size_t start = value.find_first_not_of(" \t\r\n");
            if (start != std::string::npos) {
                size_t end = value.find_last_not_of(" \t\r\n");
                value = value.substr(start, end - start + 1);
            } else {
                value.clear();
            }

            std::transform(key.begin(), key.end(), key.begin(), ::toupper);

            if (key != "TOOL_INPUT" && key != "TEXT_COLOR") {
                vars[key] = value;
            }
        } else {
            bodyLines.push_back(line);
        }
    }

    std::string strippedBody;
    for (const auto& l: bodyLines) {
        if (!strippedBody.empty())
            strippedBody += '\n';
        strippedBody += l;
    }

    vars["TOOL_INPUT"] = strippedBody;
    vars["TEXT_COLOR"] = Util::rgb_to_hex_string(textColor).substr(1);

    const static std::regex substRe(R"(%%XPP_[A-Z][A-Z0-9_]*%%)");
    std::string output;
    output.reserve(templ.length());
    size_t templatePos = 0;

    for (std::sregex_iterator it(templ.begin(), templ.end(), substRe); it != std::sregex_iterator{}; ++it) {
        std::smatch match = *it;
        std::string placeholder = match.str();

        constexpr size_t kPrefixLen = 6;  // "%%XPP_"
        constexpr size_t kSuffixLen = 2;  // "%%"
        std::string key = placeholder.substr(kPrefixLen, placeholder.length() - kPrefixLen - kSuffixLen);

        output.append(templ, templatePos, as_unsigned(match.position()) - templatePos);

        auto itVar = vars.find(key);
        if (itVar != vars.end()) {
            output.append(itVar->second);
        }

        templatePos = as_unsigned(match.position() + match.length());
    }

    output.append(templ, templatePos);
    return output;
}

auto LatexGenerator::run(const fs::path& texDir, const std::string& texFileContents) -> Result {
    std::string cmd = this->settings.genCmd;
    const auto texFilePath = Util::getLongPath(texDir) / "tex.tex";
    const auto texFilePathString = std::string{char_cast(texFilePath.u8string())};

    for (auto i = cmd.find("{}"); i != std::string::npos; i = cmd.find("{}", i + texFilePathString.length())) {
        cmd.replace(i, 2, texFilePathString);
    }

    QStringList argv = QProcess::splitCommand(QString::fromStdString(cmd));
    if (argv.empty()) {
        return GenError{FS(_F("Failed to parse LaTeX generator command: {1}") % cmd)};
    }

    const QString program = argv.takeFirst();
    const QString executable = QStandardPaths::findExecutable(program);
    if (executable.isEmpty()) {
        if (Util::isFlatpakInstallation()) {
            return GenError{
                    FS(_F("Failed to find LaTeX generator program in PATH: {1}\n\nSince installation is detected "
                          "within Flatpak, you need to install the Flatpak freedesktop Tex Live extension. For "
                          "example, by running:\n\n$ flatpak install flathub org.freedesktop.Sdk.Extension.texlive") %
                       program.toStdString())};
        } else {
            return GenError{FS(_F("Failed to find LaTeX generator program in PATH: {1}") % program.toStdString())};
        }
    }

    std::ofstream texFile(texFilePath, std::ios::binary);
    if (!texFile) {
        return GenError({FS(_F("Could not save .tex file: {1}") % texFilePathString)});
    }
    texFile.write(texFileContents.data(), static_cast<std::streamsize>(texFileContents.size()));
    texFile.close();

    QProcess process;
    process.setProgram(executable);
    process.setArguments(argv);
    process.setWorkingDirectory(QString::fromStdString(std::string{char_cast(texDir.u8string())}));
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start();

    if (!process.waitForStarted()) {
        return GenError({FS(_F("Could not start {1}: {2} (exit code: {3})") % executable.toStdString() %
                            process.errorString().toStdString() % static_cast<int>(process.error()))});
    }
    if (!process.waitForFinished(LATEX_PROCESS_TIMEOUT_MS)) {
        if (process.error() == QProcess::Timedout) {
            process.kill();
            process.waitForFinished();
            return GenError({FS(_F("LaTeX generator command timed out after {1} seconds: {2}") %
                                (LATEX_PROCESS_TIMEOUT_MS / 1000) % executable.toStdString())});
        }
        return GenError({FS(_F("LaTeX generator command failed to finish: {1}: {2} (process error: {3})") %
                            executable.toStdString() % process.errorString().toStdString() %
                            static_cast<int>(process.error()))});
    }

    const QByteArray output = process.readAllStandardOutput();
    return GenOutput{std::string(output.constData(), static_cast<std::size_t>(output.size())), process.exitCode()};
}
