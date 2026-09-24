/*
 * Audacity: A Digital Audio Editor
 */
#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QString>

#include <functional>

class QProcess;

namespace au::aistudio {
//! Calls an OpenAI-compatible chat-completions endpoint for the writing
//! assistant. In "cloud" mode it posts to the configured endpoint; in "local"
//! mode it launches a runner (e.g. llama-server) with the given model file and
//! posts to that runner on 127.0.0.1. Non-streaming. Configuration lives in
//! aimodels ModelSettings.
class AssistantClient final : public QObject
{
    Q_OBJECT

public:
    using Callback = std::function<void(bool ok, const QString& content, const QString& error)>;

    explicit AssistantClient(QObject* parent = nullptr);
    ~AssistantClient() override;

    bool isConfigured() const;
    void request(const QString& systemPrompt, const QString& userPrompt, Callback callback);

private:
    void sendChat(const QString& baseUrl, const QString& apiKey, const QString& model,
                  const QString& systemPrompt, const QString& userPrompt, Callback callback, int retriesLeft);
    bool ensureLocalRunner(const QString& runnerPath, const QString& modelPath, int port, QString* errorMessage);
    void stopRunner();

    QNetworkAccessManager m_network;
    QProcess* m_runner = nullptr;
    int m_runnerPort = 0;
    QString m_runnerModelPath;
    QString m_runnerExecutable;
};
}