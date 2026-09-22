/*
 * Audacity: A Digital Audio Editor
 */
#pragma once

#include <string>

namespace au::aicore {
// Every runtime request and result will carry this protocol version. Keeping it
// here prevents provider-specific schemas from leaking into editor modules.
inline constexpr int AI_RUNTIME_PROTOCOL_VERSION = 1;

struct JobId {
    std::string value;
};

// Provider-neutral job lifecycle. Providers may skip intermediate states, but a
// job always starts Queued and ends in exactly one terminal state.
enum class JobState {
    Queued,
    Preparing,
    Loading,
    Running,
    Decoding,
    Importing,
    Complete,
    Failed,
    Cancelled,
    Interrupted,
};

// Serializable name used by the manifest and the runtime protocol.
std::string toString(JobState state);
bool jobStateFromString(const std::string& value, JobState* state);
bool isTerminal(JobState state);

// A provider-neutral request. `parametersJson` is provider-owned JSON that the
// runtime host forwards untouched.
struct JobRequest {
    std::string providerId;
    std::string parametersJson;
};

// The mutable status of a job. This is what aiproject persists and what the UI
// observes, so it never contains provider-specific fields.
struct JobStatus {
    JobId id;
    std::string providerId;
    JobState state = JobState::Queued;
    double progress = 0.0;
    std::string message;
    std::string resultManifest;
    std::string errorCode;
    std::string errorMessage;
};
}