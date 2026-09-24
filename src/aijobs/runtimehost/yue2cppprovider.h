/*
 * Audacity: A Digital Audio Editor
 */
#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QString>

#include <functional>

class QNetworkAccessManager;
class QProcess;

namespace au::aijobs {
//! Native yue2.cpp engine settings, as passed to the runtime host.
struct Yue2CppOptions {
    QString engine;
    QString backbone;
    QString vae;
    QString transcriber;
    QString host = QStringLiteral("127.0.0.1");
    int port = 18087;
    QString backend;

    bool isConfigured() const { return !engine.isEmpty() && !backbone.isEmpty() && !vae.isEmpty(); }
};

//! Runs one generation on a yue2.cpp `yue-server`: starts (or reuses) the
//! server, POSTs /synth, polls /job, and unpacks the multipart result into the
//! job directory (output.wav, score.abc, replay.json). Blocking; the caller
//! runs it from the host's event loop.
class Yue2CppRunner final : public QObject
{
public:
    using Progress = std::function<void(double, const QString& message)>;

    explicit Yue2CppRunner(Yue2CppOptions options, QObject* parent = nullptr);
    ~Yue2CppRunner() override;

    bool generate(const QByteArray& requestJson, const QString& jobDirectory, const Progress& progress, QString* errorMessage);
    void stopServer();

private:
    QString baseUrl() const;
    bool ensureServer(QString* errorMessage);
    bool waitForHealth(int timeoutMs);
    bool getJson(const QString& path, QJsonObject* out, QString* errorMessage, int timeoutMs = 30000);
    bool postJson(const QString& path, const QByteArray& body, QJsonObject* out, QString* errorMessage);
    QByteArray getBytes(const QString& path, QByteArray* contentType, QString* errorMessage, int timeoutMs = 120000);

    Yue2CppOptions m_options;
    QProcess* m_server = nullptr;
    QNetworkAccessManager* m_http = nullptr;
};
}