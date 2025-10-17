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

using MoveFunctionPtr = void(*)(Item* item);
using StepFunctionPtr = void(*)(Creature* creature, Item* item);
using EquipFunctionPtr = bool(*)(MoveEvent* moveEvent, Player* player, Item* item, Slots_t slot, bool boolean);

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

	void onCreatureMove(Creature* actor, Creature* creature, const Tile* fromTile, const Tile* toTile, bool isStepping);
	bool onPlayerEquip(Player* player, Item* item, Slots_t slot, bool isCheck);
	bool onPlayerDeEquip(Player* player, Item* item, Slots_t slot, bool isRemoval);
	void onItemMove(Creature* actor, Item* item, Tile* tile, bool isAdd);

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

	EventPtr getEvent(const std::string& nodeName) override;
	void registerEvent(EventPtr event, xmlNodePtr p) override;

	using MoveIdListMap = std::map<uint16_t, MoveEventList>;
	void addEvent(MoveEvent moveEvent, uint16_t id, MoveIdListMap& map);
	MoveEvent* getEvent(Item* item, MoveEvent_t eventType, Slots_t slot);

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

class MoveEvent final : public Event
{
public:
	MoveEvent(LuaInterface* luaInterface) : Event(luaInterface) {}

	MoveEvent_t getEventType() const { return m_eventType; }
	void setEventType(MoveEvent_t type);

	bool configureEvent(xmlNodePtr p) override;
	bool loadFunction(const std::string& functionName) override;

	void fireStepEvent(Creature* actor, Creature* creature, Item* item, const Position& pos, const Position& fromPos, const Position& toPos);
	void fireAddRemItem(Creature* actor, Item* item, Item* tileItem, const Position& pos);
	bool fireEquip(Player* player, Item* item, Slots_t slot, bool boolean);

	void executeStep(Creature* actor, Creature* creature, Item* item, const Position& pos, const Position& fromPos, const Position& toPos);
	bool executeEquip(Player* player, Item* item, Slots_t slot, bool boolean);
	void executeAddRemItem(Creature* actor, Item* item, Item* tileItem, const Position& pos);

	uint32_t getWieldInfo() const { return m_wieldInfo; }
	uint32_t getSlot() const { return m_slot; }
	int32_t getReqLevel() const { return m_reqLevel; }
	int32_t getReqMagLv() const { return m_reqMagLevel; }
	bool isPremium() const { return m_premium; }

	const VocationMap& getVocEquipMap() const { return m_vocEquipMap; }
	const std::string& getVocationString() const { return m_vocationString; }

	static void StepInField(Creature* creature, Item* item);
	static void AddItemField(Item* item);
	static bool EquipItem(MoveEvent* moveEvent, Player* player, Item* item, Slots_t slot, bool isCheck);
	static bool DeEquipItem(MoveEvent*, Player* player, Item* item, Slots_t slot, bool isRemoval);

private:
	std::string getScriptEventName() const override;

	std::string m_vocationString;
	VocationMap m_vocEquipMap;

	MoveFunctionPtr m_moveFunction = nullptr;
	StepFunctionPtr m_stepFunction = nullptr;
	EquipFunctionPtr m_equipFunction = nullptr;

	MoveEvent_t m_eventType = MOVE_EVENT_LAST;

	uint32_t m_wieldInfo = 0;
	uint32_t m_slot = SLOTP_WHEREEVER;

	int32_t m_reqLevel = 0;
	int32_t m_reqMagLevel = 0;

	bool m_premium = false;
};
