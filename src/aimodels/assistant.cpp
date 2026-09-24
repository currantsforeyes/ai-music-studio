/*
 * Audacity: A Digital Audio Editor
 */
#include "assistant.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace au::aimodels {
QByteArray buildChatRequest(const QString& model, const QString& systemPrompt, const QString& userPrompt)
{
    QJsonArray messages;
    messages.append(QJsonObject { { QStringLiteral("role"), QStringLiteral("system") },
                                  { QStringLiteral("content"), systemPrompt } });
    messages.append(QJsonObject { { QStringLiteral("role"), QStringLiteral("user") },
                                  { QStringLiteral("content"), userPrompt } });
    const QJsonObject body { { QStringLiteral("model"), model }, { QStringLiteral("messages"), messages } };
    return QJsonDocument(body).toJson(QJsonDocument::Compact);
}

bool parseChatResponse(const QByteArray& response, QString* content, QString* errorMessage)
{
    if (content) {
        content->clear();
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(response, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("The assistant returned an invalid response");
        }
        return false;
    }
    const QJsonObject object = document.object();
    if (object.contains(QStringLiteral("error"))) {
        const QJsonObject error = object.value(QStringLiteral("error")).toObject();
        if (errorMessage) {
            *errorMessage = error.value(QStringLiteral("message"))
                            .toString(QStringLiteral("The assistant returned an error"));
        }
        return false;
    }
    const QJsonArray choices = object.value(QStringLiteral("choices")).toArray();
    if (choices.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("The assistant returned no choices");
        }
        return false;
    }
    const QString text = choices.first().toObject()
                         .value(QStringLiteral("message")).toObject()
                         .value(QStringLiteral("content")).toString().trimmed();
    if (text.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("The assistant returned an empty answer");
        }
        return false;
    }
    if (content) {
        *content = text;
    }
    return true;
}
}