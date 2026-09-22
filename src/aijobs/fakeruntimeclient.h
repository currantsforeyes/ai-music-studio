/*
 * Audacity: A Digital Audio Editor
 */
#pragma once

#include "iruntimeclient.h"

namespace au::aijobs {
// Deterministic client for UI and controller tests. It never reaches a process.
class FakeRuntimeClient final : public IRuntimeClient
{
public:
    RuntimeStatus status() const override;
    int protocolVersion() const override;
    bool isBusy() const override;
    void setJobStatusHandler(JobStatusHandler handler) override;
    bool submit(const aicore::JobRequest& request, QString* errorMessage = nullptr) override;
    bool cancel(const QString& jobId, QString* errorMessage = nullptr) override;

private:
    JobStatusHandler m_handler;
};
}