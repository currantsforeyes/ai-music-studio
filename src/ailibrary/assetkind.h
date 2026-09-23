/*
 * Audacity: A Digital Audio Editor
 */
#pragma once

#include <QString>

namespace au::ailibrary {
// Audio assets can be placed on the timeline and carry WAV metadata. Document
// assets (song plans, MIDI, transcriptions) live in the Library too but have no
// timeline or audio-detail actions.
bool isAudioAssetKind(const QString& kind);
bool isDocumentAssetKind(const QString& kind);
}