/*
 * Audacity: A Digital Audio Editor
 */
#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "aimodels/modelsettings.h"

namespace au::aimodels {
namespace {
class ModelSettingsTests : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(m_dir.isValid());
        ModelSettings::setConfigFilePathForTesting(QDir(m_dir.path()).filePath("aimodels.json"));
    }

    void TearDown() override
    {
        ModelSettings::setConfigFilePathForTesting(QString());
    }

    QTemporaryDir m_dir;
};
}

TEST_F(ModelSettingsTests, ListsBuiltInProviderUnconfigured)
{
    const ProviderConfig yue2 = ModelSettings::provider("yue2-native");
    EXPECT_EQ(yue2.displayName, QStringLiteral("YuE2"));
    EXPECT_FALSE(yue2.isConfigured());
    EXPECT_TRUE(yue2.cliPath.isEmpty());
}

TEST_F(ModelSettingsTests, PersistsAndReloadsProviderPaths)
{
    ProviderConfig config = ModelSettings::provider("yue2-native");
    config.cliPath = QStringLiteral("D:/tools/audiocpp_cli.exe");
    config.modelPath = QStringLiteral("D:/models/yue2-q4");
    config.threads = 16;

    QString error;
    ASSERT_TRUE(ModelSettings::setProvider(config, &error)) << error.toStdString();
    EXPECT_TRUE(QFile::exists(ModelSettings::configFilePath()));

    const ProviderConfig reloaded = ModelSettings::provider("yue2-native");
    EXPECT_TRUE(reloaded.isConfigured());
    EXPECT_EQ(reloaded.cliPath, config.cliPath);
    EXPECT_EQ(reloaded.modelPath, config.modelPath);
    EXPECT_EQ(reloaded.threads, 16);
}

TEST_F(ModelSettingsTests, KeepsUnconfiguredFieldsAccountedFor)
{
    ProviderConfig config;
    config.id = "yue2-native";
    config.displayName = "YuE2";
    config.modelPath = QStringLiteral("D:/models/yue2-q4");
    QString error;
    ASSERT_TRUE(ModelSettings::setProvider(config, &error)) << error.toStdString();

    const ProviderConfig reloaded = ModelSettings::provider("yue2-native");
    EXPECT_FALSE(reloaded.isConfigured()); // no CLI path yet
    EXPECT_EQ(reloaded.modelPath, config.modelPath);
    EXPECT_TRUE(ModelSettings::providers().size() >= 1);
}

TEST_F(ModelSettingsTests, RejectsProvidersWithoutId)
{
    ProviderConfig config;
    QString error;
    EXPECT_FALSE(ModelSettings::setProvider(config, &error));
    EXPECT_FALSE(error.isEmpty());
}

TEST_F(ModelSettingsTests, PreservesExtraProviders)
{
    ProviderConfig other;
    other.id = "custom-provider";
    other.displayName = "Custom";
    other.cliPath = "cli";
    other.modelPath = "model";
    QString error;
    ASSERT_TRUE(ModelSettings::setProvider(other, &error)) << error.toStdString();

    const ProviderConfigList all = ModelSettings::providers();
    bool foundCustom = false;
    bool foundYue2 = false;
    for (const ProviderConfig& config : all) {
        foundCustom = foundCustom || config.id == "custom-provider";
        foundYue2 = foundYue2 || config.id == "yue2-native";
    }
    EXPECT_TRUE(foundCustom);
    EXPECT_TRUE(foundYue2);
}

TEST_F(ModelSettingsTests, PersistsAndReloadsAssistantConfig)
{
    AssistantConfig config;
    config.mode = QStringLiteral("local");
    config.baseUrl = QStringLiteral("https://example.test/v1");
    config.apiKey = QStringLiteral("secret");
    config.model = QStringLiteral("my-model");
    config.runnerPath = QStringLiteral("D:/tools/llama-server.exe");
    config.modelPath = QStringLiteral("D:/models/assistant.gguf");
    config.port = 8081;
    QString error;
    ASSERT_TRUE(ModelSettings::setAssistant(config, &error)) << error.toStdString();

    const AssistantConfig reloaded = ModelSettings::assistant();
    EXPECT_EQ(reloaded.mode, config.mode);
    EXPECT_EQ(reloaded.baseUrl, config.baseUrl);
    EXPECT_EQ(reloaded.apiKey, config.apiKey);
    EXPECT_EQ(reloaded.model, config.model);
    EXPECT_EQ(reloaded.runnerPath, config.runnerPath);
    EXPECT_EQ(reloaded.modelPath, config.modelPath);
    EXPECT_EQ(reloaded.port, config.port);

    // Saving provider settings must not drop the assistant config.
    ProviderConfig yue2 = ModelSettings::provider(QStringLiteral("yue2-native"));
    yue2.cliPath = QStringLiteral("cli");
    yue2.modelPath = QStringLiteral("model");
    ASSERT_TRUE(ModelSettings::setProvider(yue2, &error));
    EXPECT_EQ(ModelSettings::assistant().apiKey, QStringLiteral("secret"));
}

TEST_F(ModelSettingsTests, DetectsYue2CppInstall)
{
    QDir base(m_dir.path());
    ASSERT_TRUE(base.mkpath(QStringLiteral("resources/yue2-cpp")));
    ASSERT_TRUE(base.mkpath(QStringLiteral("data/models/yue2-cpp")));
    QFile engine(base.filePath(QStringLiteral("resources/yue2-cpp/yue-server.exe")));
    ASSERT_TRUE(engine.open(QIODevice::WriteOnly));
    engine.write("x");
    engine.close();
    for (const QString& name : { "YuE2-3B-BF16.gguf", "YuE2-Vae-F32.gguf", "SheetSage2-F32.gguf" }) {
        QFile file(base.filePath(QStringLiteral("data/models/yue2-cpp/") + name));
        ASSERT_TRUE(file.open(QIODevice::WriteOnly));
        file.write("g");
        file.close();
    }

    const Yue2CppConfig config = ModelSettings::detectYue2CppAt(m_dir.path());
    EXPECT_TRUE(config.isConfigured());
    EXPECT_TRUE(config.enginePath.endsWith(QStringLiteral("yue-server.exe")));
    EXPECT_TRUE(config.backbonePath.endsWith(QStringLiteral("YuE2-3B-BF16.gguf")));
    EXPECT_TRUE(config.vaePath.endsWith(QStringLiteral("YuE2-Vae-F32.gguf")));
    EXPECT_TRUE(config.transcriberPath.endsWith(QStringLiteral("SheetSage2-F32.gguf")));
}

TEST_F(ModelSettingsTests, PersistsYue2CppConfig)
{
    Yue2CppConfig config;
    config.enginePath = QStringLiteral("D:/engine/yue-server.exe");
    config.backbonePath = QStringLiteral("D:/models/YuE2-3B-Q8_0.gguf");
    config.vaePath = QStringLiteral("D:/models/YuE2-Vae-F32.gguf");
    config.ggmlBackend = QStringLiteral("CUDA0");
    QString error;
    ASSERT_TRUE(ModelSettings::setYue2Cpp(config, &error)) << error.toStdString();

    const Yue2CppConfig reloaded = ModelSettings::yue2Cpp();
    EXPECT_EQ(reloaded.enginePath, config.enginePath);
    EXPECT_EQ(reloaded.backbonePath, config.backbonePath);
    EXPECT_EQ(reloaded.vaePath, config.vaePath);
    EXPECT_EQ(reloaded.ggmlBackend, config.ggmlBackend);
    EXPECT_EQ(reloaded.port, 18087);
}

}