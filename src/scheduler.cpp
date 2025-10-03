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

#include "otpch.h"

#include "scheduler.h"

Scheduler g_scheduler;

Scheduler::Scheduler() :
	work_guard(boost::asio::make_work_guard(io_context))
{
	//
}

uint32_t Scheduler::addEvent(SchedulerTaskPtr task)
{
	// check if the event has a valid id
	if (task->getEventId() == 0) {
		task->setEventId(++lastEventId);
	}

	uint32_t eventId = task->getEventId();

	boost::asio::post(io_context, [this, scheduledTask = std::move(task)]() mutable {
		// insert the event id in the list of active events
		auto it = eventIdTimerMap.emplace(scheduledTask->getEventId(), boost::asio::steady_timer{ io_context });
		auto& timer = it.first->second;

		timer.expires_after(std::chrono::milliseconds(scheduledTask->getDelay()));
		timer.async_wait([this, scheduledTask = std::move(scheduledTask)](const boost::system::error_code& error) mutable {
			eventIdTimerMap.erase(scheduledTask->getEventId());

			if (error == boost::asio::error::operation_aborted || getState() == THREAD_STATE_TERMINATED) {
				// the timer has been manually canceled(timer->cancel()) or Scheduler::shutdown has been called
				return;
			}

			g_dispatcher.addTask(std::move(scheduledTask));
		});
	});
	return eventId;
}

void Scheduler::stopEvent(uint32_t eventId)
{
	if (eventId == 0) {
		return;
	}

	// lock mutex?
	if (getState() != THREAD_STATE_RUNNING) {
		return;
	}

	boost::asio::post(io_context, [this, eventId]() {
		// search the event id
		auto it = eventIdTimerMap.find(eventId);
		if (it != eventIdTimerMap.end()) {
			it->second.cancel();
		}
	});
}

void Scheduler::shutdown()
{
	setState(THREAD_STATE_TERMINATED);
	boost::asio::post(io_context, [this]() {
		// cancel all active timers
		for (auto& it : eventIdTimerMap) {
			it.second.cancel();
		}

		io_context.stop();
	});
}
