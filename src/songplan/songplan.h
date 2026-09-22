/*
 * Audacity: A Digital Audio Editor
 */
#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace au::songplan {

struct SongSection {
    QString id;
    QString name;
    double startSeconds = 0.0;
    double endSeconds = 0.0;
};

struct ChordEvent {
    double startSeconds = 0.0;
    double durationSeconds = 0.0;
    QString symbol;
};

struct NoteEvent {
    double startSeconds = 0.0;
    double durationSeconds = 0.0;
    int midiPitch = 60;
    QString lyric;
};

// Normalized, provider-independent song plan. The original provider score is
// never mutated: editing a plan produces a new immutable revision.
struct SongPlan {
    QString id;
    QString sourceProviderId;
    QString sourceScoreAssetId;
    QString sourceFormat; // "abc" | "midi" | "other"
    double tempo = 0.0;
    QString timeSignature;
    QString key;
    int revision = 0;
    QList<SongSection> sections;
    QList<ChordEvent> chords;
    QList<NoteEvent> melody;
};

// Basic structural validation. Returns false and fills errors when invalid.
bool validate(const SongPlan& plan, QStringList* errors = nullptr);
}