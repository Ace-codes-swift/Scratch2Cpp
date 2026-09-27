#include "scratch/Coroutine.hpp"

#include <SDL3/SDL.h>

#include "scratch/Runtime.hpp"

namespace scratch {

std::coroutine_handle<> Task::promise_type::FinalAwaiter::await_suspend(Handle h) noexcept {
    promise_type& p = h.promise();
    if (p.continuation) {
        // Nested call finished: resume the caller within the same thread.
        if (p.thread) p.thread->leaf = p.continuation;
        return p.continuation;
    }
    if (p.thread) {
        p.thread->finished = true;
        p.thread->leaf = nullptr;
    }
    return std::noop_coroutine();
}

void Task::promise_type::unhandled_exception() {
    try {
        std::rethrow_exception(std::current_exception());
    } catch (const std::exception& e) {
        SDL_Log("Scratch script threw an exception: %s", e.what());
    } catch (...) {
        SDL_Log("Scratch script threw an unknown exception");
    }
    if (thread) {
        thread->finished = true;
        thread->leaf = nullptr;
    }
}

std::coroutine_handle<> Task::await_suspend(Handle caller) noexcept {
    promise_type& callee = handle_.promise();
    callee.thread = caller.promise().thread;
    callee.continuation = caller;
    if (callee.thread) callee.thread->leaf = handle_;
    return handle_;
}

bool Yield::await_suspend(Task::Handle h) noexcept {
    Thread* t = h.promise().thread;
    if (!t) return true;
    if (t->warpDepth > 0) {
        // scratch-vm keeps running warp-mode procedures without yielding
        // until they have used more than 500 ms of wall time. --gpu raises
        // that cap so heavy render scripts can finish in one burst.
        const double limit = (t->runtime && t->runtime->config().gpuAccel) ? 30.0 : 0.5;
        if (Runtime::now() - t->warpStart < limit) return false;
        t->warpStart = Runtime::now();
    }
    t->leaf = h;
    return true;
}

WarpGuard::WarpGuard(Thread& t) : thread_(t) {
    if (thread_.warpDepth == 0) thread_.warpStart = Runtime::now();
    ++thread_.warpDepth;
}

WarpGuard::~WarpGuard() {
    if (thread_.warpDepth > 0) --thread_.warpDepth;
}

}  // namespace scratch
