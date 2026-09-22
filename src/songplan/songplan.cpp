/*
 * Audacity: A Digital Audio Editor
 */
#include "songplan.h"

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
}