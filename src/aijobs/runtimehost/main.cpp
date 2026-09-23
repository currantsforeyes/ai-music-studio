/*
 * Audacity: A Digital Audio Editor
 */
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPair>
#include <QPointer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStringList>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTextStream>
#include <QTimer>
#include <QUuid>

#include <cmath>
#include <cstring>

#include <algorithm>

#include "aijobs/yue2provider.h"

namespace {
constexpr int ProtocolVersion = 1;

QByteArray response(bool ok, const QString& code, const QJsonObject& values = {})
{
    QJsonObject payload = values;
    payload.insert("ok", ok);
    payload.insert("code", code);
    payload.insert("protocolVersion", ProtocolVersion);
    return QJsonDocument(payload).toJson(QJsonDocument::Compact) + '\n';
}

// CUDA runtime DLLs (cudart/cublas/cufft) are loaded dynamically by the
// audio.cpp CLI and live under the CUDA Toolkit, not next to the executable.
QStringList cudaRuntimeDirectories()
{
    QStringList result;
    const QString cudaPath = qEnvironmentVariable("CUDA_PATH");
    if (!cudaPath.isEmpty()) {
        result << QDir(cudaPath).filePath("bin/x64") << QDir(cudaPath).filePath("bin");
    }
    for (const char* variable : { "ProgramFiles", "ProgramFiles(x86)" }) {
        const QString root = qEnvironmentVariable(variable);
        if (root.isEmpty()) {
            continue;
        }
        const QDir cuda(root + "/NVIDIA GPU Computing Toolkit/CUDA");
        const QStringList versions = cuda.entryList({ "v*" }, QDir::Dirs, QDir::Name | QDir::Reversed);
        for (const QString& version : versions) {
            result << cuda.filePath(version + "/bin/x64") << cuda.filePath(version + "/bin");
        }
    }
    return result;
}

bool writeDeterministicWav(const QString& path)
{
    constexpr int sampleRate = 48000;
    constexpr int frames = sampleRate;
    QByteArray wav(44 + frames * 2, '\0');
    auto write16 = [&wav](int offset, quint16 value) {
        wav[offset] = static_cast<char>(value & 0xff);
        wav[offset + 1] = static_cast<char>((value >> 8) & 0xff);
    };
    auto write32 = [&wav](int offset, quint32 value) {
        for (int index = 0; index < 4; ++index) {
            wav[offset + index] = static_cast<char>((value >> (index * 8)) & 0xff);
        }
    };
    std::memcpy(wav.data(), "RIFF", 4);
    write32(4, static_cast<quint32>(wav.size() - 8));
    std::memcpy(wav.data() + 8, "WAVEfmt ", 8);
    write32(16, 16);
    write16(20, 1);
    write16(22, 1);
    write32(24, sampleRate);
    write32(28, sampleRate * 2);
    write16(32, 2);
    write16(34, 16);
    std::memcpy(wav.data() + 36, "data", 4);
    write32(40, frames * 2);
    for (int frame = 0; frame < frames; ++frame) {
        const double sample = std::sin((2.0 * M_PI * 440.0 * frame) / sampleRate) * 0.2;
        write16(44 + frame * 2, static_cast<quint16>(static_cast<qint16>(sample * 32767.0)));
    }
    QFile output(path);
    return output.open(QIODevice::WriteOnly | QIODevice::Truncate) && output.write(wav) == wav.size();
}

class RuntimeHost final : public QObject
{
public:
    RuntimeHost(QString token, QString workspace, QString yue2Cli, QString yue2Model, int yue2Threads)
        : m_token(std::move(token)), m_workspace(std::move(workspace)),
          m_yue2Cli(std::move(yue2Cli)), m_yue2Model(std::move(yue2Model)), m_yue2Threads(yue2Threads)
    {
        m_jobTimer.setSingleShot(true);
        connect(&m_jobTimer, &QTimer::timeout, this, [this] { finishTestJob(); });
        m_progressTimer.setInterval(250);
        connect(&m_progressTimer, &QTimer::timeout, this, [this] { sendTestProgress(); });
        connect(&m_server, &QTcpServer::newConnection, this, [this] {
            while (auto* socket = m_server.nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead, socket, [this, socket] {
                    while (socket->canReadLine()) {
                        const auto document = QJsonDocument::fromJson(socket->readLine().trimmed());
                        const auto request = document.object();
                        if (document.isNull() || request.value("token").toString() != m_token) {
                            socket->write(response(false, "unauthorized"));
                            continue;
                        }
                        if (request.value("protocolVersion").toInt() != ProtocolVersion) {
                            socket->write(response(false, "unsupported-protocol"));
                            continue;
                        }
                        const QString action = request.value("action").toString();
                        if (action == "health") {
                            socket->write(response(true, "healthy", { { "state", "healthy" } }));
                        } else if (action == "submit-job") {
                            handleSubmitJob(socket, request);
                        } else if (action == "test-job") {
                            startTestJob(socket);
                        } else if (action == "test-worker-failure") {
                            startFailureTest(socket);
                        } else if (action == "cancel") {
                            cancelJob(socket, request.value("jobId").toString());
                        } else {
                            socket->write(response(false, "unknown-action"));
                        }
                    }
                });
            }
        });
    }

    bool listen()
    {
        return m_server.listen(QHostAddress::LocalHost, 0);
    }

    quint16 port() const { return m_server.serverPort(); }

private:
    void handleSubmitJob(QTcpSocket* socket, const QJsonObject& request)
    {
        const QString providerId = request.value("providerId").toString();
        if (providerId == "test-provider") {
            startTestJob(socket);
        } else if (providerId == "yue2-native") {
            startYue2Job(socket, request);
        } else {
            socket->write(response(false, "unknown-provider", { { "providerId", providerId } }));
        }
    }

    void sendProgressValue(const QString& jobId, double progress, const QString& message)
    {
        if (!m_activeJobSocket) {
            return;
        }
        m_activeJobSocket->write(response(true, "progress", {
            { "jobId", jobId },
            { "state", "running" },
            { "progress", progress },
            { "message", message }
        }));
    }

    // --- test provider -----------------------------------------------------

    void startTestJob(QTcpSocket* socket)
    {
        if (!m_activeJobId.isEmpty()) {
            socket->write(response(false, "job-already-running", { { "jobId", m_activeJobId } }));
            return;
        }
        m_activeJobId = "test-provider-job";
        m_activeJobSocket = socket;
        m_cancelled = false;
        socket->write(response(true, "accepted", { { "jobId", m_activeJobId }, { "state", "running" } }));
        m_progressStep = 0;
        m_jobTimer.start(1500);
        m_progressTimer.start();
    }

    void sendTestProgress()
    {
        if (m_activeJobId.isEmpty()) {
            return;
        }
        m_progressStep = std::min(m_progressStep + 1, 6);
        const double progress = std::min(0.9, m_progressStep * 0.15);
        sendProgressValue(m_activeJobId, progress, QString("Rendering %1%").arg(int(progress * 100.0)));
    }

    void startFailureTest(QTcpSocket* socket)
    {
        if (!m_activeJobId.isEmpty()) {
            socket->write(response(false, "job-already-running", { { "jobId", m_activeJobId } }));
            return;
        }
        m_activeJobId = "test-worker-failure-job";
        m_activeJobSocket = socket;
        socket->write(response(true, "accepted", { { "jobId", m_activeJobId }, { "state", "running" } }));
        QTimer::singleShot(500, this, [] { QCoreApplication::exit(70); });
    }

    void finishTestJob()
    {
        if (m_activeJobId.isEmpty()) {
            return;
        }
        m_progressTimer.stop();
        const QString jobId = m_activeJobId;
        QTcpSocket* socket = m_activeJobSocket;
        m_activeJobId.clear();
        m_activeJobSocket = nullptr;
        const QString jobDirectory = QDir(m_workspace).filePath("jobs/" + jobId);
        if (!QDir().mkpath(jobDirectory)) {
            if (socket) {
                socket->write(response(false, "workspace-create-failed", { { "jobId", jobId } }));
            }
            return;
        }
        const QString wavPath = QDir(jobDirectory).filePath("output.wav");
        if (!writeDeterministicWav(wavPath)) {
            if (socket) {
                socket->write(response(false, "wav-write-failed", { { "jobId", jobId } }));
            }
            return;
        }
        const QJsonObject manifest {
            { "protocolVersion", ProtocolVersion },
            { "jobId", jobId },
            { "providerId", "test-provider" },
            { "state", "complete" },
            { "asset", "output.wav" }
        };
        QFile manifestFile(QDir(jobDirectory).filePath("result.json"));
        if (!manifestFile.open(QIODevice::WriteOnly | QIODevice::Truncate)
            || manifestFile.write(QJsonDocument(manifest).toJson(QJsonDocument::Compact)) < 1) {
            if (socket) {
                socket->write(response(false, "manifest-write-failed", { { "jobId", jobId } }));
            }
            return;
        }
        if (socket) {
            socket->write(response(true, "complete", { { "jobId", jobId }, { "resultManifest", "jobs/test-provider-job/result.json" } }));
        }
    }

    // --- native YuE2 (audio.cpp) provider ----------------------------------

    QString providerEnvironmentPath() const
    {
        QString path = QProcessEnvironment::systemEnvironment().value("PATH");
        for (const QString& directory : cudaRuntimeDirectories()) {
            if (QDir(directory).exists()) {
                path = directory + QDir::listSeparator() + path;
            }
        }
        return path;
    }

    void startYue2Job(QTcpSocket* socket, const QJsonObject& request)
    {
        if (!m_activeJobId.isEmpty()) {
            socket->write(response(false, "job-already-running", { { "jobId", m_activeJobId } }));
            return;
        }
        au::aijobs::Yue2JobParameters parameters;
        QString error;
        if (!au::aijobs::parseYue2Parameters(request.value("parameters").toString().toUtf8(),
                                             m_yue2Cli, m_yue2Model, m_yue2Threads, &parameters, &error)) {
            socket->write(response(false, "invalid-parameters", { { "message", error } }));
            return;
        }

        const QString jobId = QStringLiteral("yue2-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
        const QString jobDirectory = QDir(m_workspace).filePath("jobs/" + jobId);
        if (!QDir().mkpath(jobDirectory)) {
            socket->write(response(false, "workspace-create-failed", { { "jobId", jobId } }));
            return;
        }
        const QString outputPath = QDir(jobDirectory).filePath("output.wav");

        m_activeJobId = jobId;
        m_activeJobSocket = socket;
        m_providerOutputPath = outputPath;
        m_providerLog.clear();
        m_providerArtifacts.clear();
        m_cancelled = false;
        socket->write(response(true, "accepted", { { "jobId", jobId }, { "state", "running" } }));
        sendProgressValue(jobId, 0.05, QStringLiteral("Starting YuE2"));

        m_providerProcess = new QProcess(this);
        m_providerProcess->setWorkingDirectory(jobDirectory);
        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        environment.insert("PATH", providerEnvironmentPath());
        m_providerProcess->setProcessEnvironment(environment);
        connect(m_providerProcess, &QProcess::readyReadStandardOutput, this, [this] { drainYue2Stdout(); });
        connect(m_providerProcess, &QProcess::readyReadStandardError, this, [this] { drainYue2Stderr(); });
        connect(m_providerProcess, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
                [this](int exitCode, QProcess::ExitStatus) { finishYue2Job(exitCode); });
        m_providerProcess->start(parameters.cliPath,
                                 au::aijobs::buildYue2Arguments(parameters, outputPath, jobDirectory));
        if (!m_providerProcess->waitForStarted(5000)) {
            socket->write(response(false, "provider-start-failed", { { "jobId", jobId } }));
            m_providerProcess->deleteLater();
            m_providerProcess = nullptr;
            m_activeJobId.clear();
            m_activeJobSocket = nullptr;
            m_providerOutputPath.clear();
        }
    }

    void drainYue2Stdout()
    {
        if (!m_providerProcess) {
            return;
        }
        while (m_providerProcess->canReadLine()) {
            const QString line = QString::fromUtf8(m_providerProcess->readLine()).trimmed();
            if (line.isEmpty() || m_activeJobId.isEmpty()) {
                continue;
            }
            const au::aijobs::Yue2Progress progress = au::aijobs::yue2ProgressFromLogLine(line);
            if (progress.recognized) {
                sendProgressValue(m_activeJobId, progress.progress, progress.message);
            }
            // The CLI prints artifact_out[<id>]=<path> for each persisted artifact.
            if (line.startsWith(QStringLiteral("artifact_out["))) {
                const int close = line.indexOf(']');
                const int equals = close >= 0 ? line.indexOf('=', close + 1) : -1;
                if (close > 13 && equals > close) {
                    m_providerArtifacts.append({ line.mid(13, close - 13), line.mid(equals + 1) });
                }
            }
        }
    }

    void drainYue2Stderr()
    {
        if (!m_providerProcess) {
            return;
        }
        const QString text = QString::fromUtf8(m_providerProcess->readAllStandardError());
        for (const QString& line : text.split('\n', Qt::SkipEmptyParts)) {
            m_providerLog.append(line.trimmed());
        }
        constexpr int maximumLines = 20;
        while (m_providerLog.size() > maximumLines) {
            m_providerLog.removeFirst();
        }
    }

    void finishYue2Job(int exitCode)
    {
        drainYue2Stdout();
        drainYue2Stderr();
        QProcess* process = m_providerProcess;
        m_providerProcess = nullptr;
        if (process) {
            process->deleteLater();
        }

        const QString jobId = m_activeJobId;
        QTcpSocket* socket = m_activeJobSocket;
        const QString outputPath = m_providerOutputPath;
        const bool cancelled = m_cancelled;
        m_activeJobId.clear();
        m_activeJobSocket = nullptr;
        m_providerOutputPath.clear();
        m_cancelled = false;
        if (jobId.isEmpty() || cancelled) {
            return;
        }

        const QString jobDirectory = QDir(m_workspace).filePath("jobs/" + jobId);
        if (exitCode == 0 && QFileInfo::exists(outputPath)) {
            QJsonArray artifacts;
            for (const auto& artifact : m_providerArtifacts) {
                artifacts.append(QJsonObject {
                    { "id", artifact.first },
                    { "path", QDir(m_workspace).relativeFilePath(artifact.second) }
                });
            }
            const QJsonObject manifest {
                { "protocolVersion", ProtocolVersion },
                { "jobId", jobId },
                { "providerId", "yue2-native" },
                { "state", "complete" },
                { "asset", "output.wav" },
                { "artifacts", artifacts }
            };
            QFile manifestFile(QDir(jobDirectory).filePath("result.json"));
            if (!manifestFile.open(QIODevice::WriteOnly | QIODevice::Truncate)
                || manifestFile.write(QJsonDocument(manifest).toJson(QJsonDocument::Compact)) < 1) {
                if (socket) {
                    socket->write(response(false, "manifest-write-failed", { { "jobId", jobId } }));
                }
                return;
            }
            if (socket) {
                socket->write(response(true, "complete", { { "jobId", jobId }, { "resultManifest", "jobs/" + jobId + "/result.json" } }));
            }
        } else if (socket) {
            socket->write(response(false, "failed", {
                { "jobId", jobId }, { "exitCode", exitCode }, { "log", m_providerLog.join('\n') }
            }));
        }
    }

    // --- shared cancellation ----------------------------------------------

    void cancelJob(QTcpSocket* socket, const QString& jobId)
    {
        if (m_activeJobId.isEmpty() || jobId != m_activeJobId) {
            socket->write(response(false, "job-not-running", { { "jobId", jobId } }));
            return;
        }
        const QString activeJobId = m_activeJobId;
        if (m_providerProcess) {
            QProcess* process = m_providerProcess;
            m_providerProcess = nullptr;
            process->disconnect(this);
            process->kill();
            process->deleteLater();
        } else {
            m_jobTimer.stop();
            m_progressTimer.stop();
        }
        m_cancelled = true;
        m_activeJobId.clear();
        m_activeJobSocket = nullptr;
        m_providerOutputPath.clear();
        socket->write(response(true, "cancelled", { { "jobId", activeJobId }, { "state", "cancelled" } }));
    }

    QString m_token;
    QString m_workspace;
    QString m_yue2Cli;
    QString m_yue2Model;
    int m_yue2Threads = 8;
    QTcpServer m_server;
    QTimer m_jobTimer;
    QTimer m_progressTimer;
    int m_progressStep = 0;
    QString m_activeJobId;
    QPointer<QTcpSocket> m_activeJobSocket;
    QProcess* m_providerProcess = nullptr;
    QString m_providerOutputPath;
    QStringList m_providerLog;
    QList<QPair<QString, QString>> m_providerArtifacts;
    bool m_cancelled = false;
};

int selfTest(const QString& workspace)
{
    const QString token = "self-test-token";
    RuntimeHost host(token, workspace, {}, {}, 8);
    if (!host.listen()) {
        return 10;
    }

    QTcpSocket client;
    client.connectToHost(QHostAddress::LocalHost, host.port());
    if (!client.waitForConnected(2000)) {
        return 11;
    }
    const auto waitForResponse = [&client] {
        QElapsedTimer timer;
        timer.start();
        while (!client.canReadLine() && timer.elapsed() < 2000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        }
        return client.canReadLine() ? QJsonDocument::fromJson(client.readLine().trimmed()).object() : QJsonObject {};
    };
    const auto send = [&client, &waitForResponse](const QJsonObject& request) {
        client.write(QJsonDocument(request).toJson(QJsonDocument::Compact) + '\n');
        client.flush();
        return waitForResponse();
    };
    const auto denied = send({ { "token", "wrong" }, { "protocolVersion", ProtocolVersion }, { "action", "health" } });
    if (denied.value("code").toString() != "unauthorized") {
        return 12;
    }
    const auto accepted = send({ { "token", token }, { "protocolVersion", ProtocolVersion }, { "action", "test-job" } });
    if (accepted.value("code").toString() != "accepted") {
        return 13;
    }
    const auto completed = waitForResponse();
    if (completed.value("code").toString() != "complete") {
        return 14;
    }
    const QDir output(workspace);
    return QFile::exists(output.filePath("jobs/test-provider-job/output.wav"))
           && QFile::exists(output.filePath("jobs/test-provider-job/result.json")) ? 0 : 15;
}
}

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    QCommandLineParser parser;
    parser.addOption({ "workspace", "Workspace for job output.", "path" });
    parser.addOption({ "token", "Required session token.", "token" });
    parser.addOption({ "yue2-cli", "Path to the native YuE2 (audio.cpp) audiocpp_cli executable.", "path" });
    parser.addOption({ "yue2-model", "Directory holding the YuE2 GGUF model and sidecars.", "path" });
    parser.addOption({ "yue2-threads", "Worker threads for the native YuE2 provider.", "n", "8" });
    parser.addOption(QCommandLineOption("self-test", "Run authenticated loopback and test-provider verification."));
    parser.process(application);

    const QString workspace = parser.value("workspace");
    if (workspace.isEmpty()) {
        return 2;
    }
    if (parser.isSet("self-test")) {
        return selfTest(workspace);
    }
    const QString token = parser.value("token");
    if (token.isEmpty()) {
        return 3;
    }
    RuntimeHost host(token, workspace, parser.value("yue2-cli"), parser.value("yue2-model"),
                     parser.value("yue2-threads").toInt());
    if (!host.listen()) {
        return 4;
    }
    QTextStream(stdout) << QJsonDocument(QJsonObject { { "port", host.port() }, { "protocolVersion", ProtocolVersion } }).toJson(QJsonDocument::Compact)
                        << Qt::endl;
    return application.exec();
}