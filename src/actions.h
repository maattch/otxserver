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
#include "const.h"
#include "lua_definitions.h"

class Action;
using ActionPtr = std::unique_ptr<Action>;

class Action : public Event
{
public:
	explicit Action(LuaInterface* luaInterface) : Event(luaInterface) {}
	virtual ~Action() = default;

	// copyable
	Action(const Action&) = default;
	Action& operator=(const Action&) = default;

	bool configureEvent(xmlNodePtr p) override;

	// scripting
	virtual bool executeUse(Player* player, Item* item, const PositionEx& posFrom,
		const PositionEx& posTo, bool extendedUse, uint32_t creatureId);

	bool getAllowFarUse() const { return allowFarUse; }
	void setAllowFarUse(bool v) { allowFarUse = v; }

	bool getCheckLineOfSight() const { return checkLineOfSight; }
	void setCheckLineOfSight(bool v) { checkLineOfSight = v; }

	virtual ReturnValue canExecuteAction(const Player* player, const Position& pos);
	virtual bool hasOwnErrorHandler() { return false; }

protected:
	std::string getScriptEventName() const override { return "onUse"; }

	bool allowFarUse = false;
	bool checkLineOfSight = true;
};

class Actions final : public BaseEvents
{
public:
	Actions() = default;

	// non-copyable
	Actions(const Actions&) = delete;
	Actions& operator=(const Actions&) = delete;

	void init();
	void terminate();

	bool useItem(Player* player, const Position& pos, uint8_t index, Item* item);
	bool useItemEx(Player* player, const Position& fromPos, const Position& toPos,
		uint8_t toStackPos, Item* item, bool isHotkey, uint32_t creatureId = 0);

	static ReturnValue canUse(const Player* player, const Position& pos);
	ReturnValue canUseEx(const Player* player, const Position& pos, const Item* item);
	static ReturnValue canUseFar(const Creature* creature, const Position& toPos, bool checkLineOfSight);

	bool hasAction(const Item* item) { return getAction(item) != nullptr; }

private:
	std::string getScriptBaseName() const override { return "actions"; }
	void clear() override;

	EventPtr getEvent(const std::string& nodeName) override;
	void registerEvent(EventPtr event, xmlNodePtr p) override;

	bool executeUse(Action* action, Player* player, Item* item, const PositionEx& posEx, uint32_t creatureId);
	ReturnValue internalUseItem(Player* player, const Position& pos, uint8_t index, Item* item, uint32_t creatureId);

	bool executeUseEx(Action* action, Player* player, Item* item, const PositionEx& fromPosEx, const PositionEx& toPosEx, bool isHotkey, uint32_t creatureId);
	ReturnValue internalUseItemEx(Player* player, const PositionEx& fromPosEx, const PositionEx& toPosEx, Item* item, bool isHotkey, uint32_t creatureId);

	Action* getAction(const Item* item);

	LuaInterface* getInterface() override { return m_interface.get(); }

	LuaInterfacePtr m_interface;

	ActionPtr defaultAction;
	std::map<uint16_t, Action> useItemMap;
	std::map<uint16_t, Action> uniqueItemMap;
	std::map<uint16_t, Action> actionItemMap;
};

extern Actions g_actions;
