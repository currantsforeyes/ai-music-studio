/*
 * Audacity: A Digital Audio Editor
 */
#pragma once

#include "actions/actionable.h"
#include "actions/iactionsdispatcher.h"
#include "assistantclient.h"
#include "context/iglobalcontext.h"
#include "global/async/asyncable.h"
#include "modularity/ioc.h"
#include "songplan/songplan.h"
#include "trackedit/iselectioncontroller.h"
#include "trackedit/itrackeditinteraction.h"
#include "trackedit/itracksinteraction.h"

#include <QHash>
#include <QString>
#include <QStringList>

namespace au::aicore { struct JobStatus; }
namespace au::aijobs { class RuntimeHostSupervisor; }

namespace au::aistudio {
class AIStudioController final : public muse::actions::Actionable, public muse::async::Asyncable, public muse::Contextable
{
    muse::ContextInject<muse::actions::IActionsDispatcher> dispatcher { this };
    muse::ContextInject<au::context::IGlobalContext> globalContext { this };
    muse::ContextInject<au::trackedit::ITracksInteraction> tracks { this };
    muse::ContextInject<au::trackedit::ITrackeditInteraction> trackedit { this };
    muse::ContextInject<au::trackedit::ISelectionController> selectionController { this };

public:
    AIStudioController(const muse::modularity::ContextPtr& ctx)
        : muse::Contextable(ctx) {}

    void init();
    bool canReceiveAction(const muse::actions::ActionCode& code) const override;

private:
    void openJobs();
    void enableProjectWorkspace();
    void importLocalWav(const QString& sourcePath);
    void setLibraryAssetFavourite(const QString& assetId, bool favourite);
    void setLibraryAssetsFavourite(const QStringList& assetIds, bool favourite);
    void moveLibraryAssetsToFolder(const QStringList& assetIds, const QString& folder);
    void moveLibraryAssetsToUnfiled(const QStringList& assetIds);
    void createLibraryFolder(const QString& folder);
    void renameLibraryFolder(const QString& folder, const QString& newFolder);
    void deleteLibraryFolder(const QString& folder);
    void renameLibraryAsset(const QString& assetId, const QString& name);
    void deleteLibraryAsset(const QString& assetId);
    void setLibraryAssetTags(const QString& assetId, const QString& tags);
    void setLibraryAssetsTags(const QStringList& assetIds, const QString& tags);
    void readLibraryAssetAudioDetails(const QString& assetId);
    void readLibraryAssetsAudioDetails(const QStringList& assetIds);
    void revealLibraryAssetInExplorer(const QString& assetId);
    void addLibraryAssetToTimeline(const QString& assetId);
    void copyGlobalLibraryAssetToProject(const QString& projectPath, const QString& assetId);
    void refreshWorkspaceStatus();
    void refreshLibraryAssets();
    void recordJobStatus(const au::aicore::JobStatus& status);
    void refreshJobs();
    void cancelJob(const QString& jobId);
    void retryJob(const QString& jobId);
    void insertJobOutput(const QString& jobId);
    void submitYue2Job(const QString& providerId, const QString& lyrics, const QString& style, const QString& seed,
                       const QString& title, const QString& cot, const QString& duration, const QString& steps,
                       const QString& guidance, const QVariantMap& sampling);
    void importPromptFile(const QString& path);
    void exportPromptFile(const QString& path, const QString& style, const QString& lyrics, const QString& title,
                          const QString& seed, const QString& cot, const QString& duration, const QString& steps,
                          const QString& guidance, const QVariantMap& sampling);
    void importPendingResults();
    void createPrompt(const QString& lyrics);
    void improvePrompt(const QString& style);
    void writeLyrics(const QString& style);
    void runAssistant(const QString& field, const QString& systemPrompt, const QString& userPrompt);
    void setAssistantConfig(const QString& mode, const QString& baseUrl, const QString& model,
                            const QString& apiKey, const QString& runnerPath, const QString& modelPath, int port);
    void applyAssistantSettings();
    void reusePromptForTrack(const au::trackedit::ClipKey& clipKey);
    void regenerateFromPlan();
    void loadExample(int index);
    void loadPlanForClip(const au::trackedit::ClipKey& clipKey);
    void loadPlanForJob(const QString& jobId);
    QString jobIdForClip(const au::trackedit::ClipKey& clipKey) const;
    void onClipSelectionChanged();
    void applyModelSettings();
    void setModelCliPath(const QString& path);
    void setModelModelPath(const QString& path);
    void refreshPlans();
    void createPlan(const QString& name);
    void loadPlan(const QString& planId);
    void setPlanMetadata(double tempo, const QString& key, const QString& timeSignature);
    void addPlanSection(const QString& name, double startSeconds, double endSeconds);
    void removePlanSection(int index);
    void addPlanChord(const QString& symbol, double startSeconds, double durationSeconds);
    void removePlanChord(int index);
    void addPlanNote(int midiPitch, double startSeconds, double durationSeconds, const QString& lyric);
    void removePlanNote(int index);
    void savePlanRevision();
    void pushPlanDetail();
    void registerPlanAsset(const au::songplan::SongPlan& plan);
    void registerJobArtifacts(const QString& jobId, const QString& resultManifest);
    void updateJobPlaceholder(const QString& jobId, double progress);
    void removeJobPlaceholder(const QString& jobId);
    std::shared_ptr<au::aijobs::RuntimeHostSupervisor> m_runtimeHost;
    std::unique_ptr<AssistantClient> m_assistant;
    QString m_activeWorkspace;
    au::songplan::SongPlan m_planDraft;
    QString m_planDraftSeed;
    bool m_planDraftLoaded = false;
    // While a generation is running, a titled silent placeholder track stands in
    // for the pending output and is replaced by the real audio on completion.
    QHash<QString, au::trackedit::TrackId> m_jobPlaceholders;
};
}
