#pragma once

#include <string_view>

namespace humtrace::ui {

enum class JobState { Empty, Opening, Ready, Hashing, Analyzing, Finalizing, Complete,
                      Cancelling, Cancelled, Failed, Invalidated };

[[nodiscard]] constexpr bool is_active(JobState s) {
    return s == JobState::Hashing || s == JobState::Analyzing || s == JobState::Finalizing ||
           s == JobState::Cancelling;
}

// Valid transitions of the analysis job (see docs/specs/long-file-workflow.md).
[[nodiscard]] constexpr bool can_transition(JobState from, JobState to) {
    using S = JobState;
    switch (from) {
    case S::Empty: return to == S::Opening;
    case S::Opening: return to == S::Ready || to == S::Failed || to == S::Empty;
    case S::Ready: case S::Cancelled: case S::Failed: case S::Complete: case S::Invalidated:
        return to == S::Opening || to == S::Hashing || to == S::Empty ||
               (from == S::Complete && to == S::Invalidated);
    case S::Hashing: return to == S::Analyzing || to == S::Cancelling || to == S::Failed;
    case S::Analyzing: return to == S::Finalizing || to == S::Cancelling || to == S::Failed;
    case S::Finalizing: return to == S::Complete || to == S::Cancelling || to == S::Failed;
    case S::Cancelling: return to == S::Cancelled || to == S::Failed || to == S::Complete;
    }
    return false;
}

[[nodiscard]] constexpr std::string_view label(JobState s) {
    switch (s) {
    case JobState::Empty: return "No recording";
    case JobState::Opening: return "Opening";
    case JobState::Ready: return "Ready";
    case JobState::Hashing: return "Hashing input";
    case JobState::Analyzing: return "Analyzing";
    case JobState::Finalizing: return "Finalizing";
    case JobState::Complete: return "Complete";
    case JobState::Cancelling: return "Cancelling";
    case JobState::Cancelled: return "Cancelled (partial results discarded)";
    case JobState::Failed: return "Failed";
    case JobState::Invalidated: return "Settings changed; re-analyze";
    }
    return "";
}

} // namespace humtrace::ui
