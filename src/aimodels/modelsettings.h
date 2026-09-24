/*
 * Audacity: A Digital Audio Editor
 */
#pragma once

#include <QList>
#include <QString>

namespace au::aimodels {

//! Runtime configuration for a local generation provider: where its executable
//! and model data live. This is what used to require AI_YUE2_CLI / AI_YUE2_MODEL
//! environment variables to be set by hand.
struct ProviderConfig {
    QString id;
    QString displayName;
    QString cliPath;
    QString modelPath;
    int threads = 0;

    bool isConfigured() const { return !cliPath.isEmpty() && !modelPath.isEmpty(); }
};

using ProviderConfigList = QList<ProviderConfig>;

//! Configuration for the native yue2.cpp engine (`yue-server`): the executable,
//! the backbone and VAE GGUFs (plus the optional SheetSage2 transcriber), and
//! how it listens. This unlocks exact replay, score-first and covers.
struct Yue2CppConfig {
    QString enginePath;
    QString backbonePath;
    QString vaePath;
    QString transcriberPath;
    QString planToolPath;
    QString transcribeToolPath;
    QString host = QStringLiteral("127.0.0.1");
    int port = 18087;
    QString ggmlBackend;

    bool isConfigured() const { return !enginePath.isEmpty() && !backbonePath.isEmpty() && !vaePath.isEmpty(); }
};

//! Configuration for the optional writing assistant: any OpenAI-compatible
//! chat-completions endpoint (OpenRouter by default).
struct AssistantConfig {
    //! "cloud" (OpenAI-compatible endpoint) or "local" (a runner we launch).
    QString mode = QStringLiteral("cloud");
    QString baseUrl = QStringLiteral("https://openrouter.ai/api/v1");
    QString apiKey;
    QString model = QStringLiteral("meta-llama/llama-3.1-8b-instruct");
    //! Local mode: the runner executable (e.g. llama-server) and model file.
    QString runnerPath;
    QString modelPath;
    int port = 8080;
};

//! Provider-neutral, machine-level settings persisted as JSON in the
//! application config directory (override the location in tests).
class ModelSettings
{
public:
    static QString configFilePath();
    static void setConfigFilePathForTesting(const QString& path);

    //! Built-in provider definitions merged with the saved configuration, so a
    //! provider is always listed even before it has been configured.
    static QList<ProviderConfig> providers();
    static ProviderConfig provider(const QString& id);

    static bool setProvider(const ProviderConfig& config, QString* errorMessage = nullptr);

    //! Assistant configuration, stored beside the provider settings.
    static AssistantConfig assistant();
    static bool setAssistant(const AssistantConfig& config, QString* errorMessage = nullptr);

    //! Native yue2.cpp engine configuration and discovery.
    static Yue2CppConfig yue2Cpp();
    static bool setYue2Cpp(const Yue2CppConfig& config, QString* errorMessage = nullptr);
    //! Detects the engine under one YuE2-Studio-style install directory.
    static Yue2CppConfig detectYue2CppAt(const QString& baseDir);
    //! Scans the usual install locations for the engine and its models.
    static Yue2CppConfig detectYue2Cpp();
};
}