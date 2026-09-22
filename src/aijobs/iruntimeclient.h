/*
 * Audacity: A Digital Audio Editor
 */
#pragma once

#include "aicore/aicoretypes.h"

#include <functional>

#include <QString>

namespace au::aijobs {
enum class RuntimeStatus {
    Unavailable,
    Healthy,
};

// Receives every lifecycle change for any submitted job, provider-neutral.
using JobStatusHandler = std::function<void(const aicore::JobStatus&)>;

// The editor-side view of the local runtime host. Implementations are
// asynchronous: submit() only enqueues, and results arrive through the handler.
class IRuntimeClient
{
public:
    virtual ~IRuntimeClient() = default;

    virtual RuntimeStatus status() const = 0;
    virtual int protocolVersion() const = 0;
    virtual bool isBusy() const = 0;

    virtual void setJobStatusHandler(JobStatusHandler handler) = 0;

    virtual bool submit(const aicore::JobRequest& request, QString* errorMessage = nullptr) = 0;
    virtual bool cancel(const QString& jobId, QString* errorMessage = nullptr) = 0;
};
}