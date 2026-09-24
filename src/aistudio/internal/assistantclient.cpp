/*
 * Audacity: A Digital Audio Editor
 */
#include "assistantclient.h"

#include "aimodels/assistant.h"
#include "aimodels/modelsettings.h"

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QTimer>
#include <QUrl>

namespace au::aistudio {
AssistantClient::AssistantClient(QObject* parent)
    : QObject(parent)
{
}

AssistantClient::~AssistantClient()
{
    stopRunner();
}

bool AssistantClient::isConfigured() const
{
    const au::aimodels::AssistantConfig config = au::aimodels::ModelSettings::assistant();
    if (config.mode == QLatin1String("local")) {
        return !config.runnerPath.trimmed().isEmpty() && !config.modelPath.trimmed().isEmpty() && config.port > 0;
    }
    return !config.baseUrl.trimmed().isEmpty()
           && !config.apiKey.trimmed().isEmpty()
           && !config.model.trimmed().isEmpty();
}

void AssistantClient::stopRunner()
{
    if (m_runner && m_runner->state() != QProcess::NotRunning) {
        m_runner->terminate();
        if (!m_runner->waitForFinished(1000)) {
            m_runner->kill();
            m_runner->waitForFinished(1000);
        }
    }
    m_runnerPort = 0;
    m_runnerModelPath.clear();
    m_runnerExecutable.clear();
}

bool AssistantClient::ensureLocalRunner(const QString& runnerPath, const QString& modelPath, int port, QString* errorMessage)
{
    if (m_runner && m_runner->state() != QProcess::NotRunning
        && m_runnerPort == port && m_runnerModelPath == modelPath && m_runnerExecutable == runnerPath) {
        return true;
    }
    stopRunner();
    if (!m_runner) {
        m_runner = new QProcess(this);
    }
    m_runner->setProcessChannelMode(QProcess::MergedChannels);
    const QStringList arguments {
        QStringLiteral("-m"), modelPath,
        QStringLiteral("--host"), QStringLiteral("127.0.0.1"),
        QStringLiteral("--port"), QString::number(port)
    };
    m_runner->start(runnerPath, arguments);
    if (!m_runner->waitForStarted(5000)) {
        if (errorMessage) {
            *errorMessage = tr("Could not start the local runner: %1").arg(runnerPath);
        }
        return false;
    }
    m_runnerPort = port;
    m_runnerModelPath = modelPath;
    m_runnerExecutable = runnerPath;
    return true;
}

void AssistantClient::request(const QString& systemPrompt, const QString& userPrompt, Callback callback)
{
    const au::aimodels::AssistantConfig config = au::aimodels::ModelSettings::assistant();

    QString baseUrl;
    QString apiKey;
    QString model;
    if (config.mode == QLatin1String("local")) {
        QString error;
        if (!ensureLocalRunner(config.runnerPath, config.modelPath, config.port, &error)) {
            if (callback) {
                callback(false, QString(), error);
            }
            return;
        }
        baseUrl = QStringLiteral("http://127.0.0.1:%1/v1").arg(config.port);
        apiKey = QStringLiteral("local");
        model = QStringLiteral("local");
    } else {
        if (config.baseUrl.trimmed().isEmpty() || config.apiKey.trimmed().isEmpty() || config.model.trimmed().isEmpty()) {
            if (callback) {
                callback(false, QString(), tr("Configure the writing assistant (base URL, model, API key) first"));
            }
            return;
        }
        baseUrl = config.baseUrl;
        apiKey = config.apiKey;
        model = config.model;
    }

    // Local runners need a moment before they accept connections; retry.
    sendChat(baseUrl, apiKey, model, systemPrompt, userPrompt, std::move(callback), 20);
}

void AssistantClient::sendChat(const QString& baseUrl, const QString& apiKey, const QString& model,
                               const QString& systemPrompt, const QString& userPrompt, Callback callback, int retriesLeft)
{
    QString base = baseUrl.trimmed();
    while (base.endsWith(QLatin1Char('/'))) {
        base.chop(1);
    }
    const QUrl url(base + QStringLiteral("/chat/completions"));

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(apiKey).toUtf8());

    QNetworkReply* reply = m_network.post(request,
                                          au::aimodels::buildChatRequest(model, systemPrompt, userPrompt));
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, baseUrl, apiKey, model, systemPrompt, userPrompt, callback, retriesLeft]() {
        reply->deleteLater();
        const QByteArray body = reply->readAll();
        if (reply->error() == QNetworkReply::ConnectionRefusedError && retriesLeft > 0) {
            QTimer::singleShot(500, this, [=]() {
                sendChat(baseUrl, apiKey, model, systemPrompt, userPrompt, callback, retriesLeft - 1);
            });
            return;
        }
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