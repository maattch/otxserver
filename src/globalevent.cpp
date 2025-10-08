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

#include "globalevent.h"

#include "player.h"

#include "otx/util.hpp"

GlobalEvents g_globalEvents;

void GlobalEvents::init()
{
	m_interface = std::make_unique<LuaInterface>("GlobalEvent Interface");
	m_interface->initState();
	g_lua.addScriptInterface(m_interface.get());
}

void GlobalEvents::terminate()
{
	thinkMap.clear();
	serverMap.clear();
	timerMap.clear();

	g_lua.removeScriptInterface(m_interface.get());
	m_interface.reset();
}

void GlobalEvents::clear()
{
	thinkMap.clear();
	serverMap.clear();
	timerMap.clear();

	m_interface->reInitState();
}

EventPtr GlobalEvents::getEvent(const std::string& nodeName)
{
	if (otx::util::as_lower_string(nodeName) == "globalevent") {
		return EventPtr(new GlobalEvent(getInterface()));
	}
	return nullptr;
}

void GlobalEvents::registerEvent(EventPtr event, xmlNodePtr)
{
	// event is guaranteed to be globalevent
	auto globalEvent = static_cast<GlobalEvent*>(event.get());

	GlobalEventMap* map;
	if (globalEvent->getEventType() == GLOBALEVENT_TIMER) {
		map = &timerMap;
	} else if (globalEvent->getEventType() != GLOBALEVENT_NONE) {
		map = &serverMap;
	} else {
		map = &thinkMap;
	}

	const std::string eventName = globalEvent->getName();
	if (!map->emplace(eventName, std::move(*globalEvent)).second) {
		std::clog << "[Warning - GlobalEvents::configureEvent] Duplicate registered globalevent with name: " << eventName << std::endl;
	}
}

void GlobalEvents::startup()
{
	execute(GLOBALEVENT_STARTUP);
	addSchedulerTask(1000, [this]() { timer(); });
	addSchedulerTask(SCHEDULER_MINTICKS, [this]() { think(); });
}

void GlobalEvents::timer()
{
	time_t now = time(nullptr);
	tm* ts = localtime(&now);
	for (auto& it : timerMap) {
		const uint32_t interval = it.second.getInterval();
		int32_t h = interval >> 16;
		int32_t m = (interval >> 8) & 0xFF;
		int32_t s = interval & 0xFF;
		if (h != ts->tm_hour || m != ts->tm_min || s != ts->tm_sec) {
			continue;
		}

		if (!it.second.executeEvent()) {
			std::clog << "[Error - GlobalEvents::timer] Couldn't execute event: " << it.second.getName() << std::endl;
		}
	}

	addSchedulerTask(1000, [this]() { timer(); });
}

void GlobalEvents::think()
{
	const int64_t now = otx::util::mstime();
	for (auto& it : thinkMap) {
		if ((it.second.getLastExecution() + it.second.getInterval()) > now) {
			continue;
		}

		it.second.setLastExecution(now);
		if (!it.second.executeEvent()) {
			std::clog << "[Error - GlobalEvents::think] Couldn't execute event: " << it.second.getName() << std::endl;
		}
	}

	addSchedulerTask(SCHEDULER_MINTICKS, [this]() { think(); });
}

void GlobalEvents::execute(GlobalEvent_t type)
{
	for (auto& it : serverMap) {
		if (it.second.getEventType() == type) {
			it.second.executeEvent();
		}
	}
}

GlobalEvent::GlobalEvent(LuaInterface* luaInterface) :
	Event(luaInterface),
	m_lastExecution(otx::util::mstime())
{
	//
}

bool GlobalEvent::configureEvent(xmlNodePtr p)
{
	std::string strValue;
	if (!readXMLString(p, "name", strValue)) {
		std::clog << "[Error - GlobalEvent::configureEvent] No name for a globalevent." << std::endl;
		return false;
	}

	m_name = strValue;
	m_eventType = GLOBALEVENT_NONE;
	if (readXMLString(p, "type", strValue)) {
		const std::string tmpStrValue = otx::util::as_lower_string(strValue);
		if (tmpStrValue == "startup" || tmpStrValue == "start" || tmpStrValue == "load") {
			m_eventType = GLOBALEVENT_STARTUP;
		} else if (tmpStrValue == "shutdown" || tmpStrValue == "quit" || tmpStrValue == "exit") {
			m_eventType = GLOBALEVENT_SHUTDOWN;
		} else if (tmpStrValue == "globalsave" || tmpStrValue == "global") {
			m_eventType = GLOBALEVENT_GLOBALSAVE;
		} else if (tmpStrValue == "record" || tmpStrValue == "playersrecord") {
			m_eventType = GLOBALEVENT_RECORD;
		} else {
			std::clog << "[Error - GlobalEvent::configureEvent] No valid type \"" << strValue << "\" for globalevent with name " << m_name << std::endl;
			return false;
		}
		return true;
	} else if (readXMLString(p, "time", strValue) || readXMLString(p, "at", strValue)) {
		auto params = vectorAtoi(explodeString(strValue, ":"));
		if (params.empty()) {
			std::clog << "[Error - GlobalEvent::configureEvent] Invalid time \"" << strValue << "\" for globalevent with name " << m_name << std::endl;
			return false;
		}

		const int32_t hour = params[0];
		if (hour < 0 || hour > 23) {
			std::clog << "[Error - GlobalEvent::configureEvent] No valid hour \"" << strValue << "\" for globalevent with name " << m_name << std::endl;
			return false;
		}

		m_interval |= hour << 16;
		if (params.size() > 1) {
			const int32_t minute = params[1];
			if (minute < 0 || minute > 59) {
				std::clog << "[Error - GlobalEvent::configureEvent] No valid minute \"" << strValue << "\" for globalevent with name " << m_name << std::endl;
				return false;
			}

			m_interval |= params[1] << 8;
			if (params.size() > 2) {
				const int32_t second = params[2];
				if (second < 0 || second > 59) {
					std::clog << "[Error - GlobalEvent::configureEvent] No valid second \"" << strValue << "\" for globalevent with name " << m_name << std::endl;
					return false;
				}

				m_interval |= second;
			}
		}

		m_eventType = GLOBALEVENT_TIMER;
		return true;
	}

	int32_t intValue;
	if (readXMLInteger(p, "interval", intValue)) {
		m_interval = std::max<int32_t>(SCHEDULER_MINTICKS, intValue);
		return true;
	}

	std::clog << "[Error - GlobalEvent::configureEvent] No interval for globalevent with name " << m_name << std::endl;
	return false;
}

std::string GlobalEvent::getScriptEventName() const
{
	switch (m_eventType) {
		case GLOBALEVENT_STARTUP:    return "onStartup";
		case GLOBALEVENT_SHUTDOWN:   return "onShutdown";
		case GLOBALEVENT_GLOBALSAVE: return "onGlobalSave";
		case GLOBALEVENT_RECORD:     return "onRecord";
		case GLOBALEVENT_TIMER:      return "onTime";
		default:
			return "onThink";
	}
}

void GlobalEvent::executeRecord(uint32_t current, uint32_t old, Player* player)
{
	// onRecord(current, old, cid)
	if (!otx::lua::reserveScriptEnv()) {
		std::clog << "[Error - GlobalEvent::executeRecord] Call stack overflow." << std::endl;
		return;
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	env.setScriptId(m_scriptId, m_interface);

	lua_State* L = m_interface->getState();
	m_interface->pushFunction(m_scriptId);

	lua_pushnumber(L, current);
	lua_pushnumber(L, old);
	lua_pushnumber(L, env.addThing(player));
	otx::lua::callVoidFunction(L, 3);
}

bool GlobalEvent::executeEvent()
{
	// onThink(interval)
	// onTime(interval)
	if (!otx::lua::reserveScriptEnv()) {
		std::clog << "[Error - GlobalEvent::executeEvent] Call stack overflow." << std::endl;
		return false;
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	env.setScriptId(m_scriptId, m_interface);

	lua_State* L = m_interface->getState();
	m_interface->pushFunction(m_scriptId);

	int params = 0;
	if (m_eventType == GLOBALEVENT_NONE || m_eventType == GLOBALEVENT_TIMER) {
		lua_pushnumber(L, m_interval);
		params = 1;
	}
	return otx::lua::callFunction(L, params);
}
