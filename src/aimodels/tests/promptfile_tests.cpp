/*
 * Audacity: A Digital Audio Editor
 */
#include <gtest/gtest.h>

#include <QString>

#include "aimodels/promptfile.h"

namespace au::aimodels {
namespace {
PromptFields sample()
{
    PromptFields fields;
    fields.style = QStringLiteral("indie pop, 82 BPM");
    fields.lyrics = QStringLiteral("[Verse]\nhello\n[Chorus]\nworld");
    fields.title = QStringLiteral("Juliet In Blue");
    fields.cot = QStringLiteral("full");
    fields.seed = QStringLiteral("42");
    fields.steps = QStringLiteral("8");
    fields.guidanceScale = QStringLiteral("3.5");
    fields.abcTemperature = QStringLiteral("1.5");
    fields.abcTopP = QStringLiteral("0.95");
    fields.abcTopK = QStringLiteral("40");
    fields.abcRepetitionPenalty = QStringLiteral("1.1");
    fields.semanticTemperature = QStringLiteral("1.2");
    fields.semanticTopP = QStringLiteral("0.9");
    fields.semanticTopK = QStringLiteral("50");
    fields.semanticRepetitionPenalty = QStringLiteral("1.05");
    return fields;
}
}

TEST(PromptFileTests, JsonRoundTrip)
{
    const PromptFields fields = sample();
    const QString json = serializePrompt(fields, false);

    PromptFields parsed;
    QString error;
    ASSERT_TRUE(parsePrompt(json, false, &parsed, &error)) << error.toStdString();

    EXPECT_EQ(parsed.style, fields.style);
    EXPECT_EQ(parsed.lyrics, fields.lyrics);
    EXPECT_EQ(parsed.title, fields.title);
    EXPECT_EQ(parsed.cot, fields.cot);
    EXPECT_EQ(parsed.seed, fields.seed);
    EXPECT_EQ(parsed.steps, fields.steps);
    EXPECT_EQ(parsed.guidanceScale, fields.guidanceScale);
    EXPECT_EQ(parsed.abcTemperature, fields.abcTemperature);
    EXPECT_EQ(parsed.abcTopP, fields.abcTopP);
    EXPECT_EQ(parsed.abcTopK, fields.abcTopK);
    EXPECT_EQ(parsed.abcRepetitionPenalty, fields.abcRepetitionPenalty);
    EXPECT_EQ(parsed.semanticTemperature, fields.semanticTemperature);
    EXPECT_EQ(parsed.semanticTopP, fields.semanticTopP);
    EXPECT_EQ(parsed.semanticTopK, fields.semanticTopK);
    EXPECT_EQ(parsed.semanticRepetitionPenalty, fields.semanticRepetitionPenalty);
}

TEST(PromptFileTests, YamlRoundTrip)
{
    const PromptFields fields = sample();
    const QString yaml = serializePrompt(fields, true);
    EXPECT_NE(yaml.indexOf(QStringLiteral("lyrics: |-")), -1);

    PromptFields parsed;
    QString error;
    ASSERT_TRUE(parsePrompt(yaml, true, &parsed, &error)) << error.toStdString();

    EXPECT_EQ(parsed.style, fields.style);
    EXPECT_EQ(parsed.lyrics, fields.lyrics);
    EXPECT_EQ(parsed.title, fields.title);
    EXPECT_EQ(parsed.cot, fields.cot);
    EXPECT_EQ(parsed.seed, fields.seed);
    EXPECT_EQ(parsed.steps, fields.steps);
    EXPECT_EQ(parsed.guidanceScale, fields.guidanceScale);
    EXPECT_EQ(parsed.abcTemperature, fields.abcTemperature);
    EXPECT_EQ(parsed.abcTopP, fields.abcTopP);
    EXPECT_EQ(parsed.abcTopK, fields.abcTopK);
    EXPECT_EQ(parsed.abcRepetitionPenalty, fields.abcRepetitionPenalty);
    EXPECT_EQ(parsed.semanticTemperature, fields.semanticTemperature);
    EXPECT_EQ(parsed.semanticTopP, fields.semanticTopP);
    EXPECT_EQ(parsed.semanticTopK, fields.semanticTopK);
    EXPECT_EQ(parsed.semanticRepetitionPenalty, fields.semanticRepetitionPenalty);
}

TEST(PromptFileTests, SparseOutputOmitsEmptyFields)
{
    PromptFields fields;
    fields.style = QStringLiteral("pop");
    const QString json = serializePrompt(fields, false);
    EXPECT_FALSE(json.contains(QStringLiteral("lyrics")));
    EXPECT_FALSE(json.contains(QStringLiteral("seed")));
}

TEST(PromptFileTests, RejectsMalformedJson)
{
    PromptFields parsed;
    QString error;
    EXPECT_FALSE(parsePrompt(QStringLiteral("{ not json"), false, &parsed, &error));
    EXPECT_FALSE(error.isEmpty());
}

TEST(PromptFileTests, YamlReadsQuotedScalars)
{
    const QString yaml = QStringLiteral("style: \"pop \\\"quoted\\\"\"\ncot: melody\nseed: 7\n");
    PromptFields parsed;
    QString error;
    ASSERT_TRUE(parsePrompt(yaml, true, &parsed, &error)) << error.toStdString();
    EXPECT_EQ(parsed.style, QStringLiteral("pop \"quoted\""));
    EXPECT_EQ(parsed.cot, QStringLiteral("melody"));
    EXPECT_EQ(parsed.seed, QStringLiteral("7"));
}

}