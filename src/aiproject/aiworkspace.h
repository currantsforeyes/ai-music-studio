/*
 * Audacity: A Digital Audio Editor
 */
#pragma once

#include <QString>

namespace au::aiproject {
inline constexpr const char* AI_WORKSPACE_DIRECTORY = "ai";
inline constexpr const char* AI_MANIFEST_FILE = "manifest.json";

// Owns the project-adjacent ai/ workspace directory and manifest lifecycle.
// Job records are persisted by aijobs::JobStore, which shares this manifest.
class WorkspaceStore final
{
public:
    static QString workspacePathForProject(const QString& projectPath);
    static bool create(const QString& projectPath, QString* errorMessage = nullptr);
    static bool isEnabled(const QString& projectPath);
};
}