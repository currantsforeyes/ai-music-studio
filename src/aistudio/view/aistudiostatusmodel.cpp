/*
 * Audacity: A Digital Audio Editor
 */
#include "aistudiostatusmodel.h"

using namespace au::aistudio;

AIStudioStatusModel::AIStudioStatusModel()
    : QObject(), m_runtimeStatus(tr("Runtime host not started")),
      m_workspaceStatus(tr("AI workspace not enabled for this project")),
      m_libraryStatus(tr("No Library activity yet"))
{
}

AIStudioStatusModel* AIStudioStatusModel::instance()
{
    // Deliberately leaked. The QML singleton outlives the QApplication, which
    // is deleted before static destruction in main(); owning it by the app or
    // by static storage would double-delete it during shutdown.
    static AIStudioStatusModel* model = new AIStudioStatusModel();
    return model;
}

QString AIStudioStatusModel::runtimeStatus() const
{
    return m_runtimeStatus;
}

QString AIStudioStatusModel::workspaceStatus() const
{
    return m_workspaceStatus;
}

QVariantList AIStudioStatusModel::libraryAssets() const
{
    return m_libraryAssets;
}

QVariantList AIStudioStatusModel::globalLibraryAssets() const
{
    return m_globalLibraryAssets;
}

QStringList AIStudioStatusModel::libraryFolders() const
{
    return m_libraryFolders;
}

QString AIStudioStatusModel::libraryStatus() const
{
    return m_libraryStatus;
}

QVariantList AIStudioStatusModel::jobs() const
{
    return m_jobs;
}

QVariantList AIStudioStatusModel::plans() const
{
    return m_plans;
}

QVariantMap AIStudioStatusModel::planDetail() const
{
    return m_planDetail;
}

QString AIStudioStatusModel::modelCliPath() const
{
    return m_modelCliPath;
}

QString AIStudioStatusModel::modelModelPath() const
{
    return m_modelModelPath;
}

bool AIStudioStatusModel::modelConfigured() const
{
    return m_modelConfigured;
}

QString AIStudioStatusModel::reuseStyle() const
{
    return m_reuseStyle;
}

QString AIStudioStatusModel::reuseLyrics() const
{
    return m_reuseLyrics;
}

QString AIStudioStatusModel::reuseTitle() const
{
    return m_reuseTitle;
}

QString AIStudioStatusModel::reuseSeed() const
{
    return m_reuseSeed;
}

QString AIStudioStatusModel::reuseCot() const
{
    return m_reuseCot;
}

QString AIStudioStatusModel::reuseSteps() const
{
    return m_reuseSteps;
}

QString AIStudioStatusModel::reuseGuidance() const
{
    return m_reuseGuidance;
}

QVariantMap AIStudioStatusModel::reuseSampling() const
{
    return m_reuseSampling;
}

QVariantList AIStudioStatusModel::examples() const
{
    return m_examples;
}

bool AIStudioStatusModel::assistantBusy() const
{
    return m_assistantBusy;
}

QString AIStudioStatusModel::assistantStatus() const
{
    return m_assistantStatus;
}

QString AIStudioStatusModel::assistantBaseUrl() const
{
    return m_assistantBaseUrl;
}

QString AIStudioStatusModel::assistantModel() const
{
    return m_assistantModel;
}

bool AIStudioStatusModel::assistantHasKey() const
{
    return m_assistantHasKey;
}

QString AIStudioStatusModel::currentSeed() const
{
    return m_currentSeed;
}

void AIStudioStatusModel::setRuntimeStatus(const QString& status)
{
    if (m_runtimeStatus == status) {
        return;
    }
    m_runtimeStatus = status;
    emit runtimeStatusChanged();
}

void AIStudioStatusModel::setWorkspaceStatus(const QString& status)
{
    if (m_workspaceStatus == status) {
        return;
    }
    m_workspaceStatus = status;
    emit workspaceStatusChanged();
}

void AIStudioStatusModel::setLibraryAssets(const QVariantList& assets)
{
    if (m_libraryAssets == assets) {
        return;
    }
    m_libraryAssets = assets;
    emit libraryAssetsChanged();
}

void AIStudioStatusModel::setGlobalLibraryAssets(const QVariantList& assets)
{
    if (m_globalLibraryAssets == assets) {
        return;
    }
    m_globalLibraryAssets = assets;
    emit globalLibraryAssetsChanged();
}

void AIStudioStatusModel::setLibraryFolders(const QStringList& folders)
{
    if (m_libraryFolders == folders) {
        return;
    }
    m_libraryFolders = folders;
    emit libraryFoldersChanged();
}

void AIStudioStatusModel::setLibraryStatus(const QString& status)
{
    if (m_libraryStatus == status) {
        return;
    }
    m_libraryStatus = status;
    emit libraryStatusChanged();
}

void AIStudioStatusModel::setJobs(const QVariantList& jobs)
{
    if (m_jobs == jobs) {
        return;
    }
    m_jobs = jobs;
    emit jobsChanged();
}

void AIStudioStatusModel::setPlans(const QVariantList& plans)
{
    if (m_plans == plans) {
        return;
    }
    m_plans = plans;
    emit plansChanged();
}

void AIStudioStatusModel::setPlanDetail(const QVariantMap& planDetail)
{
    if (m_planDetail == planDetail) {
        return;
    }
    m_planDetail = planDetail;
    emit planDetailChanged();
}

void AIStudioStatusModel::updateModelCliPath(const QString& path)
{
    if (m_modelCliPath == path) {
        return;
    }
    m_modelCliPath = path;
    emit modelPathsChanged();
}

void AIStudioStatusModel::updateModelModelPath(const QString& path)
{
    if (m_modelModelPath == path) {
        return;
    }
    m_modelModelPath = path;
    emit modelPathsChanged();
}

void AIStudioStatusModel::setModelConfigured(bool configured)
{
    if (m_modelConfigured == configured) {
        return;
    }
    m_modelConfigured = configured;
    emit modelPathsChanged();
}

void AIStudioStatusModel::setPromptReuse(const QString& style, const QString& lyrics, const QString& title,
                                         const QString& seed, const QString& cot, const QString& steps,
                                         const QString& guidance, const QVariantMap& sampling)
{
    m_reuseStyle = style;
    m_reuseLyrics = lyrics;
    m_reuseTitle = title;
    m_reuseSeed = seed;
    m_reuseCot = cot;
    m_reuseSteps = steps;
    m_reuseGuidance = guidance;
    m_reuseSampling = sampling;
    emit promptReuseChanged();
}

void AIStudioStatusModel::updateCurrentSeed(const QString& seed)
{
    if (m_currentSeed == seed) {
        return;
    }
    m_currentSeed = seed;
    emit currentSeedChanged();
}

void AIStudioStatusModel::setExamples(const QVariantList& examples)
{
    if (m_examples == examples) {
        return;
    }
    m_examples = examples;
    emit examplesChanged();
}

void AIStudioStatusModel::updateAssistantBusy(bool busy)
{
    if (m_assistantBusy == busy) {
        return;
    }
    m_assistantBusy = busy;
    emit assistantBusyChanged();
}

void AIStudioStatusModel::updateAssistantStatus(const QString& status)
{
    if (m_assistantStatus == status) {
        return;
    }
    m_assistantStatus = status;
    emit assistantStatusChanged();
}

void AIStudioStatusModel::updateAssistantConfig(const QString& baseUrl, const QString& model, bool hasKey)
{
    m_assistantBaseUrl = baseUrl;
    m_assistantModel = model;
    m_assistantHasKey = hasKey;
    emit assistantConfigChanged();
}

void AIStudioStatusModel::enableProjectWorkspace()
{
    emit workspaceEnableRequested();
}

void AIStudioStatusModel::setModelCliPath(const QString& path)
{
    emit modelCliPathSetRequested(path);
}

void AIStudioStatusModel::setModelModelPath(const QString& path)
{
    emit modelModelPathSetRequested(path);
}

void AIStudioStatusModel::refreshLibrary()
{
    emit libraryRefreshRequested();
}

void AIStudioStatusModel::importLocalWav(const QString& sourcePath)
{
    if (!sourcePath.isEmpty()) {
        emit libraryImportRequested(sourcePath);
    }
}

void AIStudioStatusModel::setLibraryAssetFavourite(const QString& assetId, bool favourite)
{
    if (!assetId.isEmpty()) {
        emit libraryAssetFavouriteRequested(assetId, favourite);
    }
}

void AIStudioStatusModel::setLibraryAssetsFavourite(const QStringList& assetIds, bool favourite)
{
    if (!assetIds.isEmpty()) {
        emit libraryAssetsFavouriteRequested(assetIds, favourite);
    }
}

void AIStudioStatusModel::moveLibraryAssetsToFolder(const QStringList& assetIds, const QString& folder)
{
    if (!assetIds.isEmpty() && !folder.trimmed().isEmpty()) {
        emit libraryAssetsMoveRequested(assetIds, folder);
    }
}

void AIStudioStatusModel::moveLibraryAssetsToUnfiled(const QStringList& assetIds)
{
    if (!assetIds.isEmpty()) {
        emit libraryAssetsUnfileRequested(assetIds);
    }
}

void AIStudioStatusModel::createLibraryFolder(const QString& folder)
{
    if (!folder.trimmed().isEmpty()) {
        emit libraryFolderCreateRequested(folder);
    }
}

void AIStudioStatusModel::renameLibraryFolder(const QString& folder, const QString& newFolder)
{
    if (!folder.isEmpty() && !newFolder.trimmed().isEmpty()) {
        emit libraryFolderRenameRequested(folder, newFolder);
    }
}

void AIStudioStatusModel::deleteLibraryFolder(const QString& folder)
{
    if (!folder.isEmpty()) {
        emit libraryFolderDeleteRequested(folder);
    }
}

void AIStudioStatusModel::renameLibraryAsset(const QString& assetId, const QString& name)
{
    if (!assetId.isEmpty() && !name.trimmed().isEmpty()) {
        emit libraryAssetRenameRequested(assetId, name);
    }
}

void AIStudioStatusModel::deleteLibraryAsset(const QString& assetId)
{
    if (!assetId.isEmpty()) {
        emit libraryAssetDeleteRequested(assetId);
    }
}

void AIStudioStatusModel::setLibraryAssetTags(const QString& assetId, const QString& tags)
{
    if (!assetId.isEmpty()) {
        emit libraryAssetTagsRequested(assetId, tags);
    }
}

void AIStudioStatusModel::setLibraryAssetsTags(const QStringList& assetIds, const QString& tags)
{
    if (!assetIds.isEmpty()) {
        emit libraryAssetsTagsRequested(assetIds, tags);
    }
}

void AIStudioStatusModel::readLibraryAssetAudioDetails(const QString& assetId)
{
    if (!assetId.isEmpty()) {
        emit libraryAssetAudioDetailsRequested(assetId);
    }
}

void AIStudioStatusModel::readLibraryAssetsAudioDetails(const QStringList& assetIds)
{
    if (!assetIds.isEmpty()) {
        emit libraryAssetsAudioDetailsRequested(assetIds);
    }
}

void AIStudioStatusModel::revealLibraryAssetInExplorer(const QString& assetId)
{
    if (!assetId.isEmpty()) {
        emit libraryAssetRevealRequested(assetId);
    }
}

void AIStudioStatusModel::addLibraryAssetToTimeline(const QString& assetId)
{
    if (!assetId.isEmpty()) {
        emit libraryAssetInsertionRequested(assetId);
    }
}

void AIStudioStatusModel::copyGlobalLibraryAssetToProject(const QString& projectPath, const QString& assetId)
{
    if (!projectPath.isEmpty() && !assetId.isEmpty()) {
        emit globalLibraryAssetCopyRequested(projectPath, assetId);
    }
}

void AIStudioStatusModel::refreshJobs()
{
    emit jobsRefreshRequested();
}

void AIStudioStatusModel::cancelJob(const QString& jobId)
{
    if (!jobId.isEmpty()) {
        emit jobCancelRequested(jobId);
    }
}

void AIStudioStatusModel::retryJob(const QString& jobId)
{
    if (!jobId.isEmpty()) {
        emit jobRetryRequested(jobId);
    }
}

void AIStudioStatusModel::insertJobOutput(const QString& jobId)
{
    if (!jobId.isEmpty()) {
        emit jobInsertRequested(jobId);
    }
}

void AIStudioStatusModel::runYue2Job(const QString& lyrics, const QString& style, const QString& seed, const QString& title,
                                     const QString& cot, const QString& steps, const QString& guidance, const QVariantMap& sampling)
{
    if (!lyrics.trimmed().isEmpty()) {
        emit yue2JobRequested(lyrics, style, seed, title, cot, steps, guidance, sampling);
    }
}

void AIStudioStatusModel::importPromptFile(const QString& path)
{
    emit importPromptRequested(path);
}

void AIStudioStatusModel::exportPromptFile(const QString& path, const QString& style, const QString& lyrics,
                                           const QString& title, const QString& seed, const QString& cot,
                                           const QString& steps, const QString& guidance, const QVariantMap& sampling)
{
    emit exportPromptRequested(path, style, lyrics, title, seed, cot, steps, guidance, sampling);
}

void AIStudioStatusModel::regenerateFromPlan()
{
    emit regeneratePlanRequested();
}

void AIStudioStatusModel::loadExample(int index)
{
    emit exampleLoadRequested(index);
}

void AIStudioStatusModel::createPrompt(const QString& lyrics)
{
    emit createPromptRequested(lyrics);
}

void AIStudioStatusModel::improvePrompt(const QString& style)
{
    emit improvePromptRequested(style);
}

void AIStudioStatusModel::writeLyrics(const QString& style)
{
    emit writeLyricsRequested(style);
}

void AIStudioStatusModel::setAssistantConfig(const QString& baseUrl, const QString& model, const QString& apiKey)
{
    emit assistantConfigRequested(baseUrl, model, apiKey);
}

void AIStudioStatusModel::notifyAssistantResult(const QString& field, const QString& text)
{
    emit assistantResult(field, text);
}

void AIStudioStatusModel::refreshPlans()
{
    emit plansRefreshRequested();
}

void AIStudioStatusModel::createPlan(const QString& name)
{
    emit planCreateRequested(name);
}

void AIStudioStatusModel::loadPlan(const QString& planId)
{
    if (!planId.isEmpty()) {
        emit planLoadRequested(planId);
    }
}

void AIStudioStatusModel::setPlanMetadata(double tempo, const QString& key, const QString& timeSignature)
{
    emit planMetadataRequested(tempo, key, timeSignature);
}

void AIStudioStatusModel::addPlanSection(const QString& name, double startSeconds, double endSeconds)
{
    emit planSectionAddRequested(name, startSeconds, endSeconds);
}

void AIStudioStatusModel::removePlanSection(int index)
{
    if (index >= 0) {
        emit planSectionRemoveRequested(index);
    }
}

void AIStudioStatusModel::addPlanChord(const QString& symbol, double startSeconds, double durationSeconds)
{
    emit planChordAddRequested(symbol, startSeconds, durationSeconds);
}

void AIStudioStatusModel::removePlanChord(int index)
{
    if (index >= 0) {
        emit planChordRemoveRequested(index);
    }
}

void AIStudioStatusModel::addPlanNote(int midiPitch, double startSeconds, double durationSeconds, const QString& lyric)
{
    emit planNoteAddRequested(midiPitch, startSeconds, durationSeconds, lyric);
}

void AIStudioStatusModel::removePlanNote(int index)
{
    if (index >= 0) {
        emit planNoteRemoveRequested(index);
    }
}

void AIStudioStatusModel::savePlanRevision()
{
    emit planSaveRequested();
}
