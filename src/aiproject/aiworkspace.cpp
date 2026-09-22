/*
 * Audacity: A Digital Audio Editor
 */
#include "aiworkspace.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>

using namespace au::aiproject;

QString WorkspaceStore::workspacePathForProject(const QString& projectPath)
{
    return QDir(QFileInfo(projectPath).absolutePath()).filePath(AI_WORKSPACE_DIRECTORY);
}

bool WorkspaceStore::create(const QString& projectPath, QString* errorMessage)
{
    if (projectPath.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Save the project before enabling its AI workspace");
        }
        return false;
    }

    const QString workspacePath = workspacePathForProject(projectPath);
    if (!QDir().mkpath(QDir(workspacePath).filePath("jobs"))) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Could not create the AI workspace");
        }
        return false;
    }

    const QString manifestPath = QDir(workspacePath).filePath(AI_MANIFEST_FILE);
    if (QFileInfo::exists(manifestPath)) {
        return true;
    }

    QFile manifest(manifestPath);
    const QJsonObject document {
        { "schemaVersion", 1 },
        { "projectFile", QFileInfo(projectPath).fileName() },
        { "jobs", QJsonArray() }
    };
    if (!manifest.open(QIODevice::WriteOnly | QIODevice::NewOnly)
        || manifest.write(QJsonDocument(document).toJson(QJsonDocument::Indented)) < 1) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Could not write the AI workspace manifest");
        }
        return false;
    }
    return true;
}

bool WorkspaceStore::isEnabled(const QString& projectPath)
{
    return !projectPath.isEmpty()
           && QFileInfo::exists(QDir(workspacePathForProject(projectPath)).filePath(AI_MANIFEST_FILE));
}