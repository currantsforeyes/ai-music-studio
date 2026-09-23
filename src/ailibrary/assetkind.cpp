/*
 * Audacity: A Digital Audio Editor
 */
#include "assetkind.h"

#include <QStringList>

namespace au::ailibrary {
bool isAudioAssetKind(const QString& kind)
{
    static const QStringList audioKinds {
        QStringLiteral("upload"), QStringLiteral("recording"), QStringLiteral("generation"),
        QStringLiteral("stem"), QStringLiteral("vocal"), QStringLiteral("instrument"),
        QStringLiteral("mixdown"), QStringLiteral("reference"), QStringLiteral("audio")
    };
    return audioKinds.contains(kind);
}

bool isDocumentAssetKind(const QString& kind)
{
    return !isAudioAssetKind(kind);
}
}