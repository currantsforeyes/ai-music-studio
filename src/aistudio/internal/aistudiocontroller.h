/*
 * Audacity: A Digital Audio Editor
 */
#pragma once

#include "actions/actionable.h"
#include "actions/iactionsdispatcher.h"
#include "context/iglobalcontext.h"
#include "modularity/ioc.h"
#include "songplan/songplan.h"

#include <QString>
#include <QStringList>

namespace au::aicore { struct JobStatus; }
namespace au::aijobs { class RuntimeHostSupervisor; }

namespace au::aistudio {
class AIStudioController final : public muse::actions::Actionable, public muse::Contextable
{
    muse::ContextInject<muse::actions::IActionsDispatcher> dispatcher { this };
    muse::ContextInject<au::context::IGlobalContext> globalContext { this };

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
    std::shared_ptr<au::aijobs::RuntimeHostSupervisor> m_runtimeHost;
    QString m_activeWorkspace;
    au::songplan::SongPlan m_planDraft;
    bool m_planDraftLoaded = false;
};
}
