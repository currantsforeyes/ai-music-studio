/*
 * Audacity: A Digital Audio Editor
 */
#include "aicoretypes.h"

#include <utility>

namespace au::aicore {
std::string toString(JobState state)
{
    switch (state) {
    case JobState::Queued:
        return "queued";
    case JobState::Preparing:
        return "preparing";
    case JobState::Loading:
        return "loading";
    case JobState::Running:
        return "running";
    case JobState::Decoding:
        return "decoding";
    case JobState::Importing:
        return "importing";
    case JobState::Complete:
        return "complete";
    case JobState::Failed:
        return "failed";
    case JobState::Cancelled:
        return "cancelled";
    case JobState::Interrupted:
        return "interrupted";
    }
    return "queued";
}

bool jobStateFromString(const std::string& value, JobState* state)
{
    if (!state) {
        return false;
    }
    static const std::pair<const char*, JobState> table[] = {
        { "queued", JobState::Queued },
        { "preparing", JobState::Preparing },
        { "loading", JobState::Loading },
        { "running", JobState::Running },
        { "decoding", JobState::Decoding },
        { "importing", JobState::Importing },
        { "complete", JobState::Complete },
        { "failed", JobState::Failed },
        { "cancelled", JobState::Cancelled },
        { "interrupted", JobState::Interrupted },
    };
    for (const auto& entry : table) {
        if (value == entry.first) {
            *state = entry.second;
            return true;
        }
    }
    return false;
}

bool isTerminal(JobState state)
{
    switch (state) {
    case JobState::Complete:
    case JobState::Failed:
    case JobState::Cancelled:
    case JobState::Interrupted:
        return true;
    default:
        return false;
    }
}
}