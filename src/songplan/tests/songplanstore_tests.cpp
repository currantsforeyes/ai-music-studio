/*
 * Audacity: A Digital Audio Editor
 */
#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
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

}