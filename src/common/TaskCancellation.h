#pragma once

#include <atomic>
#include <memory>

namespace miacode::task {

using CancellationFlag = std::shared_ptr<std::atomic_bool>;

// Caught at the worker boundary; stack unwinding releases intermediate results.
struct Cancelled {};

// A worker installs one scope for its synchronous call tree. Thread-local state
// keeps cancellation out of domain data and isolates concurrent tasks. Callers
// outside a scope retain the synchronous behavior of the parser and analyzer.
class CancellationScope final {
public:
    explicit CancellationScope(const CancellationFlag& token) noexcept
        : token_(token), previous_(current_)
    {
        current_ = this;
    }

    ~CancellationScope() { current_ = previous_; }
    CancellationScope(const CancellationScope&) = delete;
    CancellationScope& operator=(const CancellationScope&) = delete;

    static void check()
    {
        if (current_ && current_->token_->load(std::memory_order_relaxed)) {
            current_->unwinding_ = true;
            throw Cancelled{};
        }
    }

    // Bound polling overhead in tight pairing and judgment loops. The first
    // checkpoint checks immediately, then every 256 checkpoints in this task.
    static void checkpoint()
    {
        if (current_ && (current_->checkpointCount_++ & 255u) == 0) check();
    }

    static bool cancellationUnwinding() noexcept
    {
        return current_ && current_->unwinding_;
    }

private:
    CancellationFlag token_;
    CancellationScope* previous_;
    unsigned checkpointCount_ = 0;
    bool unwinding_ = false;
    inline static thread_local CancellationScope* current_ = nullptr;
};

} // namespace miacode::task
