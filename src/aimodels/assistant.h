/*
 * Audacity: A Digital Audio Editor
 */
#pragma once

#include <QByteArray>
#include <QString>

namespace au::aimodels {

//! Builds an OpenAI-compatible chat-completions request body.
QByteArray buildChatRequest(const QString& model, const QString& systemPrompt, const QString& userPrompt);

//! Extracts the assistant text from a chat-completions response. Returns false
//! with a reason on transport-level error objects, missing choices or empty text.
bool parseChatResponse(const QByteArray& response, QString* content, QString* errorMessage = nullptr);
}