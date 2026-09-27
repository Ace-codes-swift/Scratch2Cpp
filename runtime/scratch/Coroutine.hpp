// Coroutine.hpp - Scratch "threads" on top of C++20 coroutines.
//
// Every Scratch script is a cooperative thread: it runs until it reaches the
// end of a loop iteration (or a wait) where scratch-vm yields back to the
// sequencer. We model each script as a coroutine returning `Task`; loops
// `co_await Yield{}` at the end of each iteration, exactly where scratch-vm
// yields. Custom blocks (procedures) are also coroutines and are awaited with
// `co_await proc(...)`, which transfers control into the callee and back.
#pragma once

#include <coroutine>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace scratch {

class Target;
class Runtime;
struct Thread;
struct HatBinding;

class Task {
public:
    struct promise_type;
    using Handle = std::coroutine_handle<promise_type>;

    struct promise_type {
        Thread* thread = nullptr;                 // owning Scratch thread
        std::coroutine_handle<> continuation{};   // caller coroutine (nested calls)

        Task get_return_object() { return Task{Handle::from_promise(*this)}; }
        std::suspend_always initial_suspend() noexcept { return {}; }

        struct FinalAwaiter {
            bool await_ready() noexcept { return false; }
            std::coroutine_handle<> await_suspend(Handle h) noexcept;
            void await_resume() noexcept {}
        };
        FinalAwaiter final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception();
    };

    Task() = default;
    explicit Task(Handle h) : handle_(h) {}
    Task(Task&& other) noexcept : handle_(other.handle_) { other.handle_ = nullptr; }
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            destroy();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
    ~Task() { destroy(); }

    Handle handle() const { return handle_; }
    bool valid() const { return handle_ != nullptr; }
    bool done() const { return !handle_ || handle_.done(); }

    // Awaiting a Task from another Task runs the callee inside the same
    // Scratch thread (symmetric transfer) until it completes.
    bool await_ready() const noexcept { return false; }
    std::coroutine_handle<> await_suspend(Handle caller) noexcept;
    void await_resume() noexcept {}

private:
    void destroy() {
        if (handle_) {
            handle_.destroy();
            handle_ = nullptr;
        }
    }
    Handle handle_{};
};

// One running Scratch script instance.
struct Thread {
    enum class Hat { GreenFlag, KeyPressed, SpriteClicked, StageClicked, BroadcastReceived,
                     BackdropSwitched, StartAsClone, TimerGreaterThan, Internal };

    Runtime* runtime = nullptr;
    Target* target = nullptr;
    Hat hat = Hat::Internal;
    std::string option;                        // key name / broadcast name / backdrop name
    const HatBinding* binding = nullptr;       // script that started this thread (for restarts)
    Task task;                                 // root coroutine
    std::coroutine_handle<> leaf{};            // coroutine to resume next tick
    bool finished = false;                     // root coroutine returned
    bool stopped = false;                      // stopped externally (stop all, restart...)
    int warpDepth = 0;                         // >0 while inside a "run without screen refresh" block
    double warpStart = 0.0;                    // time when warp mode began (seconds)
    std::vector<std::weak_ptr<Thread>> waitingFor;  // broadcast-and-wait bookkeeping

    bool active() const { return !finished && !stopped && leaf; }
    void step() {
        if (active()) leaf.resume();
    }
};

// `co_await Yield{}`: hand control back to the sequencer until the next tick.
// In warp mode (custom blocks marked "run without screen refresh") the yield
// is skipped unless the thread has been warping for more than 500 ms, matching
// scratch-vm's warp timer.
struct Yield {
    bool await_ready() const noexcept { return false; }
    bool await_suspend(Task::Handle h) noexcept;
    void await_resume() const noexcept {}
};

// `co_await CurrentThread{}` gives generated code access to its Thread.
struct CurrentThread {
    Thread* thread = nullptr;
    bool await_ready() const noexcept { return false; }
    bool await_suspend(Task::Handle h) noexcept {
        thread = h.promise().thread;
        return false;  // never actually suspends
    }
    Thread* await_resume() const noexcept { return thread; }
};

// RAII helper placed in a warp procedure's frame.
class WarpGuard {
public:
    explicit WarpGuard(Thread& t);
    ~WarpGuard();
    WarpGuard(const WarpGuard&) = delete;
    WarpGuard& operator=(const WarpGuard&) = delete;

private:
    Thread& thread_;
};

using ScriptFactory = std::function<Task(Target&, Thread&)>;

}  // namespace scratch
