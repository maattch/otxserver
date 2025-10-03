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

#include "dispatcher.h"

Dispatcher g_dispatcher;

void Dispatcher::threadMain()
{
	std::vector<TaskPtr> tmpTaskList;
	// NOTE: second argument defer_lock is to prevent from immediate locking
	std::unique_lock<std::mutex> taskLockUnique(taskLock, std::defer_lock);

	while (getState() != THREAD_STATE_TERMINATED) {
		// check if there are tasks waiting
		taskLockUnique.lock();

		if (taskList.empty()) {
			// if the list is empty wait for signal
			taskSignal.wait(taskLockUnique);
		}

		tmpTaskList.swap(taskList);
		taskLockUnique.unlock();

		for (auto& task : tmpTaskList) {
			if (!task->hasExpired()) {
				++dispatcherCycle;
				// execute it
				(*task)();
			}
		}

		tmpTaskList.clear();
	}
}

void Dispatcher::addTask(TaskPtr task)
{
	bool do_signal = false;

	taskLock.lock();

	if (getState() == THREAD_STATE_RUNNING) {
		do_signal = taskList.empty();
		taskList.push_back(std::move(task));
	}

	taskLock.unlock();

	// send a signal if the list was empty
	if (do_signal) {
		taskSignal.notify_one();
	}
}

void Dispatcher::shutdown()
{
	auto task = std::make_unique<Task>([this]() {
		setState(THREAD_STATE_TERMINATED);
		taskSignal.notify_one();
	});

	std::lock_guard<std::mutex> lockClass(taskLock);
	taskList.push_back(std::move(task));

	taskSignal.notify_one();
}
