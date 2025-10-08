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

#include "baseevents.h"

class GlobalEvent;
using GlobalEventMap = std::map<std::string, GlobalEvent>;

enum GlobalEvent_t : uint8_t
{
	GLOBALEVENT_NONE,
	GLOBALEVENT_TIMER,
	GLOBALEVENT_STARTUP,
	GLOBALEVENT_SHUTDOWN,
	GLOBALEVENT_GLOBALSAVE,
	GLOBALEVENT_RECORD
};

class GlobalEvents final : public BaseEvents
{
public:
	GlobalEvents() = default;

	void init();
	void terminate();

	void startup();

	void timer();
	void think();
	void execute(GlobalEvent_t type);

	GlobalEventMap& getServerMap() { return serverMap; }

private:
	std::string getScriptBaseName() const override { return "globalevents"; }
	void clear() override;

	EventPtr getEvent(const std::string& nodeName) override;
	void registerEvent(EventPtr event, xmlNodePtr p) override;

	LuaInterface* getInterface() override { return m_interface.get(); }

	LuaInterfacePtr m_interface;

	GlobalEventMap thinkMap;
	GlobalEventMap serverMap;
	GlobalEventMap timerMap;
};

extern GlobalEvents g_globalEvents;

class GlobalEvent final : public Event
{
public:
	GlobalEvent(LuaInterface* luaInterface);

	bool configureEvent(xmlNodePtr p) override;

	void executeRecord(uint32_t current, uint32_t old, Player* player);
	bool executeEvent();

	GlobalEvent_t getEventType() const { return m_eventType; }
	const std::string& getName() const { return m_name; }

	uint32_t getInterval() const { return m_interval; }

	int64_t getLastExecution() const { return m_lastExecution; }
	void setLastExecution(int64_t time) { m_lastExecution = time; }

private:
	std::string getScriptEventName() const override;

	std::string m_name;
	int64_t m_lastExecution;
	uint32_t m_interval = 0;
	GlobalEvent_t m_eventType = GLOBALEVENT_NONE;
};
