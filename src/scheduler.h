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

#include "dispatcher.h"
#include "thread_holder_base.h"

#define createSchedulerTask(delay, function) std::make_unique<SchedulerTask>(delay, function)
#define addSchedulerTask(delay, function) g_scheduler.addEvent(createSchedulerTask(delay, function))

static constexpr uint32_t SCHEDULER_MINTICKS = 50;

class SchedulerTask;
using SchedulerTaskPtr = std::unique_ptr<SchedulerTask>;

class SchedulerTask final : public Task
{
public:
	SchedulerTask(uint32_t delay, TaskFunc&& f) :
		Task(std::move(f)),
		delay(delay) {}

	// non-copyable
	SchedulerTask(const SchedulerTask&) = delete;
	SchedulerTask& operator=(const SchedulerTask&) = delete;

	void setEventId(uint32_t id) { eventId = id; }
	uint32_t getEventId() const { return eventId; }

	uint32_t getDelay() const { return delay; }

private:
	uint32_t eventId = 0;
	uint32_t delay = 0;
};

class Scheduler final : public ThreadHolder<Scheduler>
{
public:
	Scheduler();

	// non-copyable
	Scheduler(const Scheduler&) = delete;
	void operator=(const Scheduler&) = delete;

	uint32_t addEvent(SchedulerTaskPtr task);
	void stopEvent(uint32_t eventId);

	void shutdown();

	void threadMain() { io_context.run(); }

private:
	std::atomic<uint32_t> lastEventId{ 0 };
	std::unordered_map<uint32_t, boost::asio::steady_timer> eventIdTimerMap;
	boost::asio::io_context io_context;
	boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard;
};

extern Scheduler g_scheduler;
