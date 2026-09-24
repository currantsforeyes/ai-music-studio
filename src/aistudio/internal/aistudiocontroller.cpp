/*
 * Audacity: A Digital Audio Editor
 */
#include "aistudiocontroller.h"

#include "assistantclient.h"

#include <QObject>

#include "aicore/aicoretypes.h"
#include "aijobs/jobstore.h"
#include "aijobs/runtimehostsupervisor.h"
#include "ailibrary/assetkind.h"
#include "ailibrary/globalassetcatalogue.h"
#include "ailibrary/libraryassetstore.h"
#include "aimodels/modelsettings.h"
#include "aimodels/promptfile.h"
#include "aiproject/aiworkspace.h"
#include "aistudio/view/aistudiostatusmodel.h"
#include "songplan/songplan.h"
#include "songplan/songplanstore.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtEndian>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QUuid>
#include <QVariantMap>
#include <QUrl>

#include <algorithm>
#include <memory>

using namespace au::aistudio;
using namespace muse;
using namespace muse::actions;

static const ActionCode OPEN_JOBS_CODE("ai.openJobs");
static const ActionCode REUSE_PROMPT_CODE("ai.reusePrompt");
static const ActionCode REPLAY_TRACK_CODE("ai.replayTrack");
static const ActionCode VARY_TRACK_CODE("ai.varyTrack");
static const QString AI_STUDIO_DOCK("aiStudioPanel");

namespace {
struct WavMetadata {
    double durationSeconds = 0.0;
    int sampleRate = 0;
    int channels = 0;
};

WavMetadata wavMetadata(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly) || file.read(4) != "RIFF") {
        return {};
    }
    if (file.read(4).size() != 4 || file.read(4) != "WAVE") {
        return {};
    }

    quint16 channels = 0;
    quint16 blockAlign = 0;
    quint32 sampleRate = 0;
    quint32 dataBytes = 0;
    bool hasFormat = false;
    bool hasData = false;
    while (file.pos() + 8 <= file.size()) {
        const QByteArray chunkId = file.read(4);
        const QByteArray chunkSizeBytes = file.read(4);
        if (chunkId.size() != 4 || chunkSizeBytes.size() != 4) {
            break;
        }
        const quint32 chunkSize = qFromLittleEndian<quint32>(
            reinterpret_cast<const uchar*>(chunkSizeBytes.constData()));
        if (chunkId == "fmt ") {
            const QByteArray format = file.read(std::min<quint32>(chunkSize, 16));
            if (format.size() >= 16) {
                channels = qFromLittleEndian<quint16>(reinterpret_cast<const uchar*>(format.constData() + 2));
                sampleRate = qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(format.constData() + 4));
                blockAlign = qFromLittleEndian<quint16>(reinterpret_cast<const uchar*>(format.constData() + 12));
                hasFormat = channels > 0 && sampleRate > 0 && blockAlign > 0;
            }
        } else if (chunkId == "data") {
            dataBytes = chunkSize;
            hasData = true;
        }
        const qint64 nextChunk = file.pos() + qint64(chunkSize - (chunkId == "fmt " ? std::min<quint32>(chunkSize, 16) : 0))
                                + (chunkSize % 2);
        if (!file.seek(nextChunk)) {
            break;
        }
        if (hasFormat && hasData) {
            break;
        }
    }
    if (!hasFormat || !hasData) {
        return {};
    }
    return { double(dataBytes) / (double(sampleRate) * double(blockAlign)), int(sampleRate), int(channels) };
}
}

namespace {
QVariantList promptExamples()
{
    QVariantList examples;
    const auto add = [&examples](const QString& name, const QString& style, const QString& lyrics,
                                 const QString& cot) {
        examples.append(QVariantMap {
            { "name", name },
            { "style", style },
            { "lyrics", lyrics },
            { "cot", cot },
            { "steps", QStringLiteral("8") }
        });
    };

    add(QStringLiteral("Indie pop ballad"),
        QStringLiteral("indie pop-rock ballad, clean electric guitar arpeggios, close-miked piano, warm male baritone, 82 BPM"),
        QStringLiteral("[Verse]\nMorning light on the kitchen floor\nI keep your letter by the door\n[Chorus]\nAnd I sing, oh I sing\nFor the quiet everything"),
        QStringLiteral("full"));

    add(QStringLiteral("Synthwave"),
        QStringLiteral("synthwave, analog synths, drum machine, female voice, 112 BPM"),
        QStringLiteral("[Verse]\nNeon rivers under midnight\nChrome reflections in your eyes\n[Chorus]\nWe drive into the static\nWhere the city never dies"),
        QStringLiteral("full"));

    add(QStringLiteral("Funk disco"),
        QStringLiteral("funk disco, slap bass, wah guitar, brass section, 118 BPM"),
        QStringLiteral("[Verse]\nStep into the rhythm, let it move your feet\nUptown speakers got a brand new beat\n[Chorus]\nShake it to the left, shake it to the right\nDancing in the glow of a Saturday night"),
        QStringLiteral("full"));

    add(QStringLiteral("Country"),
        QStringLiteral("country, male baritone, pedal steel, fiddle, 96 BPM"),
        QStringLiteral("[Verse]\nDust on the dashboard, gravel in my shoes\nThis old highway only carries the blues\n[Chorus]\nTake me home where the cottonwoods grow\nWhere the river runs easy and the evenings are slow"),
        QStringLiteral("full"));

    add(QStringLiteral("City pop"),
        QStringLiteral("city pop, electric piano, funky bass, 108 BPM"),
        QStringLiteral("[Verse]\nTaxi lights on a rain-slick street\nCoffee cooling to a steady beat\n[Chorus]\nMidnight avenue, hold me in the glow\nWe can take it slow"),
        QStringLiteral("full"));

    return examples;
}
}

void AIStudioController::init()
{
    m_runtimeHost = std::make_shared<au::aijobs::RuntimeHostSupervisor>();
    AIStudioStatusModel::instance()->setRuntimeStatus(m_runtimeHost->statusText());
    QObject::connect(m_runtimeHost.get(), &au::aijobs::RuntimeHostSupervisor::statusChanged,
                     AIStudioStatusModel::instance(), &AIStudioStatusModel::setRuntimeStatus);
    m_runtimeHost->setJobStatusHandler([this](const au::aicore::JobStatus& status) { recordJobStatus(status); });
    applyModelSettings();
    AIStudioStatusModel::instance()->setExamples(promptExamples());
    m_assistant = std::make_unique<AssistantClient>();
    applyAssistantSettings();
    if (selectionController()) {
        selectionController()->clipsSelected().onReceive(this, [this](const au::trackedit::ClipKeyList&) {
            onClipSelectionChanged();
        });
    }
    QObject::connect(AIStudioStatusModel::instance(), &AIStudioStatusModel::workspaceEnableRequested,
                     m_runtimeHost.get(), [this] { enableProjectWorkspace(); });
    QObject::connect(AIStudioStatusModel::instance(), &AIStudioStatusModel::modelCliPathSetRequested,
                     m_runtimeHost.get(), [this](const QString& path) { setModelCliPath(path); });
    QObject::connect(AIStudioStatusModel::instance(), &AIStudioStatusModel::modelModelPathSetRequested,
                     m_runtimeHost.get(), [this](const QString& path) { setModelModelPath(path); });
    QObject::connect(AIStudioStatusModel::instance(), &AIStudioStatusModel::libraryRefreshRequested,
                     m_runtimeHost.get(), [this] { refreshWorkspaceStatus(); });
    QObject::connect(AIStudioStatusModel::instance(), &AIStudioStatusModel::libraryImportRequested,
                     m_runtimeHost.get(), [this](const QString& sourcePath) { importLocalWav(sourcePath); });
    QObject::connect(AIStudioStatusModel::instance(), &AIStudioStatusModel::libraryAssetFavouriteRequested,
                     m_runtimeHost.get(), [this](const QString& assetId, bool favourite) {
        setLibraryAssetFavourite(assetId, favourite);
    });
    QObject::connect(AIStudioStatusModel::instance(), &AIStudioStatusModel::libraryAssetsFavouriteRequested,
                     m_runtimeHost.get(), [this](const QStringList& assetIds, bool favourite) {
        setLibraryAssetsFavourite(assetIds, favourite);
    });
    QObject::connect(AIStudioStatusModel::instance(), &AIStudioStatusModel::libraryAssetsMoveRequested,
                     m_runtimeHost.get(), [this](const QStringList& assetIds, const QString& folder) {
        moveLibraryAssetsToFolder(assetIds, folder);
    });
    QObject::connect(AIStudioStatusModel::instance(), &AIStudioStatusModel::libraryAssetsUnfileRequested,
                     m_runtimeHost.get(), [this](const QStringList& assetIds) { moveLibraryAssetsToUnfiled(assetIds); });
    QObject::connect(AIStudioStatusModel::instance(), &AIStudioStatusModel::libraryFolderCreateRequested,
                     m_runtimeHost.get(), [this](const QString& folder) { createLibraryFolder(folder); });
    QObject::connect(AIStudioStatusModel::instance(), &AIStudioStatusModel::libraryFolderRenameRequested,
                     m_runtimeHost.get(), [this](const QString& folder, const QString& newFolder) {
        renameLibraryFolder(folder, newFolder);
    });
    QObject::connect(AIStudioStatusModel::instance(), &AIStudioStatusModel::libraryFolderDeleteRequested,
                     m_runtimeHost.get(), [this](const QString& folder) { deleteLibraryFolder(folder); });
    QObject::connect(AIStudioStatusModel::instance(), &AIStudioStatusModel::libraryAssetRenameRequested,
                     m_runtimeHost.get(), [this](const QString& assetId, const QString& name) {
        renameLibraryAsset(assetId, name);
    });
    QObject::connect(AIStudioStatusModel::instance(), &AIStudioStatusModel::libraryAssetDeleteRequested,
                     m_runtimeHost.get(), [this](const QString& assetId) { deleteLibraryAsset(assetId); });
    QObject::connect(AIStudioStatusModel::instance(), &AIStudioStatusModel::libraryAssetTagsRequested,
                     m_runtimeHost.get(), [this](const QString& assetId, const QString& tags) {
        setLibraryAssetTags(assetId, tags);
    });
    QObject::connect(AIStudioStatusModel::instance(), &AIStudioStatusModel::libraryAssetsTagsRequested,
                     m_runtimeHost.get(), [this](const QStringList& assetIds, const QString& tags) {
        setLibraryAssetsTags(assetIds, tags);
    });
    QObject::connect(AIStudioStatusModel::instance(), &AIStudioStatusModel::libraryAssetAudioDetailsRequested,
                     m_runtimeHost.get(), [this](const QString& assetId) { readLibraryAssetAudioDetails(assetId); });
    QObject::connect(AIStudioStatusModel::instance(), &AIStudioStatusModel::libraryAssetsAudioDetailsRequested,
                     m_runtimeHost.get(), [this](const QStringList& assetIds) { readLibraryAssetsAudioDetails(assetIds); });
    QObject::connect(AIStudioStatusModel::instance(), &AIStudioStatusModel::libraryAssetRevealRequested,
                     m_runtimeHost.get(), [this](const QString& assetId) { revealLibraryAssetInExplorer(assetId); });
    QObject::connect(AIStudioStatusModel::instance(), &AIStudioStatusModel::libraryAssetInsertionRequested,
                     m_runtimeHost.get(), [this](const QString& assetId) { addLibraryAssetToTimeline(assetId); });
    QObject::connect(AIStudioStatusModel::instance(), &AIStudioStatusModel::globalLibraryAssetCopyRequested,
                     m_runtimeHost.get(), [this](const QString& projectPath, const QString& assetId) {
        copyGlobalLibraryAssetToProject(projectPath, assetId);
    });
    QObject::connect(AIStudioStatusModel::instance(), &AIStudioStatusModel::jobsRefreshRequested,
                     m_runtimeHost.get(), [this] { refreshJobs(); });
    QObject::connect(AIStudioStatusModel::instance(), &AIStudioStatusModel::jobCancelRequested,
                     m_runtimeHost.get(), [this](const QString& jobId) { cancelJob(jobId); });
    QObject::connect(AIStudioStatusModel::instance(), &AIStudioStatusModel::jobRetryRequested,
                     m_runtimeHost.get(), [this](const QString& jobId) { retryJob(jobId); });
    QObject::connect(AIStudioStatusModel::instance(), &AIStudioStatusModel::jobInsertRequested,
                     m_runtimeHost.get(), [this](const QString& jobId) { insertJobOutput(jobId); });
    QObject::connect(AIStudioStatusModel::instance(), &AIStudioStatusModel::yue2JobRequested,
                     m_runtimeHost.get(), [this](const QString& providerId, const QString& lyrics, const QString& style,
                                                 const QString& seed, const QString& title, const QString& cot,
                                                 const QString& duration, const QString& steps, const QString& guidance,
                                                 const QVariantMap& sampling) {
        submitYue2Job(providerId, lyrics, style, seed, title, cot, duration, steps, guidance, sampling);
    });
    QObject::connect(AIStudioStatusModel::instance(), &AIStudioStatusModel::regeneratePlanRequested,
                     m_runtimeHost.get(), [this] { regenerateFromPlan(); });
    QObject::connect(AIStudioStatusModel::instance(), &AIStudioStatusModel::exampleLoadRequested,
                     m_runtimeHost.get(), [this](int index) { loadExample(index); });
    QObject::connect(AIStudioStatusModel::instance(), &AIStudioStatusModel::createPromptRequested,
                     m_runtimeHost.get(), [this](const QString& lyrics) { createPrompt(lyrics); });
    QObject::connect(AIStudioStatusModel::instance(), &AIStudioStatusModel::improvePromptRequested,
                     m_runtimeHost.get(), [this](const QString& style) { improvePrompt(style); });
    QObject::connect(AIStudioStatusModel::instance(), &AIStudioStatusModel::writeLyricsRequested,
                     m_runtimeHost.get(), [this](const QString& style) { writeLyrics(style); });
    QObject::connect(AIStudioStatusModel::instance(), &AIStudioStatusModel::assistantConfigRequested,
                     m_runtimeHost.get(), [this](const QString& mode, const QString& baseUrl, const QString& model,
                                                 const QString& apiKey, const QString& runnerPath, const QString& modelPath,
                                                 int port) {
        setAssistantConfig(mode, baseUrl, model, apiKey, runnerPath, modelPath, port);
    });
    QObject::connect(AIStudioStatusModel::instance(), &AIStudioStatusModel::importPromptRequested,
                     m_runtimeHost.get(), [this](const QString& path) { importPromptFile(path); });
    QObject::connect(AIStudioStatusModel::instance(), &AIStudioStatusModel::exportPromptRequested,
                     m_runtimeHost.get(), [this](const QString& path, const QString& style, const QString& lyrics,
                                                 const QString& title, const QString& seed, const QString& cot,
                                                 const QString& duration, const QString& steps, const QString& guidance,
                                                 const QVariantMap& sampling) {
        exportPromptFile(path, style, lyrics, title, seed, cot, duration, steps, guidance, sampling);
    });
    QObject::connect(AIStudioStatusModel::instance(), &AIStudioStatusModel::plansRefreshRequested,
                     m_runtimeHost.get(), [this] { refreshPlans(); });
    QObject::connect(AIStudioStatusModel::instance(), &AIStudioStatusModel::planCreateRequested,
                     m_runtimeHost.get(), [this](const QString& name) { createPlan(name); });
    QObject::connect(AIStudioStatusModel::instance(), &AIStudioStatusModel::planLoadRequested,
                     m_runtimeHost.get(), [this](const QString& planId) { loadPlan(planId); });
    QObject::connect(AIStudioStatusModel::instance(), &AIStudioStatusModel::planMetadataRequested,
                     m_runtimeHost.get(), [this](double tempo, const QString& key, const QString& timeSignature) {
        setPlanMetadata(tempo, key, timeSignature);
    });
    QObject::connect(AIStudioStatusModel::instance(), &AIStudioStatusModel::planSectionAddRequested,
                     m_runtimeHost.get(), [this](const QString& name, double start, double end) {
        addPlanSection(name, start, end);
    });
    QObject::connect(AIStudioStatusModel::instance(), &AIStudioStatusModel::planSectionRemoveRequested,
                     m_runtimeHost.get(), [this](int index) { removePlanSection(index); });
    QObject::connect(AIStudioStatusModel::instance(), &AIStudioStatusModel::planChordAddRequested,
                     m_runtimeHost.get(), [this](const QString& symbol, double start, double duration) {
        addPlanChord(symbol, start, duration);
    });
    QObject::connect(AIStudioStatusModel::instance(), &AIStudioStatusModel::planChordRemoveRequested,
                     m_runtimeHost.get(), [this](int index) { removePlanChord(index); });
    QObject::connect(AIStudioStatusModel::instance(), &AIStudioStatusModel::planNoteAddRequested,
                     m_runtimeHost.get(), [this](int midiPitch, double start, double duration, const QString& lyric) {
        addPlanNote(midiPitch, start, duration, lyric);
    });
    QObject::connect(AIStudioStatusModel::instance(), &AIStudioStatusModel::planNoteRemoveRequested,
                     m_runtimeHost.get(), [this](int index) { removePlanNote(index); });
    QObject::connect(AIStudioStatusModel::instance(), &AIStudioStatusModel::planSaveRequested,
                     m_runtimeHost.get(), [this] { savePlanRevision(); });
    dispatcher()->reg(this, OPEN_JOBS_CODE, this, &AIStudioController::openJobs);
    dispatcher()->reg(this, REUSE_PROMPT_CODE, [this](const muse::actions::ActionData& args) {
        if (args.count() < 1) {
            return;
        }
        reusePromptForTrack(args.arg<au::trackedit::ClipKey>(0));
    });
    dispatcher()->reg(this, REPLAY_TRACK_CODE, [this](const muse::actions::ActionData& args) {
        if (args.count() < 1) {
            return;
        }
        replayTrackForClip(args.arg<au::trackedit::ClipKey>(0), false);
    });
    dispatcher()->reg(this, VARY_TRACK_CODE, [this](const muse::actions::ActionData& args) {
        if (args.count() < 1) {
            return;
        }
        replayTrackForClip(args.arg<au::trackedit::ClipKey>(0), true);
    });
}

bool AIStudioController::canReceiveAction(const ActionCode& code) const
{
    return code == OPEN_JOBS_CODE || code == REUSE_PROMPT_CODE
           || code == REPLAY_TRACK_CODE || code == VARY_TRACK_CODE;
}

void AIStudioController::openJobs()
{
    refreshWorkspaceStatus();
    if (m_activeWorkspace.isEmpty()) {
        m_runtimeHost->start();
    } else {
        m_runtimeHost->restartInWorkspace(m_activeWorkspace);
    }
    dispatcher()->dispatch("dock-set-open", ActionData::make_arg2<QString, bool>(AI_STUDIO_DOCK, true));
}

void AIStudioController::enableProjectWorkspace()
{
    const auto project = globalContext()->currentProject();
    const QString projectPath = project ? project->path().toQString() : QString();
    if (projectPath.isEmpty()) {
        // Nothing to attach to yet; surface the "save the project" guidance.
        refreshWorkspaceStatus();
        return;
    }
    if (au::aiproject::WorkspaceStore::isEnabled(projectPath) && !m_activeWorkspace.isEmpty()) {
        // Already attached to this project; just refresh instead of restarting.
        refreshWorkspaceStatus();
        return;
    }
    QString error;
    if (!au::aiproject::WorkspaceStore::create(projectPath, &error)) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(error);
        AIStudioStatusModel::instance()->setLibraryStatus(error);
        return;
    }

    const QString workspace = au::aiproject::WorkspaceStore::workspacePathForProject(projectPath);
    m_activeWorkspace = workspace;
    m_planDraftLoaded = false;
    AIStudioStatusModel::instance()->setPlanDetail({});
    AIStudioStatusModel::instance()->setWorkspaceStatus(QObject::tr("AI workspace enabled: %1").arg(workspace));
    AIStudioStatusModel::instance()->setLibraryStatus(QObject::tr("Library ready for project imports"));
    refreshLibraryAssets();
    refreshJobs();
    refreshPlans();
    m_runtimeHost->restartInWorkspace(workspace);
}

void AIStudioController::refreshWorkspaceStatus()
{
    const auto project = globalContext()->currentProject();
    const QString projectPath = project ? project->path().toQString() : QString();
    if (projectPath.isEmpty()) {
        m_activeWorkspace.clear();
        AIStudioStatusModel::instance()->setLibraryAssets({});
        AIStudioStatusModel::instance()->setLibraryFolders({});
        AIStudioStatusModel::instance()->setJobs({});
        AIStudioStatusModel::instance()->setPlans({});
        m_planDraftLoaded = false;
        AIStudioStatusModel::instance()->setPlanDetail({});
        AIStudioStatusModel::instance()->setWorkspaceStatus(QObject::tr("Save the project to enable its AI workspace"));
        AIStudioStatusModel::instance()->setLibraryStatus(QObject::tr("No project Library is available"));
    } else if (au::aiproject::WorkspaceStore::isEnabled(projectPath)) {
        const QString workspacePath = au::aiproject::WorkspaceStore::workspacePathForProject(projectPath);
        if (m_activeWorkspace != workspacePath) {
            m_planDraftLoaded = false;
            AIStudioStatusModel::instance()->setPlanDetail({});
        }
        m_activeWorkspace = workspacePath;
        QString error;
        const int recovered = au::aijobs::JobStore::recoverInterrupted(m_activeWorkspace, &error);
        if (recovered > 0) {
            AIStudioStatusModel::instance()->setWorkspaceStatus(QObject::tr("Recovered %1 interrupted AI job(s)").arg(recovered));
        } else if (recovered < 0) {
            AIStudioStatusModel::instance()->setWorkspaceStatus(error);
        } else {
            AIStudioStatusModel::instance()->setWorkspaceStatus(QObject::tr("AI workspace enabled: %1")
                                                                 .arg(m_activeWorkspace));
        }
        refreshLibraryAssets();
        refreshJobs();
        refreshPlans();
        importPendingResults();
    } else {
        m_activeWorkspace.clear();
        AIStudioStatusModel::instance()->setLibraryAssets({});
        AIStudioStatusModel::instance()->setLibraryFolders({});
        AIStudioStatusModel::instance()->setJobs({});
        AIStudioStatusModel::instance()->setPlans({});
        m_planDraftLoaded = false;
        AIStudioStatusModel::instance()->setPlanDetail({});
        AIStudioStatusModel::instance()->setWorkspaceStatus(QObject::tr("AI workspace not enabled for this project"));
        AIStudioStatusModel::instance()->setLibraryStatus(QObject::tr("This project has no AI Library yet"));
    }
}

void AIStudioController::refreshLibraryAssets()
{
    if (m_activeWorkspace.isEmpty()) {
        AIStudioStatusModel::instance()->setLibraryAssets({});
        AIStudioStatusModel::instance()->setLibraryFolders({});
        return;
    }

    QString error;
    const QList<au::ailibrary::ProjectAsset> assets = au::ailibrary::LibraryAssetStore::projectAssets(m_activeWorkspace, &error);
    if (!error.isEmpty()) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(error);
        return;
    }
    const QStringList folders = au::ailibrary::LibraryAssetStore::projectFolders(m_activeWorkspace, &error);
    if (!error.isEmpty()) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(error);
        return;
    }

    const auto project = globalContext()->currentProject();
    const QString projectPath = project ? project->path().toQString() : QString();
    QString catalogueError;
    if (!projectPath.isEmpty()
        && !au::ailibrary::GlobalAssetCatalogue::syncProject(projectPath, m_activeWorkspace, assets, &catalogueError)) {
        // A project manifest is portable and authoritative. A local catalogue
        // failure must not hide or block the current project's Library.
        AIStudioStatusModel::instance()->setLibraryStatus(catalogueError);
    }

    catalogueError.clear();
    const QList<au::ailibrary::GlobalAssetRecord> globalAssets
        = au::ailibrary::GlobalAssetCatalogue::allAssets(&catalogueError);
    if (!catalogueError.isEmpty()) {
        AIStudioStatusModel::instance()->setLibraryStatus(catalogueError);
    }
    QVariantList globalRows;
    for (const au::ailibrary::GlobalAssetRecord& record : globalAssets) {
        QFileInfo assetFile(record.asset.filePath);
        if (assetFile.isRelative()) {
            assetFile.setFile(QDir(record.workspacePath).filePath(record.asset.filePath));
        }
        const QString availability = !record.asset.filePath.isEmpty() && !assetFile.exists()
                                     ? QStringLiteral("missing") : record.asset.status;
        const bool audioKind = au::ailibrary::isAudioAssetKind(record.asset.kind);
        globalRows.append(QVariantMap {
            { "id", record.asset.id }, { "name", record.asset.name }, { "kind", record.asset.kind },
            { "origin", record.asset.origin }, { "status", availability }, { "filePath", record.asset.filePath },
            { "createdAt", record.asset.createdAt }, { "durationSeconds", record.asset.durationSeconds },
            { "sampleRate", record.asset.sampleRate }, { "channels", record.asset.channels },
            { "favourite", record.asset.favourite }, { "folder", record.asset.folder }, { "tags", record.asset.tags },
            { "provenanceId", record.asset.provenanceId }, { "sourceAssetIds", record.asset.sourceAssetIds },
            { "projectPath", record.projectPath }, { "isCurrentProject", record.projectPath == projectPath },
            { "canAddToTimeline", audioKind && availability == QStringLiteral("available") },
            { "canReadAudioDetails", audioKind }
        });
    }
    AIStudioStatusModel::instance()->setGlobalLibraryAssets(globalRows);

    QVariantList rows;
    for (const au::ailibrary::ProjectAsset& asset : assets) {
        QFileInfo assetFile(asset.filePath);
        if (assetFile.isRelative()) {
            assetFile.setFile(QDir(m_activeWorkspace).filePath(asset.filePath));
        }
        const QString availability = !asset.filePath.isEmpty() && !assetFile.exists()
                                     ? QStringLiteral("missing") : asset.status;
        const bool audioKind = au::ailibrary::isAudioAssetKind(asset.kind);
        rows.append(QVariantMap {
            { "id", asset.id }, { "name", asset.name }, { "kind", asset.kind },
            { "origin", asset.origin }, { "status", availability }, { "filePath", asset.filePath },
            { "createdAt", asset.createdAt }, { "durationSeconds", asset.durationSeconds },
            { "sampleRate", asset.sampleRate }, { "channels", asset.channels }, { "favourite", asset.favourite }
            , { "folder", asset.folder }, { "tags", asset.tags }, { "provenanceId", asset.provenanceId }
            , { "sourceAssetIds", asset.sourceAssetIds }
            , { "canAddToTimeline", audioKind && availability == QStringLiteral("available") }
            , { "canReadAudioDetails", audioKind }
        });
    }
    AIStudioStatusModel::instance()->setLibraryAssets(rows);
    AIStudioStatusModel::instance()->setLibraryFolders(folders);
}

void AIStudioController::copyGlobalLibraryAssetToProject(const QString& projectPath, const QString& assetId)
{
    if (m_activeWorkspace.isEmpty()) {
        AIStudioStatusModel::instance()->setLibraryStatus(QObject::tr("Open and enable a project Library before copying an archived asset"));
        return;
    }
    QString error;
    const QList<au::ailibrary::GlobalAssetRecord> records
        = au::ailibrary::GlobalAssetCatalogue::allAssets(&error);
    if (!error.isEmpty()) {
        AIStudioStatusModel::instance()->setLibraryStatus(error);
        return;
    }
    const auto record = std::find_if(records.cbegin(), records.cend(), [&projectPath, &assetId](const auto& candidate) {
        return candidate.projectPath == projectPath && candidate.asset.id == assetId;
    });
    if (record == records.cend()) {
        AIStudioStatusModel::instance()->setLibraryStatus(QObject::tr("The archived Library asset is no longer indexed"));
        return;
    }
    QFileInfo source(record->asset.filePath);
    if (source.isRelative()) {
        source.setFile(QDir(record->workspacePath).filePath(record->asset.filePath));
    }
    if (!source.exists() || !source.isFile()) {
        AIStudioStatusModel::instance()->setLibraryStatus(QObject::tr("The archived Library audio file is missing"));
        return;
    }
    importLocalWav(source.filePath());
}

void AIStudioController::importLocalWav(const QString& sourcePath)
{
    if (m_activeWorkspace.isEmpty()) {
        const QString message = QObject::tr("Enable the project AI workspace before importing audio");
        AIStudioStatusModel::instance()->setWorkspaceStatus(message);
        AIStudioStatusModel::instance()->setLibraryStatus(message);
        return;
    }
    const QFileInfo source(sourcePath);
    if (!source.exists() || !source.isFile() || source.suffix().compare("wav", Qt::CaseInsensitive) != 0) {
        const QString message = QObject::tr("Choose an existing WAV file to import into the AI Library");
        AIStudioStatusModel::instance()->setWorkspaceStatus(message);
        AIStudioStatusModel::instance()->setLibraryStatus(message);
        return;
    }

    QFile input(source.filePath());
    if (!input.open(QIODevice::ReadOnly)) {
        const QString message = QObject::tr("Could not read the WAV file for Library import");
        AIStudioStatusModel::instance()->setWorkspaceStatus(message);
        AIStudioStatusModel::instance()->setLibraryStatus(message);
        return;
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&input)) {
        const QString message = QObject::tr("Could not checksum the WAV file for Library import");
        AIStudioStatusModel::instance()->setWorkspaceStatus(message);
        AIStudioStatusModel::instance()->setLibraryStatus(message);
        return;
    }
    const QString checksum = QString::fromLatin1(hash.result().toHex());
    QString lookupError;
    const QList<au::ailibrary::ProjectAsset> existingAssets
        = au::ailibrary::LibraryAssetStore::projectAssets(m_activeWorkspace, &lookupError);
    if (!lookupError.isEmpty()) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(lookupError);
        AIStudioStatusModel::instance()->setLibraryStatus(lookupError);
        return;
    }
    const auto duplicate = std::find_if(existingAssets.cbegin(), existingAssets.cend(), [&checksum](const auto& asset) {
        return !asset.contentChecksum.isEmpty() && asset.contentChecksum == checksum;
    });

    const QString relativeDirectory = "assets/imported";
    const QDir workspace(m_activeWorkspace);
    if (!workspace.mkpath(relativeDirectory)) {
        const QString message = QObject::tr("Could not create the AI Library import folder");
        AIStudioStatusModel::instance()->setWorkspaceStatus(message);
        AIStudioStatusModel::instance()->setLibraryStatus(message);
        return;
    }
    const QString assetId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString relativePath = relativeDirectory + "/" + assetId + "-" + source.fileName();
    const QString destinationPath = workspace.filePath(relativePath);
    if (!QFile::copy(source.filePath(), destinationPath)) {
        const QString message = QObject::tr("Could not copy the WAV file into the AI Library");
        AIStudioStatusModel::instance()->setWorkspaceStatus(message);
        AIStudioStatusModel::instance()->setLibraryStatus(message);
        return;
    }

    au::ailibrary::ProjectAsset asset;
    asset.id = assetId;
    asset.name = source.completeBaseName();
    asset.kind = "upload";
    asset.origin = "uploaded";
    asset.filePath = relativePath;
    const WavMetadata metadata = wavMetadata(source.filePath());
    asset.durationSeconds = metadata.durationSeconds;
    asset.sampleRate = metadata.sampleRate;
    asset.channels = metadata.channels;
    asset.createdAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    asset.provenanceId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    asset.contentChecksum = checksum;
    asset.status = "available";
    au::ailibrary::AssetProvenance provenance;
    provenance.id = asset.provenanceId;
    provenance.assetId = asset.id;
    provenance.operation = "import";
    provenance.providerId = "local-file";
    provenance.createdAt = asset.createdAt;
    provenance.outputChecksum = checksum;
    QString error;
    if (!au::ailibrary::LibraryAssetStore::addProjectAsset(m_activeWorkspace, asset, &provenance, &error)) {
        QFile::remove(destinationPath);
        AIStudioStatusModel::instance()->setWorkspaceStatus(error);
        AIStudioStatusModel::instance()->setLibraryStatus(error);
        return;
    }
    refreshLibraryAssets();
    const QString message = duplicate == existingAssets.cend()
                            ? QObject::tr("Imported %1 into the AI Library").arg(source.fileName())
                            : QObject::tr("Imported %1; it matches existing Library asset %2")
                                  .arg(source.fileName(), duplicate->name);
    AIStudioStatusModel::instance()->setWorkspaceStatus(message);
    AIStudioStatusModel::instance()->setLibraryStatus(message);
}

void AIStudioController::setLibraryAssetFavourite(const QString& assetId, bool favourite)
{
    if (m_activeWorkspace.isEmpty()) {
        AIStudioStatusModel::instance()->setLibraryStatus(QObject::tr("Enable the project AI workspace before changing Library favourites"));
        return;
    }

    QString error;
    if (!au::ailibrary::LibraryAssetStore::setProjectAssetFavourite(m_activeWorkspace, assetId, favourite, &error)) {
        AIStudioStatusModel::instance()->setLibraryStatus(error);
        return;
    }
    refreshLibraryAssets();
    AIStudioStatusModel::instance()->setLibraryStatus(favourite
        ? QObject::tr("Asset added to Favourites")
        : QObject::tr("Asset removed from Favourites"));
}

void AIStudioController::setLibraryAssetsFavourite(const QStringList& assetIds, bool favourite)
{
    if (m_activeWorkspace.isEmpty()) {
        AIStudioStatusModel::instance()->setLibraryStatus(QObject::tr("Enable the project AI workspace before changing Library favourites"));
        return;
    }

    QString error;
    if (!au::ailibrary::LibraryAssetStore::setProjectAssetsFavourite(m_activeWorkspace, assetIds, favourite, &error)) {
        AIStudioStatusModel::instance()->setLibraryStatus(error);
        return;
    }
    refreshLibraryAssets();
    AIStudioStatusModel::instance()->setLibraryStatus(favourite
        ? QObject::tr("Added %1 Library asset(s) to Favourites").arg(assetIds.size())
        : QObject::tr("Removed %1 Library asset(s) from Favourites").arg(assetIds.size()));
}

void AIStudioController::moveLibraryAssetsToFolder(const QStringList& assetIds, const QString& folder)
{
    if (m_activeWorkspace.isEmpty()) {
        AIStudioStatusModel::instance()->setLibraryStatus(QObject::tr("Enable the project AI workspace before moving Library assets"));
        return;
    }

    QString error;
    if (!au::ailibrary::LibraryAssetStore::moveProjectAssetsToFolder(m_activeWorkspace, assetIds, folder, &error)) {
        AIStudioStatusModel::instance()->setLibraryStatus(error);
        return;
    }
    AIStudioStatusModel::instance()->setLibraryStatus(QObject::tr("Moved %1 Library asset(s) to %2")
                                                       .arg(assetIds.size()).arg(folder.trimmed()));
    refreshLibraryAssets();
}

void AIStudioController::moveLibraryAssetsToUnfiled(const QStringList& assetIds)
{
    if (m_activeWorkspace.isEmpty()) {
        AIStudioStatusModel::instance()->setLibraryStatus(QObject::tr("Enable the project AI workspace before moving Library assets"));
        return;
    }

    QString error;
    if (!au::ailibrary::LibraryAssetStore::clearProjectAssetsFolder(m_activeWorkspace, assetIds, &error)) {
        AIStudioStatusModel::instance()->setLibraryStatus(error);
        return;
    }
    AIStudioStatusModel::instance()->setLibraryStatus(QObject::tr("Moved %1 Library asset(s) to Unfiled").arg(assetIds.size()));
    refreshLibraryAssets();
}

void AIStudioController::createLibraryFolder(const QString& folder)
{
    if (m_activeWorkspace.isEmpty()) {
        AIStudioStatusModel::instance()->setLibraryStatus(QObject::tr("Enable the project AI workspace before creating a Library folder"));
        return;
    }
    QString error;
    if (!au::ailibrary::LibraryAssetStore::createProjectFolder(m_activeWorkspace, folder, &error)) {
        AIStudioStatusModel::instance()->setLibraryStatus(error);
        return;
    }
    AIStudioStatusModel::instance()->setLibraryStatus(QObject::tr("Created Library folder %1").arg(folder.trimmed()));
    refreshLibraryAssets();
}

void AIStudioController::renameLibraryFolder(const QString& folder, const QString& newFolder)
{
    if (m_activeWorkspace.isEmpty()) {
        AIStudioStatusModel::instance()->setLibraryStatus(QObject::tr("Enable the project AI workspace before renaming a Library folder"));
        return;
    }
    QString error;
    if (!au::ailibrary::LibraryAssetStore::renameProjectFolder(m_activeWorkspace, folder, newFolder, &error)) {
        AIStudioStatusModel::instance()->setLibraryStatus(error);
        return;
    }
    AIStudioStatusModel::instance()->setLibraryStatus(QObject::tr("Renamed Library folder to %1").arg(newFolder.trimmed()));
    refreshLibraryAssets();
}

void AIStudioController::deleteLibraryFolder(const QString& folder)
{
    if (m_activeWorkspace.isEmpty()) {
        AIStudioStatusModel::instance()->setLibraryStatus(QObject::tr("Enable the project AI workspace before deleting a Library folder"));
        return;
    }
    QString error;
    if (!au::ailibrary::LibraryAssetStore::deleteProjectFolder(m_activeWorkspace, folder, &error)) {
        AIStudioStatusModel::instance()->setLibraryStatus(error);
        return;
    }
    AIStudioStatusModel::instance()->setLibraryStatus(QObject::tr("Deleted empty Library folder %1").arg(folder));
    refreshLibraryAssets();
}

void AIStudioController::renameLibraryAsset(const QString& assetId, const QString& name)
{
    if (m_activeWorkspace.isEmpty()) {
        AIStudioStatusModel::instance()->setLibraryStatus(QObject::tr("Enable the project AI workspace before renaming a Library asset"));
        return;
    }
    QString error;
    if (!au::ailibrary::LibraryAssetStore::renameProjectAsset(m_activeWorkspace, assetId, name, &error)) {
        AIStudioStatusModel::instance()->setLibraryStatus(error);
        return;
    }
    AIStudioStatusModel::instance()->setLibraryStatus(QObject::tr("Renamed Library asset to %1").arg(name.trimmed()));
    refreshLibraryAssets();
}

void AIStudioController::deleteLibraryAsset(const QString& assetId)
{
    if (m_activeWorkspace.isEmpty()) {
        AIStudioStatusModel::instance()->setLibraryStatus(QObject::tr("Enable the project AI workspace before removing a Library asset"));
        return;
    }
    QString error;
    if (!au::ailibrary::LibraryAssetStore::deleteProjectAsset(m_activeWorkspace, assetId, &error)) {
        AIStudioStatusModel::instance()->setLibraryStatus(error);
        return;
    }
    AIStudioStatusModel::instance()->setLibraryStatus(QObject::tr("Removed asset from the Library"));
    refreshLibraryAssets();
}

void AIStudioController::setLibraryAssetTags(const QString& assetId, const QString& tags)
{
    if (m_activeWorkspace.isEmpty()) {
        AIStudioStatusModel::instance()->setLibraryStatus(QObject::tr("Enable the project AI workspace before changing Library tags"));
        return;
    }
    QStringList cleanedTags;
    for (const QString& tag : tags.split(',', Qt::SkipEmptyParts)) {
        const QString cleaned = tag.trimmed();
        if (!cleaned.isEmpty() && !cleanedTags.contains(cleaned, Qt::CaseInsensitive)) {
            cleanedTags.append(cleaned);
        }
    }
    QString error;
    if (!au::ailibrary::LibraryAssetStore::setProjectAssetTags(m_activeWorkspace, assetId, cleanedTags, &error)) {
        AIStudioStatusModel::instance()->setLibraryStatus(error);
        return;
    }
    AIStudioStatusModel::instance()->setLibraryStatus(QObject::tr("Updated Library asset tags"));
    refreshLibraryAssets();
}

void AIStudioController::setLibraryAssetsTags(const QStringList& assetIds, const QString& tags)
{
    if (m_activeWorkspace.isEmpty()) {
        AIStudioStatusModel::instance()->setLibraryStatus(QObject::tr("Enable the project AI workspace before changing Library tags"));
        return;
    }
    QStringList cleanedTags;
    for (const QString& tag : tags.split(',', Qt::SkipEmptyParts)) {
        const QString cleaned = tag.trimmed();
        if (!cleaned.isEmpty() && !cleanedTags.contains(cleaned, Qt::CaseInsensitive)) {
            cleanedTags.append(cleaned);
        }
    }
    QString error;
    if (!au::ailibrary::LibraryAssetStore::setProjectAssetsTags(m_activeWorkspace, assetIds, cleanedTags, &error)) {
        AIStudioStatusModel::instance()->setLibraryStatus(error);
        return;
    }
    AIStudioStatusModel::instance()->setLibraryStatus(QObject::tr("Updated tags for %1 Library asset(s)").arg(assetIds.size()));
    refreshLibraryAssets();
}

void AIStudioController::readLibraryAssetAudioDetails(const QString& assetId)
{
    if (m_activeWorkspace.isEmpty()) {
        AIStudioStatusModel::instance()->setLibraryStatus(QObject::tr("Enable the project AI workspace before reading WAV details"));
        return;
    }
    QString error;
    const QList<au::ailibrary::ProjectAsset> assets
        = au::ailibrary::LibraryAssetStore::projectAssets(m_activeWorkspace, &error);
    if (!error.isEmpty()) {
        AIStudioStatusModel::instance()->setLibraryStatus(error);
        return;
    }
    const auto found = std::find_if(assets.cbegin(), assets.cend(), [&assetId](const auto& asset) {
        return asset.id == assetId;
    });
    if (found == assets.cend()) {
        AIStudioStatusModel::instance()->setLibraryStatus(QObject::tr("The selected AI Library asset no longer exists"));
        return;
    }
    const WavMetadata metadata = wavMetadata(QDir(m_activeWorkspace).filePath(found->filePath));
    if (metadata.sampleRate <= 0) {
        AIStudioStatusModel::instance()->setLibraryStatus(QObject::tr("Could not read standard WAV details for the selected Library asset"));
        return;
    }
    if (!au::ailibrary::LibraryAssetStore::setProjectAssetAudioMetadata(m_activeWorkspace, assetId,
                                                                          metadata.durationSeconds, metadata.sampleRate,
                                                                          metadata.channels, &error)) {
        AIStudioStatusModel::instance()->setLibraryStatus(error);
        return;
    }
    refreshLibraryAssets();
    AIStudioStatusModel::instance()->setLibraryStatus(QObject::tr("Updated WAV details for %1").arg(found->name));
}

void AIStudioController::readLibraryAssetsAudioDetails(const QStringList& assetIds)
{
    if (assetIds.size() == 1) {
        readLibraryAssetAudioDetails(assetIds.front());
        return;
    }
    if (m_activeWorkspace.isEmpty()) {
        AIStudioStatusModel::instance()->setLibraryStatus(QObject::tr("Enable the project AI workspace before reading WAV details"));
        return;
    }
    QString error;
    const QList<au::ailibrary::ProjectAsset> assets
        = au::ailibrary::LibraryAssetStore::projectAssets(m_activeWorkspace, &error);
    if (!error.isEmpty()) {
        AIStudioStatusModel::instance()->setLibraryStatus(error);
        return;
    }

    int updated = 0;
    QStringList skippedAssets;
    for (const QString& assetId : assetIds) {
        const auto found = std::find_if(assets.cbegin(), assets.cend(), [&assetId](const auto& asset) {
            return asset.id == assetId;
        });
        if (found == assets.cend()) {
            skippedAssets.append(assetId);
            continue;
        }
        const WavMetadata metadata = wavMetadata(QDir(m_activeWorkspace).filePath(found->filePath));
        if (metadata.sampleRate <= 0
            || !au::ailibrary::LibraryAssetStore::setProjectAssetAudioMetadata(m_activeWorkspace, assetId,
                                                                                metadata.durationSeconds, metadata.sampleRate,
                                                                                metadata.channels, &error)) {
            skippedAssets.append(found->name);
            error.clear();
            continue;
        }
        ++updated;
    }
    refreshLibraryAssets();
    AIStudioStatusModel::instance()->setLibraryStatus(!skippedAssets.isEmpty()
        ? QObject::tr("Updated WAV details for %1 Library asset(s); skipped: %2")
              .arg(updated).arg(skippedAssets.join(", "))
        : QObject::tr("Updated WAV details for %1 Library asset(s)").arg(updated));
}

void AIStudioController::revealLibraryAssetInExplorer(const QString& assetId)
{
    if (m_activeWorkspace.isEmpty()) {
        AIStudioStatusModel::instance()->setLibraryStatus(QObject::tr("Enable the project AI workspace before revealing a Library asset"));
        return;
    }
    QString error;
    const QList<au::ailibrary::ProjectAsset> assets = au::ailibrary::LibraryAssetStore::projectAssets(m_activeWorkspace, &error);
    if (!error.isEmpty()) {
        AIStudioStatusModel::instance()->setLibraryStatus(error);
        return;
    }
    const auto found = std::find_if(assets.cbegin(), assets.cend(), [&assetId](const auto& asset) {
        return asset.id == assetId;
    });
    if (found == assets.cend()) {
        AIStudioStatusModel::instance()->setLibraryStatus(QObject::tr("The selected AI Library asset no longer exists"));
        return;
    }
    const QString assetPath = QDir(m_activeWorkspace).filePath(found->filePath);
    const QFileInfo assetInfo(assetPath);
    if (!assetInfo.exists()) {
        AIStudioStatusModel::instance()->setLibraryStatus(QObject::tr("The selected AI Library file is missing"));
        return;
    }
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(assetInfo.absolutePath()))) {
        AIStudioStatusModel::instance()->setLibraryStatus(QObject::tr("Could not open the Library asset folder"));
        return;
    }
    AIStudioStatusModel::instance()->setLibraryStatus(QObject::tr("Opened the Library asset folder"));
}

void AIStudioController::addLibraryAssetToTimeline(const QString& assetId)
{
    if (m_activeWorkspace.isEmpty()) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(QObject::tr("Enable the project AI workspace before adding a Library asset"));
        return;
    }
    const auto project = globalContext()->currentProject();
    if (!project) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(QObject::tr("Open a project before adding a Library asset to the timeline"));
        return;
    }
    QString error;
    const QList<au::ailibrary::ProjectAsset> assets = au::ailibrary::LibraryAssetStore::projectAssets(m_activeWorkspace, &error);
    if (!error.isEmpty()) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(error);
        return;
    }
    const auto found = std::find_if(assets.cbegin(), assets.cend(), [&assetId](const auto& asset) {
        return asset.id == assetId;
    });
    if (found == assets.cend()) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(QObject::tr("The selected AI Library asset no longer exists"));
        return;
    }
    const QString assetPath = QDir(m_activeWorkspace).filePath(found->filePath);
    if (!QFileInfo::exists(assetPath)) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(QObject::tr("The selected AI Library file is missing"));
        return;
    }
    if (!project->import(muse::io::path_t(assetPath), false)) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(QObject::tr("Could not add the AI Library asset to the timeline"));
        return;
    }
    AIStudioStatusModel::instance()->setWorkspaceStatus(QObject::tr("Added %1 to the timeline; the Library asset remains available")
                                                        .arg(found->name));
}

void AIStudioController::recordJobStatus(const au::aicore::JobStatus& status)
{
    if (m_activeWorkspace.isEmpty()) {
        return;
    }
    const QString jobId = QString::fromStdString(status.id.value);
    QString error;
    if (au::aijobs::JobStore::upsert(m_activeWorkspace, status, &error)) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(
            QObject::tr("Provider job %1 in the AI workspace")
                .arg(QString::fromStdString(au::aicore::toString(status.state))));
        const bool terminal = au::aicore::isTerminal(status.state);
        if (!terminal && QString::fromStdString(status.providerId) == QStringLiteral("yue2-native")) {
            updateJobPlaceholder(jobId, status.progress);
        }
        if (status.state == au::aicore::JobState::Complete) {
            registerJobArtifacts(jobId, QString::fromStdString(status.resultManifest));
            removeJobPlaceholder(jobId);
            if (QString::fromStdString(status.providerId) == QLatin1String("yue2-cpp-plan")) {
                // A score-only job: show its plan straight away for editing.
                loadPlanForJob(jobId);
            }
            // Generations are placed on the timeline automatically, so there is
            // no separate "insert" step for the user to perform.
            QString insertError;
            bool inserted = false;
            const bool hasOutput
                = !au::aijobs::JobStore::resultAssetPath(m_activeWorkspace, jobId, &insertError).isEmpty();
            if (hasOutput
                && au::aijobs::JobStore::isInserted(m_activeWorkspace, jobId, &inserted, &insertError) && !inserted) {
                insertJobOutput(jobId);
            }
        } else if (terminal) {
            removeJobPlaceholder(jobId);
        }
        refreshJobs();
    } else {
        AIStudioStatusModel::instance()->setWorkspaceStatus(error);
    }
}

namespace {
QString placeholderTrackTitle(const QString& jobId, double progress)
{
    const int percent = qBound(0, int(progress * 100.0 + 0.5), 100);
    const int filled = percent / 10;
    const QString bar = QString(filled, QLatin1Char('#')) + QString(10 - filled, QLatin1Char('-'));
    return QObject::tr("Generating [%1] %2%  %3").arg(bar).arg(percent).arg(jobId);
}
}

void AIStudioController::updateJobPlaceholder(const QString& jobId, double progress)
{
    if (!tracks() || !globalContext()->currentProject()) {
        return;
    }
    const QString title = placeholderTrackTitle(jobId, progress);
    const auto existing = m_jobPlaceholders.find(jobId);
    if (existing != m_jobPlaceholders.end()) {
        // Progress is shown live in the placeholder track's title.
        tracks()->changeTrackTitle(existing.value(), muse::String::fromQString(title));
        return;
    }
    const au::trackedit::TrackId trackId = tracks()->addWaveTrack(2);
    if (trackId < 0) {
        return;
    }
    tracks()->changeTrackTitle(trackId, muse::String::fromQString(title));
    // A silent clip so the pending render is visible on the timeline; it is
    // replaced by the rendered audio (and deleted) when the job completes.
    tracks()->insertSilence(au::trackedit::TrackIdList { trackId }, 0.0, 0.0, 30.0);
    m_jobPlaceholders.insert(jobId, trackId);
    AIStudioStatusModel::instance()->setWorkspaceStatus(
        QObject::tr("Generating %1 — placeholder added to the timeline").arg(jobId));
}

void AIStudioController::removeJobPlaceholder(const QString& jobId)
{
    const auto it = m_jobPlaceholders.find(jobId);
    if (it == m_jobPlaceholders.end()) {
        return;
    }
    if (tracks()) {
        tracks()->deleteTracks(au::trackedit::TrackIdList { it.value() });
    }
    m_jobPlaceholders.erase(it);
}

void AIStudioController::refreshJobs()
{
    if (m_activeWorkspace.isEmpty()) {
        AIStudioStatusModel::instance()->setJobs({});
        return;
    }
    QString error;
    const QList<au::aicore::JobStatus> jobs = au::aijobs::JobStore::jobs(m_activeWorkspace, &error);
    if (!error.isEmpty()) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(error);
        return;
    }
    QVariantList rows;
    for (const au::aicore::JobStatus& job : jobs) {
        bool inserted = false;
        QString insertedError;
        au::aijobs::JobStore::isInserted(m_activeWorkspace, QString::fromStdString(job.id.value), &inserted, &insertedError);
        rows.append(QVariantMap {
            { "id", QString::fromStdString(job.id.value) },
            { "providerId", QString::fromStdString(job.providerId) },
            { "state", QString::fromStdString(au::aicore::toString(job.state)) },
            { "progress", job.progress },
            { "message", QString::fromStdString(job.message) },
            { "resultManifest", QString::fromStdString(job.resultManifest) },
            { "errorMessage", QString::fromStdString(job.errorMessage) },
            { "inserted", inserted }
        });
    }
    AIStudioStatusModel::instance()->setJobs(rows);
}

void AIStudioController::cancelJob(const QString& jobId)
{
    if (m_activeWorkspace.isEmpty()) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(QObject::tr("Enable the project AI workspace before cancelling a job"));
        return;
    }
    QString error;
    if (!m_runtimeHost->cancel(jobId, &error)) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(error);
    }
}

void AIStudioController::retryJob(const QString& jobId)
{
    if (m_activeWorkspace.isEmpty()) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(QObject::tr("Enable the project AI workspace before retrying a job"));
        return;
    }
    au::aicore::JobStatus job;
    QString error;
    if (!au::aijobs::JobStore::find(m_activeWorkspace, jobId, &job, &error)) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(error);
        return;
    }
    if (!au::aicore::isTerminal(job.state)) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(QObject::tr("The selected job is still running"));
        return;
    }
    const au::aicore::JobRequest request { job.providerId, "{}" };
    if (!m_runtimeHost->submit(request, &error)) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(error);
        return;
    }
    AIStudioStatusModel::instance()->setWorkspaceStatus(QObject::tr("Retrying provider job"));
}

namespace {
QString savedRequestField(const QString& workspace, const QString& jobId, const QString& field)
{
    QFile file(QDir(workspace).filePath(QStringLiteral("jobs/%1/request.json").arg(jobId)));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    return root.value(QStringLiteral("parameters")).toObject().value(field).toString();
}
}

void AIStudioController::insertJobOutput(const QString& jobId)
{
    if (m_activeWorkspace.isEmpty()) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(QObject::tr("Enable the project AI workspace before inserting provider output"));
        return;
    }
    const auto project = globalContext()->currentProject();
    if (!project) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(QObject::tr("Open a project before inserting provider output"));
        return;
    }
    QString error;
    const QString assetPath = au::aijobs::JobStore::resultAssetPath(m_activeWorkspace, jobId, &error);
    if (assetPath.isEmpty()) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(error);
        return;
    }

    const au::trackedit::ITrackeditProjectPtr trackProject = project->trackeditProject();
    std::vector<au::trackedit::TrackId> before;
    if (trackProject) {
        before = trackProject->trackIdList();
    }

    if (!project->import(muse::io::path_t(assetPath), false)) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(QObject::tr("Could not insert the provider output"));
        return;
    }

    // Link the freshly imported track to the job so its prompt can be reused,
    // and name it after the saved song title when one was provided.
    if (trackProject) {
        const std::vector<au::trackedit::TrackId> after = trackProject->trackIdList();
        for (const au::trackedit::TrackId& trackId : after) {
            if (std::find(before.begin(), before.end(), trackId) != before.end()) {
                continue;
            }
            const QString savedTitle = savedRequestField(m_activeWorkspace, jobId, QStringLiteral("title"));
            const QString displayName = savedTitle.trimmed().isEmpty()
                                        ? QObject::tr("YuE2 %1").arg(jobId.mid(5, 8))
                                        : savedTitle.trimmed();
            if (tracks()) {
                tracks()->changeTrackTitle(trackId, muse::String::fromQString(displayName));
            }
            au::trackedit::ClipKey newClipKey;
            for (const au::trackedit::Clip& clip : trackProject->clipList(trackId)) {
                newClipKey = clip.key;
                break;
            }
            if (newClipKey.isValid() && trackedit()) {
                trackedit()->changeClipTitle(newClipKey, muse::String::fromQString(displayName));
            }
            au::aijobs::JobStore::setTrackJob(m_activeWorkspace, trackId, jobId);
            au::aijobs::JobStore::setJobTitle(m_activeWorkspace, displayName, jobId);
            // Show this clip's song plan straight away.
            if (newClipKey.isValid()) {
                loadPlanForClip(newClipKey);
            } else {
                loadPlanForJob(jobId);
            }
            break;
        }
    }

    if (au::aijobs::JobStore::markInserted(m_activeWorkspace, jobId, &error)) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(QObject::tr("Provider output inserted as a new track"));
        refreshJobs();
    } else {
        AIStudioStatusModel::instance()->setWorkspaceStatus(error);
    }
}

void AIStudioController::reusePromptForTrack(const au::trackedit::ClipKey& clipKey)
{
    if (m_activeWorkspace.isEmpty()) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(
            QObject::tr("Save the project and open AI Studio before reusing a prompt"));
        return;
    }
    const QString jobId = jobIdForClip(clipKey);
    if (jobId.isEmpty()) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(
            QObject::tr("No saved prompt is linked to this clip"));
        return;
    }
    QFile file(QDir(m_activeWorkspace).filePath(QStringLiteral("jobs/%1/request.json").arg(jobId)));
    if (!file.open(QIODevice::ReadOnly)) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(
            QObject::tr("Could not read the saved prompt for this clip"));
        return;
    }
    const QJsonObject parameters
        = QJsonDocument::fromJson(file.readAll()).object().value(QStringLiteral("parameters")).toObject();
    const QString seed = parameters.contains(QStringLiteral("seed"))
                         ? QString::number(parameters.value(QStringLiteral("seed")).toInt())
                         : QString();
    const QString steps = parameters.contains(QStringLiteral("steps"))
                          ? QString::number(parameters.value(QStringLiteral("steps")).toInt())
                          : QString();
    const QString duration = parameters.contains(QStringLiteral("duration"))
                             ? QString::number(parameters.value(QStringLiteral("duration")).toDouble())
                             : QString();
    const QString guidance = parameters.contains(QStringLiteral("guidance_scale"))
                             ? QString::number(parameters.value(QStringLiteral("guidance_scale")).toDouble())
                             : QString();
    const QString lyrics = parameters.contains(QStringLiteral("lyrics"))
                           ? parameters.value(QStringLiteral("lyrics")).toString()
                           : parameters.value(QStringLiteral("text")).toString();
    QVariantMap sampling;
    const QJsonObject options = parameters.value(QStringLiteral("options")).toObject();
    for (auto it = options.constBegin(); it != options.constEnd(); ++it) {
        sampling.insert(it.key(), it.value().toVariant().toString());
    }
    AIStudioStatusModel::instance()->setPromptReuse(
        parameters.value(QStringLiteral("style")).toString(),
        lyrics,
        parameters.value(QStringLiteral("title")).toString(),
        seed,
        parameters.value(QStringLiteral("cot")).toString(),
        duration,
        steps,
        guidance,
        sampling);
    loadPlanForJob(jobId);
    dispatcher()->dispatch("dock-set-open", ActionData::make_arg2<QString, bool>(AI_STUDIO_DOCK, true));
    AIStudioStatusModel::instance()->setWorkspaceStatus(
        QObject::tr("Loaded the saved prompt — adjust and generate again"));
}

void AIStudioController::replayTrackForClip(const au::trackedit::ClipKey& clipKey, bool vary)
{
    if (m_activeWorkspace.isEmpty()) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(
            QObject::tr("Save the project and open AI Studio before replaying"));
        return;
    }
    const QString jobId = jobIdForClip(clipKey);
    if (jobId.isEmpty()) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(
            QObject::tr("No saved generation is linked to this clip"));
        return;
    }
    QFile file(QDir(m_activeWorkspace).filePath(QStringLiteral("jobs/%1/replay.json").arg(jobId)));
    if (!file.open(QIODevice::ReadOnly)) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(
            QObject::tr("This clip has no replay data (only yue2.cpp generations carry it)"));
        return;
    }
    QJsonObject request = QJsonDocument::fromJson(file.readAll()).object();
    if (!request.contains(QStringLiteral("semantic_tokens"))) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(QObject::tr("This clip has no replay data"));
        return;
    }
    if (vary) {
        // New acoustic noise, same composition: a different performance.
        request.insert(QStringLiteral("seed"),
                       static_cast<int>(QRandomGenerator::global()->generate() & 0x7fffffff));
    }
    const au::aicore::JobRequest job {
        "yue2-cpp",
        QJsonDocument(request).toJson(QJsonDocument::Compact).toStdString()
    };
    QString error;
    if (!m_runtimeHost->submit(job, &error)) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(error);
        return;
    }
    AIStudioStatusModel::instance()->setWorkspaceStatus(
        vary ? QObject::tr("Rendering a variation of the selected clip")
             : QObject::tr("Replaying the selected clip"));
}

QString AIStudioController::jobIdForClip(const au::trackedit::ClipKey& clipKey) const
{
    // The generated clip's title carries the job id, so the link survives a
    // project reload (track and clip ids are reassigned when a project loads).
    const auto project = globalContext()->currentProject();
    if (project) {
        if (const au::trackedit::ITrackeditProjectPtr prj = project->trackeditProject()) {
            const au::trackedit::Clip clip = prj->clip(clipKey);
            const QString title = QString::fromStdString(clip.title.toStdString());
            if (title.startsWith(QStringLiteral("yue2-"))) {
                return title;
            }
            const QString byTitle = au::aijobs::JobStore::jobForTitle(m_activeWorkspace, title);
            if (!byTitle.isEmpty()) {
                return byTitle;
            }
        }
    }
    return au::aijobs::JobStore::jobForTrack(m_activeWorkspace, clipKey.trackId);
}

void AIStudioController::loadPlanForClip(const au::trackedit::ClipKey& clipKey)
{
    loadPlanForJob(jobIdForClip(clipKey));
}

void AIStudioController::loadPlanForJob(const QString& jobId)
{
    if (m_activeWorkspace.isEmpty() || jobId.isEmpty()
        || au::songplan::SongPlanStore::latestRevision(m_activeWorkspace, jobId, nullptr) <= 0) {
        m_planDraftLoaded = false;
        AIStudioStatusModel::instance()->setPlanDetail({});
        AIStudioStatusModel::instance()->setWorkspaceStatus(QObject::tr("This clip has no AI song plan"));
        return;
    }
    au::songplan::SongPlan plan;
    QString error;
    if (!au::songplan::SongPlanStore::loadLatest(m_activeWorkspace, jobId, &plan, &error)) {
        m_planDraftLoaded = false;
        AIStudioStatusModel::instance()->setPlanDetail({});
        AIStudioStatusModel::instance()->setWorkspaceStatus(error);
        return;
    }
    m_planDraft = plan;
    m_planDraftLoaded = true;
    m_planDraftSeed.clear();
    QFile requestFile(QDir(m_activeWorkspace).filePath(QStringLiteral("jobs/%1/request.json").arg(jobId)));
    if (requestFile.open(QIODevice::ReadOnly)) {
        const QJsonObject parameters
            = QJsonDocument::fromJson(requestFile.readAll()).object().value(QStringLiteral("parameters")).toObject();
        if (parameters.contains(QStringLiteral("seed"))) {
            m_planDraftSeed = QString::number(parameters.value(QStringLiteral("seed")).toInt());
        }
    }
    pushPlanDetail();
    AIStudioStatusModel::instance()->setWorkspaceStatus(
        QObject::tr("Song plan for the selected clip — %1 sections").arg(plan.sections.size()));
}

void AIStudioController::onClipSelectionChanged()
{
    if (!selectionController() || !selectionController()->hasSelectedClips()) {
        m_planDraftLoaded = false;
        AIStudioStatusModel::instance()->setPlanDetail({});
        return;
    }
    const au::trackedit::ClipKeyList clips = selectionController()->selectedClips();
    if (clips.empty()) {
        m_planDraftLoaded = false;
        AIStudioStatusModel::instance()->setPlanDetail({});
        return;
    }
    loadPlanForClip(clips.front());
}

void AIStudioController::applyModelSettings()
{
    const au::aimodels::ProviderConfig yue2
        = au::aimodels::ModelSettings::provider(QStringLiteral("yue2-native"));
    const QString envCli = qEnvironmentVariable("AI_YUE2_CLI");
    const QString envModel = qEnvironmentVariable("AI_YUE2_MODEL");
    const bool configured = yue2.isConfigured() || (!envCli.isEmpty() && !envModel.isEmpty());
    m_runtimeHost->setProviderConfig(yue2.cliPath, yue2.modelPath,
                                     yue2.threads > 0 ? QString::number(yue2.threads) : QString());
    AIStudioStatusModel::instance()->updateModelCliPath(yue2.cliPath.isEmpty() ? envCli : yue2.cliPath);
    AIStudioStatusModel::instance()->updateModelModelPath(yue2.modelPath.isEmpty() ? envModel : yue2.modelPath);
    AIStudioStatusModel::instance()->setModelConfigured(configured);

    const au::aimodels::Yue2CppConfig yue2cpp = au::aimodels::ModelSettings::yue2Cpp();
    m_runtimeHost->setYue2CppConfig(yue2cpp.enginePath, yue2cpp.backbonePath, yue2cpp.vaePath, yue2cpp.transcriberPath,
                                    yue2cpp.planToolPath, yue2cpp.transcribeToolPath,
                                    yue2cpp.host, yue2cpp.port, yue2cpp.ggmlBackend);
    AIStudioStatusModel::instance()->setModelYue2CppStatus(
        yue2cpp.isConfigured()
            ? QObject::tr("yue2.cpp engine available: %1").arg(yue2cpp.backbonePath.section(QLatin1Char('/'), -1))
            : QObject::tr("yue2.cpp engine not found"));
}

void AIStudioController::setModelCliPath(const QString& path)
{
    au::aimodels::ProviderConfig yue2 = au::aimodels::ModelSettings::provider(QStringLiteral("yue2-native"));
    yue2.cliPath = path;
    QString error;
    if (!au::aimodels::ModelSettings::setProvider(yue2, &error)) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(error);
        return;
    }
    applyModelSettings();
    if (m_runtimeHost && !m_runtimeHost->isBusy() && !m_activeWorkspace.isEmpty()) {
        m_runtimeHost->restartInWorkspace(m_activeWorkspace);
    }
    AIStudioStatusModel::instance()->setWorkspaceStatus(QObject::tr("Saved the YuE2 CLI path"));
}

void AIStudioController::setModelModelPath(const QString& path)
{
    au::aimodels::ProviderConfig yue2 = au::aimodels::ModelSettings::provider(QStringLiteral("yue2-native"));
    yue2.modelPath = path;
    QString error;
    if (!au::aimodels::ModelSettings::setProvider(yue2, &error)) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(error);
        return;
    }
    applyModelSettings();
    if (m_runtimeHost && !m_runtimeHost->isBusy() && !m_activeWorkspace.isEmpty()) {
        m_runtimeHost->restartInWorkspace(m_activeWorkspace);
    }
    AIStudioStatusModel::instance()->setWorkspaceStatus(QObject::tr("Saved the YuE2 model folder"));
}

namespace {
QJsonObject samplingOptions(const QVariantMap& sampling)
{
    QJsonObject options;
    for (auto it = sampling.constBegin(); it != sampling.constEnd(); ++it) {
        const QString value = it.value().toString().trimmed();
        if (!value.isEmpty()) {
            options.insert(it.key(), value);
        }
    }
    return options;
}
}

void AIStudioController::submitYue2Job(const QString& providerId, const QString& lyrics, const QString& style,
                                       const QString& seed, const QString& title, const QString& cot,
                                       const QString& duration, const QString& steps, const QString& guidance,
                                       const QVariantMap& sampling)
{
    if (m_activeWorkspace.isEmpty()) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(QObject::tr("Enable the project AI workspace before generating a song"));
        return;
    }
    bool seedOk = false;
    int seedValue = seed.trimmed().toInt(&seedOk);
    if (!seedOk) {
        // No seed supplied: pick one so repeated generations differ. The seed is
        // recorded in the request, so Reuse Prompt reproduces the same song.
        seedValue = static_cast<int>(QRandomGenerator::global()->generate() & 0x7fffffff);
    }
    AIStudioStatusModel::instance()->updateCurrentSeed(QString::number(seedValue));
    bool stepsOk = false;
    const int stepsValue = steps.trimmed().toInt(&stepsOk);
    bool guidanceOk = false;
    const double guidanceValue = guidance.trimmed().toDouble(&guidanceOk);
    bool durationOk = false;
    const double durationValue = duration.trimmed().toDouble(&durationOk);

    const QString effectiveProvider = providerId.trimmed().isEmpty() ? QStringLiteral("yue2-native") : providerId.trimmed();
    QJsonObject parameters;
    if (effectiveProvider.startsWith(QLatin1String("yue2-cpp"))) {
        // The yue2.cpp server takes its own Yue2Request JSON.
        parameters.insert(QStringLiteral("style"), style);
        parameters.insert(QStringLiteral("lyrics"), lyrics);
        parameters.insert(QStringLiteral("cot"), cot.trimmed().isEmpty() ? QStringLiteral("full") : cot.trimmed());
        parameters.insert(QStringLiteral("steps"), stepsOk && stepsValue > 0 ? stepsValue : 32);
        parameters.insert(QStringLiteral("output_format"), QStringLiteral("wav16"));
        parameters.insert(QStringLiteral("lm_seed"), seedValue);
        parameters.insert(QStringLiteral("seed"), seedValue);
        if (durationOk && durationValue > 0.0) {
            parameters.insert(QStringLiteral("duration"), durationValue);
        }
        if (guidanceOk && guidanceValue > 0.0) {
            parameters.insert(QStringLiteral("cfg_scale"), guidanceValue);
        }
        const auto addSampling = [&parameters, &sampling](const QString& prefix, const QString& field) {
            QJsonObject block;
            const auto put = [&block, &sampling, &prefix](const QString& suffix) {
                const QString value = sampling.value(prefix + QLatin1Char('_') + suffix).toString().trimmed();
                bool ok = false;
                const double number = value.toDouble(&ok);
                if (ok) {
                    block.insert(suffix, number);
                }
            };
            put(QStringLiteral("temperature"));
            put(QStringLiteral("top_p"));
            put(QStringLiteral("top_k"));
            put(QStringLiteral("repetition_penalty"));
            if (!block.isEmpty()) {
                parameters.insert(field, block);
            }
        };
        addSampling(QStringLiteral("abc"), QStringLiteral("abc_sampling"));
        addSampling(QStringLiteral("semantic"), QStringLiteral("semantic_sampling"));
    } else {
        parameters.insert(QStringLiteral("lyrics"), lyrics);
        if (!style.trimmed().isEmpty()) {
            parameters.insert(QStringLiteral("style"), style);
        }
        if (!cot.trimmed().isEmpty()) {
            parameters.insert(QStringLiteral("cot"), cot.trimmed());
        }
        parameters.insert(QStringLiteral("seed"), seedValue);
        if (stepsOk && stepsValue > 0) {
            parameters.insert(QStringLiteral("steps"), stepsValue);
        }
        if (guidanceOk && guidanceValue > 0.0) {
            parameters.insert(QStringLiteral("guidance_scale"), guidanceValue);
        }
        if (!sampling.isEmpty()) {
            parameters.insert(QStringLiteral("options"), samplingOptions(sampling));
        }
    }
    if (!title.trimmed().isEmpty()) {
        parameters.insert(QStringLiteral("title"), title);
    }
    const au::aicore::JobRequest request {
        effectiveProvider.toStdString(),
        QJsonDocument(parameters).toJson(QJsonDocument::Compact).toStdString()
    };
    QString error;
    if (!m_runtimeHost->submit(request, &error)) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(error);
        return;
    }
    AIStudioStatusModel::instance()->setWorkspaceStatus(
        QObject::tr("Submitted %1 generation job (seed %2)").arg(effectiveProvider).arg(seedValue));
}

void AIStudioController::regenerateFromPlan()
{
    if (m_activeWorkspace.isEmpty()) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(
            QObject::tr("Save the project and open AI Studio before regenerating"));
        return;
    }
    if (!m_planDraftLoaded) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(
            QObject::tr("Open a song plan before regenerating"));
        return;
    }

    // Reuse the source generation's lyrics/style/title, and feed the edited
    // plan back as an external ABC score so the model follows this structure.
    QString text;
    QString style;
    QString title;
    QFile requestFile(QDir(m_activeWorkspace).filePath(QStringLiteral("jobs/%1/request.json").arg(m_planDraft.id)));
    if (requestFile.open(QIODevice::ReadOnly)) {
        const QJsonObject parameters
            = QJsonDocument::fromJson(requestFile.readAll()).object().value(QStringLiteral("parameters")).toObject();
        text = parameters.contains(QStringLiteral("lyrics"))
               ? parameters.value(QStringLiteral("lyrics")).toString()
               : parameters.value(QStringLiteral("text")).toString();
        style = parameters.value(QStringLiteral("style")).toString();
        title = parameters.value(QStringLiteral("title")).toString();
    }
    if (text.trimmed().isEmpty()) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(
            QObject::tr("Regenerating needs lyrics; generate a song from the Create panel first"));
        return;
    }

    const QByteArray abc = au::songplan::writeAbcPlan(m_planDraft);
    if (abc.isEmpty()) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(
            QObject::tr("The song plan produced an empty score"));
        return;
    }

    bool seedOk = false;
    const QString seedText = AIStudioStatusModel::instance()->currentSeed();
    int seedValue = seedText.trimmed().toInt(&seedOk);
    if (!seedOk) {
        seedValue = static_cast<int>(QRandomGenerator::global()->generate() & 0x7fffffff);
    }

    QJsonObject parameters {
        { "lyrics", text },
        { "cot", QStringLiteral("full") },
        { "seed", seedValue },
        { "abc", QString::fromUtf8(abc) }
    };
    if (!style.trimmed().isEmpty()) {
        parameters.insert("style", style);
    }
    if (!title.trimmed().isEmpty()) {
        parameters.insert("title", title);
    }
    const au::aicore::JobRequest request {
        "yue2-native",
        QJsonDocument(parameters).toJson(QJsonDocument::Compact).toStdString()
    };
    QString error;
    if (!m_runtimeHost->submit(request, &error)) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(error);
        return;
    }
    AIStudioStatusModel::instance()->updateCurrentSeed(QString::number(seedValue));
    AIStudioStatusModel::instance()->setWorkspaceStatus(
        QObject::tr("Regenerating from the edited song plan (seed %1)").arg(seedValue));
}

void AIStudioController::loadExample(int index)
{
    const QVariantList examples = promptExamples();
    if (index < 0 || index >= examples.size()) {
        return;
    }
    const QVariantMap example = examples.at(index).toMap();
    AIStudioStatusModel::instance()->setPromptReuse(
        example.value(QStringLiteral("style")).toString(),
        example.value(QStringLiteral("lyrics")).toString(),
        example.value(QStringLiteral("name")).toString(),
        QString(),
        example.value(QStringLiteral("cot")).toString(),
        QString(),
        example.value(QStringLiteral("steps")).toString(),
        QString(),
        QVariantMap());
    dispatcher()->dispatch("dock-set-open", ActionData::make_arg2<QString, bool>(AI_STUDIO_DOCK, true));
    AIStudioStatusModel::instance()->setWorkspaceStatus(
        QObject::tr("Loaded example \"%1\"").arg(example.value(QStringLiteral("name")).toString()));
}

void AIStudioController::createPrompt(const QString& lyrics)
{
    runAssistant(QStringLiteral("style"),
                 QObject::tr("You are a music producer. Write a concise song style prompt - genre, instrumentation, "
                             "vocal type and tempo - that fits the lyrics the user provides. Reply with the style "
                             "text only, with no quotes or explanation."),
                 lyrics.trimmed().isEmpty() ? QObject::tr("Write a style for an upbeat modern song.") : lyrics);
}

void AIStudioController::improvePrompt(const QString& style)
{
    runAssistant(QStringLiteral("style"),
                 QObject::tr("Improve this song style prompt. Keep it concise and in the same spirit, and return "
                             "only the improved style text with no quotes or explanation."),
                 style);
}

void AIStudioController::writeLyrics(const QString& style)
{
    runAssistant(QStringLiteral("lyrics"),
                 QObject::tr("Write song lyrics for the given style. Use [Verse] and [Chorus] section tags and "
                             "singable, original lines. Reply with the lyrics only."),
                 style.trimmed().isEmpty() ? QObject::tr("a heartfelt indie pop song") : style);
}

void AIStudioController::runAssistant(const QString& field, const QString& systemPrompt, const QString& userPrompt)
{
    if (!m_assistant) {
        return;
    }
    if (userPrompt.trimmed().isEmpty()) {
        AIStudioStatusModel::instance()->updateAssistantStatus(QObject::tr("Nothing to send to the assistant yet"));
        return;
    }
    AIStudioStatusModel::instance()->updateAssistantBusy(true);
    AIStudioStatusModel::instance()->updateAssistantStatus(QObject::tr("Asking the writing assistant…"));
    m_assistant->request(systemPrompt, userPrompt, [this, field](bool ok, const QString& content, const QString& error) {
        AIStudioStatusModel::instance()->updateAssistantBusy(false);
        if (!ok) {
            AIStudioStatusModel::instance()->updateAssistantStatus(error);
            return;
        }
        AIStudioStatusModel::instance()->updateAssistantStatus(
            QObject::tr("Assistant updated the %1").arg(field == QLatin1String("lyrics") ? QObject::tr("lyrics") : QObject::tr("style")));
        AIStudioStatusModel::instance()->notifyAssistantResult(field, content);
    });
}

void AIStudioController::setAssistantConfig(const QString& mode, const QString& baseUrl, const QString& model,
                                            const QString& apiKey, const QString& runnerPath, const QString& modelPath, int port)
{
    au::aimodels::AssistantConfig config = au::aimodels::ModelSettings::assistant();
    config.mode = mode.trimmed().isEmpty() ? QStringLiteral("cloud") : mode.trimmed();
    config.baseUrl = baseUrl.trimmed();
    config.model = model.trimmed();
    config.runnerPath = runnerPath.trimmed();
    config.modelPath = modelPath.trimmed();
    if (port > 0) {
        config.port = port;
    }
    if (!apiKey.isEmpty()) {
        config.apiKey = apiKey;
    }
    QString error;
    if (!au::aimodels::ModelSettings::setAssistant(config, &error)) {
        AIStudioStatusModel::instance()->updateAssistantStatus(error);
        return;
    }
    applyAssistantSettings();
    AIStudioStatusModel::instance()->updateAssistantStatus(QObject::tr("Saved assistant settings"));
}

void AIStudioController::applyAssistantSettings()
{
    const au::aimodels::AssistantConfig config = au::aimodels::ModelSettings::assistant();
    AIStudioStatusModel::instance()->updateAssistantConfig(
        config.mode, config.baseUrl, config.model, !config.apiKey.trimmed().isEmpty(),
        config.runnerPath, config.modelPath, config.port);
}

void AIStudioController::importPendingResults()
{
    if (m_activeWorkspace.isEmpty()) {
        return;
    }
    QString error;
    const QList<au::aicore::JobStatus> jobs = au::aijobs::JobStore::jobs(m_activeWorkspace, &error);
    if (!error.isEmpty()) {
        return;
    }
    // A completed job whose output is present but was never placed on the
    // timeline (e.g. the app closed before inserting) is imported now.
    for (const au::aicore::JobStatus& job : jobs) {
        const QString jobId = QString::fromStdString(job.id.value);
        QString reason;
        if (!au::aijobs::JobStore::resultAssetPath(m_activeWorkspace, jobId, &reason).isEmpty()) {
            insertJobOutput(jobId);
        }
    }
}

void AIStudioController::importPromptFile(const QString& path)
{
    QFile file(path);
    if (path.isEmpty() || !file.open(QIODevice::ReadOnly)) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(QObject::tr("Could not read the prompt file"));
        return;
    }
    const bool yaml = path.endsWith(QLatin1String(".yaml"), Qt::CaseInsensitive)
                      || path.endsWith(QLatin1String(".yml"), Qt::CaseInsensitive);
    au::aimodels::PromptFields fields;
    QString error;
    if (!au::aimodels::parsePrompt(QString::fromUtf8(file.readAll()), yaml, &fields, &error)) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(error);
        return;
    }
    QVariantMap sampling;
    const auto put = [&sampling](const QString& key, const QString& value) {
        if (!value.trimmed().isEmpty()) {
            sampling.insert(key, value);
        }
    };
    put(QStringLiteral("abc_temperature"), fields.abcTemperature);
    put(QStringLiteral("abc_top_p"), fields.abcTopP);
    put(QStringLiteral("abc_top_k"), fields.abcTopK);
    put(QStringLiteral("abc_repetition_penalty"), fields.abcRepetitionPenalty);
    put(QStringLiteral("semantic_temperature"), fields.semanticTemperature);
    put(QStringLiteral("semantic_top_p"), fields.semanticTopP);
    put(QStringLiteral("semantic_top_k"), fields.semanticTopK);
    put(QStringLiteral("semantic_repetition_penalty"), fields.semanticRepetitionPenalty);
    AIStudioStatusModel::instance()->setPromptReuse(
        fields.style, fields.lyrics, fields.title, fields.seed, fields.cot, fields.duration, fields.steps, fields.guidanceScale, sampling);
    dispatcher()->dispatch("dock-set-open", ActionData::make_arg2<QString, bool>(AI_STUDIO_DOCK, true));
    AIStudioStatusModel::instance()->setWorkspaceStatus(
        QObject::tr("Loaded prompt from %1").arg(QFileInfo(path).fileName()));
}

void AIStudioController::exportPromptFile(const QString& path, const QString& style, const QString& lyrics,
                                          const QString& title, const QString& seed, const QString& cot,
                                          const QString& duration, const QString& steps, const QString& guidance,
                                          const QVariantMap& sampling)
{
    au::aimodels::PromptFields fields;
    fields.style = style;
    fields.lyrics = lyrics;
    fields.title = title;
    fields.seed = seed;
    fields.cot = cot;
    fields.duration = duration;
    fields.steps = steps;
    fields.guidanceScale = guidance;
    fields.abcTemperature = sampling.value(QStringLiteral("abc_temperature")).toString();
    fields.abcTopP = sampling.value(QStringLiteral("abc_top_p")).toString();
    fields.abcTopK = sampling.value(QStringLiteral("abc_top_k")).toString();
    fields.abcRepetitionPenalty = sampling.value(QStringLiteral("abc_repetition_penalty")).toString();
    fields.semanticTemperature = sampling.value(QStringLiteral("semantic_temperature")).toString();
    fields.semanticTopP = sampling.value(QStringLiteral("semantic_top_p")).toString();
    fields.semanticTopK = sampling.value(QStringLiteral("semantic_top_k")).toString();
    fields.semanticRepetitionPenalty = sampling.value(QStringLiteral("semantic_repetition_penalty")).toString();
    const bool yaml = path.endsWith(QLatin1String(".yaml"), Qt::CaseInsensitive)
                      || path.endsWith(QLatin1String(".yml"), Qt::CaseInsensitive);
    const QString text = au::aimodels::serializePrompt(fields, yaml);
    QSaveFile file(path);
    if (path.isEmpty() || !file.open(QIODevice::WriteOnly) || file.write(text.toUtf8()) < 1 || !file.commit()) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(QObject::tr("Could not write the prompt file"));
        return;
    }
    AIStudioStatusModel::instance()->setWorkspaceStatus(
        QObject::tr("Saved prompt to %1").arg(QFileInfo(path).fileName()));
}

void AIStudioController::refreshPlans()
{
    if (m_activeWorkspace.isEmpty()) {
        AIStudioStatusModel::instance()->setPlans({});
        return;
    }
    QString error;
    const QStringList ids = au::songplan::SongPlanStore::planIds(m_activeWorkspace, &error);
    if (!error.isEmpty()) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(error);
        return;
    }
    QVariantList rows;
    for (const QString& id : ids) {
        au::songplan::SongPlan plan;
        QString loadError;
        if (!au::songplan::SongPlanStore::loadLatest(m_activeWorkspace, id, &plan, &loadError)) {
            continue;
        }
        rows.append(QVariantMap {
            { "id", plan.id },
            { "revision", plan.revision },
            { "tempo", plan.tempo },
            { "key", plan.key },
            { "sections", plan.sections.size() },
            { "chords", plan.chords.size() },
            { "melody", plan.melody.size() }
        });
    }
    AIStudioStatusModel::instance()->setPlans(rows);
}

void AIStudioController::createPlan(const QString& name)
{
    if (m_activeWorkspace.isEmpty()) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(QObject::tr("Enable the project AI workspace before creating a song plan"));
        return;
    }
    au::songplan::SongPlan plan;
    const QString cleanName = name.trimmed();
    plan.id = cleanName.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : cleanName;
    plan.revision = 1;
    plan.sourceFormat = "other";
    QString error;
    if (!au::songplan::SongPlanStore::saveRevision(m_activeWorkspace, plan, &error)) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(error);
        return;
    }
    registerPlanAsset(plan);
    AIStudioStatusModel::instance()->setWorkspaceStatus(QObject::tr("Created song plan %1").arg(plan.id));
    refreshPlans();
}

void AIStudioController::loadPlan(const QString& planId)
{
    if (m_activeWorkspace.isEmpty()) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(QObject::tr("Enable the project AI workspace before editing a song plan"));
        return;
    }
    au::songplan::SongPlan plan;
    QString error;
    if (!au::songplan::SongPlanStore::loadLatest(m_activeWorkspace, planId, &plan, &error)) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(error);
        return;
    }
    m_planDraft = plan;
    m_planDraftLoaded = true;
    pushPlanDetail();
    AIStudioStatusModel::instance()->setWorkspaceStatus(QObject::tr("Editing song plan %1").arg(plan.id));
}

void AIStudioController::pushPlanDetail()
{
    if (!m_planDraftLoaded) {
        AIStudioStatusModel::instance()->setPlanDetail({});
        return;
    }
    QVariantList sections;
    for (const au::songplan::SongSection& section : m_planDraft.sections) {
        sections.append(QVariantMap {
            { "id", section.id }, { "name", section.name },
            { "startSeconds", section.startSeconds }, { "endSeconds", section.endSeconds }
        });
    }
    QVariantList chords;
    for (const au::songplan::ChordEvent& chord : m_planDraft.chords) {
        chords.append(QVariantMap {
            { "startSeconds", chord.startSeconds }, { "durationSeconds", chord.durationSeconds },
            { "symbol", chord.symbol }
        });
    }
    QVariantList melody;
    for (const au::songplan::NoteEvent& note : m_planDraft.melody) {
        melody.append(QVariantMap {
            { "startSeconds", note.startSeconds }, { "durationSeconds", note.durationSeconds },
            { "midiPitch", note.midiPitch }, { "lyric", note.lyric }
        });
    }
    AIStudioStatusModel::instance()->setPlanDetail(QVariantMap {
        { "loaded", true },
        { "id", m_planDraft.id },
        { "revision", m_planDraft.revision },
        { "tempo", m_planDraft.tempo },
        { "key", m_planDraft.key },
        { "timeSignature", m_planDraft.timeSignature },
        { "sourceFormat", m_planDraft.sourceFormat },
        { "seed", m_planDraftSeed },
        { "sections", sections },
        { "chords", chords },
        { "melody", melody }
    });
}

void AIStudioController::setPlanMetadata(double tempo, const QString& key, const QString& timeSignature)
{
    if (!m_planDraftLoaded) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(QObject::tr("Select a song plan before editing it"));
        return;
    }
    if (tempo > 0.0) {
        m_planDraft.tempo = tempo;
    }
    if (!key.trimmed().isEmpty()) {
        m_planDraft.key = key.trimmed();
    }
    if (!timeSignature.trimmed().isEmpty()) {
        m_planDraft.timeSignature = timeSignature.trimmed();
    }
    pushPlanDetail();
}

void AIStudioController::addPlanSection(const QString& name, double startSeconds, double endSeconds)
{
    if (!m_planDraftLoaded) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(QObject::tr("Select a song plan before editing it"));
        return;
    }
    au::songplan::SongSection section;
    section.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    section.name = name.trimmed().isEmpty() ? QObject::tr("Section") : name.trimmed();
    section.startSeconds = startSeconds;
    section.endSeconds = endSeconds;
    m_planDraft.sections.append(section);
    pushPlanDetail();
}

void AIStudioController::removePlanSection(int index)
{
    if (!m_planDraftLoaded || index < 0 || index >= m_planDraft.sections.size()) {
        return;
    }
    m_planDraft.sections.removeAt(index);
    pushPlanDetail();
}

void AIStudioController::addPlanChord(const QString& symbol, double startSeconds, double durationSeconds)
{
    if (!m_planDraftLoaded) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(QObject::tr("Select a song plan before editing it"));
        return;
    }
    au::songplan::ChordEvent chord;
    chord.symbol = symbol.trimmed();
    chord.startSeconds = startSeconds;
    chord.durationSeconds = durationSeconds;
    m_planDraft.chords.append(chord);
    pushPlanDetail();
}

void AIStudioController::removePlanChord(int index)
{
    if (!m_planDraftLoaded || index < 0 || index >= m_planDraft.chords.size()) {
        return;
    }
    m_planDraft.chords.removeAt(index);
    pushPlanDetail();
}

void AIStudioController::addPlanNote(int midiPitch, double startSeconds, double durationSeconds, const QString& lyric)
{
    if (!m_planDraftLoaded) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(QObject::tr("Select a song plan before editing it"));
        return;
    }
    au::songplan::NoteEvent note;
    note.midiPitch = std::max(0, std::min(127, midiPitch));
    note.startSeconds = startSeconds;
    note.durationSeconds = durationSeconds;
    note.lyric = lyric;
    m_planDraft.melody.append(note);
    pushPlanDetail();
}

void AIStudioController::removePlanNote(int index)
{
    if (!m_planDraftLoaded || index < 0 || index >= m_planDraft.melody.size()) {
        return;
    }
    m_planDraft.melody.removeAt(index);
    pushPlanDetail();
}

void AIStudioController::savePlanRevision()
{
    if (!m_planDraftLoaded) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(QObject::tr("Select a song plan before saving a revision"));
        return;
    }
    QStringList problems;
    if (!au::songplan::validate(m_planDraft, &problems)) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(
            QObject::tr("The song plan is not valid: %1").arg(problems.join("; ")));
        return;
    }
    QString error;
    const au::songplan::SongPlan next = au::songplan::SongPlanStore::nextRevision(m_activeWorkspace, m_planDraft, &error);
    if (!error.isEmpty()) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(error);
        return;
    }
    if (!au::songplan::SongPlanStore::saveRevision(m_activeWorkspace, next, &error)) {
        AIStudioStatusModel::instance()->setWorkspaceStatus(error);
        return;
    }
    m_planDraft = next;
    registerPlanAsset(next);
    pushPlanDetail();
    refreshPlans();
    AIStudioStatusModel::instance()->setWorkspaceStatus(QObject::tr("Saved song plan %1 revision %2")
                                                        .arg(next.id).arg(next.revision));
}

void AIStudioController::registerPlanAsset(const au::songplan::SongPlan& plan)
{
    if (m_activeWorkspace.isEmpty()) {
        return;
    }
    au::ailibrary::ProjectAsset asset;
    asset.id = QStringLiteral("plan:%1").arg(plan.id);
    asset.name = plan.id;
    asset.kind = "song_plan";
    asset.origin = "generated";
    asset.filePath = QStringLiteral("plans/%1/r%2.json").arg(plan.id).arg(plan.revision);
    asset.updatedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    asset.createdAt = asset.updatedAt;
    asset.status = "available";

    au::ailibrary::AssetProvenance provenance;
    provenance.id = QStringLiteral("plan-prov:%1:r%2").arg(plan.id).arg(plan.revision);
    provenance.assetId = asset.id;
    provenance.operation = "song_plan";
    provenance.providerId = "songplan";
    provenance.createdAt = asset.updatedAt;
    if (!plan.sourceScoreAssetId.isEmpty()) {
        provenance.sourceAssetIds = QStringList { plan.sourceScoreAssetId };
    }

    QString error;
    if (!au::ailibrary::LibraryAssetStore::upsertProjectAsset(m_activeWorkspace, asset, &provenance, &error)) {
        AIStudioStatusModel::instance()->setLibraryStatus(error);
        return;
    }
    refreshLibraryAssets();
}

void AIStudioController::registerJobArtifacts(const QString& jobId, const QString& resultManifest)
{
    Q_UNUSED(resultManifest);
    if (m_activeWorkspace.isEmpty()) {
        return;
    }
    QString error;
    const QList<au::aijobs::JobStore::JobArtifact> artifacts
        = au::aijobs::JobStore::resultArtifacts(m_activeWorkspace, jobId, &error);
    if (!error.isEmpty() || artifacts.isEmpty()) {
        return;
    }

    for (const au::aijobs::JobStore::JobArtifact& artifact : artifacts) {
        const QFileInfo file(artifact.path);
        const bool isScore = artifact.id.contains(QStringLiteral("score"), Qt::CaseInsensitive)
                             || file.suffix().compare(QStringLiteral("abc"), Qt::CaseInsensitive) == 0;
        if (!file.exists() || !isScore) {
            continue;
        }

        // Preserve the raw provider score as a Library asset ("source before render").
        au::ailibrary::ProjectAsset asset;
        asset.id = QStringLiteral("score:") + jobId;
        asset.name = QStringLiteral("YuE2 score %1").arg(jobId);
        asset.kind = QStringLiteral("song_plan");
        asset.origin = QStringLiteral("yue2");
        asset.filePath = QDir(m_activeWorkspace).relativeFilePath(artifact.path);
        asset.updatedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        asset.createdAt = asset.updatedAt;
        asset.status = QStringLiteral("available");

        au::ailibrary::AssetProvenance provenance;
        provenance.id = QStringLiteral("score-prov:") + jobId;
        provenance.assetId = asset.id;
        provenance.operation = QStringLiteral("song_plan");
        provenance.providerId = QStringLiteral("yue2-native");
        provenance.jobId = jobId;
        provenance.createdAt = asset.updatedAt;

        if (!au::ailibrary::LibraryAssetStore::upsertProjectAsset(m_activeWorkspace, asset, &provenance, &error)) {
            AIStudioStatusModel::instance()->setLibraryStatus(error);
            continue;
        }

        // Expose the score as an editable (initially scaffold) Song Plan revision.
        if (au::songplan::SongPlanStore::latestRevision(m_activeWorkspace, jobId, nullptr) > 0) {
            continue;
        }
        au::songplan::SongPlan plan;
        plan.id = jobId;
        plan.revision = 1;
        plan.sourceProviderId = QStringLiteral("yue2-native");
        plan.sourceFormat = QStringLiteral("abc");
        plan.sourceScoreAssetId = asset.id;
        QFile scoreFile(artifact.path);
        if (scoreFile.open(QIODevice::ReadOnly)) {
            au::songplan::parseAbcPlan(scoreFile.readAll(), &plan);
        }
        if (!au::songplan::SongPlanStore::saveRevision(m_activeWorkspace, plan, &error)) {
            AIStudioStatusModel::instance()->setLibraryStatus(error);
            continue;
        }
        registerPlanAsset(plan);
    }

    refreshPlans();
    refreshLibraryAssets();
}
