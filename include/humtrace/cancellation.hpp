#pragma once

#include <atomic>
#include <stdexcept>

namespace humtrace {

// Thrown by long-running operations that observed a cancellation request.
class OperationCancelled : public std::runtime_error {
public:
    OperationCancelled() : std::runtime_error("operation cancelled") {}
};

// Shared, thread-safe cancellation flag. The requesting thread calls
// request_cancel(); workers poll cancelled() at bounded intervals.
class CancellationToken {
public:
    void request_cancel() noexcept { cancelled_.store(true, std::memory_order_relaxed); }
    [[nodiscard]] bool cancelled() const noexcept { return cancelled_.load(std::memory_order_relaxed); }

private:
    std::atomic<bool> cancelled_{false};
};

// A null token means the operation cannot be cancelled.
inline void throw_if_cancelled(const CancellationToken* token) {
    if (token != nullptr && token->cancelled())
        throw OperationCancelled();
}

} // namespace humtrace
