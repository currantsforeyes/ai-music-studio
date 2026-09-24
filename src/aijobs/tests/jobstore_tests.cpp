/*
 * Audacity: A Digital Audio Editor
 */
#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include "aicore/aicoretypes.h"
#include "aijobs/jobstore.h"
#include "aiproject/aiworkspace.h"

namespace au::aijobs {
namespace {
aicore::JobStatus makeJob(const QString& id, aicore::JobState state)
{
    aicore::JobStatus status;
    status.id.value = id.toStdString();
    status.providerId = "test-provider";
    status.state = state;
    return status;
}
}

TEST(JobStoreTests, PersistsAndReadsJobStatus)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString projectPath = directory.filePath("jobstore.aup4");
    ASSERT_TRUE(aiproject::WorkspaceStore::create(projectPath));
    const QString workspace = aiproject::WorkspaceStore::workspacePathForProject(projectPath);

    aicore::JobStatus status = makeJob("job-1", aicore::JobState::Running);
    status.progress = 0.5;
    status.message = "halfway";
    QString error;
    ASSERT_TRUE(JobStore::upsert(workspace, status, &error)) << error.toStdString();

    aicore::JobStatus restored;
    ASSERT_TRUE(JobStore::find(workspace, "job-1", &restored, &error)) << error.toStdString();
    EXPECT_EQ(restored.providerId, "test-provider");
    EXPECT_EQ(restored.state, aicore::JobState::Running);
    EXPECT_DOUBLE_EQ(restored.progress, 0.5);
    EXPECT_EQ(restored.message, "halfway");
}

TEST(JobStoreTests, UpsertReplacesRatherThanAppends)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString projectPath = directory.filePath("jobstore.aup4");
    ASSERT_TRUE(aiproject::WorkspaceStore::create(projectPath));
    const QString workspace = aiproject::WorkspaceStore::workspacePathForProject(projectPath);

    QString error;
    ASSERT_TRUE(JobStore::upsert(workspace, makeJob("job-1", aicore::JobState::Queued), &error));
    ASSERT_TRUE(JobStore::upsert(workspace, makeJob("job-1", aicore::JobState::Complete), &error));
    ASSERT_EQ(JobStore::jobs(workspace, &error).size(), 1);

    aicore::JobStatus restored;
    ASSERT_TRUE(JobStore::find(workspace, "job-1", &restored, &error));
    EXPECT_EQ(restored.state, aicore::JobState::Complete);
}

TEST(JobStoreTests, RecoversOnlyNonTerminalJobs)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString projectPath = directory.filePath("jobstore.aup4");
    ASSERT_TRUE(aiproject::WorkspaceStore::create(projectPath));
    const QString workspace = aiproject::WorkspaceStore::workspacePathForProject(projectPath);

    QString error;
    ASSERT_TRUE(JobStore::upsert(workspace, makeJob("running", aicore::JobState::Running), &error));
    ASSERT_TRUE(JobStore::upsert(workspace, makeJob("complete", aicore::JobState::Complete), &error));
    ASSERT_TRUE(JobStore::upsert(workspace, makeJob("failed", aicore::JobState::Failed), &error));

    EXPECT_EQ(JobStore::recoverInterrupted(workspace, &error), 1);

    aicore::JobStatus running;
    ASSERT_TRUE(JobStore::find(workspace, "running", &running, &error));
    EXPECT_EQ(running.state, aicore::JobState::Interrupted);

    aicore::JobStatus complete;
    ASSERT_TRUE(JobStore::find(workspace, "complete", &complete, &error));
    EXPECT_EQ(complete.state, aicore::JobState::Complete);
}

TEST(JobStoreTests, PreservesInsertionMarkerAcrossUpdates)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString projectPath = directory.filePath("jobstore.aup4");
    ASSERT_TRUE(aiproject::WorkspaceStore::create(projectPath));
    const QString workspace = aiproject::WorkspaceStore::workspacePathForProject(projectPath);

    QString error;
    aicore::JobStatus status = makeJob("job-inserted", aicore::JobState::Complete);
    ASSERT_TRUE(JobStore::upsert(workspace, status, &error));
    ASSERT_TRUE(JobStore::markInserted(workspace, "job-inserted", &error));

    status.message = "later update";
    ASSERT_TRUE(JobStore::upsert(workspace, status, &error));

    bool inserted = false;
    ASSERT_TRUE(JobStore::isInserted(workspace, "job-inserted", &inserted, &error));
    EXPECT_TRUE(inserted);
}

TEST(JobStoreTests, ResolvesCompletedResultAssetUntilInserted)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString projectPath = directory.filePath("jobstore.aup4");
    ASSERT_TRUE(aiproject::WorkspaceStore::create(projectPath));
    const QString workspace = aiproject::WorkspaceStore::workspacePathForProject(projectPath);

    const QString jobDirectory = QDir(workspace).filePath("jobs/job-2");
    ASSERT_TRUE(QDir().mkpath(jobDirectory));
    QFile asset(QDir(jobDirectory).filePath("output.wav"));
    ASSERT_TRUE(asset.open(QIODevice::WriteOnly));
    asset.write("RIFF....WAVE");
    asset.close();
    QFile manifest(QDir(jobDirectory).filePath("result.json"));
    ASSERT_TRUE(manifest.open(QIODevice::WriteOnly));
    manifest.write(QJsonDocument(QJsonObject { { "asset", "output.wav" } }).toJson(QJsonDocument::Compact));
    manifest.close();

    QString error;
    aicore::JobStatus status = makeJob("job-2", aicore::JobState::Complete);
    status.resultManifest = "jobs/job-2/result.json";
    ASSERT_TRUE(JobStore::upsert(workspace, status, &error));

    const QString assetPath = JobStore::resultAssetPath(workspace, "job-2", &error);
    EXPECT_TRUE(assetPath.endsWith("output.wav")) << assetPath.toStdString();

    ASSERT_TRUE(JobStore::markInserted(workspace, "job-2", &error));
    error.clear();
    EXPECT_TRUE(JobStore::resultAssetPath(workspace, "job-2", &error).isEmpty());
    EXPECT_FALSE(error.isEmpty());
}

TEST(JobStoreTests, ResolvesCompletedJobArtifacts)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString projectPath = directory.filePath("jobstore.aup4");
    ASSERT_TRUE(aiproject::WorkspaceStore::create(projectPath));
    const QString workspace = aiproject::WorkspaceStore::workspacePathForProject(projectPath);

    const QString jobDirectory = QDir(workspace).filePath("jobs/job-art");
    ASSERT_TRUE(QDir().mkpath(jobDirectory));
    QFile score(QDir(jobDirectory).filePath("score.abc"));
    ASSERT_TRUE(score.open(QIODevice::WriteOnly));
    score.write("X:1\nQ:1/4=120\nK:C\n");
    score.close();
    QFile result(QDir(jobDirectory).filePath("result.json"));
    ASSERT_TRUE(result.open(QIODevice::WriteOnly));
    result.write(QJsonDocument(QJsonObject {
        { "asset", "output.wav" },
        { "artifacts", QJsonArray { QJsonObject { { "id", "score" }, { "path", "jobs/job-art/score.abc" } } } }
    }).toJson(QJsonDocument::Compact));
    result.close();

    QString error;
    aicore::JobStatus status = makeJob("job-art", aicore::JobState::Complete);
    status.resultManifest = "jobs/job-art/result.json";
    ASSERT_TRUE(JobStore::upsert(workspace, status, &error));

    const QList<JobStore::JobArtifact> artifacts = JobStore::resultArtifacts(workspace, "job-art", &error);
    ASSERT_EQ(artifacts.size(), 1);
    EXPECT_EQ(artifacts.front().id, QStringLiteral("score"));
    EXPECT_TRUE(artifacts.front().path.endsWith(QStringLiteral("score.abc")));
    EXPECT_TRUE(QFileInfo::exists(artifacts.front().path));
}

TEST(JobStoreTests, LinksTracksToJobs)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString projectPath = directory.filePath("jobstore.aup4");
    ASSERT_TRUE(aiproject::WorkspaceStore::create(projectPath));
    const QString workspace = aiproject::WorkspaceStore::workspacePathForProject(projectPath);

    QString error;
    ASSERT_TRUE(JobStore::setTrackJob(workspace, 42, "job-42", &error)) << error.toStdString();
    EXPECT_EQ(JobStore::jobForTrack(workspace, 42), QStringLiteral("job-42"));
    EXPECT_TRUE(JobStore::jobForTrack(workspace, 99).isEmpty());

    ASSERT_TRUE(JobStore::setTrackJob(workspace, 43, "job-43", &error));
    EXPECT_EQ(JobStore::jobForTrack(workspace, 42), QStringLiteral("job-42"));
    EXPECT_EQ(JobStore::jobForTrack(workspace, 43), QStringLiteral("job-43"));

    // A later status update must not drop the track links.
    ASSERT_TRUE(JobStore::upsert(workspace, makeJob("job-42", aicore::JobState::Complete), &error));
    EXPECT_EQ(JobStore::jobForTrack(workspace, 42), QStringLiteral("job-42"));
}

}