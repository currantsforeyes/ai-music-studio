/*
 * Audacity: A Digital Audio Editor
 */
#pragma once

#include "songplan.h"

#include <QList>
#include <QString>

namespace au::songplan {
// Immutable, project-adjacent song-plan revisions stored as
// <workspace>/plans/<planId>/r<revision>.json. An existing revision can never
// be overwritten; editing creates a new revision.
class SongPlanStore final
{
public:
    static bool saveRevision(const QString& workspacePath, const SongPlan& plan,
                             QString* errorMessage = nullptr);

    static QList<int> revisions(const QString& workspacePath, const QString& planId,
                                QString* errorMessage = nullptr);
    static int latestRevision(const QString& workspacePath, const QString& planId,
                              QString* errorMessage = nullptr);
    static bool loadRevision(const QString& workspacePath, const QString& planId, int revision,
                             SongPlan* plan, QString* errorMessage = nullptr);
    static bool loadLatest(const QString& workspacePath, const QString& planId,
                           SongPlan* plan, QString* errorMessage = nullptr);

    // All plan ids present in the workspace, sorted.
    static QStringList planIds(const QString& workspacePath, QString* errorMessage = nullptr);

    // Returns a copy of plan whose revision is latest + 1, ready to save.
    static SongPlan nextRevision(const QString& workspacePath, const SongPlan& plan,
                                 QString* errorMessage = nullptr);
};
}