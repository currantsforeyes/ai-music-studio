/*
 * Audacity: A Digital Audio Editor
 */
#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include "aimodels/assistant.h"

namespace au::aimodels {
TEST(AssistantTests, BuildsChatRequest)
{
    const QByteArray json = buildChatRequest(QStringLiteral("model-x"),
                                             QStringLiteral("system text"),
                                             QStringLiteral("user text"));
    const QJsonObject object = QJsonDocument::fromJson(json).object();
    EXPECT_EQ(object.value(QStringLiteral("model")).toString(), QStringLiteral("model-x"));
    const QJsonArray messages = object.value(QStringLiteral("messages")).toArray();
    ASSERT_EQ(messages.size(), 2);
    EXPECT_EQ(messages.at(0).toObject().value(QStringLiteral("role")).toString(), QStringLiteral("system"));
    EXPECT_EQ(messages.at(0).toObject().value(QStringLiteral("content")).toString(), QStringLiteral("system text"));
    EXPECT_EQ(messages.at(1).toObject().value(QStringLiteral("role")).toString(), QStringLiteral("user"));
    EXPECT_EQ(messages.at(1).toObject().value(QStringLiteral("content")).toString(), QStringLiteral("user text"));
}

TEST(AssistantTests, ParsesChatResponse)
{
    const QByteArray response = R"({"choices":[{"message":{"content":"  hello world  "}}]})";
    QString content;
    QString error;
    ASSERT_TRUE(parseChatResponse(response, &content, &error)) << error.toStdString();
    EXPECT_EQ(content, QStringLiteral("hello world"));
}

TEST(AssistantTests, ReportsApiError)
{
    const QByteArray response = R"({"error":{"message":"bad key"}})";
    QString content;
    QString error;
    EXPECT_FALSE(parseChatResponse(response, &content, &error));
    EXPECT_EQ(error, QStringLiteral("bad key"));
}

TEST(AssistantTests, RejectsGarbage)
{
    QString content;
    QString error;
    EXPECT_FALSE(parseChatResponse("not json", &content, &error));
    EXPECT_FALSE(error.isEmpty());
}

}