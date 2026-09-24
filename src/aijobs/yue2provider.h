/*
 * Audacity: A Digital Audio Editor
 */
#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace au::aijobs {
// Provider-owned request for the native YuE2 (audio.cpp) provider. The fields
// are filled from the runtime host's configured defaults plus the
// provider-neutral JobRequest parameters JSON.
struct Yue2JobParameters {
    QString cliPath;
    QString modelDirectory;
    QString mainModel = QStringLiteral("yue2-3b-q4_0.gguf");
    QString decoderModel = QStringLiteral("yue2-vae-f16.gguf");
    QString text;
    QString style;
    QString abc;
    QString abcFile;
    QString backend = QStringLiteral("cuda");
    int threads = 8;
    int seed = 0;
    int steps = 8;
    // Score mode: "full" (melody + chords), "melody" (melody only), "off".
    QString cot = QStringLiteral("full");
    double guidanceScale = 0.0;
};

// Parses parametersJson over the given defaults. Returns false (with a reason)
// when the CLI path, model directory, or lyrics are missing or the JSON is bad.
bool parseYue2Parameters(const QByteArray& parametersJson,
                         const QString& defaultCliPath,
                         const QString& defaultModelDirectory,
                         int defaultThreads,
                         Yue2JobParameters* parameters,
                         QString* errorMessage = nullptr);

// Builds the audiocpp_cli argument list for one generation job. When
// artifactDirectory is set, generated artifacts (such as the YuE2 score/ABC)
// are written there via --out-dir.
QStringList buildYue2Arguments(const Yue2JobParameters& parameters, const QString& outputPath,
                              const QString& artifactDirectory = QString());

// A coarse progress milestone derived from one audiocpp_cli log line. The CLI
// does not report a percentage, so we map the stage-timing lines it prints.
struct Yue2Progress {
    bool recognized = false;
    double progress = 0.0;
    QString message;
};

Yue2Progress yue2ProgressFromLogLine(const QString& line);
}