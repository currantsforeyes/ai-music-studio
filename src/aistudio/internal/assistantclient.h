/*
 * Audacity: A Digital Audio Editor
 */
#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QString>

#include <functional>

namespace au::aistudio {
//! Calls an OpenAI-compatible chat-completions endpoint for the writing
//! assistant. Configuration lives in aimodels ModelSettings. Non-streaming.
class AssistantClient final : public QObject
{
    Q_OBJECT

public:
    using Callback = std::function<void(bool ok, const QString& content, const QString& error)>;

    explicit AssistantClient(QObject* parent = nullptr);

    bool isConfigured() const;
    void request(const QString& systemPrompt, const QString& userPrompt, Callback callback);

private:
    QNetworkAccessManager m_network;
};
}