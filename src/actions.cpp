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

#include "actions.h"

#include "beds.h"
#include "configmanager.h"
#include "const.h"
#include "container.h"
#include "game.h"
#include "house.h"
#include "item.h"
#include "player.h"
#include "spells.h"

#include "otx/util.hpp"

Actions g_actions;

void Actions::init()
{
	m_interface = std::make_unique<LuaInterface>("Action Interface");
	m_interface->initState();
	g_lua.addScriptInterface(m_interface.get());
}

void Actions::terminate()
{
	useItemMap.clear();
	uniqueItemMap.clear();
	actionItemMap.clear();

	defaultAction.reset();

	g_lua.removeScriptInterface(m_interface.get());
	m_interface.reset();
}

void Actions::clear()
{
	useItemMap.clear();
	uniqueItemMap.clear();
	actionItemMap.clear();

	defaultAction.reset();

	m_interface->reInitState();
}

Event* Actions::getEvent(const std::string& nodeName)
{
	if (otx::util::as_lower_string(nodeName) == "action") {
		return new Action(getInterface());
	}
	return nullptr;
}

bool Actions::registerEvent(Event* event, xmlNodePtr p)
{
	// event is guaranteed to be action
	Action* action = static_cast<Action*>(event);

	std::string strValue;
	if (readXMLString(p, "default", strValue) && booleanString(strValue)) {
		if (!defaultAction) {
			defaultAction = std::unique_ptr<Action>(action);
		} else {
			std::clog << "[Warning - Actions::registerEvent] You cannot define more than one default action, if you want to do so please define \"override\"." << std::endl;
			delete action;
		}
		return true;
	}

	bool success = true;
	std::string endValue;
	if (readXMLString(p, "itemid", strValue)) {
		auto intVector = parseStringInts(strValue);
		if (intVector.empty()) {
			std::clog << "[Warning - Actions::registerEvent] Invalid itemid - '" << strValue << "'" << std::endl;
			return false;
		}

		for (const int32_t id : intVector) {
			if (!useItemMap.emplace(id, *action).second) {
				std::clog << "[Warning - Actions::registerEvent] Duplicate registered item id: " << id << std::endl;
				success = false;
			}
		}
	} else if (readXMLString(p, "fromid", strValue) && readXMLString(p, "toid", endValue)) {
		auto intVector = vectorAtoi(explodeString(strValue, ";"));
		auto endVector = vectorAtoi(explodeString(endValue, ";"));
		if (!intVector.empty() && intVector.size() == endVector.size()) {
			for (size_t i = 0, size = intVector.size(); i < size; ++i) {
				const int32_t firstId = intVector[i];
				while (intVector[i] <= endVector[i]) {
					if (!useItemMap.emplace(intVector[i], *action).second) {
						std::clog << "[Warning - Actions::registerEvent] Duplicate registered item with id: " << intVector[i] << ", in fromid: " << firstId << " and toid: " << endVector[i] << std::endl;
					}
					++intVector[i];
				}
			}
		} else {
			std::clog << "[Warning - Actions::registerEvent] Malformed entry (from item: \"" << strValue << "\", to item: \"" << endValue << "\")" << std::endl;
		}
	}

	if (readXMLString(p, "uniqueid", strValue)) {
		auto intVector = parseStringInts(strValue);
		if (intVector.empty()) {
			std::clog << "[Warning - Actions::registerEvent] Invalid uniqueid - '" << strValue << "'" << std::endl;
			return false;
		}

		for (const int32_t id : intVector) {
			if (!uniqueItemMap.emplace(id, *action).second) {
				std::clog << "[Warning - Actions::registerEvent] Duplicate registered item uid: " << id << std::endl;
				success = false;
			}
		}
	} else if (readXMLString(p, "fromuid", strValue) && readXMLString(p, "touid", endValue)) {
		auto intVector = vectorAtoi(explodeString(strValue, ";"));
		auto endVector = vectorAtoi(explodeString(endValue, ";"));
		if (!intVector.empty() && intVector.size() == endVector.size()) {
			for (size_t i = 0, size = intVector.size(); i < size; ++i) {
				const int32_t firstId = intVector[i];
				while (intVector[i] <= endVector[i]) {
					if (!uniqueItemMap.emplace(intVector[i], *action).second) {
						std::clog << "[Warning - Actions::registerEvent] Duplicate registered item with uid: " << intVector[i] << ", in fromuid: " << firstId << " and touid: " << endVector[i] << std::endl;
					}
					++intVector[i];
				}
			}
		} else {
			std::clog << "[Warning - Actions::registerEvent] Malformed entry (from unique: \"" << strValue << "\", to unique: \"" << endValue << "\")" << std::endl;
		}
	}

	if (readXMLString(p, "actionid", strValue) || readXMLString(p, "aid", strValue)) {
		auto intVector = parseStringInts(strValue);
		if (intVector.empty()) {
			std::clog << "[Warning - Actions::registerEvent] Invalid actionid - '" << strValue << "'" << std::endl;
			return false;
		}

		for (const int32_t id : intVector) {
			if (!actionItemMap.emplace(id, *action).second) {
				std::clog << "[Warning - Actions::registerEvent] Duplicate registered item aid: " << id << std::endl;
				success = false;
			}
		}
	} else if (readXMLString(p, "fromaid", strValue) && readXMLString(p, "toaid", endValue)) {
		auto intVector = vectorAtoi(explodeString(strValue, ";"));
		auto endVector = vectorAtoi(explodeString(endValue, ";"));
		if (!intVector.empty() && intVector.size() == endVector.size()) {
			for (size_t i = 0, size = intVector.size(); i < size; ++i) {
				const int32_t firstId = intVector[i];
				while (intVector[i] <= endVector[i]) {
					if (!actionItemMap.emplace(intVector[i], *action).second) {
						std::clog << "[Warning - Actions::registerEvent] Duplicate registered item with aid: " << intVector[i] << ", in fromaid: " << firstId << " and toaid: " << endVector[i] << std::endl;
					}
					++intVector[i];
				}
			}
		} else {
			std::clog << "[Warning - Actions::registerEvent] Malformed entry (from action: \"" << strValue << "\", to action: \"" << endValue << "\")" << std::endl;
		}
	}

	if (success) {
		delete action;
	}
	return success;
}

ReturnValue Actions::canUse(const Player* player, const Position& pos)
{
	const Position& playerPos = player->getPosition();
	if (pos.x == 0xFFFF) {
		return RET_NOERROR;
	}

	if (playerPos.z > pos.z) {
		return RET_FIRSTGOUPSTAIRS;
	}else if (playerPos.z < pos.z) {
		return RET_FIRSTGODOWNSTAIRS;
	}

	if (!Position::areInRange<1, 1, 0>(playerPos, pos)) {
		return RET_TOOFARAWAY;
	}

	if (Tile* tile = g_game.getTile(pos)) {
		HouseTile* houseTile = tile->getHouseTile();
		if (houseTile && houseTile->getHouse() && !houseTile->getHouse()->isInvited(player)) {
			return RET_PLAYERISNOTINVITED;
		}
	}
	return RET_NOERROR;
}

ReturnValue Actions::canUseEx(const Player* player, const Position& pos, const Item* item)
{
	Action* action = getAction(item);
	if (!action && defaultAction) {
		action = defaultAction.get();
	}

	if (action) {
		return action->canExecuteAction(player, pos);
	}
	return RET_NOERROR;
}

ReturnValue Actions::canUseFar(const Creature* creature, const Position& toPos, bool checkLineOfSight)
{
	if (toPos.x == 0xFFFF) {
		return RET_NOERROR;
	}

	const Position& creaturePos = creature->getPosition();
	if (creaturePos.z > toPos.z) {
		return RET_FIRSTGOUPSTAIRS;
	} else if (creaturePos.z < toPos.z) {
		return RET_FIRSTGODOWNSTAIRS;
	}

	if (!Position::areInRange<7, 5, 0>(toPos, creaturePos)) {
		return RET_TOOFARAWAY;
	}

	if (checkLineOfSight && !g_game.canThrowObjectTo(creaturePos, toPos)) {
		return RET_CANNOTTHROW;
	}
	return RET_NOERROR;
}

Action* Actions::getAction(const Item* item)
{
	int32_t tmpId = item->getUniqueId();
	if (tmpId != 0) {
		auto it = uniqueItemMap.find(tmpId);
		if (it != uniqueItemMap.end()) {
			return &it->second;
		}
	}

	tmpId = item->getActionId();
	if (tmpId != 0) {
		auto it = actionItemMap.find(tmpId);
		if (it != actionItemMap.end()) {
			return &it->second;
		}
	}

	auto it = useItemMap.find(item->getID());
	if (it != useItemMap.end()) {
		return &it->second;
	}

	if (Action* runeSpell = g_spells.getRuneSpell(item->getID())) {
		return runeSpell;
	}
	return nullptr;
}

bool Actions::executeUse(Action* action, Player* player, Item* item,
	const PositionEx& posEx, uint32_t creatureId)
{
	return action->executeUse(player, item, posEx, posEx, false, creatureId);
}

ReturnValue Actions::internalUseItem(Player* player, const Position& pos, uint8_t index, Item* item, uint32_t creatureId)
{
	if (Door* door = item->getDoor()) {
		if (!door->canUse(player)) {
			return RET_CANNOTUSETHISOBJECT;
		}
	}

	int32_t tmp = 0;
	if (item->getParent()) {
		tmp = item->getParent()->__getIndexOfThing(item);
	}

	PositionEx posEx(pos, tmp);

	Action* action = getAction(item);
	if (!action && defaultAction) {
		action = defaultAction.get();
	}

	if (action && executeUse(action, player, item, posEx, creatureId)) {
		return RET_NOERROR;
	}

	if (BedItem* bed = item->getBed()) {
		if (!bed->canUse(player)) {
			return RET_CANNOTUSETHISOBJECT;
		}

		bed->sleep(player);
		return RET_NOERROR;
	}

	if (Container* container = item->getContainer()) {
		if (container->getCorpseOwner() && !player->canOpenCorpse(container->getCorpseOwner())
			&& g_config.getBool(ConfigManager::CHECK_CORPSE_OWNER)) {
			return RET_YOUARENOTTHEOWNER;
		}

		Container* tmpContainer = nullptr;
		if (Depot* depot = container->getDepot()) {
			if (player->hasFlag(PlayerFlag_CannotPickupItem)) {
				return RET_CANNOTUSETHISOBJECT;
			}

			if (Depot* playerDepot = player->getDepot(depot->getDepotId(), true)) {
				player->useDepot(depot->getDepotId(), true);
				playerDepot->setParent(depot->getParent());
				tmpContainer = playerDepot;
			}
		}

		if (!tmpContainer) {
			tmpContainer = container;
		}

		int32_t oldId = player->getContainerID(tmpContainer);
		if (oldId != -1) {
			player->onCloseContainer(tmpContainer);
			player->closeContainer(oldId);
		} else {
			player->addContainer(index, tmpContainer);
			player->onSendContainer(tmpContainer);
		}

		return RET_NOERROR;
	}

	if (item->isReadable()) {
		if (item->canWriteText()) {
			player->setWriteItem(item, item->getMaxWriteLength());
			player->sendTextWindow(item, item->getMaxWriteLength(), true);
		} else {
			player->setWriteItem(nullptr);
			player->sendTextWindow(item, 0, false);
		}

		return RET_NOERROR;
	}

	const ItemType& it = Item::items[item->getID()];
	if (it.transformUseTo) {
		g_game.transformItem(item, it.transformUseTo);
		g_game.startDecay(item);
		return RET_NOERROR;
	}

	if (item->isPremiumScroll()) {
		std::ostringstream ss;
		ss << " You have received " << it.premiumDays << " premium days.";
		player->sendTextMessage(MSG_INFO_DESCR, ss.str());

		player->addPremiumDays(it.premiumDays);
		g_game.internalRemoveItem(nullptr, item, 1);
		return RET_NOERROR;
	}

	return RET_CANNOTUSETHISOBJECT;
}

bool Actions::useItem(Player* player, const Position& pos, uint8_t index, Item* item)
{
	if (!player->canDoAction()) {
		return false;
	}

	if (player->hasCondition(CONDITION_EXHAUST, EXHAUST_USEITEM)) {
		return false;
	}
	player->setNextActionTask(nullptr);
	player->stopWalk();

	// player->setNextAction(otx::util::mstime() + g_config.getNumber(ConfigManager::ACTIONS_DELAY_INTERVAL) - 10);
	if (Condition* privCondition = Condition::createCondition(CONDITIONID_DEFAULT, CONDITION_EXHAUST, g_config.getNumber(ConfigManager::ACTIONS_DELAY_INTERVAL), 0, false, EXHAUST_USEITEM)) {
		player->addCondition(privCondition);
	}

	ReturnValue ret = internalUseItem(player, pos, index, item, 0);
	if (ret == RET_NOERROR) {
		return true;
	}

	player->sendCancelMessage(ret);
	return false;
}

bool Actions::executeUseEx(Action* action, Player* player, Item* item, const PositionEx& fromPosEx,
	const PositionEx& toPosEx, bool isHotkey, uint32_t creatureId)
{
	return action->executeUse(player, item, fromPosEx, toPosEx, isHotkey, creatureId) || action->hasOwnErrorHandler();
}

ReturnValue Actions::internalUseItemEx(Player* player, const PositionEx& fromPosEx, const PositionEx& toPosEx,
	Item* item, bool isHotkey, uint32_t creatureId)
{
	Action* action = getAction(item);
	if (!action && defaultAction) {
		action = defaultAction.get();
	}

	if (action) {
		ReturnValue ret = action->canExecuteAction(player, toPosEx);
		if (ret != RET_NOERROR) {
			return ret;
		}

		// only continue with next action in the list if the previous returns false
		if (executeUseEx(action, player, item, fromPosEx, toPosEx, isHotkey, creatureId)) {
			return RET_NOERROR;
		}
	}
	return RET_CANNOTUSETHISOBJECT;
}

bool Actions::useItemEx(Player* player, const Position& fromPos, const Position& toPos,
	uint8_t toStackPos, Item* item, bool isHotkey, uint32_t creatureId /* = 0*/)
{
	if (!player->canDoAction()) {
		return false;
	}

	player->setNextActionTask(nullptr);
	player->stopWalk();

	int32_t fromStackPos = 0;
	if (item->getParent()) {
		fromStackPos = item->getParent()->__getIndexOfThing(item);
	}

	PositionEx fromPosEx(fromPos, fromStackPos);
	PositionEx toPosEx(toPos, toStackPos);

	ReturnValue ret = internalUseItemEx(player, fromPosEx, toPosEx, item, isHotkey, creatureId);
	if (ret == RET_NOERROR) {
		return true;
	}

	player->sendCancelMessage(ret);
	return false;
}

bool Action::configureEvent(xmlNodePtr p)
{
	std::string strValue;
	if (readXMLString(p, "allowfaruse", strValue) || readXMLString(p, "allowFarUse", strValue)) {
		setAllowFarUse(booleanString(strValue));
	}

	if (readXMLString(p, "blockwalls", strValue) || readXMLString(p, "blockWalls", strValue)) {
		setCheckLineOfSight(booleanString(strValue));
	}
	return true;
}

ReturnValue Action::canExecuteAction(const Player* player, const Position& pos)
{
	if (player->hasCustomFlag(PlayerCustomFlag_CanUseFar)) {
		return RET_NOERROR;
	}

	if (!getAllowFarUse()) {
		return Actions::canUse(player, pos);
	}
	return Actions::canUseFar(player, pos, getCheckLineOfSight());
}

bool Action::executeUse(Player* player, Item* item, const PositionEx& fromPos, const PositionEx& toPos, bool extendedUse, uint32_t)
{
	// onUse(cid, item, fromPosition, itemEx, toPosition)
	if (!otx::lua::reserveScriptEnv()) {
		std::clog << "[Error - Action::executeUse]: Call stack overflow." << std::endl;
		return false;
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	env.setScriptId(m_scriptId, m_interface);

	lua_State* L = m_interface->getState();
	m_interface->pushFunction(m_scriptId);

	lua_pushnumber(L, env.addThing(player));
	otx::lua::pushThing(L, item, env.addThing(item));
	otx::lua::pushPosition(L, fromPos, fromPos.stackpos);

	Thing* thing = g_game.internalGetThing(player, toPos, toPos.stackpos);
	if (thing && (thing != item || !extendedUse)) {
		otx::lua::pushThing(L, thing, env.addThing(thing));
		otx::lua::pushPosition(L, toPos, toPos.stackpos);
	} else {
		otx::lua::pushThing(L, nullptr, 0);
		otx::lua::pushPosition(L, PositionEx());
	}
	return otx::lua::callFunction(L, 5);
}
