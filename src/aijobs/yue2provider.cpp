/*
 * Audacity: A Digital Audio Editor
 */
#include "yue2provider.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QObject>

#include <algorithm>

namespace au::aijobs {

bool parseYue2Parameters(const QByteArray& parametersJson,
                         const QString& defaultCliPath,
                         const QString& defaultModelDirectory,
                         int defaultThreads,
                         Yue2JobParameters* parameters,
                         QString* errorMessage)
{
    if (!parameters) {
        return false;
    }

    Yue2JobParameters result;
    result.cliPath = defaultCliPath;
    result.modelDirectory = defaultModelDirectory;
    result.threads = defaultThreads;

    if (!parametersJson.isEmpty()) {
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(parametersJson, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            if (errorMessage) {
                *errorMessage = QObject::tr("The YuE2 job parameters are not valid JSON");
            }
            return false;
        }
        const QJsonObject values = document.object();
        if (values.contains("cliPath")) {
            result.cliPath = values.value("cliPath").toString(result.cliPath);
        }
        if (values.contains("modelDirectory")) {
            result.modelDirectory = values.value("modelDirectory").toString(result.modelDirectory);
        }
        if (values.contains("mainModel")) {
            result.mainModel = values.value("mainModel").toString(result.mainModel);
        }
        if (values.contains("decoderModel")) {
            result.decoderModel = values.value("decoderModel").toString(result.decoderModel);
        }
        if (values.contains("text")) {
            result.text = values.value("text").toString();
        }
        if (values.contains("style")) {
            result.style = values.value("style").toString();
        }
        if (values.contains("backend")) {
            result.backend = values.value("backend").toString(result.backend);
        }
        if (values.contains("threads")) {
            result.threads = values.value("threads").toInt(result.threads);
        }
        if (values.contains("seed")) {
            result.seed = values.value("seed").toInt(result.seed);
        }
        if (values.contains("steps")) {
            result.steps = values.value("steps").toInt(result.steps);
        }
        if (values.contains("cot")) {
            result.cot = values.value("cot").toBool(true);
        }
    }

    if (result.cliPath.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QObject::tr("The YuE2 CLI path is not configured");
        }
        return false;
    }
    if (result.modelDirectory.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QObject::tr("The YuE2 model directory is not configured");
        }
        return false;
    }
    if (result.text.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QObject::tr("YuE2 generation needs lyrics");
        }
        return false;
    }

    result.threads = std::max(1, result.threads);
    result.steps = std::max(1, result.steps);
    *parameters = result;
    return true;
}

QStringList buildYue2Arguments(const Yue2JobParameters& parameters, const QString& outputPath)
{
    QStringList arguments {
        QStringLiteral("--task"), QStringLiteral("gen"),
        QStringLiteral("--family"), QStringLiteral("yue2"),
        QStringLiteral("--model"), parameters.modelDirectory,
        QStringLiteral("--backend"), parameters.backend,
        QStringLiteral("--threads"), QString::number(parameters.threads),
        QStringLiteral("--text"), parameters.text
    };

    if (!parameters.style.trimmed().isEmpty()) {
        arguments << QStringLiteral("--request-option") << QStringLiteral("style=%1").arg(parameters.style);
    }

    arguments << QStringLiteral("--request-option") << QStringLiteral("cot=%1").arg(parameters.cot ? QStringLiteral("full") : QStringLiteral("off"))
              << QStringLiteral("--request-option") << QStringLiteral("seed=%1").arg(parameters.seed)
              << QStringLiteral("--request-option") << QStringLiteral("num_inference_steps=%1").arg(parameters.steps)
              << QStringLiteral("--session-option") << QStringLiteral("yue2.model_gguf=%1").arg(parameters.mainModel)
              << QStringLiteral("--session-option") << QStringLiteral("yue2.vae_gguf=%1").arg(parameters.decoderModel)
              << QStringLiteral("--out") << outputPath
              << QStringLiteral("--metrics")
              << QStringLiteral("--log");
    return arguments;
}

Yue2Progress yue2ProgressFromLogLine(const QString& line)
{
    struct Marker {
        const char* needle;
        double progress;
        const char* message;
    };
    // Ordered most specific first; each matches a stage-timing line the CLI
    // prints with --log.
    static const Marker markers[] = {
        { "yue2.semantic.abc_generated_tokens", 0.20, "Song plan generated" },
        { "yue2.ar.generate.total_ms", 0.50, "Semantic tokens generated" },
        { "yue2.semantic_ms", 0.55, "Semantic stage complete" },
        { "yue2.nar_ms", 0.80, "Audio synthesized" },
        { "yue2.vae_decode_ms", 0.95, "Decoding audio" },
        { "session.wall_ms", 0.99, "Finalizing" },
    };

    for (const Marker& marker : markers) {
        if (line.contains(QLatin1String(marker.needle))) {
            Yue2Progress progress;
            progress.recognized = true;
            progress.progress = marker.progress;
            progress.message = QString::fromLatin1(marker.message);
            return progress;
        }
    }
    return {};
}
}