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

#include "movement.h"

#include "combat.h"
#include "creature.h"
#include "game.h"
#include "player.h"
#include "tile.h"
#include "tools.h"

#include "otx/util.hpp"

MoveEvents g_moveEvents;

MoveEvent* MoveEventScript::event = nullptr;

void MoveEventScript::registerFunctions()
{
	lua_register(m_luaState, "callFunction", MoveEventScript::luaCallFunction);
}

int32_t MoveEventScript::luaCallFunction(lua_State* L)
{
	// callFunction(...)
	if (!MoveEventScript::event) {
		otx::lua::reportErrorEx(L, "MoveEvent not set!");
		lua_pushboolean(L, false);
		return 1;
	}

	if (MoveEventScript::event->getEventType() == MOVE_EVENT_EQUIP || MoveEventScript::event->getEventType() == MOVE_EVENT_DE_EQUIP) {
		ScriptEnvironment& env = otx::lua::getScriptEnv();
		bool boolean = otx::lua::popBoolean(L);
		Slots_t slot = static_cast<Slots_t>(otx::lua::popNumber(L));

		Item* item = env.getItemByUID(otx::lua::popNumber(L));
		if (!item) {
			otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_ITEM_NOT_FOUND));
			lua_pushboolean(L, false);
			return 1;
		}

		Player* player = otx::lua::getPlayer(L, otx::lua::popNumber(L));
		if (!player) {
			otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
			lua_pushboolean(L, false);
			return 1;
		}

		if (MoveEventScript::event->getEventType() != MOVE_EVENT_EQUIP) {
			lua_pushboolean(L, MoveEvent::DeEquipItem(MoveEventScript::event, player, item, slot, boolean));
		} else {
			lua_pushboolean(L, MoveEvent::EquipItem(MoveEventScript::event, player, item, slot, boolean));
		}

		return 1;
	} else if (MoveEventScript::event->getEventType() == MOVE_EVENT_STEP_IN) {
		ScriptEnvironment& env = otx::lua::getScriptEnv();
		Item* item = env.getItemByUID(otx::lua::popNumber(L));
		if (!item) {
			otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_ITEM_NOT_FOUND));
			lua_pushboolean(L, false);
			return 1;
		}

		Creature* creature = otx::lua::getCreature(L, otx::lua::popNumber(L));
		if (!creature) {
			otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
			lua_pushboolean(L, false);
			return 1;
		}

		lua_pushboolean(L, MoveEvent::StepInField(creature, item));
		return 1;
	} else if (MoveEventScript::event->getEventType() == MOVE_EVENT_ADD_ITEM) {
		ScriptEnvironment& env = otx::lua::getScriptEnv();
		Item* item = env.getItemByUID(otx::lua::popNumber(L));
		if (!item) {
			otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_ITEM_NOT_FOUND));
			lua_pushboolean(L, false);
			return 1;
		}

		lua_pushboolean(L, MoveEvent::AddItemField(item));
		return 1;
	}

	otx::lua::reportErrorEx(L, "callFunction not available for current event.");
	lua_pushboolean(L, false);
	return 1;
}

void MoveEvents::init()
{
	m_interface = new MoveEventScript();
	m_interface->initState();
	g_lua.addScriptInterface(m_interface);
}

void MoveEvents::terminate()
{
	m_itemIdMap.clear();
	m_actionIdMap.clear();
	m_uniqueIdMap.clear();
	m_positionMap.clear();

	m_lastCacheTile = nullptr;
	m_lastCacheItemVector.clear();

	g_lua.removeScriptInterface(m_interface);
	delete m_interface;
	m_interface = nullptr;
}

void MoveEvents::clear()
{
	m_itemIdMap.clear();
	m_actionIdMap.clear();
	m_uniqueIdMap.clear();
	m_positionMap.clear();

	m_interface->reInitState();

	m_lastCacheTile = nullptr;
	m_lastCacheItemVector.clear();
}

EventPtr MoveEvents::getEvent(const std::string& nodeName)
{
	const std::string tmpNodeName = otx::util::as_lower_string(nodeName);
	if (tmpNodeName == "movevent" || tmpNodeName == "moveevent" || tmpNodeName == "movement") {
		return EventPtr(new MoveEvent(getInterface()));
	}
	return nullptr;
}

void MoveEvents::registerEvent(EventPtr event, xmlNodePtr p)
{
	// it is guaranteed to be a moveevent
	auto moveEvent = static_cast<MoveEvent*>(event.get());

	std::string strValue, endStrValue;
	const MoveEvent_t eventType = moveEvent->getEventType();
	if ((eventType == MOVE_EVENT_ADD_ITEM || eventType == MOVE_EVENT_REMOVE_ITEM) && readXMLString(p, "tileitem", strValue) && booleanString(strValue)) {
		switch (eventType) {
			case MOVE_EVENT_ADD_ITEM:
				moveEvent->setEventType(MOVE_EVENT_ADD_TILEITEM);
				break;
			case MOVE_EVENT_REMOVE_ITEM:
				moveEvent->setEventType(MOVE_EVENT_REMOVE_TILEITEM);
				break;

			default:
				break;
		}
	}

	if (readXMLString(p, "itemid", strValue)) {
		for (const std::string& s : explodeString(strValue, ";")) {
			auto intVector = vectorAtoi(explodeString(s, "-"));
			if (intVector.empty()) {
				continue;
			}

			addEvent(*moveEvent, intVector[0], m_itemIdMap);
			if (eventType == MOVE_EVENT_EQUIP) {
				ItemType& iType = Item::items.getItemType(intVector[0]);
				iType.wieldInfo = moveEvent->getWieldInfo();
				iType.minReqLevel = moveEvent->getReqLevel();
				iType.minReqMagicLevel = moveEvent->getReqMagLv();
				iType.vocationString = moveEvent->getVocationString();
			}

			if (intVector.size() > 1) {
				while (intVector[0] < intVector[1]) {
					addEvent(*moveEvent, ++intVector[0], m_itemIdMap);
					if (eventType == MOVE_EVENT_EQUIP) {
						ItemType& iType = Item::items.getItemType(intVector[0]);
						iType.wieldInfo = moveEvent->getWieldInfo();
						iType.minReqLevel = moveEvent->getReqLevel();
						iType.minReqMagicLevel = moveEvent->getReqMagLv();
						iType.vocationString = moveEvent->getVocationString();
					}
				}
			}
		}
	}

	if (readXMLString(p, "fromid", strValue) && readXMLString(p, "toid", endStrValue)) {
		auto intVector = vectorAtoi(explodeString(strValue, ";"));
		auto endIntVector = vectorAtoi(explodeString(endStrValue, ";"));
		if (!intVector.empty() && intVector.size() == endIntVector.size()) {
			for (size_t i = 0, size = intVector.size(); i < size; ++i) {
				addEvent(*moveEvent, intVector[i], m_itemIdMap);
				if (eventType == MOVE_EVENT_EQUIP) {
					ItemType& iType = Item::items.getItemType(intVector[i]);
					iType.wieldInfo = moveEvent->getWieldInfo();
					iType.minReqLevel = moveEvent->getReqLevel();
					iType.minReqMagicLevel = moveEvent->getReqMagLv();
					iType.vocationString = moveEvent->getVocationString();
				}

				while (intVector[i] < endIntVector[i]) {
					addEvent(*moveEvent, ++intVector[i], m_itemIdMap);
					if (eventType == MOVE_EVENT_EQUIP) {
						ItemType& iType = Item::items.getItemType(intVector[i]);
						iType.wieldInfo = moveEvent->getWieldInfo();
						iType.minReqLevel = moveEvent->getReqLevel();
						iType.minReqMagicLevel = moveEvent->getReqMagLv();
						iType.vocationString = moveEvent->getVocationString();
					}
				}
			}
		} else {
			std::clog << "[Warning - MoveEvents::registerEvent] Malformed entry (from item: \"" << strValue << "\", to item: \"" << endStrValue << "\")" << std::endl;
		}
	}

	if (readXMLString(p, "uniqueid", strValue)) {
		for (const std::string& s : explodeString(strValue, ";")) {
			auto intVector = vectorAtoi(explodeString(s, "-"));
			if (intVector.empty()) {
				continue;
			}

			addEvent(*moveEvent, intVector[0], m_uniqueIdMap);
			if (intVector.size() > 1) {
				while (intVector[0] < intVector[1]) {
					addEvent(*moveEvent, ++intVector[0], m_uniqueIdMap);
				}
			}
		}
	}

	if (readXMLString(p, "fromuid", strValue) && readXMLString(p, "touid", endStrValue)) {
		auto intVector = vectorAtoi(explodeString(strValue, ";"));
		auto endIntVector = vectorAtoi(explodeString(endStrValue, ";"));
		if (!intVector.empty() && intVector.size() == endIntVector.size()) {
			for (size_t i = 0, size = intVector.size(); i < size; ++i) {
				addEvent(*moveEvent, intVector[i], m_uniqueIdMap);
				while (intVector[i] < endIntVector[i]) {
					addEvent(*moveEvent, ++intVector[i], m_uniqueIdMap);
				}
			}
		} else {
			std::clog << "[Warning - MoveEvents::registerEvent] Malformed entry (from unique: \"" << strValue << "\", to unique: \"" << endStrValue << "\")" << std::endl;
		}
	}

	if (readXMLString(p, "actionid", strValue) || readXMLString(p, "aid", strValue)) {
		for (const std::string& s : explodeString(strValue, ";")) {
			auto intVector = vectorAtoi(explodeString(s, "-"));
			if (intVector.empty()) {
				continue;
			}

			addEvent(*moveEvent, intVector[0], m_actionIdMap);
			if (intVector.size() > 1) {
				while (intVector[0] < intVector[1]) {
					addEvent(*moveEvent, ++intVector[0], m_actionIdMap);
				}
			}
		}
	}

	if (readXMLString(p, "fromaid", strValue) && readXMLString(p, "toaid", endStrValue)) {
		auto intVector = vectorAtoi(explodeString(strValue, ";"));
		auto endIntVector = vectorAtoi(explodeString(endStrValue, ";"));
		if (!intVector.empty() && intVector.size() == endIntVector.size()) {
			for (size_t i = 0, size = intVector.size(); i < size; ++i) {
				addEvent(*moveEvent, intVector[i], m_actionIdMap);
				while (intVector[i] < endIntVector[i]) {
					addEvent(*moveEvent, ++intVector[i], m_actionIdMap);
				}
			}
		} else {
			std::clog << "[Warning - MoveEvents::registerEvent] Malformed entry (from action: \"" << strValue << "\", to action: \"" << endStrValue << "\")" << std::endl;
		}
	}

	if (readXMLString(p, "pos", strValue) || readXMLString(p, "position", strValue)) {
		for (const std::string& s : explodeString(strValue, ";")) {
			auto intVector = vectorAtoi(explodeString(s, ","));
			if (intVector.size() > 2) {
				addEvent(*moveEvent, Position(intVector[0], intVector[1], intVector[2]), m_positionMap);
			}
		}
	}
}

void MoveEvents::addEvent(MoveEvent moveEvent, uint16_t id, MoveIdListMap& map)
{
	auto it = map.find(id);
	if (it != map.end()) {
		auto& moveEventList = it->second.moveEvent[moveEvent.getEventType()];
		for (MoveEvent& existingMoveEvent : moveEventList) {
			if (existingMoveEvent.getSlot() == moveEvent.getSlot()) {
				std::clog << "[Warning - MoveEvents::addEvent] Duplicate move event found: " << id << std::endl;
				return;
			}
		}

		moveEventList.push_back(std::move(moveEvent));
	} else {
		MoveEventList& moveEventList = map[id];
		moveEventList.moveEvent[moveEvent.getEventType()].push_back(std::move(moveEvent));
	}
}

MoveEvent* MoveEvents::getEvent(Item* item, MoveEvent_t eventType)
{
	uint16_t tmpId = item->getUniqueId();
	if (tmpId != 0) {
		auto it = m_uniqueIdMap.find(tmpId);
		if (it != m_uniqueIdMap.end()) {
			auto& moveEventList = it->second.moveEvent[eventType];
			if (!moveEventList.empty()) {
				return &(*moveEventList.begin());
			}
		}
	}

	tmpId = item->getActionId();
	if (tmpId != 0) {
		auto it = m_actionIdMap.find(tmpId);
		if (it != m_actionIdMap.end()) {
			auto& moveEventList = it->second.moveEvent[eventType];
			if (!moveEventList.empty()) {
				return &(*moveEventList.begin());
			}
		}
	}

	auto it = m_itemIdMap.find(item->getID());
	if (it != m_itemIdMap.end()) {
		auto& moveEventList = it->second.moveEvent[eventType];
		if (!moveEventList.empty()) {
			return &(*moveEventList.begin());
		}
	}
	return nullptr;
}

MoveEvent* MoveEvents::getEvent(Item* item, MoveEvent_t eventType, Slots_t slot)
{
	uint32_t slotp = 0;
	switch (slot) {
		case SLOT_HEAD:
			slotp = SLOTP_HEAD;
			break;
		case SLOT_NECKLACE:
			slotp = SLOTP_NECKLACE;
			break;
		case SLOT_BACKPACK:
			slotp = SLOTP_BACKPACK;
			break;
		case SLOT_ARMOR:
			slotp = SLOTP_ARMOR;
			break;
		case SLOT_RIGHT:
			slotp = SLOTP_RIGHT;
			break;
		case SLOT_LEFT:
			slotp = SLOTP_LEFT;
			break;
		case SLOT_LEGS:
			slotp = SLOTP_LEGS;
			break;
		case SLOT_FEET:
			slotp = SLOTP_FEET;
			break;
		case SLOT_AMMO:
			slotp = SLOTP_AMMO;
			break;
		case SLOT_RING:
			slotp = SLOTP_RING;
			break;
		default:
			break;
	}

	auto it = m_itemIdMap.find(item->getID());
	if (it == m_itemIdMap.end()) {
		return nullptr;
	}

	for (MoveEvent& moveEvent : it->second.moveEvent[eventType]) {
		if (moveEvent.getSlot() & slotp) {
			return &moveEvent;
		}
	}
	return nullptr;
}

void MoveEvents::addEvent(MoveEvent moveEvent, Position pos, MovePosListMap& map)
{
	auto it = map.find(pos);
	if (it != map.end()) {
		auto& moveEventList = it->second.moveEvent[moveEvent.getEventType()];
		if (!moveEventList.empty()) {
			std::clog << "[Warning - MoveEvents::addEvent] Duplicate move event found: " << pos << std::endl;
			return;
		}

		moveEventList.push_back(std::move(moveEvent));
	} else {
		MoveEventList& moveEventList = map[pos];
		moveEventList.moveEvent[moveEvent.getEventType()].push_back(std::move(moveEvent));
	}
}

MoveEvent* MoveEvents::getEvent(const Tile* tile, MoveEvent_t eventType)
{
	auto it = m_positionMap.find(tile->getPosition());
	if (it == m_positionMap.end()) {
		return nullptr;
	}

	auto& moveEventList = it->second.moveEvent[eventType];
	if (!moveEventList.empty()) {
		return &(*moveEventList.begin());
	}
	return nullptr;
}

bool MoveEvents::hasEquipEvent(Item* item)
{
	MoveEvent* event;
	return (event = getEvent(item, MOVE_EVENT_EQUIP)) && !event->isScripted() && (event = getEvent(item, MOVE_EVENT_DE_EQUIP)) && !event->isScripted();
}

bool MoveEvents::hasTileEvent(Item* item)
{
	return getEvent(item, MOVE_EVENT_STEP_IN) ||
		   getEvent(item, MOVE_EVENT_STEP_OUT) ||
		   getEvent(item, MOVE_EVENT_ADD_TILEITEM) ||
		   getEvent(item, MOVE_EVENT_REMOVE_TILEITEM);
}

void MoveEvents::onCreatureMove(Creature* actor, Creature* creature,
	const Tile* fromTile, const Tile* toTile, bool isStepping)
{
	MoveEvent_t eventType = MOVE_EVENT_STEP_OUT;

	const Tile* tile = fromTile;
	if (isStepping) {
		eventType = MOVE_EVENT_STEP_IN;
		tile = toTile;
	}

	Position fromPos;
	if (fromTile) {
		fromPos = fromTile->getPosition();
	}

	Position toPos;
	if (toTile) {
		toPos = toTile->getPosition();
	}

	MoveEvent* moveEvent = nullptr;
	if ((moveEvent = getEvent(tile, eventType))) {
		moveEvent->fireStepEvent(actor, creature, nullptr, Position(), fromPos, toPos);
	}

	if (!tile) {
		return;
	}

	Item* tileItem = nullptr;
	if (m_lastCacheTile == tile) {
		if (m_lastCacheItemVector.empty()) {
			return;
		}

		// We cannot use iterators here since the scripts can invalidate the iterator
		for (uint32_t i = 0; i < m_lastCacheItemVector.size(); ++i) {
			if ((tileItem = m_lastCacheItemVector[i]) && (moveEvent = getEvent(tileItem, eventType))) {
				moveEvent->fireStepEvent(actor, creature, tileItem, tile->getPosition(), fromPos, toPos);
			}
		}
		return;
	}

	m_lastCacheTile = tile;
	m_lastCacheItemVector.clear();

	// We cannot use iterators here since the scripts can invalidate the iterator
	for (int32_t i = tile->__getFirstIndex(), j = tile->__getLastIndex(); i < j; ++i) {
		// already checked the ground
		Thing* thing = tile->__getThing(i);
		if (!thing) {
			continue;
		}

		tileItem = thing->getItem();
		if (!tileItem) {
			continue;
		}

		if ((moveEvent = getEvent(tileItem, eventType))) {
			m_lastCacheItemVector.push_back(tileItem);
			moveEvent->fireStepEvent(actor, creature, tileItem, tile->getPosition(), fromPos, toPos);
		} else if (hasTileEvent(tileItem)) {
			m_lastCacheItemVector.push_back(tileItem);
		}
	}
}

bool MoveEvents::onPlayerEquip(Player* player, Item* item, Slots_t slot, bool isCheck)
{
	if (MoveEvent* moveEvent = getEvent(item, MOVE_EVENT_EQUIP, slot)) {
		return moveEvent->fireEquip(player, item, slot, isCheck);
	}
	return true;
}

bool MoveEvents::onPlayerDeEquip(Player* player, Item* item, Slots_t slot, bool isRemoval)
{
	if (MoveEvent* moveEvent = getEvent(item, MOVE_EVENT_DE_EQUIP, slot)) {
		return moveEvent->fireEquip(player, item, slot, isRemoval);
	}
	return true;
}

uint32_t MoveEvents::onItemMove(Creature* actor, Item* item, Tile* tile, bool isAdd)
{
	MoveEvent_t eventType = MOVE_EVENT_REMOVE_ITEM, tileEventType = MOVE_EVENT_REMOVE_TILEITEM;
	if (isAdd) {
		eventType = MOVE_EVENT_ADD_ITEM;
		tileEventType = MOVE_EVENT_ADD_TILEITEM;
	}

	uint32_t ret = 1;
	MoveEvent* moveEvent = getEvent(tile, eventType);
	if (moveEvent) {
		ret &= moveEvent->fireAddRemItem(actor, item, nullptr, tile->getPosition());
	}

	moveEvent = getEvent(item, eventType);
	if (moveEvent) {
		ret &= moveEvent->fireAddRemItem(actor, item, nullptr, tile->getPosition());
	}

	if (!tile) {
		return ret;
	}

	Item* tileItem = nullptr;
	if (m_lastCacheTile == tile) {
		if (m_lastCacheItemVector.empty()) {
			return ret;
		}

		// We cannot use iterators here since the scripts can invalidate the iterator
		for (uint32_t i = 0; i < m_lastCacheItemVector.size(); ++i) {
			if ((tileItem = m_lastCacheItemVector[i]) && tileItem != item
				&& (moveEvent = getEvent(tileItem, tileEventType))) {
				ret &= moveEvent->fireAddRemItem(actor, item, tileItem, tile->getPosition());
			}
		}

		return ret;
	}

	m_lastCacheTile = tile;
	m_lastCacheItemVector.clear();

	// we cannot use iterators here since the scripts can invalidate the iterator
	Thing* thing = nullptr;
	for (int32_t i = tile->__getFirstIndex(), j = tile->__getLastIndex(); i < j; ++i) // already checked the ground
	{
		if (!(thing = tile->__getThing(i)) || !(tileItem = thing->getItem()) || tileItem == item) {
			continue;
		}

		if ((moveEvent = getEvent(tileItem, tileEventType))) {
			m_lastCacheItemVector.push_back(tileItem);
			ret &= moveEvent->fireAddRemItem(actor, item, tileItem, tile->getPosition());
		} else if (hasTileEvent(tileItem)) {
			m_lastCacheItemVector.push_back(tileItem);
		}
	}

	return ret;
}

void MoveEvents::onAddTileItem(const Tile* tile, Item* item)
{
	if (m_lastCacheTile != tile) {
		return;
	}

	auto it = std::find(m_lastCacheItemVector.begin(), m_lastCacheItemVector.end(), item);
	if (it == m_lastCacheItemVector.end() && hasTileEvent(item)) {
		m_lastCacheItemVector.push_back(item);
	}
}

void MoveEvents::onRemoveTileItem(const Tile* tile, Item* item)
{
	if (m_lastCacheTile != tile) {
		return;
	}

	for (uint32_t i = 0; i < m_lastCacheItemVector.size(); ++i) {
		if (m_lastCacheItemVector[i] != item) {
			continue;
		}

		m_lastCacheItemVector[i] = nullptr;
		break;
	}
}

MoveEvent::MoveEvent(LuaInterface* _interface) :
	Event(_interface)
{
	m_stepFunction = nullptr;
	m_moveFunction = nullptr;
	m_equipFunction = nullptr;
	m_slot = SLOTP_WHEREEVER;

	m_eventType = MOVE_EVENT_LAST;
	m_wieldInfo = 0;
	m_reqLevel = 0;
	m_reqMagLevel = 0;
	m_premium = false;
}

std::string MoveEvent::getScriptEventName() const
{
	switch (m_eventType) {
		case MOVE_EVENT_STEP_IN:     return "onStepIn";
		case MOVE_EVENT_STEP_OUT:    return "onStepOut";
		case MOVE_EVENT_EQUIP:       return "onEquip";
		case MOVE_EVENT_DE_EQUIP:    return "onDeEquip";
		case MOVE_EVENT_ADD_ITEM:    return "onAddItem";
		case MOVE_EVENT_REMOVE_ITEM: return "onRemoveItem";

		default:
			std::clog << "[Error - MoveEvent::getScriptEventName] No valid event type." << std::endl;
			return "";
	}
}

bool MoveEvent::configureEvent(xmlNodePtr p)
{
	std::string strValue;
	int32_t intValue;
	if (readXMLString(p, "type", strValue) || readXMLString(p, "event", strValue)) {
		std::string tmpStrValue = otx::util::as_lower_string(strValue);
		if (tmpStrValue == "stepin") {
			m_eventType = MOVE_EVENT_STEP_IN;
		} else if (tmpStrValue == "stepout") {
			m_eventType = MOVE_EVENT_STEP_OUT;
		} else if (tmpStrValue == "equip") {
			m_eventType = MOVE_EVENT_EQUIP;
		} else if (tmpStrValue == "deequip") {
			m_eventType = MOVE_EVENT_DE_EQUIP;
		} else if (tmpStrValue == "additem") {
			m_eventType = MOVE_EVENT_ADD_ITEM;
		} else if (tmpStrValue == "removeitem") {
			m_eventType = MOVE_EVENT_REMOVE_ITEM;
		} else {
			if (tmpStrValue == "function" || tmpStrValue == "buffer" || tmpStrValue == "script") {
				std::clog << "[Error - MoveEvent::configureMoveEvent] No event type found." << std::endl;
			} else {
				std::clog << "[Error - MoveEvent::configureMoveEvent] Unknown event type \"" << strValue << "\"" << std::endl;
			}

			return false;
		}

		if (m_eventType == MOVE_EVENT_EQUIP || m_eventType == MOVE_EVENT_DE_EQUIP) {
			if (readXMLString(p, "slot", strValue)) {
				tmpStrValue = otx::util::as_lower_string(strValue);
				if (tmpStrValue == "head") {
					m_slot = SLOTP_HEAD;
				} else if (tmpStrValue == "necklace") {
					m_slot = SLOTP_NECKLACE;
				} else if (tmpStrValue == "backpack") {
					m_slot = SLOTP_BACKPACK;
				} else if (tmpStrValue == "armor") {
					m_slot = SLOTP_ARMOR;
				} else if (tmpStrValue == "left-hand") {
					m_slot = SLOTP_LEFT;
				} else if (tmpStrValue == "right-hand") {
					m_slot = SLOTP_RIGHT;
				} else if (tmpStrValue == "hands" || tmpStrValue == "two-handed") {
					m_slot = SLOTP_TWO_HAND;
				} else if (tmpStrValue == "hand" || tmpStrValue == "shield") {
					m_slot = SLOTP_RIGHT | SLOTP_LEFT;
				} else if (tmpStrValue == "legs") {
					m_slot = SLOTP_LEGS;
				} else if (tmpStrValue == "feet") {
					m_slot = SLOTP_FEET;
				} else if (tmpStrValue == "ring") {
					m_slot = SLOTP_RING;
				} else if (tmpStrValue == "ammo" || tmpStrValue == "ammunition") {
					m_slot = SLOTP_AMMO;
				} else if (tmpStrValue == "pickupable") {
					m_slot = SLOTP_RIGHT | SLOTP_LEFT | SLOTP_AMMO;
				} else if (tmpStrValue == "wherever" || tmpStrValue == "any") {
					m_slot = SLOTP_WHEREEVER;
				} else {
					std::clog << "[Warning - MoveEvent::configureMoveEvent] Unknown slot type \"" << strValue << "\"" << std::endl;
				}
			}

			m_wieldInfo = 0;
			if (readXMLInteger(p, "lvl", intValue) || readXMLInteger(p, "level", intValue)) {
				m_reqLevel = intValue;
				if (m_reqLevel > 0) {
					m_wieldInfo |= WIELDINFO_LEVEL;
				}
			}

			if (readXMLInteger(p, "maglv", intValue) || readXMLInteger(p, "maglevel", intValue)) {
				m_reqMagLevel = intValue;
				if (m_reqMagLevel > 0) {
					m_wieldInfo |= WIELDINFO_MAGLV;
				}
			}

			if (readXMLString(p, "prem", strValue) || readXMLString(p, "premium", strValue)) {
				m_premium = booleanString(strValue);
				if (m_premium) {
					m_wieldInfo |= WIELDINFO_PREMIUM;
				}
			}

			StringVec vocStringVec;
			std::string error = "";
			for (xmlNodePtr vocationNode = p->children; vocationNode; vocationNode = vocationNode->next) {
				if (!parseVocationNode(vocationNode, m_vocEquipMap, vocStringVec, error)) {
					std::clog << "[Warning - MoveEvent::configureEvent] " << error << std::endl;
				}
			}

			if (!m_vocEquipMap.empty()) {
				m_wieldInfo |= WIELDINFO_VOCREQ;
			}

			m_vocationString = parseVocationString(vocStringVec);
		}
	} else {
		std::clog << "[Error - MoveEvent::configureMoveEvent] No event type found." << std::endl;
		return false;
	}

	return true;
}

bool MoveEvent::loadFunction(const std::string& functionName)
{
	std::string tmpFunctionName = otx::util::as_lower_string(functionName);
	if (tmpFunctionName == "onstepinfield") {
		m_stepFunction = StepInField;
	} else if (tmpFunctionName == "onaddfield") {
		m_moveFunction = AddItemField;
	} else if (tmpFunctionName == "onequipitem") {
		m_equipFunction = EquipItem;
	} else if (tmpFunctionName == "ondeequipitem") {
		m_equipFunction = DeEquipItem;
	} else {
		std::clog << "[Warning - MoveEvent::loadFunction] Function \"" << functionName << "\" does not exist." << std::endl;
		return false;
	}

	m_scripted = false;
	return true;
}

void MoveEvent::setEventType(MoveEvent_t type)
{
	m_eventType = type;
}

uint32_t MoveEvent::StepInField(Creature* creature, Item* item)
{
	if (MagicField* field = item->getMagicField()) {
		field->onStepInField(creature);
		return 1;
	}

	return LUA_ERROR_ITEM_NOT_FOUND;
}

uint32_t MoveEvent::AddItemField(Item* item)
{
	if (MagicField* field = item->getMagicField()) {
		if (Tile* tile = item->getTile()) {
			if (CreatureVector* creatures = tile->getCreatures()) {
				for (CreatureVector::iterator cit = creatures->begin(); cit != creatures->end(); ++cit) {
					field->onStepInField(*cit);
				}
			}
		}

		return 1;
	}

	return LUA_ERROR_ITEM_NOT_FOUND;
}

bool MoveEvent::EquipItem(MoveEvent* moveEvent, Player* player, Item* item, Slots_t slot, bool isCheck)
{
	if (player->isItemAbilityEnabled(slot)) {
		return true;
	}

	if (!player->hasFlag(PlayerFlag_IgnoreEquipCheck) && moveEvent->getWieldInfo() != 0) {
		if (player->getLevel() < static_cast<uint32_t>(moveEvent->getReqLevel()) || player->getMagicLevel() < static_cast<uint32_t>(moveEvent->getReqMagLv())) {
			return false;
		}

		if (moveEvent->isPremium() && !player->isPremium()) {
			return false;
		}

		if (!moveEvent->getVocEquipMap().empty() && moveEvent->getVocEquipMap().find(player->getVocationId()) == moveEvent->getVocEquipMap().end()) {
			return false;
		}
	}

	if (isCheck) {
		return true;
	}

	const ItemType& it = Item::items[item->getID()];
	if (it.transformEquipTo) {
		Item* newItem = g_game.transformItem(item, it.transformEquipTo);
		g_game.startDecay(newItem);
	}

	player->setItemAbility(slot, true);
	if (!it.hasAbilities()) {
		return true;
	}

	if (it.abilities->invisible) {
		Condition* condition = Condition::createCondition(static_cast<ConditionId_t>(slot), CONDITION_INVISIBLE, -1, 0);
		player->addCondition(condition);
	}

	if (it.abilities->manaShield) {
		Condition* condition = Condition::createCondition(static_cast<ConditionId_t>(slot), CONDITION_MANASHIELD, -1, 0);
		player->addCondition(condition);
	}

	if (it.abilities->speed) {
		g_game.changeSpeed(player, it.abilities->speed);
	}

	if (it.abilities->conditionSuppressions) {
		player->setConditionSuppressions(it.abilities->conditionSuppressions, false);
		player->sendIcons();
	}

	if (it.abilities->regeneration) {
		Condition* condition = Condition::createCondition(static_cast<ConditionId_t>(slot), CONDITION_REGENERATION, -1, 0);
		if (it.abilities->healthGain) {
			condition->setParam(CONDITIONPARAM_HEALTHGAIN, it.abilities->healthGain);
		}

		if (it.abilities->healthTicks) {
			condition->setParam(CONDITIONPARAM_HEALTHTICKS, it.abilities->healthTicks);
		}

		if (it.abilities->manaGain) {
			condition->setParam(CONDITIONPARAM_MANAGAIN, it.abilities->manaGain);
		}

		if (it.abilities->manaTicks) {
			condition->setParam(CONDITIONPARAM_MANATICKS, it.abilities->manaTicks);
		}

		player->addCondition(condition);
	}

	bool needUpdateSkills = false;
	for (uint32_t i = SKILL_FIRST; i <= SKILL_LAST; ++i) {
		if (it.abilities->skills[i]) {
			player->setVarSkill(static_cast<Skills_t>(i), it.abilities->skills[i]);
			if (!needUpdateSkills) {
				needUpdateSkills = true;
			}
		}

		if (it.abilities->skillsPercent[i]) {
			player->setVarSkill(static_cast<Skills_t>(i), (player->getSkillLevel(i) * ((it.abilities->skillsPercent[i] - 100) / 100.f)));
			if (!needUpdateSkills) {
				needUpdateSkills = true;
			}
		}
	}

	if (needUpdateSkills) {
		player->sendSkills();
	}

	bool needUpdateStats = false;
	for (uint32_t s = STAT_FIRST; s <= STAT_LAST; ++s) {
		if (it.abilities->stats[s]) {
			player->setVarStats(static_cast<Stats_t>(s), it.abilities->stats[s]);
			if (!needUpdateStats) {
				needUpdateStats = true;
			}
		}

		if (it.abilities->statsPercent[s]) {
			player->setVarStats(static_cast<Stats_t>(s), (player->getDefaultStats(static_cast<Stats_t>(s)) * ((it.abilities->statsPercent[s] - 100) / 100.f)));
			if (!needUpdateStats) {
				needUpdateStats = true;
			}
		}
	}

	if (needUpdateStats) {
		player->sendStats();
	}
	return true;
}

bool MoveEvent::DeEquipItem(MoveEvent*, Player* player, Item* item, Slots_t slot, bool isRemoval)
{
	if (!player->isItemAbilityEnabled(slot)) {
		return true;
	}

	const ItemType& it = Item::items[item->getID()];
	if (isRemoval && it.transformDeEquipTo) {
		g_game.transformItem(item, it.transformDeEquipTo);
		g_game.startDecay(item);
	}

	player->setItemAbility(slot, false);
	if (!it.hasAbilities()) {
		return true;
	}

	if (it.abilities->invisible) {
		player->removeCondition(CONDITION_INVISIBLE, static_cast<ConditionId_t>(slot));
	}

	if (it.abilities->manaShield) {
		player->removeCondition(CONDITION_MANASHIELD, static_cast<ConditionId_t>(slot));
	}

	if (it.abilities->speed) {
		g_game.changeSpeed(player, -it.abilities->speed);
	}

	if (it.abilities->conditionSuppressions) {
		player->setConditionSuppressions(it.abilities->conditionSuppressions, true);
		player->sendIcons();
	}

	if (it.abilities->regeneration) {
		player->removeCondition(CONDITION_REGENERATION, static_cast<ConditionId_t>(slot));
	}

	bool needUpdateSkills = false;
	for (uint32_t i = SKILL_FIRST; i <= SKILL_LAST; ++i) {
		if (it.abilities->skills[i]) {
			needUpdateSkills = true;
			player->setVarSkill(static_cast<Skills_t>(i), -it.abilities->skills[i]);
		}

		if (it.abilities->skillsPercent[i]) {
			needUpdateSkills = true;
			player->setVarSkill(static_cast<Skills_t>(i), -(player->getSkillLevel(i) * ((it.abilities->skillsPercent[i] - 100) / 100.f)));
		}
	}

	if (needUpdateSkills) {
		player->sendSkills();
	}

	bool needUpdateStats = false;
	for (uint32_t s = STAT_FIRST; s <= STAT_LAST; ++s) {
		if (it.abilities->stats[s]) {
			needUpdateStats = true;
			player->setVarStats(static_cast<Stats_t>(s), -it.abilities->stats[s]);
		}

		if (it.abilities->statsPercent[s]) {
			needUpdateStats = true;
			player->setVarStats(static_cast<Stats_t>(s), -(player->getDefaultStats(static_cast<Stats_t>(s)) * ((it.abilities->statsPercent[s] - 100) / 100.f)));
		}
	}

	if (needUpdateStats) {
		player->sendStats();
	}

	return true;
}

uint32_t MoveEvent::fireStepEvent(Creature* actor, Creature* creature, Item* item, const Position& pos, const Position& fromPos, const Position& toPos)
{
	if (isScripted()) {
		return executeStep(actor, creature, item, pos, fromPos, toPos);
	}
	return m_stepFunction(creature, item);
}

uint32_t MoveEvent::executeStep(Creature* actor, Creature* creature, Item* item, const Position& pos, const Position& fromPos, const Position& toPos)
{
	// onStepIn(cid, item, position, lastPosition, fromPosition, toPosition, actor)
	// onStepOut(cid, item, position, lastPosition, fromPosition, toPosition, actor)
	if (!otx::lua::reserveScriptEnv()) {
		std::clog << "[Error - MoveEvent::executeStep] Call stack overflow." << std::endl;
		return 0;
	}

	MoveEventScript::event = this;

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	env.setScriptId(m_scriptId, m_interface);

	lua_State* L = m_interface->getState();
	m_interface->pushFunction(m_scriptId);

	lua_pushnumber(L, env.addThing(creature));
	otx::lua::pushThing(L, item, env.addThing(item));
	otx::lua::pushPosition(L, pos, 0);
	otx::lua::pushPosition(L, creature->getLastPosition(), 0);
	otx::lua::pushPosition(L, fromPos, 0);
	otx::lua::pushPosition(L, toPos, 0);
	lua_pushnumber(L, env.addThing(actor));
	return otx::lua::callFunction(L, 7);
}

bool MoveEvent::fireEquip(Player* player, Item* item, Slots_t slot, bool boolean)
{
	if (isScripted()) {
		return executeEquip(player, item, slot, boolean);
	}

	return m_equipFunction(this, player, item, slot, boolean);
}

bool MoveEvent::executeEquip(Player* player, Item* item, Slots_t slot, bool boolean)
{
	// onEquip(cid, item, slot, boolean)
	// onDeEquip(cid, item, slot, boolean)
	if (!otx::lua::reserveScriptEnv()) {
		std::clog << "[Error - MoveEvent::executeEquip] Call stack overflow." << std::endl;
		return 0;
	}

	MoveEventScript::event = this;

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	env.setScriptId(m_scriptId, m_interface);

	lua_State* L = m_interface->getState();
	m_interface->pushFunction(m_scriptId);

	lua_pushnumber(L, env.addThing(player));
	otx::lua::pushThing(L, item);
	lua_pushnumber(L, slot);
	lua_pushboolean(L, boolean);
	return otx::lua::callFunction(L, 4);
}

uint32_t MoveEvent::fireAddRemItem(Creature* actor, Item* item, Item* tileItem, const Position& pos)
{
	if (isScripted()) {
		return executeAddRemItem(actor, item, tileItem, pos);
	}
	return m_moveFunction(item);
}

uint32_t MoveEvent::executeAddRemItem(Creature* actor, Item* item, Item* tileItem, const Position& pos)
{
	// onAddItem(moveItem, tileItem, position, cid)
	// onRemoveItem(moveItem, tileItem, position, cid)
	if (!otx::lua::reserveScriptEnv()) {
		std::clog << "[Error - MoveEvent::executeAddRemItem] Call stack overflow." << std::endl;
		return 0;
	}

	MoveEventScript::event = this;

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	env.setScriptId(m_scriptId, m_interface);

	lua_State* L = m_interface->getState();
	m_interface->pushFunction(m_scriptId);

	otx::lua::pushThing(L, item);
	otx::lua::pushThing(L, tileItem);
	otx::lua::pushPosition(L, pos);
	lua_pushnumber(L, env.addThing(actor));
	return otx::lua::callFunction(L, 4);
}
