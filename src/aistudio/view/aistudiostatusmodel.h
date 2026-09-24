/*
 * Audacity: A Digital Audio Editor
 */
#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

namespace au::aistudio {
class AIStudioStatusModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString runtimeStatus READ runtimeStatus NOTIFY runtimeStatusChanged)
    Q_PROPERTY(QString workspaceStatus READ workspaceStatus NOTIFY workspaceStatusChanged)
    Q_PROPERTY(QVariantList libraryAssets READ libraryAssets NOTIFY libraryAssetsChanged)
    Q_PROPERTY(QVariantList globalLibraryAssets READ globalLibraryAssets NOTIFY globalLibraryAssetsChanged)
    Q_PROPERTY(QStringList libraryFolders READ libraryFolders NOTIFY libraryFoldersChanged)
    Q_PROPERTY(QString libraryStatus READ libraryStatus NOTIFY libraryStatusChanged)
    Q_PROPERTY(QVariantList jobs READ jobs NOTIFY jobsChanged)
    Q_PROPERTY(QVariantList plans READ plans NOTIFY plansChanged)
    Q_PROPERTY(QVariantMap planDetail READ planDetail NOTIFY planDetailChanged)
    Q_PROPERTY(QString modelCliPath READ modelCliPath NOTIFY modelPathsChanged)
    Q_PROPERTY(QString modelModelPath READ modelModelPath NOTIFY modelPathsChanged)
    Q_PROPERTY(bool modelConfigured READ modelConfigured NOTIFY modelPathsChanged)
    Q_PROPERTY(QString reuseStyle READ reuseStyle NOTIFY promptReuseChanged)
    Q_PROPERTY(QString reuseLyrics READ reuseLyrics NOTIFY promptReuseChanged)
    Q_PROPERTY(QString reuseTitle READ reuseTitle NOTIFY promptReuseChanged)
    Q_PROPERTY(QString reuseSeed READ reuseSeed NOTIFY promptReuseChanged)
    Q_PROPERTY(QString reuseCot READ reuseCot NOTIFY promptReuseChanged)
    Q_PROPERTY(QString reuseSteps READ reuseSteps NOTIFY promptReuseChanged)
    Q_PROPERTY(QString reuseGuidance READ reuseGuidance NOTIFY promptReuseChanged)
    Q_PROPERTY(QVariantMap reuseSampling READ reuseSampling NOTIFY promptReuseChanged)
    Q_PROPERTY(QString currentSeed READ currentSeed NOTIFY currentSeedChanged)

public:
    static AIStudioStatusModel* instance();

    QString runtimeStatus() const;
    QString workspaceStatus() const;
    QVariantList libraryAssets() const;
    QVariantList globalLibraryAssets() const;
    QStringList libraryFolders() const;
    QString libraryStatus() const;
    QVariantList jobs() const;
    QVariantList plans() const;
    QVariantMap planDetail() const;
    QString modelCliPath() const;
    QString modelModelPath() const;
    bool modelConfigured() const;
    QString reuseStyle() const;
    QString reuseLyrics() const;
    QString reuseTitle() const;
    QString reuseSeed() const;
    QString reuseCot() const;
    QString reuseSteps() const;
    QString reuseGuidance() const;
    QVariantMap reuseSampling() const;
    QString currentSeed() const;
    void setRuntimeStatus(const QString& status);
    void setWorkspaceStatus(const QString& status);
    void setLibraryAssets(const QVariantList& assets);
    void setGlobalLibraryAssets(const QVariantList& assets);
    void setLibraryFolders(const QStringList& folders);
    void setLibraryStatus(const QString& status);
    void setJobs(const QVariantList& jobs);
    void setPlans(const QVariantList& plans);
    void setPlanDetail(const QVariantMap& planDetail);
    void updateModelCliPath(const QString& path);
    void updateModelModelPath(const QString& path);
    void setModelConfigured(bool configured);
    void setPromptReuse(const QString& style, const QString& lyrics, const QString& title, const QString& seed,
                        const QString& cot, const QString& steps, const QString& guidance, const QVariantMap& sampling);
    void updateCurrentSeed(const QString& seed);

    Q_INVOKABLE void enableProjectWorkspace();
    Q_INVOKABLE void setModelCliPath(const QString& path);
    Q_INVOKABLE void setModelModelPath(const QString& path);
    Q_INVOKABLE void refreshLibrary();
    Q_INVOKABLE void importLocalWav(const QString& sourcePath);
    Q_INVOKABLE void setLibraryAssetFavourite(const QString& assetId, bool favourite);
    Q_INVOKABLE void setLibraryAssetsFavourite(const QStringList& assetIds, bool favourite);
    Q_INVOKABLE void moveLibraryAssetsToFolder(const QStringList& assetIds, const QString& folder);
    Q_INVOKABLE void moveLibraryAssetsToUnfiled(const QStringList& assetIds);
    Q_INVOKABLE void createLibraryFolder(const QString& folder);
    Q_INVOKABLE void renameLibraryFolder(const QString& folder, const QString& newFolder);
    Q_INVOKABLE void deleteLibraryFolder(const QString& folder);
    Q_INVOKABLE void renameLibraryAsset(const QString& assetId, const QString& name);
    Q_INVOKABLE void deleteLibraryAsset(const QString& assetId);
    Q_INVOKABLE void setLibraryAssetTags(const QString& assetId, const QString& tags);
    Q_INVOKABLE void setLibraryAssetsTags(const QStringList& assetIds, const QString& tags);
    Q_INVOKABLE void readLibraryAssetAudioDetails(const QString& assetId);
    Q_INVOKABLE void readLibraryAssetsAudioDetails(const QStringList& assetIds);
    Q_INVOKABLE void revealLibraryAssetInExplorer(const QString& assetId);
    Q_INVOKABLE void addLibraryAssetToTimeline(const QString& assetId);
    Q_INVOKABLE void copyGlobalLibraryAssetToProject(const QString& projectPath, const QString& assetId);
    Q_INVOKABLE void refreshJobs();
    Q_INVOKABLE void cancelJob(const QString& jobId);
    Q_INVOKABLE void retryJob(const QString& jobId);
    Q_INVOKABLE void insertJobOutput(const QString& jobId);
    Q_INVOKABLE void runYue2Job(const QString& lyrics, const QString& style, const QString& seed, const QString& title,
                                const QString& cot, const QString& steps, const QString& guidance, const QVariantMap& sampling);
    Q_INVOKABLE void regenerateFromPlan();
    Q_INVOKABLE void importPromptFile(const QString& path);
    Q_INVOKABLE void exportPromptFile(const QString& path, const QString& style, const QString& lyrics,
                                      const QString& title, const QString& seed, const QString& cot,
                                      const QString& steps, const QString& guidance, const QVariantMap& sampling);
    Q_INVOKABLE void refreshPlans();
    Q_INVOKABLE void createPlan(const QString& name);
    Q_INVOKABLE void loadPlan(const QString& planId);
    Q_INVOKABLE void setPlanMetadata(double tempo, const QString& key, const QString& timeSignature);
    Q_INVOKABLE void addPlanSection(const QString& name, double startSeconds, double endSeconds);
    Q_INVOKABLE void removePlanSection(int index);
    Q_INVOKABLE void addPlanChord(const QString& symbol, double startSeconds, double durationSeconds);
    Q_INVOKABLE void removePlanChord(int index);
    Q_INVOKABLE void addPlanNote(int midiPitch, double startSeconds, double durationSeconds, const QString& lyric);
    Q_INVOKABLE void removePlanNote(int index);
    Q_INVOKABLE void savePlanRevision();

signals:
    void runtimeStatusChanged();
    void workspaceStatusChanged();
    void libraryAssetsChanged();
    void globalLibraryAssetsChanged();
    void libraryFoldersChanged();
    void libraryStatusChanged();
    void workspaceEnableRequested();
    void modelPathsChanged();
    void modelCliPathSetRequested(const QString& path);
    void modelModelPathSetRequested(const QString& path);
    void libraryRefreshRequested();
    void libraryImportRequested(const QString& sourcePath);
    void libraryAssetFavouriteRequested(const QString& assetId, bool favourite);
    void libraryAssetsFavouriteRequested(const QStringList& assetIds, bool favourite);
    void libraryAssetsMoveRequested(const QStringList& assetIds, const QString& folder);
    void libraryAssetsUnfileRequested(const QStringList& assetIds);
    void libraryFolderCreateRequested(const QString& folder);
    void libraryFolderRenameRequested(const QString& folder, const QString& newFolder);
    void libraryFolderDeleteRequested(const QString& folder);
    void libraryAssetRenameRequested(const QString& assetId, const QString& name);
    void libraryAssetDeleteRequested(const QString& assetId);
    void libraryAssetTagsRequested(const QString& assetId, const QString& tags);
    void libraryAssetsTagsRequested(const QStringList& assetIds, const QString& tags);
    void libraryAssetAudioDetailsRequested(const QString& assetId);
    void libraryAssetsAudioDetailsRequested(const QStringList& assetIds);
    void libraryAssetRevealRequested(const QString& assetId);
    void libraryAssetInsertionRequested(const QString& assetId);
    void globalLibraryAssetCopyRequested(const QString& projectPath, const QString& assetId);
    void jobsChanged();
    void jobsRefreshRequested();
    void jobCancelRequested(const QString& jobId);
    void jobRetryRequested(const QString& jobId);
    void jobInsertRequested(const QString& jobId);
    void yue2JobRequested(const QString& lyrics, const QString& style, const QString& seed, const QString& title,
                          const QString& cot, const QString& steps, const QString& guidance, const QVariantMap& sampling);
    void regeneratePlanRequested();
    void importPromptRequested(const QString& path);
    void exportPromptRequested(const QString& path, const QString& style, const QString& lyrics, const QString& title,
                               const QString& seed, const QString& cot, const QString& steps, const QString& guidance,
                               const QVariantMap& sampling);
    void promptReuseChanged();
    void currentSeedChanged();
    void plansChanged();
    void plansRefreshRequested();
    void planCreateRequested(const QString& name);
    void planDetailChanged();
    void planLoadRequested(const QString& planId);
    void planMetadataRequested(double tempo, const QString& key, const QString& timeSignature);
    void planSectionAddRequested(const QString& name, double startSeconds, double endSeconds);
    void planSectionRemoveRequested(int index);
    void planChordAddRequested(const QString& symbol, double startSeconds, double durationSeconds);
    void planChordRemoveRequested(int index);
    void planNoteAddRequested(int midiPitch, double startSeconds, double durationSeconds, const QString& lyric);
    void planNoteRemoveRequested(int index);
    void planSaveRequested();

private:
    AIStudioStatusModel();
    QString m_runtimeStatus;
    QString m_workspaceStatus;
    QVariantList m_libraryAssets;
    QVariantList m_globalLibraryAssets;
    QStringList m_libraryFolders;
    QString m_libraryStatus;
    QVariantList m_jobs;
    QVariantList m_plans;
    QVariantMap m_planDetail;
    QString m_modelCliPath;
    QString m_modelModelPath;
    bool m_modelConfigured = false;
    QString m_reuseStyle;
    QString m_reuseLyrics;
    QString m_reuseTitle;
    QString m_reuseSeed;
    QString m_reuseCot;
    QString m_reuseSteps;
    QString m_reuseGuidance;
    QVariantMap m_reuseSampling;
    QString m_currentSeed;
};
}
