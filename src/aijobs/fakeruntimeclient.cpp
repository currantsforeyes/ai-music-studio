/*
 * Audacity: A Digital Audio Editor
 */
#include "fakeruntimeclient.h"

#include <QObject>

#include <utility>

using namespace au::aijobs;

RuntimeStatus FakeRuntimeClient::status() const
{
    return RuntimeStatus::Unavailable;
}

int FakeRuntimeClient::protocolVersion() const
{
    return au::aicore::AI_RUNTIME_PROTOCOL_VERSION;
}

bool FakeRuntimeClient::isBusy() const
{
    return false;
}

void FakeRuntimeClient::setJobStatusHandler(JobStatusHandler handler)
{
    m_handler = std::move(handler);
}

bool FakeRuntimeClient::submit(const aicore::JobRequest&, QString* errorMessage)
{
    if (errorMessage) {
        *errorMessage = QObject::tr("The fake runtime client cannot submit jobs");
    }
    return false;
}

bool FakeRuntimeClient::cancel(const QString&, QString* errorMessage)
{
    if (errorMessage) {
        *errorMessage = QObject::tr("The fake runtime client cannot cancel jobs");
    }
    return false;
}