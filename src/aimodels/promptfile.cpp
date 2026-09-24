/*
 * Audacity: A Digital Audio Editor
 */
#include "promptfile.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QStringList>

#include <cmath>

namespace au::aimodels {
namespace {
bool isNumber(const QString& text)
{
    bool ok = false;
    text.trimmed().toDouble(&ok);
    return ok;
}

QString jsonToString(const QJsonValue& value)
{
    if (value.isString()) {
        return value.toString();
    }
    if (value.isDouble()) {
        const double number = value.toDouble();
        if (qFuzzyCompare(number + 1.0, std::floor(number) + 1.0)) {
            return QString::number(static_cast<qint64>(number));
        }
        return QString::number(number);
    }
    if (value.isBool()) {
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    }
    return {};
}

void assign(PromptFields* fields, const QString& key, const QString& value)
{
    if (key == QLatin1String("style")) {
        fields->style = value;
    } else if (key == QLatin1String("lyrics")) {
        fields->lyrics = value;
    } else if (key == QLatin1String("title")) {
        fields->title = value;
    } else if (key == QLatin1String("abc")) {
        fields->abc = value;
    } else if (key == QLatin1String("cot")) {
        fields->cot = value;
    } else if (key == QLatin1String("seed")) {
        fields->seed = value;
    } else if (key == QLatin1String("steps")) {
        fields->steps = value;
    } else if (key == QLatin1String("guidance_scale")) {
        fields->guidanceScale = value;
    }
}
}

QString serializePrompt(const PromptFields& fields, bool yaml)
{
    if (!yaml) {
        QJsonObject object;
        const auto putString = [&object](const char* key, const QString& value) {
            if (!value.trimmed().isEmpty()) {
                object.insert(QLatin1String(key), value);
            }
        };
        putString("style", fields.style);
        putString("lyrics", fields.lyrics);
        putString("title", fields.title);
        putString("abc", fields.abc);
        putString("cot", fields.cot);
        if (isNumber(fields.seed)) {
            object.insert(QStringLiteral("seed"), fields.seed.trimmed().toInt());
        }
        if (isNumber(fields.steps)) {
            object.insert(QStringLiteral("steps"), fields.steps.trimmed().toInt());
        }
        if (isNumber(fields.guidanceScale)) {
            object.insert(QStringLiteral("guidance_scale"), fields.guidanceScale.trimmed().toDouble());
        }
        return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Indented));
    }

    QString out;
    const auto scalarLine = [&out](const QString& key, const QString& value) {
        if (value.isEmpty()) {
            return;
        }
        if (value.contains(QLatin1Char('\n'))) {
            out += key + QStringLiteral(": |-\n");
            const QStringList lines = value.split(QLatin1Char('\n'));
            for (const QString& line : lines) {
                out += QStringLiteral("  ") + line + QLatin1Char('\n');
            }
            return;
        }
        QString escaped = value;
        escaped.replace(QLatin1Char('\\'), QLatin1String("\\\\"));
        escaped.replace(QLatin1Char('"'), QLatin1String("\\\""));
        out += key + QStringLiteral(": \"") + escaped + QStringLiteral("\"\n");
    };
    const auto numericLine = [&](const QString& key, const QString& value) {
        if (value.trimmed().isEmpty()) {
            return;
        }
        if (isNumber(value)) {
            out += key + QStringLiteral(": ") + value.trimmed() + QLatin1Char('\n');
        } else {
            scalarLine(key, value);
        }
    };

    scalarLine(QStringLiteral("style"), fields.style);
    scalarLine(QStringLiteral("lyrics"), fields.lyrics);
    scalarLine(QStringLiteral("title"), fields.title);
    scalarLine(QStringLiteral("abc"), fields.abc);
    scalarLine(QStringLiteral("cot"), fields.cot);
    numericLine(QStringLiteral("seed"), fields.seed);
    numericLine(QStringLiteral("steps"), fields.steps);
    numericLine(QStringLiteral("guidance_scale"), fields.guidanceScale);
    return out;
}

bool parsePrompt(const QString& text, bool yaml, PromptFields* fields, QString* errorMessage)
{
    if (!fields) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("A prompt target is required");
        }
        return false;
    }
    *fields = PromptFields {};

    if (!yaml) {
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(text.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("The prompt file is not a JSON object");
            }
            return false;
        }
        const QJsonObject object = document.object();
        for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
            assign(fields, it.key(), jsonToString(it.value()));
        }
        return true;
    }

    const QStringList lines = text.split(QLatin1Char('\n'));
    for (int index = 0; index < lines.size(); ++index) {
        const QString line = lines.at(index);
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#'))) {
            continue;
        }
        const int colon = line.indexOf(QLatin1Char(':'));
        if (colon <= 0) {
            continue;
        }
        const QString key = line.left(colon).trimmed();
        QString value = line.mid(colon + 1).trimmed();

        if (value == QLatin1String("|") || value == QLatin1String("|-")
            || value == QLatin1String("|+") || value == QLatin1String(">")
            || value == QLatin1String(">-")) {
            const bool folded = value.startsWith(QLatin1Char('>'));
            QStringList block;
            int look = index + 1;
            while (look < lines.size()) {
                const QString blockLine = lines.at(look);
                if (blockLine.trimmed().isEmpty()) {
                    block.append(QString());
                    ++look;
                    continue;
                }
                if (!blockLine.startsWith(QLatin1Char(' ')) && !blockLine.startsWith(QLatin1Char('\t'))) {
                    break;
                }
                block.append(blockLine.mid(2));
                ++look;
            }
            while (!block.isEmpty() && block.last().isEmpty()) {
                block.removeLast();
            }
            value = block.join(folded ? QStringLiteral(" ") : QStringLiteral("\n"));
            index = look - 1;
        } else if (value.size() >= 2 && value.startsWith(QLatin1Char('"')) && value.endsWith(QLatin1Char('"'))) {
            value = value.mid(1, value.size() - 2);
            value.replace(QLatin1String("\\\""), QLatin1String("\""));
            value.replace(QLatin1String("\\\\"), QLatin1String("\\"));
        }
        assign(fields, key, value);
    }
    return true;
}
}