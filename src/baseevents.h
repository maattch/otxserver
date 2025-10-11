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

#include "lua_definitions.h"

class Event;
using EventPtr = std::unique_ptr<Event>;

class BaseEvents
{
public:
	BaseEvents() = default;
	virtual ~BaseEvents() = default;

	bool loadFromXml();
	bool reload();

	bool isLoaded() const { return m_loaded; }

protected:
	virtual std::string getScriptBaseName() const = 0;
	virtual void clear() = 0;

	virtual void registerEvent(EventPtr event, xmlNodePtr p) = 0;
	virtual EventPtr getEvent(const std::string& nodeName) = 0;

	virtual LuaInterface* getInterface() = 0;

	bool m_loaded = false;
};

class Event
{
public:
	explicit Event(LuaInterface* scriptInterface) : m_interface(scriptInterface) {}
	virtual ~Event() = default;

	virtual bool configureEvent(xmlNodePtr p) = 0;
	virtual bool isScripted() const { return m_scripted; }
	int getScriptId() const { return m_scriptId; }

	bool loadScript(const std::string& script);

	virtual bool loadFunction(const std::string&) { return false; }

protected:
	virtual std::string getScriptEventName() const = 0;

	LuaInterface* m_interface;
	int m_scriptId = 0;
	bool m_scripted = false;
};

class CallBack
{
public:
	CallBack() = default;
	virtual ~CallBack() = default;

	bool loadCallBack(LuaInterface* scriptInterface, const std::string& name);

protected:
	LuaInterface* m_interface = nullptr;
	int m_scriptId = 0;

private:
	std::string m_callbackName;
	bool m_loaded = false;
};
