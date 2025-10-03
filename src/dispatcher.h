////////////////////////////////////////////////////////////////////////
// OpenTibia - an opensource roleplaying game
////////////////////////////////////////////////////////////////////////
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
////////////////////////////////////////////////////////////////////////

#pragma once

#include "thread_holder_base.h"

#define addDispatcherTask(function) g_dispatcher.addTask(std::make_unique<Task>(function))
#define addTimedDispatcherTask(delay, function) g_dispatcher.addTask(std::make_unique<Task>(delay, function))

static constexpr uint32_t DISPATCHER_TASK_EXPIRATION = 2000;
static constexpr auto SYSTEM_TIME_ZERO = std::chrono::system_clock::time_point(std::chrono::milliseconds(0));

class Task;
using TaskFunc = std::function<void(void)>;
using TaskPtr = std::unique_ptr<Task>;

class Task
{
public:
	Task(TaskFunc&& f) :
		m_func(std::move(f)) {}
	Task(uint32_t ms, TaskFunc&& f) :
		m_expiration(std::chrono::system_clock::now() + std::chrono::milliseconds(ms)),
		m_func(std::move(f)) {}
	virtual ~Task() = default;

	void operator()() { m_func(); }

	void unsetExpiration() { m_expiration = SYSTEM_TIME_ZERO; }
	bool hasExpired() const
	{
		if (m_expiration == SYSTEM_TIME_ZERO) {
			return false;
		}
		return m_expiration < std::chrono::system_clock::now();
	}

private:
	TaskFunc m_func;
	std::chrono::system_clock::time_point m_expiration = SYSTEM_TIME_ZERO;
};

class Dispatcher final : public ThreadHolder<Dispatcher>
{
public:
	Dispatcher() = default;

	// non-copyable
	Dispatcher(const Dispatcher&) = delete;
	Dispatcher& operator=(const Dispatcher&) = delete;

	void addTask(TaskPtr task);

	void shutdown();

	uint64_t getDispatcherCycle() const { return dispatcherCycle; }

	void threadMain();

private:
	std::mutex taskLock;
	std::condition_variable taskSignal;

	std::vector<TaskPtr> taskList;
	uint64_t dispatcherCycle = 0;
};

extern Dispatcher g_dispatcher;
