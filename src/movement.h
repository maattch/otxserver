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
#include "creature.h"

class MoveEvent;

using MoveFunctionPtr = uint32_t(*)(Item* item);
using StepFunctionPtr = uint32_t(*)(Creature* creature, Item* item);
using EquipFunctionPtr = bool(*)(MoveEvent* moveEvent, Player* player, Item* item, slots_t slot, bool boolean);

enum MoveEvent_t : uint8_t
{
	MOVE_EVENT_STEP_IN = 0,
	MOVE_EVENT_STEP_OUT,
	MOVE_EVENT_EQUIP,
	MOVE_EVENT_DE_EQUIP,
	MOVE_EVENT_ADD_ITEM,
	MOVE_EVENT_REMOVE_ITEM,
	MOVE_EVENT_ADD_TILEITEM,
	MOVE_EVENT_REMOVE_TILEITEM,

	MOVE_EVENT_LAST
};

class MoveEventScript final : public LuaInterface
{
public:
	MoveEventScript() : LuaInterface("MoveEvents Interface") {}

	static MoveEvent* event;

private:
	void registerFunctions();
	static int32_t luaCallFunction(lua_State* L);
};

class MoveEvents final : public BaseEvents
{
public:
	MoveEvents() = default;

	// non-copyable
	MoveEvents(const MoveEvents&) = delete;
	MoveEvents& operator=(const MoveEvents&) = delete;

	void init();
	void terminate();

	uint32_t onCreatureMove(Creature* actor, Creature* creature, const Tile* fromTile, const Tile* toTile, bool isStepping);
	bool onPlayerEquip(Player* player, Item* item, slots_t slot, bool isCheck);
	bool onPlayerDeEquip(Player* player, Item* item, slots_t slot, bool isRemoval);
	uint32_t onItemMove(Creature* actor, Item* item, Tile* tile, bool isAdd);

	MoveEvent* getEvent(Item* item, MoveEvent_t eventType);
	bool hasEquipEvent(Item* item);
	bool hasTileEvent(Item* item);

	void onRemoveTileItem(const Tile* tile, Item* item);
	void onAddTileItem(const Tile* tile, Item* item);

private:
	struct MoveEventList
	{
		std::list<MoveEvent> moveEvent[MOVE_EVENT_LAST];
	};

	std::string getScriptBaseName() const override { return "movements"; }
	void clear() override;

	Event* getEvent(const std::string& nodeName) override;
	bool registerEvent(Event* event, xmlNodePtr p) override;

	using MoveIdListMap = std::map<uint16_t, MoveEventList>;
	void addEvent(MoveEvent moveEvent, uint16_t id, MoveIdListMap& map);
	MoveEvent* getEvent(Item* item, MoveEvent_t eventType, slots_t slot);

	using MovePosListMap = std::map<Position, MoveEventList>;
	void addEvent(MoveEvent moveEvent, Position pos, MovePosListMap& map);
	MoveEvent* getEvent(const Tile* tile, MoveEvent_t eventType);

	LuaInterface* getInterface() override { return m_interface; }

	MoveEventScript* m_interface = nullptr;

	MoveIdListMap m_itemIdMap;
	MoveIdListMap m_uniqueIdMap;
	MoveIdListMap m_actionIdMap;

	MovePosListMap m_positionMap;

	std::vector<Item*> m_lastCacheItemVector;
	const Tile* m_lastCacheTile = nullptr;
};

extern MoveEvents g_moveEvents;

class MoveEvent : public Event
{
public:
	MoveEvent(LuaInterface* _interface);
	virtual ~MoveEvent() {}

	MoveEvent_t getEventType() const { return m_eventType; }
	void setEventType(MoveEvent_t type);

	virtual bool configureEvent(xmlNodePtr p);
	virtual bool loadFunction(const std::string& functionName);

	uint32_t fireStepEvent(Creature* actor, Creature* creature, Item* item, const Position& pos, const Position& fromPos, const Position& toPos);
	uint32_t fireAddRemItem(Creature* actor, Item* item, Item* tileItem, const Position& pos);
	bool fireEquip(Player* player, Item* item, slots_t slot, bool boolean);

	uint32_t executeStep(Creature* actor, Creature* creature, Item* item, const Position& pos, const Position& fromPos, const Position& toPos);
	bool executeEquip(Player* player, Item* item, slots_t slot, bool boolean);
	uint32_t executeAddRemItem(Creature* actor, Item* item, Item* tileItem, const Position& pos);

	uint32_t getWieldInfo() const { return wieldInfo; }
	uint32_t getSlot() const { return slot; }
	int32_t getReqLevel() const { return reqLevel; }
	int32_t getReqMagLv() const { return reqMagLevel; }
	bool isPremium() const { return premium; }

	const VocationMap& getVocEquipMap() const { return vocEquipMap; }
	const std::string& getVocationString() const { return vocationString; }

	static uint32_t StepInField(Creature* creature, Item* item);
	static uint32_t AddItemField(Item* item);
	static bool EquipItem(MoveEvent* moveEvent, Player* player, Item* item, slots_t slot, bool isCheck);
	static bool DeEquipItem(MoveEvent*, Player* player, Item* item, slots_t slot, bool isRemoval);

protected:
	MoveEvent_t m_eventType;

	virtual std::string getScriptEventName() const;

	MoveFunctionPtr moveFunction = nullptr;
	StepFunctionPtr stepFunction = nullptr;
	EquipFunctionPtr equipFunction = nullptr;

	uint32_t wieldInfo, slot;
	int32_t reqLevel, reqMagLevel;
	bool premium;

	VocationMap vocEquipMap;
	std::string vocationString;
};
