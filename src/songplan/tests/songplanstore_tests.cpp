/*
 * Audacity: A Digital Audio Editor
 */
#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QByteArray>
#include <QTemporaryDir>

#include "songplan/songplan.h"
#include "songplan/songplanstore.h"

namespace au::songplan {
namespace {
SongPlan makePlan()
{
    SongPlan plan;
    plan.id = "plan-1";
    plan.sourceProviderId = "yue2";
    plan.sourceFormat = "abc";
    plan.tempo = 120.0;
    plan.timeSignature = "4/4";
    plan.key = "C";
    plan.revision = 1;

    SongSection intro;
    intro.id = "s1";
    intro.name = "Intro";
    intro.startSeconds = 0.0;
    intro.endSeconds = 8.0;
    SongSection verse;
    verse.id = "s2";
    verse.name = "Verse";
    verse.startSeconds = 8.0;
    verse.endSeconds = 24.0;
    plan.sections << intro << verse;

    ChordEvent chord;
    chord.startSeconds = 0.0;
    chord.durationSeconds = 4.0;
    chord.symbol = "C";
    plan.chords << chord;

    NoteEvent note;
    note.startSeconds = 0.0;
    note.durationSeconds = 1.0;
    note.midiPitch = 60;
    note.lyric = "la";
    plan.melody << note;
    return plan;
}
}

TEST(SongPlanStoreTests, SavesAndLoadsRevisionRoundTrip)
{
    QTemporaryDir workspace;
    ASSERT_TRUE(workspace.isValid());

    const SongPlan plan = makePlan();
    QString error;
    ASSERT_TRUE(SongPlanStore::saveRevision(workspace.path(), plan, &error)) << error.toStdString();

    SongPlan restored;
    ASSERT_TRUE(SongPlanStore::loadRevision(workspace.path(), "plan-1", 1, &restored, &error)) << error.toStdString();
    EXPECT_EQ(restored.id, "plan-1");
    EXPECT_EQ(restored.sourceProviderId, "yue2");
    EXPECT_DOUBLE_EQ(restored.tempo, 120.0);
    ASSERT_EQ(restored.sections.size(), 2);
    EXPECT_EQ(restored.sections.at(1).name, "Verse");
    ASSERT_EQ(restored.chords.size(), 1);
    EXPECT_EQ(restored.chords.front().symbol, "C");
    ASSERT_EQ(restored.melody.size(), 1);
    EXPECT_EQ(restored.melody.front().midiPitch, 60);
    EXPECT_EQ(restored.melody.front().lyric, "la");
}

TEST(SongPlanStoreTests, RefusesToOverwriteExistingRevision)
{
    QTemporaryDir workspace;
    ASSERT_TRUE(workspace.isValid());

    QString error;
    ASSERT_TRUE(SongPlanStore::saveRevision(workspace.path(), makePlan(), &error));
    EXPECT_FALSE(SongPlanStore::saveRevision(workspace.path(), makePlan(), &error));
    EXPECT_FALSE(error.isEmpty());
}

TEST(SongPlanStoreTests, ListsAndFindsLatestRevision)
{
    QTemporaryDir workspace;
    ASSERT_TRUE(workspace.isValid());

    QString error;
    SongPlan first = makePlan();
    ASSERT_TRUE(SongPlanStore::saveRevision(workspace.path(), first, &error));
    SongPlan second = first;
    second.revision = 2;
    second.key = "G";
    ASSERT_TRUE(SongPlanStore::saveRevision(workspace.path(), second, &error));

    EXPECT_EQ(SongPlanStore::revisions(workspace.path(), "plan-1", &error), QList<int>({ 1, 2 }));
    EXPECT_EQ(SongPlanStore::latestRevision(workspace.path(), "plan-1", &error), 2);
    EXPECT_EQ(SongPlanStore::latestRevision(workspace.path(), "missing", &error), 0);
}

TEST(SongPlanStoreTests, NextRevisionIncrementsFromLatest)
{
    QTemporaryDir workspace;
    ASSERT_TRUE(workspace.isValid());

    QString error;
    ASSERT_TRUE(SongPlanStore::saveRevision(workspace.path(), makePlan(), &error));
    const SongPlan next = SongPlanStore::nextRevision(workspace.path(), makePlan(), &error);
    EXPECT_EQ(next.revision, 2);
}

TEST(SongPlanStoreTests, ListsPlanIdsAndLoadsLatest)
{
    QTemporaryDir workspace;
    ASSERT_TRUE(workspace.isValid());

    QString error;
    ASSERT_TRUE(SongPlanStore::saveRevision(workspace.path(), makePlan(), &error));
    SongPlan second = makePlan();
    second.revision = 2;
    second.key = "D";
    ASSERT_TRUE(SongPlanStore::saveRevision(workspace.path(), second, &error));

    EXPECT_EQ(SongPlanStore::planIds(workspace.path(), &error), QStringList({ "plan-1" }));

    SongPlan latest;
    ASSERT_TRUE(SongPlanStore::loadLatest(workspace.path(), "plan-1", &latest, &error)) << error.toStdString();
    EXPECT_EQ(latest.revision, 2);
    EXPECT_EQ(latest.key, "D");
}

TEST(SongPlanStoreTests, EditingCreatesANewImmutableRevision)
{
    QTemporaryDir workspace;
    ASSERT_TRUE(workspace.isValid());

    QString error;
    ASSERT_TRUE(SongPlanStore::saveRevision(workspace.path(), makePlan(), &error));

    SongPlan draft;
    ASSERT_TRUE(SongPlanStore::loadLatest(workspace.path(), "plan-1", &draft, &error));

    // Simulate an edit: change tempo and append a chord/note, then save as a
    // new revision. The original revision must remain untouched.
    draft.tempo = 140.0;
    ChordEvent chord;
    chord.startSeconds = 4.0;
    chord.durationSeconds = 2.0;
    chord.symbol = "Am";
    draft.chords.append(chord);
    NoteEvent note;
    note.startSeconds = 4.0;
    note.durationSeconds = 0.5;
    note.midiPitch = 64;
    draft.melody.append(note);

    const SongPlan next = SongPlanStore::nextRevision(workspace.path(), draft, &error);
    EXPECT_EQ(next.revision, 2);
    ASSERT_TRUE(SongPlanStore::saveRevision(workspace.path(), next, &error)) << error.toStdString();

    // Revision 1 is unchanged.
    SongPlan original;
    ASSERT_TRUE(SongPlanStore::loadRevision(workspace.path(), "plan-1", 1, &original, &error));
    EXPECT_DOUBLE_EQ(original.tempo, 120.0);
    ASSERT_EQ(original.chords.size(), 1);

    // Revision 2 has the edits.
    SongPlan edited;
    ASSERT_TRUE(SongPlanStore::loadLatest(workspace.path(), "plan-1", &edited, &error));
    EXPECT_EQ(edited.revision, 2);
    EXPECT_DOUBLE_EQ(edited.tempo, 140.0);
    ASSERT_EQ(edited.chords.size(), 2);
    EXPECT_EQ(edited.chords.back().symbol, "Am");
    ASSERT_EQ(edited.melody.size(), 2);
    EXPECT_EQ(edited.melody.back().midiPitch, 64);
}

TEST(SongPlanTests, ValidateReportsStructuralProblems)
{
    SongPlan plan = makePlan();
    QStringList errors;
    EXPECT_TRUE(validate(plan, &errors)) << errors.join(", ").toStdString();

    plan.id.clear();
    plan.sections[0].endSeconds = 0.0;
    EXPECT_FALSE(validate(plan, &errors));
    EXPECT_FALSE(errors.isEmpty());
}

TEST(SongPlanTests, AppliesAbcHeaderFields)
{
    SongPlan plan;
    plan.id = "abc";
    const QByteArray abc("X:1\nT:Test\nM:4/4\nQ:1/4=132\nK:Am\n[V1]\n...\n");
    const int recognized = applyAbcHeader(abc, &plan);
    EXPECT_GE(recognized, 3);
    EXPECT_DOUBLE_EQ(plan.tempo, 132.0);
    EXPECT_EQ(plan.timeSignature, QStringLiteral("4/4"));
    EXPECT_EQ(plan.key, QStringLiteral("Am"));
}

TEST(SongPlanTests, ParsesAbcBodyIntoSectionsChordsAndMelody)
{
    const QByteArray abc(
        "X:1\n"
        "T:\n"
        "M:4/4\n"
        "L:1/32\n"
        "Q:1/4=100\n"
        "V: Vocal clef=treble name=\"Vocal Melody\"\n"
        "V: Ins clef=treble name=\"Ins Melody\"\n"
        "K:D\n"
        "% intro\n"
        "V: Vocal\n"
        "\"D\"z32|\"Gmaj7\"z32|\n"
        "V: Ins\n"
        "D4D4F4D2F4F2D4F4D2F2|\n"
        "% chorus\n"
        "V: Vocal\n"
        "\"D\"a16-a4b8f4-|\"Gmaj7\"f8z24|\n"
        "V: Ins\n"
        "Z4|\n"
        "% outro\n"
        "V: Vocal\n"
        "\"D\"z32|\n"
        "V: Ins\n"
        "f12e16z4|\n");

    SongPlan plan;
    plan.id = "abc-body";
    QString error;
    ASSERT_TRUE(parseAbcPlan(abc, &plan, &error)) << error.toStdString();

    EXPECT_DOUBLE_EQ(plan.tempo, 100.0);
    EXPECT_EQ(plan.timeSignature, QStringLiteral("4/4"));
    EXPECT_EQ(plan.key, QStringLiteral("D"));

    ASSERT_EQ(plan.sections.size(), 3);
    EXPECT_EQ(plan.sections[0].name, QStringLiteral("intro"));
    EXPECT_EQ(plan.sections[1].name, QStringLiteral("chorus"));
    EXPECT_EQ(plan.sections[2].name, QStringLiteral("outro"));
    for (const SongSection& section : plan.sections) {
        EXPECT_GT(section.endSeconds, section.startSeconds) << section.name.toStdString();
    }
    EXPECT_LE(plan.sections[0].endSeconds, plan.sections[1].startSeconds);

    // Chords are taken from the quoted symbols in document order.
    ASSERT_EQ(plan.chords.size(), 5);
    EXPECT_EQ(plan.chords.front().symbol, QStringLiteral("D"));
    EXPECT_EQ(plan.chords.at(3).symbol, QStringLiteral("Gmaj7"));
    EXPECT_GT(plan.chords.front().durationSeconds, 0.0);

    // Only the "Vocal" voice becomes melody; the instrumental voice must not leak.
    // a16 - a4 b8 f4 - f8 => five notes, all in the vocal register.
    ASSERT_EQ(plan.melody.size(), 5);
    EXPECT_EQ(plan.melody.front().midiPitch, 81); // lowercase 'a' is one octave up
    for (int index = 1; index < plan.melody.size(); ++index) {
        EXPECT_GE(plan.melody.at(index).startSeconds, plan.melody.at(index - 1).startSeconds);
    }
    EXPECT_GT(plan.melody.back().startSeconds, plan.melody.front().startSeconds);

    // Sections and melody share the same converted timeline (intro is 2 whole notes).
    EXPECT_DOUBLE_EQ(plan.sections[1].startSeconds, 2.0 * 4.0 * 60.0 / 100.0);
}

}