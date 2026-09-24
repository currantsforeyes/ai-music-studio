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
    config.mode = object.value(QStringLiteral("mode")).toString(config.mode);
    config.baseUrl = object.value(QStringLiteral("baseUrl")).toString(config.baseUrl);
    config.apiKey = object.value(QStringLiteral("apiKey")).toString(config.apiKey);
    config.model = object.value(QStringLiteral("model")).toString(config.model);
    config.runnerPath = object.value(QStringLiteral("runnerPath")).toString(config.runnerPath);
    config.modelPath = object.value(QStringLiteral("modelPath")).toString(config.modelPath);
    config.port = object.value(QStringLiteral("port")).toInt(config.port);
    return config;
}

bool ModelSettings::setAssistant(const AssistantConfig& config, QString* errorMessage)
{
    QJsonObject root = readRoot();
    QJsonObject object;
    object.insert(QStringLiteral("mode"), config.mode);
    object.insert(QStringLiteral("baseUrl"), config.baseUrl);
    object.insert(QStringLiteral("apiKey"), config.apiKey);
    object.insert(QStringLiteral("model"), config.model);
    object.insert(QStringLiteral("runnerPath"), config.runnerPath);
    object.insert(QStringLiteral("modelPath"), config.modelPath);
    object.insert(QStringLiteral("port"), config.port);
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

Yue2CppConfig ModelSettings::detectYue2CppAt(const QString& baseDir)
{
    Yue2CppConfig config;
    if (baseDir.isEmpty()) {
        return config;
    }
    const QDir base(baseDir);
    const QStringList engineCandidates {
        base.filePath(QStringLiteral("resources/yue2-cpp/yue-server.exe")),
        base.filePath(QStringLiteral("yue-server.exe")),
        base.filePath(QStringLiteral("bin/yue-server.exe"))
    };
    for (const QString& candidate : engineCandidates) {
        if (QFileInfo::exists(candidate)) {
            config.enginePath = QDir::cleanPath(candidate);
            break;
        }
    }
    const QStringList modelDirs {
        base.filePath(QStringLiteral("data/models/yue2-cpp")),
        base.filePath(QStringLiteral("models"))
    };
    for (const QString& modelDir : modelDirs) {
        const QDir dir(modelDir);
        if (!dir.exists()) {
            continue;
        }
        const QStringList ggufs = dir.entryList(QStringList { QStringLiteral("*.gguf") }, QDir::Files);
        for (const QString& name : ggufs) {
            const QString lower = name.toLower();
            if (config.backbonePath.isEmpty() && lower.startsWith(QStringLiteral("yue2-3b"))) {
                config.backbonePath = QDir::cleanPath(dir.filePath(name));
            } else if (config.vaePath.isEmpty() && lower.startsWith(QStringLiteral("yue2-vae"))) {
                config.vaePath = QDir::cleanPath(dir.filePath(name));
            } else if (config.transcriberPath.isEmpty() && lower.startsWith(QStringLiteral("sheetsage2"))) {
                config.transcriberPath = QDir::cleanPath(dir.filePath(name));
            }
        }
        if (!config.backbonePath.isEmpty() && !config.vaePath.isEmpty()) {
            break;
        }
    }
    return config;
}

Yue2CppConfig ModelSettings::detectYue2Cpp()
{
    const QString override = qEnvironmentVariable("YUE2CPP_ROOT");
    if (!override.isEmpty()) {
        const Yue2CppConfig found = detectYue2CppAt(override);
        if (!found.enginePath.isEmpty()) {
            return found;
        }
    }
    QStringList roots;
    for (const QFileInfo& drive : QDir::drives()) {
        roots << drive.absoluteFilePath();
    }
    roots << QDir::homePath()
          << QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    for (const QString& root : roots) {
        const QDir dir(root);
        const QStringList entries = dir.entryList(QStringList { QStringLiteral("YuE2-Studio*"), QStringLiteral("YuE2 Studio") },
                                                  QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString& entry : entries) {
            const Yue2CppConfig found = detectYue2CppAt(dir.filePath(entry));
            if (found.isConfigured()) {
                return found;
            }
        }
    }
    return {};
}

Yue2CppConfig ModelSettings::yue2Cpp()
{
    Yue2CppConfig config;
    const QJsonObject object = readRoot().value(QStringLiteral("yue2cpp")).toObject();
    config.enginePath = object.value(QStringLiteral("enginePath")).toString();
    config.backbonePath = object.value(QStringLiteral("backbonePath")).toString();
    config.vaePath = object.value(QStringLiteral("vaePath")).toString();
    config.transcriberPath = object.value(QStringLiteral("transcriberPath")).toString();
    config.host = object.value(QStringLiteral("host")).toString(config.host);
    config.port = object.value(QStringLiteral("port")).toInt(config.port);
    config.ggmlBackend = object.value(QStringLiteral("ggmlBackend")).toString();
    if (!config.isConfigured()) {
        const Yue2CppConfig detected = detectYue2Cpp();
        if (detected.isConfigured()) {
            return detected;
        }
    }
    return config;
}

bool ModelSettings::setYue2Cpp(const Yue2CppConfig& config, QString* errorMessage)
{
    QJsonObject root = readRoot();
    QJsonObject object;
    object.insert(QStringLiteral("enginePath"), config.enginePath);
    object.insert(QStringLiteral("backbonePath"), config.backbonePath);
    object.insert(QStringLiteral("vaePath"), config.vaePath);
    object.insert(QStringLiteral("transcriberPath"), config.transcriberPath);
    object.insert(QStringLiteral("host"), config.host);
    object.insert(QStringLiteral("port"), config.port);
    object.insert(QStringLiteral("ggmlBackend"), config.ggmlBackend);
    root.insert(QStringLiteral("yue2cpp"), object);

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
            *errorMessage = QStringLiteral("Could not save the yue2.cpp engine settings");
        }
        return false;
    }
    return true;
}
}