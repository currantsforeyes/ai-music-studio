/*
 * Audacity: A Digital Audio Editor
 */
#include "modelsettings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

namespace au::aimodels {
namespace {
QString g_configFilePathOverride;

ProviderConfigList builtInProviders()
{
    ProviderConfig yue2;
    yue2.id = QStringLiteral("yue2-native");
    yue2.displayName = QStringLiteral("YuE2");
    return { yue2 };
}

QJsonObject readRoot()
{
    QFile file(ModelSettings::configFilePath());
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    return document.isObject() ? document.object() : QJsonObject {};
}

ProviderConfigList savedProviders(QString* errorMessage = nullptr)
{
    ProviderConfigList result;
    QFile file(ModelSettings::configFilePath());
    if (!file.exists()) {
        return result;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not read %1").arg(file.fileName());
        }
        return result;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError && !document.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Invalid provider settings: %1").arg(parseError.errorString());
        }
        return result;
    }
    const QJsonArray array = document.object().value(QStringLiteral("providers")).toArray();
    for (const QJsonValue& value : array) {
        const QJsonObject object = value.toObject();
        ProviderConfig config;
        config.id = object.value(QStringLiteral("id")).toString();
        config.displayName = object.value(QStringLiteral("displayName")).toString();
        config.cliPath = object.value(QStringLiteral("cliPath")).toString();
        config.modelPath = object.value(QStringLiteral("modelPath")).toString();
        config.threads = object.value(QStringLiteral("threads")).toInt();
        if (!config.id.isEmpty()) {
            result.append(config);
        }
    }
    return result;
}
}

QString ModelSettings::configFilePath()
{
    if (!g_configFilePathOverride.isEmpty()) {
        return g_configFilePathOverride;
    }
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (dir.isEmpty()) {
        dir = QDir::homePath();
    }
    return QDir(dir).filePath(QStringLiteral("aimodels.json"));
}

void ModelSettings::setConfigFilePathForTesting(const QString& path)
{
    g_configFilePathOverride = path;
}

ProviderConfigList ModelSettings::providers()
{
    ProviderConfigList result = builtInProviders();
    const ProviderConfigList saved = savedProviders();
    for (const ProviderConfig& stored : saved) {
        bool matched = false;
        for (ProviderConfig& config : result) {
            if (config.id == stored.id) {
                if (!stored.displayName.isEmpty()) {
                    config.displayName = stored.displayName;
                }
                config.cliPath = stored.cliPath;
                config.modelPath = stored.modelPath;
                config.threads = stored.threads;
                matched = true;
                break;
            }
        }
        if (!matched) {
            result.append(stored);
        }
    }
    return result;
}

ProviderConfig ModelSettings::provider(const QString& id)
{
    for (const ProviderConfig& config : providers()) {
        if (config.id == id) {
            return config;
        }
    }
    return ProviderConfig {};
}

bool ModelSettings::setProvider(const ProviderConfig& config, QString* errorMessage)
{
    if (config.id.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("A provider id is required");
        }
        return false;
    }

    ProviderConfigList all = providers();
    bool found = false;
    for (ProviderConfig& existing : all) {
        if (existing.id == config.id) {
            existing = config;
            found = true;
            break;
        }
    }
    if (!found) {
        all.append(config);
    }

    QJsonArray array;
    for (const ProviderConfig& entry : all) {
        QJsonObject object;
        object.insert(QStringLiteral("id"), entry.id);
        object.insert(QStringLiteral("displayName"), entry.displayName);
        object.insert(QStringLiteral("cliPath"), entry.cliPath);
        object.insert(QStringLiteral("modelPath"), entry.modelPath);
        object.insert(QStringLiteral("threads"), entry.threads);
        array.append(object);
    }
    QJsonObject root = readRoot();
    root.insert(QStringLiteral("providers"), array);

    const QString path = configFilePath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not write %1").arg(path);
        }
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not save provider settings");
        }
        return false;
    }
    return true;
}

AssistantConfig ModelSettings::assistant()
{
    AssistantConfig config;
    const QJsonObject object = readRoot().value(QStringLiteral("assistant")).toObject();
    config.baseUrl = object.value(QStringLiteral("baseUrl")).toString(config.baseUrl);
    config.apiKey = object.value(QStringLiteral("apiKey")).toString(config.apiKey);
    config.model = object.value(QStringLiteral("model")).toString(config.model);
    return config;
}

bool ModelSettings::setAssistant(const AssistantConfig& config, QString* errorMessage)
{
    QJsonObject root = readRoot();
    QJsonObject object;
    object.insert(QStringLiteral("baseUrl"), config.baseUrl);
    object.insert(QStringLiteral("apiKey"), config.apiKey);
    object.insert(QStringLiteral("model"), config.model);
    root.insert(QStringLiteral("assistant"), object);

    const QString path = configFilePath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not write %1").arg(path);
        }
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not save assistant settings");
        }
        return false;
    }
    return true;
}
}