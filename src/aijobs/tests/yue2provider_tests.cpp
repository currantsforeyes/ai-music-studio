/*
 * Audacity: A Digital Audio Editor
 */
#include <gtest/gtest.h>

#include <QByteArray>

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
    EXPECT_EQ(parameters.cot, QStringLiteral("full"));
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

    const QStringList arguments = buildYue2Arguments(parameters, QStringLiteral("D:/job/output.wav"), QStringLiteral("D:/job"));
    EXPECT_TRUE(arguments.contains(QStringLiteral("--task")));
    EXPECT_TRUE(arguments.contains(QStringLiteral("gen")));
    EXPECT_TRUE(arguments.contains(QStringLiteral("yue2")));
    EXPECT_TRUE(arguments.contains(QStringLiteral("D:/models/yue2-q4")));
    EXPECT_TRUE(arguments.contains(QStringLiteral("D:/job/output.wav")));
    EXPECT_TRUE(arguments.contains(QStringLiteral("--out-dir")));
    EXPECT_TRUE(arguments.contains(QStringLiteral("D:/job")));
    EXPECT_TRUE(arguments.contains(QStringLiteral("style=pop")));
    EXPECT_TRUE(arguments.contains(QStringLiteral("seed=7")));
    EXPECT_TRUE(arguments.contains(QStringLiteral("num_inference_steps=8")));
    EXPECT_TRUE(arguments.contains(QStringLiteral("yue2.model_gguf=yue2-3b-q4_0.gguf")));
    EXPECT_TRUE(arguments.contains(QStringLiteral("yue2.vae_gguf=yue2-vae-f16.gguf")));
    EXPECT_TRUE(arguments.contains(QStringLiteral("--out")));
    EXPECT_TRUE(arguments.contains(QStringLiteral("--log")));
}

TEST(Yue2ProviderTests, ReadsEngineFieldNamesAndScoreMode)
{
    Yue2JobParameters parameters;
    const QByteArray json = R"({ "lyrics": "la la", "style": "pop", "cot": "melody", "guidance_scale": 3.5, "steps": 12, "seed": 5 })";
    QString error;
    ASSERT_TRUE(parseYue2Parameters(json, QStringLiteral("cli.exe"), QStringLiteral("models"), 8, &parameters, &error))
        << error.toStdString();
    EXPECT_EQ(parameters.text, QStringLiteral("la la"));
    EXPECT_EQ(parameters.cot, QStringLiteral("melody"));
    EXPECT_DOUBLE_EQ(parameters.guidanceScale, 3.5);
    EXPECT_EQ(parameters.steps, 12);

    const QStringList arguments = buildYue2Arguments(parameters, QStringLiteral("out.wav"), QStringLiteral("D:/job"));
    EXPECT_TRUE(arguments.contains(QStringLiteral("cot=melody")));
    EXPECT_TRUE(arguments.contains(QStringLiteral("guidance_scale=3.5")));
    EXPECT_TRUE(arguments.contains(QStringLiteral("num_inference_steps=12")));
}

TEST(Yue2ProviderTests, PassesExtraRequestOptions)
{
    Yue2JobParameters parameters;
    const QByteArray json = R"({ "lyrics": "la", "style": "pop", "options": { "abc_temperature": 1.5, "semantic_top_p": 0.9 } })";
    QString error;
    ASSERT_TRUE(parseYue2Parameters(json, QStringLiteral("cli.exe"), QStringLiteral("models"), 8, &parameters, &error))
        << error.toStdString();
    EXPECT_EQ(parameters.extraOptions.value(QStringLiteral("abc_temperature")), QStringLiteral("1.5"));
    EXPECT_EQ(parameters.extraOptions.value(QStringLiteral("semantic_top_p")), QStringLiteral("0.9"));

    const QStringList arguments = buildYue2Arguments(parameters, QStringLiteral("out.wav"), QStringLiteral("D:/job"));
    EXPECT_TRUE(arguments.contains(QStringLiteral("abc_temperature=1.5")));
    EXPECT_TRUE(arguments.contains(QStringLiteral("semantic_top_p=0.9")));
}

TEST(Yue2ProviderTests, PassesAbcFileRequestOption)
{
    Yue2JobParameters parameters;
    parameters.cliPath = QStringLiteral("cli.exe");
    parameters.modelDirectory = QStringLiteral("models");
    parameters.text = QStringLiteral("la");
    parameters.abcFile = QStringLiteral("D:/job/plan.abc");

    QStringList arguments = buildYue2Arguments(parameters, QStringLiteral("out.wav"), QStringLiteral("D:/job"));
    EXPECT_TRUE(arguments.contains(QStringLiteral("abc_file=D:/job/plan.abc")));
    EXPECT_FALSE(arguments.contains(QStringLiteral("abc=D:/job/plan.abc")));

    parameters.abcFile.clear();
    parameters.abc = QStringLiteral("X:1\nK:C\nC|");
    arguments = buildYue2Arguments(parameters, QStringLiteral("out.wav"), QStringLiteral("D:/job"));
    EXPECT_TRUE(arguments.contains(QStringLiteral("abc=X:1\nK:C\nC|")));
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