/*
 * Audacity: A Digital Audio Editor
 */
#pragma once

#include <QString>

namespace au::aimodels {

//! A generation prompt in the engine's own field names, so it can be moved
//! between this app and other YuE2 tooling. Values are kept as strings; empty
//! fields are omitted when serialized.
struct PromptFields {
    QString style;
    QString lyrics;
    QString title;
    QString abc;
    QString cot;
    QString seed;
    QString steps;
    QString guidanceScale;
};

//! Serializes to the engine request format (sparse, engine field names).
QString serializePrompt(const PromptFields& fields, bool yaml);

//! Parses the engine request format. `yaml` selects the parser (callers pick it
//! from the file extension). Returns false with a reason on malformed input.
bool parsePrompt(const QString& text, bool yaml, PromptFields* fields, QString* errorMessage = nullptr);
}