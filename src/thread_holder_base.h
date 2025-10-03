#pragma once

#include "const.h"

#include <atomic>
#include <thread>

template<typename Derived>
class ThreadHolder
{
public:
	ThreadHolder() = default;

	// non-copyable
	ThreadHolder(const ThreadHolder&) = delete;
	ThreadHolder& operator=(const ThreadHolder&) = delete;

	void start() {
		setState(THREAD_STATE_RUNNING);
		thread = std::thread(&Derived::threadMain, static_cast<Derived*>(this));
	}

	void stop() { setState(THREAD_STATE_CLOSING); }

	void join() {
		if (thread.joinable()) {
			thread.join();
		}
	}

protected:
	ThreadState getState() const { return threadState.load(std::memory_order_relaxed); }
	void setState(ThreadState newState) { threadState.store(newState, std::memory_order_relaxed); }

private:
	std::atomic<ThreadState> threadState{ THREAD_STATE_TERMINATED };
	std::thread thread;
};
