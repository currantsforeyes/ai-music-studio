/*
 * Audacity: A Digital Audio Editor
 */
#include "songplanstore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QSaveFile>

#include <algorithm>

using namespace au::songplan;

namespace {
constexpr auto PLAN_DIRECTORY = "plans";
constexpr int PLAN_FILE_SUFFIX_LENGTH = 5; // ".json"

QJsonObject sectionToJson(const SongSection& section)
{
    return {
        { "id", section.id }, { "name", section.name },
        { "startSeconds", section.startSeconds }, { "endSeconds", section.endSeconds }
    };
}

SongSection sectionFromJson(const QJsonObject& value)
{
    SongSection section;
    section.id = value.value("id").toString();
    section.name = value.value("name").toString();
    section.startSeconds = value.value("startSeconds").toDouble();
    section.endSeconds = value.value("endSeconds").toDouble();
    return section;
}

QJsonObject chordToJson(const ChordEvent& chord)
{
    return {
        { "startSeconds", chord.startSeconds }, { "durationSeconds", chord.durationSeconds },
        { "symbol", chord.symbol }
    };
}

ChordEvent chordFromJson(const QJsonObject& value)
{
    ChordEvent chord;
    chord.startSeconds = value.value("startSeconds").toDouble();
    chord.durationSeconds = value.value("durationSeconds").toDouble();
    chord.symbol = value.value("symbol").toString();
    return chord;
}

QJsonObject noteToJson(const NoteEvent& note)
{
    return {
        { "startSeconds", note.startSeconds }, { "durationSeconds", note.durationSeconds },
        { "midiPitch", note.midiPitch }, { "lyric", note.lyric }
    };
}

NoteEvent noteFromJson(const QJsonObject& value)
{
    NoteEvent note;
    note.startSeconds = value.value("startSeconds").toDouble();
    note.durationSeconds = value.value("durationSeconds").toDouble();
    note.midiPitch = value.value("midiPitch").toInt(60);
    note.lyric = value.value("lyric").toString();
    return note;
}

QJsonObject planToJson(const SongPlan& plan)
{
    QJsonArray sections;
    for (const SongSection& section : plan.sections) {
        sections.append(sectionToJson(section));
    }
    QJsonArray chords;
    for (const ChordEvent& chord : plan.chords) {
        chords.append(chordToJson(chord));
    }
    QJsonArray melody;
    for (const NoteEvent& note : plan.melody) {
        melody.append(noteToJson(note));
    }
    return {
        { "id", plan.id },
        { "sourceProviderId", plan.sourceProviderId },
        { "sourceScoreAssetId", plan.sourceScoreAssetId },
        { "sourceFormat", plan.sourceFormat },
        { "tempo", plan.tempo },
        { "timeSignature", plan.timeSignature },
        { "key", plan.key },
        { "revision", plan.revision },
        { "sections", sections },
        { "chords", chords },
        { "melody", melody }
    };
}

SongPlan planFromJson(const QJsonObject& value)
{
    SongPlan plan;
    plan.id = value.value("id").toString();
    plan.sourceProviderId = value.value("sourceProviderId").toString();
    plan.sourceScoreAssetId = value.value("sourceScoreAssetId").toString();
    plan.sourceFormat = value.value("sourceFormat").toString();
    plan.tempo = value.value("tempo").toDouble();
    plan.timeSignature = value.value("timeSignature").toString();
    plan.key = value.value("key").toString();
    plan.revision = value.value("revision").toInt();
    for (const QJsonValue& entry : value.value("sections").toArray()) {
        plan.sections.append(sectionFromJson(entry.toObject()));
    }
    for (const QJsonValue& entry : value.value("chords").toArray()) {
        plan.chords.append(chordFromJson(entry.toObject()));
    }
    for (const QJsonValue& entry : value.value("melody").toArray()) {
        plan.melody.append(noteFromJson(entry.toObject()));
    }
    return plan;
}

QString planDirectory(const QString& workspacePath, const QString& planId)
{
    return QDir(QDir(workspacePath).filePath(PLAN_DIRECTORY)).filePath(planId);
}

QString planFilePath(const QString& workspacePath, const QString& planId, int revision)
{
    return QDir(planDirectory(workspacePath, planId))
           .filePath(QStringLiteral("r%1.json").arg(revision));
}
}

bool SongPlanStore::saveRevision(const QString& workspacePath, const SongPlan& plan, QString* errorMessage)
{
    if (workspacePath.isEmpty() || plan.id.trimmed().isEmpty() || plan.revision <= 0) {
        if (errorMessage) {
            *errorMessage = QObject::tr("A song plan revision needs a workspace, an id, and a positive revision number");
        }
        return false;
    }

    const QString filePath = planFilePath(workspacePath, plan.id, plan.revision);
    if (QFileInfo::exists(filePath)) {
        if (errorMessage) {
            *errorMessage = QObject::tr("This song plan revision already exists and cannot be overwritten");
        }
        return false;
    }
    if (!QDir().mkpath(QFileInfo(filePath).absolutePath())) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Could not create the song plan folder");
        }
        return false;
    }

    QSaveFile output(filePath);
    if (!output.open(QIODevice::WriteOnly)
        || output.write(QJsonDocument(planToJson(plan)).toJson(QJsonDocument::Indented)) < 1
        || !output.commit()) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Could not save the song plan revision");
        }
        return false;
    }
    return true;
}

QList<int> SongPlanStore::revisions(const QString& workspacePath, const QString& planId, QString* errorMessage)
{
    Q_UNUSED(errorMessage);
    const QDir directory(planDirectory(workspacePath, planId));
    if (!directory.exists()) {
        return {};
    }
    QList<int> result;
    const QStringList entries = directory.entryList({ QStringLiteral("r*.json") }, QDir::Files);
    for (const QString& name : entries) {
        bool ok = false;
        const QString number = name.mid(1, name.size() - 1 - PLAN_FILE_SUFFIX_LENGTH);
        const int revision = number.toInt(&ok);
        if (ok) {
            result.append(revision);
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

int SongPlanStore::latestRevision(const QString& workspacePath, const QString& planId, QString* errorMessage)
{
    const QList<int> available = revisions(workspacePath, planId, errorMessage);
    return available.isEmpty() ? 0 : available.last();
}

bool SongPlanStore::loadRevision(const QString& workspacePath, const QString& planId, int revision,
                                 SongPlan* plan, QString* errorMessage)
{
    if (!plan) {
        return false;
    }
    QFile input(planFilePath(workspacePath, planId, revision));
    if (!input.open(QIODevice::ReadOnly)) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Could not read the song plan revision");
        }
        return false;
    }
    const QJsonDocument document = QJsonDocument::fromJson(input.readAll());
    if (!document.isObject()) {
        if (errorMessage) {
            *errorMessage = QObject::tr("The song plan revision is invalid");
        }
        return false;
    }
    *plan = planFromJson(document.object());
    return true;
}

SongPlan SongPlanStore::nextRevision(const QString& workspacePath, const SongPlan& plan, QString* errorMessage)
{
    SongPlan copy = plan;
    copy.revision = latestRevision(workspacePath, plan.id, errorMessage) + 1;
    return copy;
}

bool SongPlanStore::loadLatest(const QString& workspacePath, const QString& planId,
                               SongPlan* plan, QString* errorMessage)
{
    const int revision = latestRevision(workspacePath, planId, errorMessage);
    if (revision <= 0) {
        if (errorMessage) {
            *errorMessage = QObject::tr("No song plan revisions were found");
        }
        return false;
    }
    return loadRevision(workspacePath, planId, revision, plan, errorMessage);
}

QStringList SongPlanStore::planIds(const QString& workspacePath, QString* errorMessage)
{
    Q_UNUSED(errorMessage);
    const QDir root(QDir(workspacePath).filePath(PLAN_DIRECTORY));
    if (!root.exists()) {
        return {};
    }
    QStringList ids = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    ids.sort();
    return ids;
}