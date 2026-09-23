/*
 * Audacity: A Digital Audio Editor
 */
#include <gtest/gtest.h>

#include <QByteArray>
#include <QStringLiteral>

#include "aijobs/yue2provider.h"

namespace au::aijobs {

TEST(Yue2ProviderTests, UsesDefaultsAndOverrides)
{
    Yue2JobParameters parameters;
    QString error;
    const QByteArray json = R"({ "text": "la la", "style": "pop", "seed": 42, "threads": 4 })";
    ASSERT_TRUE(parseYue2Parameters(json, QStringLiteral("cli.exe"), QStringLiteral("models"), 8, &parameters, &error))
        << error.toStdString();

    EXPECT_EQ(parameters.cliPath, QStringLiteral("cli.exe"));
    EXPECT_EQ(parameters.modelDirectory, QStringLiteral("models"));
    EXPECT_EQ(parameters.text, QStringLiteral("la la"));
    EXPECT_EQ(parameters.style, QStringLiteral("pop"));
    EXPECT_EQ(parameters.seed, 42);
    EXPECT_EQ(parameters.threads, 4);
    EXPECT_EQ(parameters.steps, 8);
    EXPECT_TRUE(parameters.cot);
}

TEST(Yue2ProviderTests, RejectsMissingLyricsAndBadJson)
{
    Yue2JobParameters parameters;
    QString error;
    EXPECT_FALSE(parseYue2Parameters(QByteArray("{}"), QStringLiteral("cli.exe"), QStringLiteral("models"), 8, &parameters, &error));
    EXPECT_FALSE(error.isEmpty());

    error.clear();
    EXPECT_FALSE(parseYue2Parameters(QByteArray("{ not json"), QStringLiteral("cli.exe"), QStringLiteral("models"), 8, &parameters, &error));
    EXPECT_FALSE(error.isEmpty());

    error.clear();
    EXPECT_FALSE(parseYue2Parameters(QByteArray(R"({ "text": "la" })"), QString(), QStringLiteral("models"), 8, &parameters, &error));
    EXPECT_FALSE(error.isEmpty());
}

TEST(Yue2ProviderTests, BuildsRequiredArguments)
{
    Yue2JobParameters parameters;
    parameters.cliPath = QStringLiteral("cli.exe");
    parameters.modelDirectory = QStringLiteral("D:/models/yue2-q4");
    parameters.text = QStringLiteral("hello");
    parameters.style = QStringLiteral("pop");
    parameters.seed = 7;
    parameters.steps = 8;
    parameters.threads = 4;

    const QStringList arguments = buildYue2Arguments(parameters, QStringLiteral("D:/job/output.wav"));
    EXPECT_TRUE(arguments.contains(QStringLiteral("--task")));
    EXPECT_TRUE(arguments.contains(QStringLiteral("gen")));
    EXPECT_TRUE(arguments.contains(QStringLiteral("yue2")));
    EXPECT_TRUE(arguments.contains(QStringLiteral("D:/models/yue2-q4")));
    EXPECT_TRUE(arguments.contains(QStringLiteral("D:/job/output.wav")));
    EXPECT_TRUE(arguments.contains(QStringLiteral("style=pop")));
    EXPECT_TRUE(arguments.contains(QStringLiteral("seed=7")));
    EXPECT_TRUE(arguments.contains(QStringLiteral("num_inference_steps=8")));
    EXPECT_TRUE(arguments.contains(QStringLiteral("yue2.model_gguf=yue2-3b-q4_0.gguf")));
    EXPECT_TRUE(arguments.contains(QStringLiteral("yue2.vae_gguf=yue2-vae-f16.gguf")));
    EXPECT_TRUE(arguments.contains(QStringLiteral("--out")));
    EXPECT_TRUE(arguments.contains(QStringLiteral("--log")));
}

TEST(Yue2ProviderTests, MapsLogLinesToProgress)
{
    const Yue2Progress semantic = yue2ProgressFromLogLine(QStringLiteral("[TIMING ts=20260923-093857] yue2.semantic_ms 51195.5626"));
    EXPECT_TRUE(semantic.recognized);
    EXPECT_DOUBLE_EQ(semantic.progress, 0.55);

    const Yue2Progress decode = yue2ProgressFromLogLine(QStringLiteral("[TIMING ts=20260923-090907] yue2.vae_decode_ms 3942"));
    EXPECT_TRUE(decode.recognized);
    EXPECT_DOUBLE_EQ(decode.progress, 0.95);

    EXPECT_FALSE(yue2ProgressFromLogLine(QStringLiteral("ggml_cuda_init: found 1 CUDA devices")).recognized);
}

}