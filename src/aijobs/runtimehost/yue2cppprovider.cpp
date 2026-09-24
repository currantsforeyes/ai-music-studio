/*
 * Audacity: A Digital Audio Editor
 */
#include "yue2cppprovider.h"

#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTimer>
#include <QUrl>

namespace au::aijobs {
namespace {
QByteArray boundaryFrom(const QByteArray& contentType)
{
    const int index = contentType.indexOf("boundary=");
    if (index < 0) {
        return QByteArrayLiteral("yue2-batch-boundary");
    }
    QByteArray boundary = contentType.mid(index + 9).trimmed();
    if (boundary.startsWith('"')) {
        boundary = boundary.mid(1, boundary.size() - 2);
    }
    return boundary;
}

//! Splits a multipart/mixed body into its JSON and audio parts. The boundary
//! is ASCII and cannot occur inside the binary audio, so a byte scan is safe.
void parseMultipart(const QByteArray& body, const QByteArray& boundary,
                    QByteArray* jsonPart, QByteArray* audioPart, QByteArray* audioContentType)
{
    const QByteArray delimiter = "--" + boundary;
    int position = 0;
    while (true) {
        int start = body.indexOf(delimiter, position);
        if (start < 0) {
            break;
        }
        start += delimiter.size();
        if (body.mid(start, 2) == "\r\n") {
            start += 2;
        }
        const int headerEnd = body.indexOf("\r\n\r\n", start);
        if (headerEnd < 0) {
            break;
        }
        const QByteArray headers = body.mid(start, headerEnd - start).toLower();
        const int contentStart = headerEnd + 4;
        const int next = body.indexOf(delimiter, contentStart);
        int contentEnd = next < 0 ? body.size() : next;
        if (contentEnd >= 2 && body.mid(contentEnd - 2, 2) == "\r\n") {
            contentEnd -= 2;
        }
        const QByteArray content = body.mid(contentStart, contentEnd - contentStart);
        if (jsonPart && headers.contains("application/json")) {
            *jsonPart = content;
        } else if (audioPart && headers.contains("audio/")) {
            *audioPart = content;
            if (audioContentType) {
                *audioContentType = headers;
            }
        }
        if (next < 0) {
            break;
        }
        position = next;
    }
}

void waitMs(int milliseconds)
{
    QEventLoop loop;
    QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
    loop.exec();
}
}

Yue2CppRunner::Yue2CppRunner(Yue2CppOptions options, QObject* parent)
    : QObject(parent), m_options(std::move(options)), m_http(new QNetworkAccessManager(this))
{
}

Yue2CppRunner::~Yue2CppRunner()
{
    stopServer();
}

QString Yue2CppRunner::baseUrl() const
{
    return QStringLiteral("http://%1:%2").arg(m_options.host).arg(m_options.port);
}

void Yue2CppRunner::stopServer()
{
    if (m_server && m_server->state() != QProcess::NotRunning) {
        m_server->terminate();
        if (!m_server->waitForFinished(2000)) {
            m_server->kill();
            m_server->waitForFinished(2000);
        }
    }
}

QByteArray Yue2CppRunner::getBytes(const QString& path, QByteArray* contentType, QString* errorMessage, int timeoutMs)
{
    QNetworkRequest request(QUrl(baseUrl() + path));
    QNetworkReply* reply = m_http->get(request);
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();
    if (!reply->isFinished()) {
        reply->abort();
        if (errorMessage) {
            *errorMessage = QStringLiteral("Timed out talking to the yue2.cpp engine");
        }
        reply->deleteLater();
        return {};
    }
    const QByteArray body = reply->readAll();
    if (contentType) {
        *contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString().toUtf8();
    }
    if (reply->error() != QNetworkReply::NoError) {
        if (errorMessage) {
            *errorMessage = reply->errorString();
        }
        reply->deleteLater();
        return {};
    }
    reply->deleteLater();
    return body;
}

bool Yue2CppRunner::getJson(const QString& path, QJsonObject* out, QString* errorMessage, int timeoutMs)
{
    QByteArray contentType;
    const QByteArray body = getBytes(path, &contentType, errorMessage, timeoutMs);
    if (body.isEmpty()) {
        return false;
    }
    const QJsonDocument document = QJsonDocument::fromJson(body);
    if (!document.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("The yue2.cpp engine returned invalid JSON");
        }
        return false;
    }
    if (out) {
        *out = document.object();
    }
    return true;
}

bool Yue2CppRunner::postJson(const QString& path, const QByteArray& body, QJsonObject* out, QString* errorMessage)
{
    QNetworkRequest request(QUrl(baseUrl() + path));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    QNetworkReply* reply = m_http->post(request, body);
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(30000);
    loop.exec();
    if (!reply->isFinished()) {
        reply->abort();
        if (errorMessage) {
            *errorMessage = QStringLiteral("Timed out submitting to the yue2.cpp engine");
        }
        reply->deleteLater();
        return false;
    }
    const QByteArray responseBody = reply->readAll();
    const bool ok = reply->error() == QNetworkReply::NoError;
    if (!ok && errorMessage) {
        *errorMessage = reply->errorString();
    }
    reply->deleteLater();
    if (!ok) {
        return false;
    }
    const QJsonDocument document = QJsonDocument::fromJson(responseBody);
    if (out && document.isObject()) {
        *out = document.object();
    }
    return true;
}

bool Yue2CppRunner::waitForHealth(int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        QByteArray contentType;
        QString error;
        const QByteArray body = getBytes(QStringLiteral("/health"), &contentType, &error, 2000);
        if (body.contains("ok")) {
            return true;
        }
        waitMs(500);
    }
    return false;
}

bool Yue2CppRunner::ensureServer(QString* errorMessage)
{
    if (m_server && m_server->state() != QProcess::NotRunning && waitForHealth(500)) {
        return true;
    }
    // A yue-server the user runs by hand can be reused.
    if (waitForHealth(500)) {
        return true;
    }
    if (!m_options.isConfigured()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("The yue2.cpp engine is not configured (executable, backbone and VAE)");
        }
        return false;
    }
    stopServer();
    if (!m_server) {
        m_server = new QProcess(this);
    }
    QStringList arguments { QStringLiteral("--model"), m_options.backbone, QStringLiteral("--vae"), m_options.vae };
    if (!m_options.transcriber.isEmpty()) {
        arguments << QStringLiteral("--transcriber") << m_options.transcriber;
    }
    arguments << QStringLiteral("--host") << m_options.host << QStringLiteral("--port") << QString::number(m_options.port);

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    if (m_options.backend.trimmed().isEmpty()) {
        environment.remove(QStringLiteral("GGML_BACKEND"));
    } else {
        environment.insert(QStringLiteral("GGML_BACKEND"), m_options.backend.trimmed());
    }
    m_server->setProcessEnvironment(environment);
    m_server->setWorkingDirectory(QFileInfo(m_options.engine).absolutePath());
    m_server->start(m_options.engine, arguments);
    if (!m_server->waitForStarted(5000)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not start the yue2.cpp engine: %1").arg(m_options.engine);
        }
        return false;
    }
    if (!waitForHealth(300000)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("The yue2.cpp engine did not become healthy");
        }
        m_server->kill();
        return false;
    }
    return true;
}

bool Yue2CppRunner::generate(const QByteArray& requestJson, const QString& jobDirectory, const Progress& progress, QString* errorMessage)
{
    if (!ensureServer(errorMessage)) {
        return false;
    }
    if (progress) {
        progress(0.05, QStringLiteral("Starting the yue2.cpp engine"));
    }

    QJsonObject synthResponse;
    if (!postJson(QStringLiteral("/synth"), requestJson, &synthResponse, errorMessage)) {
        return false;
    }
    const QString remoteId = synthResponse.value(QStringLiteral("id")).toString();
    if (remoteId.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("The yue2.cpp engine returned no job id");
        }
        return false;
    }
    if (progress) {
        progress(0.1, QStringLiteral("Generating with yue2.cpp"));
    }

    QElapsedTimer timer;
    timer.start();
    const qint64 timeoutMs = 4LL * 60 * 60 * 1000;
    while (timer.elapsed() < timeoutMs) {
        QJsonObject status;
        QString statusError;
        if (!getJson(QStringLiteral("/job?id=%1").arg(remoteId), &status, &statusError, 15000)) {
            // transient: keep polling, the engine may be busy
        }
        const QString state = status.value(QStringLiteral("status")).toString();
        if (state == QLatin1String("done")) {
            break;
        }
        if (state == QLatin1String("failed") || state == QLatin1String("error") || state == QLatin1String("cancelled")) {
            if (errorMessage) {
                *errorMessage = status.value(QStringLiteral("error")).toString(QStringLiteral("The yue2.cpp job failed"));
            }
            return false;
        }
        if (progress) {
            const double fraction = qMin(0.85, 0.1 + (timer.elapsed() / 1000.0) / 120.0 * 0.75);
            progress(fraction, QStringLiteral("Generating with yue2.cpp"));
        }
        waitMs(2000);
    }

    QByteArray contentType;
    QString resultError;
    const QByteArray body = getBytes(QStringLiteral("/job?id=%1&result=1").arg(remoteId), &contentType, &resultError, 300000);
    if (body.isEmpty()) {
        if (errorMessage) {
            *errorMessage = resultError.isEmpty() ? QStringLiteral("The yue2.cpp job produced no result") : resultError;
        }
        return false;
    }

    QByteArray jsonPart;
    QByteArray audioPart;
    parseMultipart(body, boundaryFrom(contentType), &jsonPart, &audioPart, nullptr);
    if (jsonPart.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("The yue2.cpp result had no request part");
        }
        return false;
    }

    QDir().mkpath(jobDirectory);
    QFile replayFile(QDir(jobDirectory).filePath(QStringLiteral("replay.json")));
    if (replayFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        replayFile.write(jsonPart);
        replayFile.close();
    }
    const QJsonObject replay = QJsonDocument::fromJson(jsonPart).object();
    const QString abc = replay.value(QStringLiteral("abc")).toString();
    if (!abc.isEmpty()) {
        QFile scoreFile(QDir(jobDirectory).filePath(QStringLiteral("score.abc")));
        if (scoreFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            scoreFile.write(abc.toUtf8());
            scoreFile.close();
        }
    }
    if (!audioPart.isEmpty()) {
        QFile audioFile(QDir(jobDirectory).filePath(QStringLiteral("output.wav")));
        if (audioFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            audioFile.write(audioPart);
            audioFile.close();
        }
    }
    if (progress) {
        progress(0.99, QStringLiteral("yue2.cpp finished"));
    }
    return true;
}
}