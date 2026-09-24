/*
 * Audacity: A Digital Audio Editor
 */
#include "assistantclient.h"

#include "aimodels/assistant.h"
#include "aimodels/modelsettings.h"

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace au::aistudio {
AssistantClient::AssistantClient(QObject* parent)
    : QObject(parent)
{
}

bool AssistantClient::isConfigured() const
{
    const au::aimodels::AssistantConfig config = au::aimodels::ModelSettings::assistant();
    return !config.baseUrl.trimmed().isEmpty()
           && !config.apiKey.trimmed().isEmpty()
           && !config.model.trimmed().isEmpty();
}

void AssistantClient::request(const QString& systemPrompt, const QString& userPrompt, Callback callback)
{
    const au::aimodels::AssistantConfig config = au::aimodels::ModelSettings::assistant();
    if (config.baseUrl.trimmed().isEmpty() || config.apiKey.trimmed().isEmpty() || config.model.trimmed().isEmpty()) {
        if (callback) {
            callback(false, QString(), tr("Configure the writing assistant (base URL, model, API key) first"));
        }
        return;
    }

    QString base = config.baseUrl.trimmed();
    while (base.endsWith(QLatin1Char('/'))) {
        base.chop(1);
    }
    const QUrl url(base + QStringLiteral("/chat/completions"));

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(config.apiKey).toUtf8());

    QNetworkReply* reply = m_network.post(request,
                                          au::aimodels::buildChatRequest(config.model, systemPrompt, userPrompt));
    connect(reply, &QNetworkReply::finished, this, [reply, callback = std::move(callback)]() {
        reply->deleteLater();
        const QByteArray body = reply->readAll();
        QString content;
        QString error;
        if (reply->error() != QNetworkReply::NoError) {
            if (!au::aimodels::parseChatResponse(body, nullptr, &error) || error.isEmpty()) {
                error = reply->errorString();
            }
            if (callback) {
                callback(false, QString(), error);
            }
            return;
        }
        if (!au::aimodels::parseChatResponse(body, &content, &error)) {
            if (callback) {
                callback(false, QString(), error);
            }
            return;
        }
        if (callback) {
            callback(true, content, QString());
        }
    });
}
}