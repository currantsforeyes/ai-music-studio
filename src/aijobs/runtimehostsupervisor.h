/*
 * Audacity: A Digital Audio Editor
 */
#pragma once

#include "iruntimeclient.h"

#include <QObject>
#include <QString>

class QJsonObject;
class QProcess;
class QTcpSocket;
class QTimer;

namespace au::aijobs {
// Supervises the out-of-process ai_runtime_host and speaks the authenticated
// loopback protocol. It implements IRuntimeClient so the editor depends on the
// provider-neutral boundary rather than this concrete process manager.
class RuntimeHostSupervisor final : public QObject, public IRuntimeClient
{
    Q_OBJECT

public:
    explicit RuntimeHostSupervisor(QObject* parent = nullptr);
    ~RuntimeHostSupervisor() override;

    void start();
    void restartInWorkspace(const QString& workspace);
    //! Configure the native YuE2 provider from in-app settings. Empty values
    //! fall back to the AI_YUE2_* environment variables.
    void setProviderConfig(const QString& yue2Cli, const QString& yue2Model, const QString& yue2Threads);
    //! Configure the yue2.cpp engine (yue-server and its model files).
    void setYue2CppConfig(const QString& engine, const QString& backbone, const QString& vae,
                          const QString& transcriber, const QString& host, int port, const QString& backend);
    void submitTestJob();
    void submitFailureTest();
    QString statusText() const;

    // IRuntimeClient
    RuntimeStatus status() const override;
    int protocolVersion() const override;
    bool isBusy() const override;
    void setJobStatusHandler(JobStatusHandler handler) override;
    bool submit(const aicore::JobRequest& request, QString* errorMessage = nullptr) override;
    bool cancel(const QString& jobId, QString* errorMessage = nullptr) override;

signals:
    void statusChanged(const QString& status);
    void testJobCompleted(const QString& jobId, const QString& resultManifest);
    void testJobAccepted(const QString& jobId);
    void testJobCancelled(const QString& jobId);
    void testJobFailed(const QString& jobId);

private:
    void setStatus(const QString& status);
    void connectHealthCheck(quint16 port);
    bool writeRequest(const QJsonObject& request, QString* errorMessage);
    void notifyJobStatus(aicore::JobState state, const QString& jobId, const QString& resultManifest = QString());
    void emitJobStatus(const aicore::JobStatus& status);

    QProcess* m_process = nullptr;
    QTcpSocket* m_socket = nullptr;
    QTimer* m_startupTimer = nullptr;
    QString m_token;
    QString m_status;
    QString m_workspace;
    QString m_activeJobId;
    QString m_activeProviderId;
    QString m_yue2Cli;
    QString m_yue2Model;
    QString m_yue2Threads;
    QString m_yue2CppEngine;
    QString m_yue2CppBackbone;
    QString m_yue2CppVae;
    QString m_yue2CppTranscriber;
    QString m_yue2CppHost;
    int m_yue2CppPort = 18087;
    QString m_yue2CppBackend;
    bool m_healthy = false;
    JobStatusHandler m_jobStatusHandler;
};
}