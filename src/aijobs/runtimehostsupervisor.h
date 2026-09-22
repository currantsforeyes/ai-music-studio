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
    void submitTestJob();
    void submitFailureTest();
    void cancelTestJob();
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

    QProcess* m_process = nullptr;
    QTcpSocket* m_socket = nullptr;
    QTimer* m_startupTimer = nullptr;
    QString m_token;
    QString m_status;
    QString m_workspace;
    QString m_activeJobId;
    QString m_activeProviderId;
    bool m_healthy = false;
    JobStatusHandler m_jobStatusHandler;
};
}