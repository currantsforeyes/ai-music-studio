/*
 * Audacity: A Digital Audio Editor
 */
#pragma once

#include "aicore/aicoretypes.h"

#include <QList>
#include <QString>

namespace au::aijobs {
// Persisted, provider-neutral job records stored inside the project-adjacent AI
// workspace manifest. The project manifest stays the single source of truth; the
// store never launches providers and never touches the timeline.
class JobStore final
{
public:
    static bool upsert(const QString& workspacePath, const aicore::JobStatus& status,
                       QString* errorMessage = nullptr);
    static QList<aicore::JobStatus> jobs(const QString& workspacePath, QString* errorMessage = nullptr);
    static bool find(const QString& workspacePath, const QString& jobId, aicore::JobStatus* status,
                     QString* errorMessage = nullptr);

    // Marks every non-terminal job as Interrupted and returns how many changed.
    // Returns -1 and sets errorMessage when the manifest cannot be read or saved.
    static int recoverInterrupted(const QString& workspacePath, QString* errorMessage = nullptr);

    // The insertion marker is preserved across status updates and never reset.
    static bool markInserted(const QString& workspacePath, const QString& jobId, QString* errorMessage = nullptr);
    static bool isInserted(const QString& workspacePath, const QString& jobId, bool* inserted,
                           QString* errorMessage = nullptr);

    // Absolute path of a completed job's audio asset, or empty with a reason in
    // errorMessage when the job is incomplete, already inserted, or missing.
    static QString resultAssetPath(const QString& workspacePath, const QString& jobId,
                                   QString* errorMessage = nullptr);
};
}