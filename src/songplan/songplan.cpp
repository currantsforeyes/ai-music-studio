/*
 * Audacity: A Digital Audio Editor
 */
#include "songplan.h"

#include <QMap>
#include <QPair>

#include <algorithm>
#include <cmath>

namespace au::songplan {
bool validate(const SongPlan& plan, QStringList* errors)
{
    QStringList problems;
    if (plan.id.trimmed().isEmpty()) {
        problems << QStringLiteral("The song plan needs an id");
    }
    if (plan.revision < 0) {
        problems << QStringLiteral("The song plan revision cannot be negative");
    }
    if (plan.tempo < 0.0) {
        problems << QStringLiteral("The song plan tempo cannot be negative");
    }
    for (int index = 0; index < plan.sections.size(); ++index) {
        const SongSection& section = plan.sections.at(index);
        if (section.endSeconds <= section.startSeconds) {
            problems << QStringLiteral("Section %1 has a non-positive length").arg(index);
        }
        if (index > 0 && section.startSeconds < plan.sections.at(index - 1).startSeconds) {
            problems << QStringLiteral("Section %1 starts before the previous section").arg(index);
        }
    }
    if (errors) {
        *errors = problems;
    }
    return problems.isEmpty();
}

int applyAbcHeader(const QByteArray& abc, SongPlan* plan)
{
    if (!plan) {
        return 0;
    }
    int recognized = 0;
    const QStringList lines = QString::fromUtf8(abc).split('\n');
    for (const QString& raw : lines) {
        const QString line = raw.trimmed();
        if (line.startsWith(QLatin1String("Q:"))) {
            QString value = line.mid(2).trimmed();
            const int equals = value.indexOf('=');
            if (equals >= 0) {
                value = value.mid(equals + 1);
            }
            bool ok = false;
            const double tempo = value.toDouble(&ok);
            if (ok && tempo > 0.0) {
                plan->tempo = tempo;
                ++recognized;
            }
        } else if (line.startsWith(QLatin1String("M:"))) {
            const QString value = line.mid(2).trimmed();
            if (!value.isEmpty()) {
                plan->timeSignature = value;
                ++recognized;
            }
        } else if (line.startsWith(QLatin1String("K:"))) {
            const QString value = line.mid(2).trimmed();
            if (!value.isEmpty()) {
                plan->key = value;
                ++recognized;
            }
        }
    }
    return recognized;
}

namespace {
bool parseRatio(const QString& text, int* numerator, int* denominator)
{
    const QStringList parts = text.split('/');
    bool okNumerator = false;
    bool okDenominator = false;
    const int num = parts.value(0).trimmed().toInt(&okNumerator);
    int den = 4;
    if (parts.size() > 1) {
        den = parts.value(1).trimmed().toInt(&okDenominator);
    } else {
        okDenominator = true;
    }
    if (!okNumerator || !okDenominator || num <= 0 || den <= 0) {
        return false;
    }
    *numerator = num;
    *denominator = den;
    return true;
}

int semitoneForNote(const QChar& upper)
{
    static const QMap<QChar, int> semitones {
        { QLatin1Char('C'), 0 }, { QLatin1Char('D'), 2 }, { QLatin1Char('E'), 4 },
        { QLatin1Char('F'), 5 }, { QLatin1Char('G'), 7 }, { QLatin1Char('A'), 9 },
        { QLatin1Char('B'), 11 }
    };
    return semitones.value(upper, -1);
}
}

bool parseAbcPlan(const QByteArray& abc, SongPlan* plan, QString* errorMessage)
{
    if (!plan) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("A song plan is required");
        }
        return false;
    }

    applyAbcHeader(abc, plan);

    double tempo = plan->tempo > 0.0 ? plan->tempo : 120.0;
    double defaultLength = 0.125; // ABC default is 1/8 of a whole note.
    int meterNumerator = 4;
    int meterDenominator = 4;
    QString key = plan->key;
    QStringList voices;

    const QStringList lines = QString::fromUtf8(abc).split('\n');
    for (const QString& raw : lines) {
        const QString line = raw.trimmed();
        if (line.startsWith(QLatin1String("Q:"))) {
            QString value = line.mid(2).trimmed();
            const int equals = value.indexOf('=');
            if (equals >= 0) {
                value = value.mid(equals + 1);
            }
            bool ok = false;
            const double parsed = value.toDouble(&ok);
            if (ok && parsed > 0.0) {
                tempo = parsed;
            }
        } else if (line.startsWith(QLatin1String("M:"))) {
            int num = 4;
            int den = 4;
            if (parseRatio(line.mid(2).trimmed(), &num, &den)) {
                meterNumerator = num;
                meterDenominator = den;
            }
        } else if (line.startsWith(QLatin1String("L:"))) {
            const QStringList parts = line.mid(2).trimmed().split('/');
            bool okNum = false;
            bool okDen = false;
            const double num = parts.value(0).toDouble(&okNum);
            double den = 1.0;
            if (parts.size() > 1) {
                den = parts.value(1).toDouble(&okDen);
            } else {
                okDen = true;
            }
            if (okNum && okDen && num > 0.0 && den > 0.0) {
                defaultLength = num / den;
            }
        } else if (line.startsWith(QLatin1String("K:"))) {
            key = line.mid(2).trimmed();
        } else if (line.startsWith(QLatin1String("V:"))) {
            const QString name = line.mid(2).trimmed().split(' ', Qt::SkipEmptyParts).value(0);
            if (!name.isEmpty() && !voices.contains(name)) {
                voices.append(name);
            }
        }
    }

    plan->tempo = tempo;
    if (!key.isEmpty()) {
        plan->key = key;
    }

    QString melodyVoice;
    for (const QString& voice : voices) {
        if (voice.contains(QLatin1String("vocal"), Qt::CaseInsensitive)) {
            melodyVoice = voice;
            break;
        }
    }
    if (melodyVoice.isEmpty() && !voices.isEmpty()) {
        melodyVoice = voices.front();
    }

    const double secondsPerWhole = 4.0 * 60.0 / tempo;
    QMap<QString, double> voiceCursors;
    QString currentVoice;
    QString pendingChord;
    double pendingChordStart = -1.0;
    QList<SongSection> sections;

    const auto seconds = [secondsPerWhole](double whole) { return whole * secondsPerWhole; };
    // ABC voices share one timeline; the song length is the longest voice.
    const auto maxCursor = [&voiceCursors]() {
        double maximum = 0.0;
        for (double value : voiceCursors) {
            maximum = qMax(maximum, value);
        }
        return maximum;
    };

    for (const QString& raw : lines) {
        const QString line = raw.trimmed();
        if (line.isEmpty()) {
            continue;
        }
        if (line.startsWith(QLatin1Char('%'))) {
            if (!line.startsWith(QLatin1String("%%"))) {
                const QString name = line.mid(1).trimmed();
                if (!name.isEmpty()) {
                    SongSection section;
                    section.id = QStringLiteral("section-%1").arg(sections.size() + 1);
                    section.name = name;
                    section.startSeconds = seconds(maxCursor());
                    section.endSeconds = section.startSeconds;
                    sections.append(section);
                }
            }
            continue;
        }
        if (line.startsWith(QLatin1String("V:"))) {
            currentVoice = line.mid(2).trimmed().split(' ', Qt::SkipEmptyParts).value(0);
            continue;
        }
        if (line.size() >= 2 && line.at(1) == QLatin1Char(':')) {
            continue; // other header field
        }

        int index = 0;
        const int length = line.size();
        while (index < length) {
            const QChar character = line.at(index);
            if (character.isSpace() || character == QLatin1Char('|') || character == QLatin1Char('[')
                || character == QLatin1Char(']')) {
                ++index;
                continue;
            }
            if (character == QLatin1Char('%')) {
                break;
            }
            if (character == QLatin1Char('"')) {
                const int close = line.indexOf(QLatin1Char('"'), index + 1);
                if (close < 0) {
                    break;
                }
                const QString symbol = line.mid(index + 1, close - index - 1);
                if (!pendingChord.isEmpty() && pendingChordStart >= 0.0) {
                    ChordEvent event;
                    event.symbol = pendingChord;
                    event.startSeconds = seconds(pendingChordStart);
                    event.durationSeconds = seconds(voiceCursors.value(currentVoice)) - event.startSeconds;
                    plan->chords.append(event);
                }
                pendingChord = symbol;
                pendingChordStart = voiceCursors[currentVoice];
                index = close + 1;
                continue;
            }

            const bool isRest = character == QLatin1Char('z') || character == QLatin1Char('x')
                                || character == QLatin1Char('Z');
            const bool isWholeMeasureRest = character == QLatin1Char('Z');
            if (isRest) {
                ++index;
            } else {
                int accidental = 0;
                if (character == QLatin1Char('^') || character == QLatin1Char('_') || character == QLatin1Char('=')) {
                    accidental = character == QLatin1Char('^') ? 1 : (character == QLatin1Char('_') ? -1 : 0);
                    ++index;
                }
                if (index >= length) {
                    break;
                }
                const QChar letter = line.at(index);
                const QChar upper = letter.toUpper();
                if (semitoneForNote(upper) < 0) {
                    ++index; // unsupported token (grace note decorations etc.)
                    continue;
                }
                int octaveShift = letter.isLower() ? 1 : 0;
                ++index;
                while (index < length && (line.at(index) == QLatin1Char(',') || line.at(index) == QLatin1Char('\''))) {
                    octaveShift += line.at(index) == QLatin1Char('\'') ? 1 : -1;
                    ++index;
                }
                const int midiPitch = 60 + semitoneForNote(upper) + octaveShift * 12 + accidental;

                double number = 0.0;
                bool hasNumerator = false;
                while (index < length && line.at(index).isDigit()) {
                    number = number * 10.0 + line.at(index).digitValue();
                    hasNumerator = true;
                    ++index;
                }
                if (index < length && line.at(index) == QLatin1Char('/')) {
                    ++index;
                    double denominator = 0.0;
                    bool hasDenominator = false;
                    while (index < length && line.at(index).isDigit()) {
                        denominator = denominator * 10.0 + line.at(index).digitValue();
                        hasDenominator = true;
                        ++index;
                    }
                    if (!hasDenominator) {
                        denominator = 2.0;
                    }
                    number = hasNumerator ? number / denominator : 1.0 / denominator;
                } else if (!hasNumerator) {
                    number = 1.0;
                }

                while (index < length && (line.at(index) == QLatin1Char('>') || line.at(index) == QLatin1Char('<'))) {
                    ++index; // broken rhythm: approximated as equal durations
                }
                while (index < length && line.at(index) == QLatin1Char('-')) {
                    ++index; // tie: not merged in this revision
                }

                const double wholeNotes = number * defaultLength;
                if (currentVoice == melodyVoice) {
                    NoteEvent note;
                    note.startSeconds = seconds(voiceCursors[currentVoice]);
                    note.durationSeconds = seconds(wholeNotes);
                    note.midiPitch = midiPitch;
                    plan->melody.append(note);
                }
                voiceCursors[currentVoice] += wholeNotes;
                continue;
            }

            // rest duration
            double number = 0.0;
            bool hasNumerator = false;
            while (index < length && line.at(index).isDigit()) {
                number = number * 10.0 + line.at(index).digitValue();
                hasNumerator = true;
                ++index;
            }
            if (index < length && line.at(index) == QLatin1Char('/')) {
                ++index;
                double denominator = 0.0;
                bool hasDenominator = false;
                while (index < length && line.at(index).isDigit()) {
                    denominator = denominator * 10.0 + line.at(index).digitValue();
                    hasDenominator = true;
                    ++index;
                }
                if (!hasDenominator) {
                    denominator = 2.0;
                }
                number = hasNumerator ? number / denominator : 1.0 / denominator;
            } else if (!hasNumerator) {
                number = 1.0;
            }
            const double wholeNotes = isWholeMeasureRest
                                      ? number * (double(meterNumerator) / double(meterDenominator))
                                      : number * defaultLength;
            voiceCursors[currentVoice] += wholeNotes;
        }
    }

    if (!pendingChord.isEmpty() && pendingChordStart >= 0.0) {
        ChordEvent event;
        event.symbol = pendingChord;
        event.startSeconds = seconds(pendingChordStart);
        event.durationSeconds = seconds(maxCursor()) - event.startSeconds;
        plan->chords.append(event);
    }

    if (sections.isEmpty()) {
        SongSection section;
        section.id = QStringLiteral("section-1");
        section.name = QStringLiteral("Full song");
        section.startSeconds = 0.0;
        section.endSeconds = seconds(maxCursor());
        sections.append(section);
    } else {
        for (int i = 0; i < sections.size(); ++i) {
            sections[i].endSeconds = (i + 1 < sections.size())
                                     ? sections[i + 1].startSeconds
                                     : seconds(maxCursor());
        }
    }
    plan->sections = sections;
    return true;
}

namespace {
QString abcPitch(int midi)
{
    static const char* names[12] = { "C", "^C", "D", "^D", "E", "F", "^F", "G", "^G", "A", "^A", "B" };
    const int pitchClass = ((midi % 12) + 12) % 12;
    const int octave = midi / 12 - 1;
    QString token = QString::fromLatin1(names[pitchClass]);
    if (octave >= 5) {
        token = token.toLower();
        for (int index = 5; index < octave; ++index) {
            token += QLatin1Char('\'');
        }
    } else {
        for (int index = octave; index < 4; ++index) {
            token += QLatin1Char(',');
        }
    }
    return token;
}

int abcUnits(double seconds, double secondsPerUnit)
{
    if (secondsPerUnit <= 0.0 || seconds <= 0.0) {
        return 1;
    }
    return std::max(1, static_cast<int>(std::lround(seconds / secondsPerUnit)));
}
}

QByteArray writeAbcPlan(const SongPlan& plan)
{
    const double tempo = plan.tempo > 0.0 ? plan.tempo : 120.0;
    const QString meter = plan.timeSignature.trimmed().isEmpty() ? QStringLiteral("4/4") : plan.timeSignature.trimmed();
    const QString key = plan.key.trimmed().isEmpty() ? QStringLiteral("C") : plan.key.trimmed();
    const double secondsPerWhole = 4.0 * 60.0 / tempo;
    const double defaultLength = 1.0 / 32.0; // must match the L:1/32 header
    const double secondsPerUnit = secondsPerWhole * defaultLength;

    struct Event {
        double time = 0.0;
        bool isChord = false;
        QString symbol;
        int midi = 60;
        double duration = 0.0;
    };

    QList<Event> events;
    for (const ChordEvent& chord : plan.chords) {
        Event event;
        event.time = chord.startSeconds;
        event.isChord = true;
        event.symbol = chord.symbol;
        events.append(event);
    }
    for (const NoteEvent& note : plan.melody) {
        Event event;
        event.time = note.startSeconds;
        event.isChord = false;
        event.midi = note.midiPitch;
        event.duration = note.durationSeconds;
        events.append(event);
    }
    std::stable_sort(events.begin(), events.end(), [](const Event& a, const Event& b) {
        if (a.time < b.time) {
            return true;
        }
        if (b.time < a.time) {
            return false;
        }
        return a.isChord && !b.isChord; // chords before notes at the same time
    });

    QList<QPair<double, QString> > sections;
    for (const SongSection& section : plan.sections) {
        sections.append({ section.startSeconds, section.name });
    }
    std::stable_sort(sections.begin(), sections.end(), [](const QPair<double, QString>& a, const QPair<double, QString>& b) {
        return a.first < b.first;
    });

    QString music;
    double cursor = 0.0;
    int sectionIndex = 0;
    for (const Event& event : events) {
        while (sectionIndex < sections.size() && sections[sectionIndex].first <= event.time + 1e-6) {
            music += QStringLiteral("|\n% %1\n").arg(sections[sectionIndex].second);
            ++sectionIndex;
        }
        const double gap = event.time - cursor;
        if (gap > 1e-6) {
            music += QStringLiteral("z%1").arg(abcUnits(gap, secondsPerUnit));
            cursor = event.time;
        }
        if (event.isChord) {
            music += QStringLiteral("\"%1\"").arg(event.symbol);
        } else {
            const int units = abcUnits(event.duration, secondsPerUnit);
            music += abcPitch(event.midi);
            if (units != 1) {
                music += QString::number(units);
            }
            cursor = event.time + event.duration;
        }
    }
    while (sectionIndex < sections.size()) {
        music += QStringLiteral("\n% %1").arg(sections[sectionIndex].second);
        ++sectionIndex;
    }

    QString abc;
    abc += QStringLiteral("X:1\n");
    abc += QStringLiteral("T:%1\n").arg(plan.id);
    abc += QStringLiteral("M:%1\n").arg(meter);
    abc += QStringLiteral("L:1/32\n");
    abc += QStringLiteral("Q:1/4=%1\n").arg(qRound(tempo));
    abc += QStringLiteral("V: Vocal clef=treble name=\"Vocal Melody\"\n");
    abc += QStringLiteral("V: Ins clef=treble name=\"Ins Melody\"\n");
    abc += QStringLiteral("K:%1\n").arg(key);
    abc += QStringLiteral("V: Vocal\n");
    abc += music;
    abc += QStringLiteral("\n|\nV: Ins\nZ\n");
    return abc.toUtf8();
}
}