/*
 * Audacity: A Digital Audio Editor
 */
#include "runtimehostsupervisor.h"

#include <QAbstractSocket>
#include <QCoreApplication>
#include <QDir>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QTcpSocket>
#include <QTimer>
#include <QUuid>

#include <utility>

using namespace au::aijobs;

RuntimeHostSupervisor::RuntimeHostSupervisor(QObject* parent)
    : QObject(parent), m_status(tr("Runtime host not started"))
{
    m_process = new QProcess(this);
    m_socket = new QTcpSocket(this);
    m_startupTimer = new QTimer(this);
    m_startupTimer->setSingleShot(true);

    connect(m_startupTimer, &QTimer::timeout, this, [this] {
        if (m_status != tr("Runtime host healthy")) {
            setStatus(tr("Runtime host did not become ready"));
        }
    });
    connect(m_process, &QProcess::errorOccurred, this, [this] {
        setStatus(tr("Runtime host failed to start"));
    });
    connect(m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int, QProcess::ExitStatus) {
        if (!m_activeJobId.isEmpty()) {
            const QString failedJobId = m_activeJobId;
            m_activeJobId.clear();
            emit testJobFailed(failedJobId);
            notifyJobStatus(aicore::JobState::Failed, failedJobId);
            setStatus(tr("Runtime host stopped while the test provider job was running"));
        }
    });
    connect(m_process, &QProcess::readyReadStandardOutput, this, [this] {
        while (m_process->canReadLine()) {
            const auto message = QJsonDocument::fromJson(m_process->readLine().trimmed()).object();
            const int port = message.value("port").toInt();
            if (port > 0 && message.value("protocolVersion").toInt() == au::aicore::AI_RUNTIME_PROTOCOL_VERSION) {
                connectHealthCheck(static_cast<quint16>(port));
            }
        }
    });
    connect(m_socket, &QTcpSocket::connected, this, [this] {
        writeRequest(QJsonObject {
            { "token", m_token },
            { "protocolVersion", au::aicore::AI_RUNTIME_PROTOCOL_VERSION },
            { "action", "health" }
        }, nullptr);
    });
    connect(m_socket, &QTcpSocket::readyRead, this, [this] {
        while (m_socket->canReadLine()) {
            const auto response = QJsonDocument::fromJson(m_socket->readLine().trimmed()).object();
            const QString code = response.value("code").toString();
            if (response.value("ok").toBool() && code == "healthy") {
                m_startupTimer->stop();
                setStatus(tr("Runtime host healthy"));
            } else if (response.value("ok").toBool() && code == "accepted") {
                m_activeJobId = response.value("jobId").toString();
                emit testJobAccepted(m_activeJobId);
                notifyJobStatus(au::aicore::JobState::Running, m_activeJobId);
                setStatus(tr("Test provider job running"));
            } else if (response.value("ok").toBool() && code == "complete") {
                const QString jobId = response.value("jobId").toString();
                const QString resultManifest = response.value("resultManifest").toString();
                m_activeJobId.clear();
                emit testJobCompleted(jobId, resultManifest);
                notifyJobStatus(au::aicore::JobState::Complete, jobId, resultManifest);
                setStatus(tr("Test provider job complete"));
            } else if (response.value("ok").toBool() && code == "cancelled") {
                const QString jobId = response.value("jobId").toString();
                m_activeJobId.clear();
                emit testJobCancelled(jobId);
                notifyJobStatus(au::aicore::JobState::Cancelled, jobId);
                setStatus(tr("Test provider job cancelled"));
            } else {
                setStatus(tr("Runtime host request failed"));
            }
        }
    });
}

RuntimeHostSupervisor::~RuntimeHostSupervisor()
{
    if (m_process->state() != QProcess::NotRunning) {
        m_process->terminate();
        if (!m_process->waitForFinished(1000)) {
            m_process->kill();
        }
    }
}

void RuntimeHostSupervisor::start()
{
    if (m_process->state() != QProcess::NotRunning) {
        return;
    }
#ifdef Q_OS_WIN
    const QString executableName = "ai_runtime_host.exe";
#else
    const QString executableName = "ai_runtime_host";
#endif
    const QString executable = QDir(QCoreApplication::applicationDirPath()).filePath(executableName);
    if (m_workspace.isEmpty()) {
        m_workspace = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                      .filePath("AI Music Studio/runtime-host");
    }
    QDir().mkpath(m_workspace);
    m_token = QUuid::createUuid().toString(QUuid::WithoutBraces);
    setStatus(tr("Starting local runtime host"));
    m_process->start(executable, { "--workspace", m_workspace, "--token", m_token });
    m_startupTimer->start(5000);
}

void RuntimeHostSupervisor::restartInWorkspace(const QString& workspace)
{
    if (workspace.isEmpty()) {
        return;
    }
    if (m_process->state() != QProcess::NotRunning) {
        m_process->terminate();
        if (!m_process->waitForFinished(1000)) {
            m_process->kill();
            m_process->waitForFinished(1000);
        }
    }
    m_socket->abort();
    m_workspace = workspace;
    start();
}

bool RuntimeHostSupervisor::isBusy() const
{
    return !m_activeJobId.isEmpty();
}

RuntimeStatus RuntimeHostSupervisor::status() const
{
    return m_healthy ? RuntimeStatus::Healthy : RuntimeStatus::Unavailable;
}

int RuntimeHostSupervisor::protocolVersion() const
{
    return au::aicore::AI_RUNTIME_PROTOCOL_VERSION;
}

void RuntimeHostSupervisor::setJobStatusHandler(JobStatusHandler handler)
{
    m_jobStatusHandler = std::move(handler);
}

bool RuntimeHostSupervisor::submit(const aicore::JobRequest& request, QString* errorMessage)
{
    if (m_socket->state() != QAbstractSocket::ConnectedState || !m_healthy) {
        if (errorMessage) {
            *errorMessage = tr("Runtime host is not ready for a provider job");
        }
        return false;
    }
    m_activeProviderId = QString::fromStdString(request.providerId);
    setStatus(tr("Submitting provider job"));
    return writeRequest(QJsonObject {
        { "token", m_token },
        { "protocolVersion", au::aicore::AI_RUNTIME_PROTOCOL_VERSION },
        { "action", "submit-job" },
        { "providerId", m_activeProviderId },
        { "parameters", QString::fromStdString(request.parametersJson) }
    }, errorMessage);
}

bool RuntimeHostSupervisor::cancel(const QString& jobId, QString* errorMessage)
{
    if (m_socket->state() != QAbstractSocket::ConnectedState || jobId.isEmpty()) {
        if (errorMessage) {
            *errorMessage = tr("No provider job is running");
        }
        return false;
    }
    setStatus(tr("Cancelling provider job"));
    return writeRequest(QJsonObject {
        { "token", m_token },
        { "protocolVersion", au::aicore::AI_RUNTIME_PROTOCOL_VERSION },
        { "action", "cancel" },
        { "jobId", jobId }
    }, errorMessage);
}

void RuntimeHostSupervisor::submitTestJob()
{
    QString error;
    if (!submit(aicore::JobRequest { "test-provider", "{}" }, &error)) {
        setStatus(error);
    }
}

void RuntimeHostSupervisor::submitFailureTest()
{
    if (m_socket->state() != QAbstractSocket::ConnectedState || !m_healthy) {
        setStatus(tr("Runtime host is not ready for a failure test"));
        return;
    }
    setStatus(tr("Submitting worker-failure test"));
    writeRequest(QJsonObject {
        { "token", m_token },
        { "protocolVersion", au::aicore::AI_RUNTIME_PROTOCOL_VERSION },
        { "action", "test-worker-failure" }
    }, nullptr);
}

void RuntimeHostSupervisor::cancelTestJob()
{
    if (m_activeJobId.isEmpty()) {
        setStatus(tr("No provider job is running"));
        return;
    }
    QString error;
    if (!cancel(m_activeJobId, &error)) {
        setStatus(error);
    }
}

QString RuntimeHostSupervisor::statusText() const
{
    return m_status;
}

void RuntimeHostSupervisor::setStatus(const QString& status)
{
    m_healthy = (status == tr("Runtime host healthy"));
    if (m_status == status) {
        return;
    }
    m_status = status;
    emit statusChanged(m_status);
}

bool RuntimeHostSupervisor::writeRequest(const QJsonObject& request, QString* errorMessage)
{
    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        if (errorMessage) {
            *errorMessage = tr("Runtime host is not connected");
        }
        return false;
    }
    m_socket->write(QJsonDocument(request).toJson(QJsonDocument::Compact) + '\n');
    return true;
}

void RuntimeHostSupervisor::notifyJobStatus(aicore::JobState state, const QString& jobId,
                                            const QString& resultManifest)
{
    if (!m_jobStatusHandler) {
        return;
    }
    aicore::JobStatus status;
    status.id.value = jobId.toStdString();
    status.providerId = (m_activeProviderId.isEmpty() ? QStringLiteral("test-provider") : m_activeProviderId).toStdString();
    status.state = state;
    status.resultManifest = resultManifest.toStdString();
    m_jobStatusHandler(status);
}

void RuntimeHostSupervisor::connectHealthCheck(quint16 port)
{
    if (m_socket->state() == QAbstractSocket::UnconnectedState) {
        m_socket->connectToHost(QHostAddress::LocalHost, port);
    }
}