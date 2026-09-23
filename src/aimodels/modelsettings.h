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
};

using ProviderConfigList = QList<ProviderConfig>;
}