/*
 * Audacity: A Digital Audio Editor
 */
#include "jobstore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QSaveFile>

using namespace au::aijobs;

namespace {
constexpr auto MANIFEST_FILE = "manifest.json";

bool readManifest(const QString& workspacePath, QJsonObject* root, QString* errorMessage)
{
    QFile manifest(QDir(workspacePath).filePath(MANIFEST_FILE));
    if (!manifest.open(QIODevice::ReadOnly)) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Could not read the AI workspace manifest");
        }
        return false;
    }
    const QJsonDocument document = QJsonDocument::fromJson(manifest.readAll());
    if (!document.isObject()) {
        if (errorMessage) {
            *errorMessage = QObject::tr("The AI workspace manifest is invalid");
        }
        return false;
    }
    *root = document.object();
    return true;
}

bool saveManifest(const QString& workspacePath, const QJsonObject& root, QString* errorMessage)
{
    QSaveFile output(QDir(workspacePath).filePath(MANIFEST_FILE));
    if (!output.open(QIODevice::WriteOnly)
        || output.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) < 1
        || !output.commit()) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Could not save the AI workspace manifest");
        }
        return false;
    }
    return true;
}

QJsonObject toJson(const au::aicore::JobStatus& status)
{
    return {
        { "jobId", QString::fromStdString(status.id.value) },
        { "providerId", QString::fromStdString(status.providerId) },
        { "state", QString::fromStdString(au::aicore::toString(status.state)) },
        { "progress", status.progress },
        { "message", QString::fromStdString(status.message) },
        { "resultManifest", QString::fromStdString(status.resultManifest) },
        { "errorCode", QString::fromStdString(status.errorCode) },
        { "errorMessage", QString::fromStdString(status.errorMessage) },
    };
}

bool fromJson(const QJsonObject& value, au::aicore::JobStatus* status)
{
    if (!status) {
        return false;
    }
    status->id.value = value.value("jobId").toString().toStdString();
    if (status->id.value.empty()) {
        return false;
    }
    status->providerId = value.value("providerId").toString().toStdString();
    au::aicore::JobState state = au::aicore::JobState::Queued;
    if (au::aicore::jobStateFromString(value.value("state").toString().toStdString(), &state)) {
        status->state = state;
    }
    status->progress = value.value("progress").toDouble();
    status->message = value.value("message").toString().toStdString();
    status->resultManifest = value.value("resultManifest").toString().toStdString();
    status->errorCode = value.value("errorCode").toString().toStdString();
    status->errorMessage = value.value("errorMessage").toString().toStdString();
    return true;
}
}

bool JobStore::upsert(const QString& workspacePath, const au::aicore::JobStatus& status, QString* errorMessage)
{
    if (workspacePath.isEmpty() || status.id.value.empty()) {
        if (errorMessage) {
            *errorMessage = QObject::tr("An AI job needs a workspace and an id");
        }
        return false;
    }

    QJsonObject root;
    if (!readManifest(workspacePath, &root, errorMessage)) {
        return false;
    }

    QJsonArray jobs = root.value("jobs").toArray();
    QJsonObject job = toJson(status);
    bool replaced = false;
    for (qsizetype index = 0; index < jobs.size(); ++index) {
        const QJsonObject existing = jobs.at(index).toObject();
        if (existing.value("jobId").toString() == job.value("jobId").toString()) {
            if (existing.value("insertedIntoProject").toBool()) {
                job.insert("insertedIntoProject", true);
            }
            jobs.replace(index, job);
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        jobs.append(job);
    }
    root.insert("jobs", jobs);
    return saveManifest(workspacePath, root, errorMessage);
}

QList<au::aicore::JobStatus> JobStore::jobs(const QString& workspacePath, QString* errorMessage)
{
    QJsonObject root;
    if (!readManifest(workspacePath, &root, errorMessage)) {
        return {};
    }
    QList<au::aicore::JobStatus> result;
    for (const QJsonValue& value : root.value("jobs").toArray()) {
        au::aicore::JobStatus status;
        if (fromJson(value.toObject(), &status)) {
            result.append(status);
        }
    }
    return result;
}

bool JobStore::find(const QString& workspacePath, const QString& jobId, au::aicore::JobStatus* status,
                    QString* errorMessage)
{
    QJsonObject root;
    if (!readManifest(workspacePath, &root, errorMessage)) {
        return false;
    }
    for (const QJsonValue& value : root.value("jobs").toArray()) {
        if (value.toObject().value("jobId").toString() == jobId) {
            return fromJson(value.toObject(), status);
        }
    }
    if (errorMessage) {
        *errorMessage = QObject::tr("The AI job no longer exists in the workspace manifest");
    }
    return false;
}

int JobStore::recoverInterrupted(const QString& workspacePath, QString* errorMessage)
{
    QJsonObject root;
    if (!readManifest(workspacePath, &root, errorMessage)) {
        return -1;
    }

    QJsonArray jobs = root.value("jobs").toArray();
    int recovered = 0;
    for (qsizetype index = 0; index < jobs.size(); ++index) {
        QJsonObject job = jobs.at(index).toObject();
        au::aicore::JobState state = au::aicore::JobState::Queued;
        if (!au::aicore::jobStateFromString(job.value("state").toString().toStdString(), &state)
            || au::aicore::isTerminal(state)) {
            continue;
        }
        job.insert("state", QString::fromStdString(au::aicore::toString(au::aicore::JobState::Interrupted)));
        jobs.replace(index, job);
        ++recovered;
    }
    if (recovered == 0) {
        return 0;
    }
    root.insert("jobs", jobs);
    if (!saveManifest(workspacePath, root, errorMessage)) {
        return -1;
    }
    return recovered;
}

bool JobStore::markInserted(const QString& workspacePath, const QString& jobId, QString* errorMessage)
{
    QJsonObject root;
    if (!readManifest(workspacePath, &root, errorMessage)) {
        return false;
    }
    QJsonArray jobs = root.value("jobs").toArray();
    for (qsizetype index = 0; index < jobs.size(); ++index) {
        QJsonObject job = jobs.at(index).toObject();
        if (job.value("jobId").toString() == jobId) {
            job.insert("insertedIntoProject", true);
            jobs.replace(index, job);
            root.insert("jobs", jobs);
            return saveManifest(workspacePath, root, errorMessage);
        }
    }
    if (errorMessage) {
        *errorMessage = QObject::tr("The completed job is missing from the AI workspace manifest");
    }
    return false;
}

bool JobStore::isInserted(const QString& workspacePath, const QString& jobId, bool* inserted,
                          QString* errorMessage)
{
    if (!inserted) {
        return false;
    }
    QJsonObject root;
    if (!readManifest(workspacePath, &root, errorMessage)) {
        return false;
    }
    for (const QJsonValue& value : root.value("jobs").toArray()) {
        const QJsonObject job = value.toObject();
        if (job.value("jobId").toString() == jobId) {
            *inserted = job.value("insertedIntoProject").toBool();
            return true;
        }
    }
    if (errorMessage) {
        *errorMessage = QObject::tr("The AI job no longer exists in the workspace manifest");
    }
    return false;
}

QString JobStore::resultAssetPath(const QString& workspacePath, const QString& jobId, QString* errorMessage)
{
    QJsonObject root;
    if (!readManifest(workspacePath, &root, errorMessage)) {
        return {};
    }
    for (const QJsonValue& value : root.value("jobs").toArray()) {
        const QJsonObject job = value.toObject();
        if (job.value("jobId").toString() != jobId) {
            continue;
        }
        au::aicore::JobStatus status;
        if (!fromJson(job, &status) || status.state != au::aicore::JobState::Complete) {
            if (errorMessage) {
                *errorMessage = QObject::tr("The AI job is not complete");
            }
            return {};
        }
        if (job.value("insertedIntoProject").toBool()) {
            if (errorMessage) {
                *errorMessage = QObject::tr("This completed output is already inserted into the project");
            }
            return {};
        }
        const QString resultPath = QDir(workspacePath).filePath(QString::fromStdString(status.resultManifest));
        QFile result(resultPath);
        if (!result.open(QIODevice::ReadOnly)) {
            break;
        }
        const QString asset = QJsonDocument::fromJson(result.readAll()).object().value("asset").toString();
        const QString assetPath = QDir(QFileInfo(resultPath).absolutePath()).filePath(asset);
        if (QFileInfo::exists(assetPath)) {
            return assetPath;
        }
        break;
    }
    if (errorMessage) {
        *errorMessage = QObject::tr("No completed provider output is available to insert");
    }
    return {};
}