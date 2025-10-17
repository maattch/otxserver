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

#include "creatureevent.h"

#include "monster.h"
#include "player.h"

#include "otx/util.hpp"

CreatureEvents g_creatureEvents;

void CreatureEvents::init()
{
	m_interface = std::make_unique<LuaInterface>("CreatureScript Interface");
	m_interface->initState();
	g_lua.addScriptInterface(m_interface.get());
}

void CreatureEvents::terminate()
{
	m_creatureEvents.clear();

	g_lua.removeScriptInterface(m_interface.get());
	m_interface.reset();
}

void CreatureEvents::clear()
{
	for (auto& it : m_creatureEvents) {
		it.second.clearEvent();
	}
	m_interface->reInitState();
}

EventPtr CreatureEvents::getEvent(const std::string& nodeName)
{
	const std::string tmpNodeName = otx::util::as_lower_string(nodeName);
	if (tmpNodeName == "event" || tmpNodeName == "creaturevent" || tmpNodeName == "creatureevent" || tmpNodeName == "creaturescript") {
		return EventPtr(new CreatureEvent(getInterface()));
	}
	return nullptr;
}

void CreatureEvents::registerEvent(EventPtr event, xmlNodePtr)
{
	// event is guaranteed to be creatureevent
	auto creatureEvent = static_cast<CreatureEvent*>(event.get());
	if (creatureEvent->getEventType() == CREATURE_EVENT_NONE) {
		std::clog << "[Error - CreatureEvents::registerEvent] Trying to register event without a type!" << std::endl;
		return;
	}

	if (CreatureEvent* oldEvent = getEventByName(creatureEvent->getName(), false)) {
		// if there was an event with the same type that is not loaded (happens when realoading), it is reused
		if (!oldEvent->isLoaded() && oldEvent->getEventType() == creatureEvent->getEventType()) {
			oldEvent->copyEvent(creatureEvent);
		} else {
			std::cout << "[Warning - CreatureEvents::registerEvent] Duplicate event with name " << creatureEvent->getName() << std::endl;
		}
		return;
	}

	std::string eventName = creatureEvent->getName();
	m_creatureEvents.emplace(std::move(eventName), std::move(*creatureEvent));
}

CreatureEvent* CreatureEvents::getEventByName(const std::string& name, bool forceLoaded /*= true*/)
{
	auto it = m_creatureEvents.find(name);
	if (it != m_creatureEvents.end()) {
		if (!forceLoaded || it->second.isLoaded()) {
			return &it->second;
		}
	}
	return nullptr;
}

bool CreatureEvents::playerLogin(Player* player)
{
	bool result = true;
	for (auto& it : m_creatureEvents) {
		CreatureEvent& event = it.second;
		if (!event.isLoaded() || event.getEventType() != CREATURE_EVENT_LOGIN) {
			continue;
		}

		if (!event.executeLogin(player)) {
			result = false;
		}
	}
	return result;
}

bool CreatureEvents::playerLogout(Player* player, bool forceLogout)
{
	bool result = true;
	for (auto& it : m_creatureEvents) {
		CreatureEvent& event = it.second;
		if (!event.isLoaded() || event.getEventType() != CREATURE_EVENT_LOGOUT) {
			continue;
		}

		if (!event.executeLogout(player, forceLogout)) {
			result = false;
		}
	}

	if (forceLogout) {
		result = true;
	}
	return result;
}

bool CreatureEvents::executeMoveItems(Creature* actor, Item* item, const Position& frompos, const Position& pos)
{
	bool result = true;
	for (auto& it : m_creatureEvents) {
		CreatureEvent& event = it.second;
		if (!event.isLoaded() || event.getEventType() != CREATURE_EVENT_MOVEITEM) {
			continue;
		}

		if (!event.executeMoveItem(actor, item, frompos, pos)) {
			result = false;
		}
	}
	return result;
}

bool CreatureEvents::monsterSpawn(Monster* monster)
{
	bool result = true;
	for (auto& it : m_creatureEvents) {
		CreatureEvent& event = it.second;
		if (!event.isLoaded() || event.getEventType() != CREATURE_EVENT_SPAWN_GLOBAL) {
			continue;
		}

		if (!event.executeSpawn(monster)) {
			result = false;
		}
	}
	return result;
}

CreatureEventType_t CreatureEvents::getType(const std::string& eventName)
{
	if (eventName == "login") {
		return CREATURE_EVENT_LOGIN;
	} else if (eventName == "logout") {
		return CREATURE_EVENT_LOGOUT;
	} else if (eventName == "channeljoin") {
		return CREATURE_EVENT_CHANNEL_JOIN;
	} else if (eventName == "channelleave") {
		return CREATURE_EVENT_CHANNEL_LEAVE;
	} else if (eventName == "channelrequest") {
		return CREATURE_EVENT_CHANNEL_REQUEST;
	} else if (eventName == "advance") {
		return CREATURE_EVENT_ADVANCE;
	} else if (eventName == "mailsend") {
		return CREATURE_EVENT_MAIL_SEND;
	} else if (eventName == "mailreceive") {
		return CREATURE_EVENT_MAIL_RECEIVE;
	} else if (eventName == "traderequest") {
		return CREATURE_EVENT_TRADE_REQUEST;
	} else if (eventName == "tradeaccept") {
		return CREATURE_EVENT_TRADE_ACCEPT;
	} else if (eventName == "textedit") {
		return CREATURE_EVENT_TEXTEDIT;
	} else if (eventName == "houseedit") {
		return CREATURE_EVENT_HOUSEEDIT;
	} else if (eventName == "reportbug") {
		return CREATURE_EVENT_REPORTBUG;
	} else if (eventName == "reportviolation") {
		return CREATURE_EVENT_REPORTVIOLATION;
	} else if (eventName == "look") {
		return CREATURE_EVENT_LOOK;
	} else if (eventName == "spawn" || eventName == "spawn-single") {
		return CREATURE_EVENT_SPAWN_SINGLE;
	} else if (eventName == "spawnall" || eventName == "spawn-global") {
		return CREATURE_EVENT_SPAWN_GLOBAL;
	} else if (eventName == "think") {
		return CREATURE_EVENT_THINK;
	} else if (eventName == "direction") {
		return CREATURE_EVENT_DIRECTION;
	} else if (eventName == "outfit") {
		return CREATURE_EVENT_OUTFIT;
	} else if (eventName == "statschange") {
		return CREATURE_EVENT_STATSCHANGE;
	} else if (eventName == "areacombat") {
		return CREATURE_EVENT_COMBAT_AREA;
	} else if (eventName == "throw") {
		return CREATURE_EVENT_THROW;
	} else if (eventName == "push") {
		return CREATURE_EVENT_PUSH;
	} else if (eventName == "target") {
		return CREATURE_EVENT_TARGET;
	} else if (eventName == "follow") {
		return CREATURE_EVENT_FOLLOW;
	} else if (eventName == "combat") {
		return CREATURE_EVENT_COMBAT;
	} else if (eventName == "attack") {
		return CREATURE_EVENT_ATTACK;
	} else if (eventName == "cast") {
		return CREATURE_EVENT_CAST;
	} else if (eventName == "kill") {
		return CREATURE_EVENT_KILL;
	} else if (eventName == "death") {
		return CREATURE_EVENT_DEATH;
	} else if (eventName == "preparedeath") {
		return CREATURE_EVENT_PREPAREDEATH;
	} else if (eventName == "extendedopcode") {
		return CREATURE_EVENT_EXTENDED_OPCODE;
	} else if (eventName == "moveitem") {
		return CREATURE_EVENT_MOVEITEM;
	} else if (eventName == "nocountfrag") {
		return CREATURE_EVENT_NOCOUNTFRAG;
	}
	return CREATURE_EVENT_NONE;
}

std::vector<CreatureEvent*> CreatureEvents::getInvalidEvents()
{
	std::vector<CreatureEvent*> invalidEvents;
	for (auto& it : m_creatureEvents) {
		if (!it.second.isScripted()) {
			invalidEvents.push_back(&it.second);
		}
	}
	return invalidEvents;
}

void CreatureEvents::removeInvalidEvents()
{
	for (auto it = m_creatureEvents.begin(); it != m_creatureEvents.end(); ) {
		if (!it->second.isScripted()) {
			it = m_creatureEvents.erase(it);
		} else {
			++it;
		}
	}
}

bool CreatureEvent::configureEvent(xmlNodePtr p)
{
	std::string strValue;
	if (!readXMLString(p, "name", strValue)) {
		std::clog << "[Error - CreatureEvent::configureEvent] No name for creature event." << std::endl;
		return false;
	}

	m_eventName = strValue;
	if (!readXMLString(p, "type", strValue)) {
		std::clog << "[Error - CreatureEvent::configureEvent] No type for creature event." << std::endl;
		return false;
	}

	m_type = g_creatureEvents.getType(otx::util::as_lower_string(strValue));
	if (m_type == CREATURE_EVENT_NONE) {
		std::clog << "[Error - CreatureEvent::configureEvent] No valid type for creature event: " << strValue << '.' << std::endl;
		return false;
	}

	m_loaded = true;
	return true;
}

std::string CreatureEvent::getScriptEventName() const
{
	switch (m_type) {
		case CREATURE_EVENT_LOGIN:           return "onLogin";
		case CREATURE_EVENT_LOGOUT:          return "onLogout";

		case CREATURE_EVENT_SPAWN_SINGLE:
		case CREATURE_EVENT_SPAWN_GLOBAL:    return "onSpawn";

		case CREATURE_EVENT_CHANNEL_JOIN:    return "onChannelJoin";
		case CREATURE_EVENT_CHANNEL_LEAVE:   return "onChannelLeave";
		case CREATURE_EVENT_CHANNEL_REQUEST: return "onChannelRequest";
		case CREATURE_EVENT_THINK:           return "onThink";
		case CREATURE_EVENT_ADVANCE:         return "onAdvance";
		case CREATURE_EVENT_LOOK:            return "onLook";
		case CREATURE_EVENT_DIRECTION:       return "onDirection";
		case CREATURE_EVENT_OUTFIT:          return "onOutfit";
		case CREATURE_EVENT_MAIL_SEND:       return "onMailSend";
		case CREATURE_EVENT_MAIL_RECEIVE:    return "onMailReceive";
		case CREATURE_EVENT_TRADE_REQUEST:   return "onTradeRequest";
		case CREATURE_EVENT_TRADE_ACCEPT:    return "onTradeAccept";
		case CREATURE_EVENT_TEXTEDIT:        return "onTextEdit";
		case CREATURE_EVENT_HOUSEEDIT:       return "onHouseEdit";
		case CREATURE_EVENT_REPORTBUG:       return "onReportBug";
		case CREATURE_EVENT_REPORTVIOLATION: return "onReportViolation";
		case CREATURE_EVENT_STATSCHANGE:     return "onStatsChange";
		case CREATURE_EVENT_COMBAT_AREA:     return "onAreaCombat";
		case CREATURE_EVENT_THROW:           return "onThrow";
		case CREATURE_EVENT_PUSH:            return "onPush";
		case CREATURE_EVENT_TARGET:          return "onTarget";
		case CREATURE_EVENT_FOLLOW:          return "onFollow";
		case CREATURE_EVENT_COMBAT:          return "onCombat";
		case CREATURE_EVENT_ATTACK:          return "onAttack";
		case CREATURE_EVENT_CAST:            return "onCast";
		case CREATURE_EVENT_KILL:            return "onKill";
		case CREATURE_EVENT_DEATH:           return "onDeath";
		case CREATURE_EVENT_PREPAREDEATH:    return "onPrepareDeath";
		case CREATURE_EVENT_EXTENDED_OPCODE: return "onExtendedOpcode";
		case CREATURE_EVENT_MOVEITEM:        return "onMoveItem";
		case CREATURE_EVENT_NOCOUNTFRAG:     return "noCountFragArea";

		case CREATURE_EVENT_NONE:
		default:
			return "";
	}
}

void CreatureEvent::copyEvent(CreatureEvent* creatureEvent)
{
	m_scriptId = creatureEvent->m_scriptId;
	m_interface = creatureEvent->m_interface;
	m_scripted = creatureEvent->m_scripted;
	m_loaded = creatureEvent->m_loaded;
}

void CreatureEvent::clearEvent()
{
	m_scriptId = 0;
	m_interface = nullptr;
	m_scripted = false;
	m_loaded = false;
}

bool CreatureEvent::executeLogin(Player* player)
{
	// onLogin(cid)
	if (!otx::lua::reserveScriptEnv()) {
		std::clog << "[Error - CreatureEvent::executeLogin] Call stack overflow." << std::endl;
		return false;
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	env.setScriptId(m_scriptId, m_interface);

	lua_State* L = m_interface->getState();
	m_interface->pushFunction(m_scriptId);

	lua_pushnumber(L, env.addThing(player));
	return otx::lua::callFunction(L, 1);
}

bool CreatureEvent::executeLogout(Player* player, bool forceLogout)
{
	// onLogout(cid, forceLogout)
	if (!otx::lua::reserveScriptEnv()) {
		std::clog << "[Error - CreatureEvent::executeLogout] Call stack overflow." << std::endl;
		return false;
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	env.setScriptId(m_scriptId, m_interface);

	lua_State* L = m_interface->getState();
	m_interface->pushFunction(m_scriptId);

	lua_pushnumber(L, env.addThing(player));
	lua_pushboolean(L, forceLogout);
	return otx::lua::callFunction(L, 2);
}

void CreatureEvent::executeChannel(Player* player, uint16_t channelId, const UsersMap& usersMap)
{
	// onChannel[Join/Leave](cid, channel, users)
	if (!otx::lua::reserveScriptEnv()) {
		std::clog << "[Error - CreatureEvent::executeChannel] Call stack overflow." << std::endl;
		return;
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	env.setScriptId(m_scriptId, m_interface);

	lua_State* L = m_interface->getState();
	m_interface->pushFunction(m_scriptId);

	lua_pushnumber(L, env.addThing(player));
	lua_pushnumber(L, channelId);
	lua_createtable(L, usersMap.size(), 0);
	int index = 0;
	for (const auto& it : usersMap) {
		lua_pushnumber(L, env.addThing(it.second));
		lua_rawseti(L, -2, ++index);
	}
	otx::lua::callVoidFunction(L, 3);
}

void CreatureEvent::executeAdvance(Player* player, Skills_t skill, uint32_t oldLevel, uint32_t newLevel)
{
	// onAdvance(cid, skill, oldLevel, newLevel)
	if (!otx::lua::reserveScriptEnv()) {
		std::clog << "[Error - CreatureEvent::executeAdvance] Call stack overflow." << std::endl;
		return;
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	env.setScriptId(m_scriptId, m_interface);

	lua_State* L = m_interface->getState();
	m_interface->pushFunction(m_scriptId);

	lua_pushnumber(L, env.addThing(player));
	lua_pushnumber(L, skill);
	lua_pushnumber(L, oldLevel);
	lua_pushnumber(L, newLevel);
	otx::lua::callVoidFunction(L, 4);
}

bool CreatureEvent::executeMail(Player* player, Player* target, Item* item, bool openBox)
{
	// onMail[Send/Receive](cid, target, item, openBox)
	if (!otx::lua::reserveScriptEnv()) {
		std::clog << "[Error - CreatureEvent::executeMail] Call stack overflow." << std::endl;
		return false;
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	env.setScriptId(m_scriptId, m_interface);

	lua_State* L = m_interface->getState();
	m_interface->pushFunction(m_scriptId);

	lua_pushnumber(L, env.addThing(player));
	lua_pushnumber(L, env.addThing(target));
	otx::lua::pushThing(L, item, env.addThing(item));
	lua_pushboolean(L, openBox);
	return otx::lua::callFunction(L, 4);
}

bool CreatureEvent::executeTradeRequest(Player* player, Player* target, Item* item)
{
	// onTradeRequest(cid, target, item)
	if (!otx::lua::reserveScriptEnv()) {
		std::clog << "[Error - CreatureEvent::executeTradeRequest] Call stack overflow." << std::endl;
		return false;
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	env.setScriptId(m_scriptId, m_interface);

	lua_State* L = m_interface->getState();
	m_interface->pushFunction(m_scriptId);

	lua_pushnumber(L, env.addThing(player));
	lua_pushnumber(L, env.addThing(target));
	otx::lua::pushThing(L, item, env.addThing(item));
	return otx::lua::callFunction(L, 3);
}

bool CreatureEvent::executeTradeAccept(Player* player, Player* target, Item* item, Item* targetItem)
{
	// onTradeAccept(cid, target, item, targetItem)
	if (!otx::lua::reserveScriptEnv()) {
		std::clog << "[Error - CreatureEvent::executeTradeAccept] Call stack overflow." << std::endl;
		return false;
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	env.setScriptId(m_scriptId, m_interface);

	lua_State* L = m_interface->getState();
	m_interface->pushFunction(m_scriptId);

	lua_pushnumber(L, env.addThing(player));
	lua_pushnumber(L, env.addThing(target));
	otx::lua::pushThing(L, item);
	otx::lua::pushThing(L, targetItem);
	return otx::lua::callFunction(L, 4);
}

bool CreatureEvent::executeLook(Player* player, Thing* thing, const Position& position, int16_t stackpos, int32_t lookDistance)
{
	// onLook(cid, thing, position, lookDistance)
	if (!otx::lua::reserveScriptEnv()) {
		std::clog << "[Error - CreatureEvent::executeLook] Call stack overflow." << std::endl;
		return false;
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	env.setScriptId(m_scriptId, m_interface);

	lua_State* L = m_interface->getState();
	m_interface->pushFunction(m_scriptId);

	lua_pushnumber(L, env.addThing(player));
	otx::lua::pushThing(L, thing, env.addThing(thing));
	otx::lua::pushPosition(L, position, stackpos);
	lua_pushnumber(L, lookDistance);
	return otx::lua::callFunction(L, 4);
}

bool CreatureEvent::executeSpawn(Monster* monster)
{
	// onSpawn(cid)
	if (!otx::lua::reserveScriptEnv()) {
		std::clog << "[Error - CreatureEvent::executeSpawn] Call stack overflow." << std::endl;
		return false;
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	env.setScriptId(m_scriptId, m_interface);

	lua_State* L = m_interface->getState();
	m_interface->pushFunction(m_scriptId);

	lua_pushnumber(L, env.addThing(monster));
	return otx::lua::callFunction(L, 1);
}

bool CreatureEvent::executeDirection(Creature* creature, Direction old, Direction current)
{
	// onDirection(cid, old, current)
	if (!otx::lua::reserveScriptEnv()) {
		std::clog << "[Error - CreatureEvent::executeDirection] Call stack overflow." << std::endl;
		return false;
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	env.setScriptId(m_scriptId, m_interface);

	lua_State* L = m_interface->getState();
	m_interface->pushFunction(m_scriptId);

	lua_pushnumber(L, env.addThing(creature));
	lua_pushnumber(L, old);
	lua_pushnumber(L, current);
	return otx::lua::callFunction(L, 3);
}

bool CreatureEvent::executeOutfit(Creature* creature, const Outfit_t& old, const Outfit_t& current)
{
	// onOutfit(cid, old, current)
	if (!otx::lua::reserveScriptEnv()) {
		std::clog << "[Error - CreatureEvent::executeOutfit] Call stack overflow." << std::endl;
		return false;
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	env.setScriptId(m_scriptId, m_interface);

	lua_State* L = m_interface->getState();
	m_interface->pushFunction(m_scriptId);

	lua_pushnumber(L, env.addThing(creature));
	otx::lua::pushOutfit(L, old);
	otx::lua::pushOutfit(L, current);
	return otx::lua::callFunction(L, 3);
}

void CreatureEvent::executeThink(Creature* creature, uint32_t interval)
{
	// onThink(cid, interval)
	if (!otx::lua::reserveScriptEnv()) {
		std::clog << "[Error - CreatureEvent::executeThink] Call stack overflow." << std::endl;
		return;
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	env.setScriptId(m_scriptId, m_interface);

	lua_State* L = m_interface->getState();
	m_interface->pushFunction(m_scriptId);

	lua_pushnumber(L, env.addThing(creature));
	lua_pushnumber(L, interval);
	otx::lua::callVoidFunction(L, 2);
}

bool CreatureEvent::executeStatsChange(Creature* creature, Creature* attacker, StatsChange_t type, CombatType_t combat, int32_t value)
{
	// onStatsChange(cid, attacker, type, combat, value)
	if (!otx::lua::reserveScriptEnv()) {
		std::clog << "[Error - CreatureEvent::executeStatsChange] Call stack overflow." << std::endl;
		return false;
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	env.setScriptId(m_scriptId, m_interface);

	lua_State* L = m_interface->getState();
	m_interface->pushFunction(m_scriptId);

	lua_pushnumber(L, env.addThing(creature));
	lua_pushnumber(L, env.addThing(attacker));
	lua_pushnumber(L, type);
	lua_pushnumber(L, combat);
	lua_pushnumber(L, value);
	return otx::lua::callFunction(L, 5);
}

bool CreatureEvent::executeCombatArea(Creature* creature, Tile* tile, bool aggressive)
{
	// onAreaCombat(cid, ground, position, aggressive)
	if (!otx::lua::reserveScriptEnv()) {
		std::clog << "[Error - CreatureEvent::executeCombatArea] Call stack overflow." << std::endl;
		return false;
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	env.setScriptId(m_scriptId, m_interface);

	lua_State* L = m_interface->getState();
	m_interface->pushFunction(m_scriptId);

	lua_pushnumber(L, env.addThing(creature));
	otx::lua::pushThing(L, tile->ground);
	otx::lua::pushPosition(L, tile->getPosition());
	lua_pushboolean(L, aggressive);
	return otx::lua::callFunction(L, 4);
}

bool CreatureEvent::executeCombat(Creature* creature, Creature* target, bool aggressive)
{
	// onCombat(cid, target, aggressive)
	if (!otx::lua::reserveScriptEnv()) {
		std::clog << "[Error - CreatureEvent::executeCombat] Call stack overflow." << std::endl;
		return false;
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	env.setScriptId(m_scriptId, m_interface);

	lua_State* L = m_interface->getState();
	m_interface->pushFunction(m_scriptId);

	lua_pushnumber(L, env.addThing(creature));
	lua_pushnumber(L, env.addThing(target));
	lua_pushboolean(L, aggressive);
	return otx::lua::callFunction(L, 3);
}

bool CreatureEvent::executeCast(Creature* creature, Creature* target /* = nullptr*/)
{
	// onCast(cid[, target])
	if (!otx::lua::reserveScriptEnv()) {
		std::clog << "[Error - CreatureEvent::executeCast] Call stack overflow." << std::endl;
		return false;
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	env.setScriptId(m_scriptId, m_interface);

	lua_State* L = m_interface->getState();
	m_interface->pushFunction(m_scriptId);

	lua_pushnumber(L, env.addThing(creature));
	lua_pushnumber(L, env.addThing(target));
	return otx::lua::callFunction(L, 2);
}

bool CreatureEvent::executeKill(Creature* creature, Creature* target, const DeathEntry& entry)
{
	// onKill(cid, target, damage, flags)
	if (!otx::lua::reserveScriptEnv()) {
		std::clog << "[Error - CreatureEvent::executeKill] Call stack overflow." << std::endl;
		return false;
	}

	uint32_t flags = 0;
	if (entry.isLast()) {
		flags |= 1;
	}

	if (entry.isJustify()) {
		flags |= 2;
	}

	if (entry.isUnjustified()) {
		flags |= 4;
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	env.setScriptId(m_scriptId, m_interface);

	lua_State* L = m_interface->getState();
	m_interface->pushFunction(m_scriptId);

	lua_pushnumber(L, env.addThing(creature));
	lua_pushnumber(L, env.addThing(target));
	lua_pushnumber(L, entry.getDamage());
	lua_pushnumber(L, flags);
	lua_pushnumber(L, entry.getWar().war);
	return otx::lua::callFunction(L, 5);
}

bool CreatureEvent::executeDeath(Creature* creature, Item* corpse, const DeathList& deathList)
{
	// onDeath(cid, corpse, deathList)
	if (!otx::lua::reserveScriptEnv()) {
		std::clog << "[Error - CreatureEvent::executeDeath] Call stack overflow." << std::endl;
		return false;
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	env.setScriptId(m_scriptId, m_interface);

	lua_State* L = m_interface->getState();
	m_interface->pushFunction(m_scriptId);

	lua_pushnumber(L, env.addThing(creature));
	otx::lua::pushThing(L, corpse);
	lua_createtable(L, deathList.size(), 0);
	int index = 0;
	for (const auto& it : deathList) {
		if (it.isCreatureKill()) {
			lua_pushnumber(L, env.addThing(it.getKillerCreature()));
		} else {
			otx::lua::pushString(L, it.getKillerName());
		}
		lua_rawseti(L, -2, ++index);
	}
	return otx::lua::callFunction(L, 3);
}

bool CreatureEvent::executePrepareDeath(Creature* creature, const DeathList& deathList)
{
	// onPrepareDeath(cid, deathList)
	if (!otx::lua::reserveScriptEnv()) {
		std::clog << "[Error - CreatureEvent::executePrepareDeath] Call stack overflow." << std::endl;
		return false;
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	env.setScriptId(m_scriptId, m_interface);

	lua_State* L = m_interface->getState();
	m_interface->pushFunction(m_scriptId);

	lua_pushnumber(L, env.addThing(creature));
	lua_createtable(L, deathList.size(), 0);
	int index = 0;
	for (const auto& it : deathList) {
		if (it.isCreatureKill()) {
			lua_pushnumber(L, env.addThing(it.getKillerCreature()));
		} else {
			otx::lua::pushString(L, it.getKillerName());
		}
		lua_rawseti(L, -2, ++index);
	}
	return otx::lua::callFunction(L, 2);
}

bool CreatureEvent::executeTextEdit(Player* player, Item* item, const std::string& newText)
{
	// onTextEdit(cid, item, newText)
	if (!otx::lua::reserveScriptEnv()) {
		std::clog << "[Error - CreatureEvent::executeTextEdit] Call stack overflow." << std::endl;
		return false;
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	env.setScriptId(m_scriptId, m_interface);

	lua_State* L = m_interface->getState();
	m_interface->pushFunction(m_scriptId);

	lua_pushnumber(L, env.addThing(player));
	otx::lua::pushThing(L, item, env.addThing(item));
	otx::lua::pushString(L, newText);
	return otx::lua::callFunction(L, 3);
}

bool CreatureEvent::executeHouseEdit(Player* player, uint32_t houseId, uint32_t listId, const std::string& text)
{
	// onHouseEdit(cid, houseId, listId, text)
	if (!otx::lua::reserveScriptEnv()) {
		std::clog << "[Error - CreatureEvent::executeHouseEdit] Call stack overflow." << std::endl;
		return false;
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	env.setScriptId(m_scriptId, m_interface);

	lua_State* L = m_interface->getState();
	m_interface->pushFunction(m_scriptId);

	lua_pushnumber(L, env.addThing(player));
	lua_pushnumber(L, houseId);
	lua_pushnumber(L, listId);
	otx::lua::pushString(L, text);
	return otx::lua::callFunction(L, 4);
}

void CreatureEvent::executeReportBug(Player* player, const std::string& comment)
{
	// onReportBug(cid, comment)
	if (!otx::lua::reserveScriptEnv()) {
		std::clog << "[Error - CreatureEvent::executeReportBug] Call stack overflow." << std::endl;
		return;
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	env.setScriptId(m_scriptId, m_interface);

	lua_State* L = m_interface->getState();
	m_interface->pushFunction(m_scriptId);

	lua_pushnumber(L, env.addThing(player));
	otx::lua::pushString(L, comment);
	otx::lua::callVoidFunction(L, 2);
}

void CreatureEvent::executeReportViolation(Player* player, ReportType_t type, uint8_t reason,
	const std::string& name, const std::string& comment, const std::string& translation, uint32_t statementId)
{
	// onReportViolation(cid, type, reason, name, comment, translation, statementId)
	if (!otx::lua::reserveScriptEnv()) {
		std::clog << "[Error - CreatureEvent::executeReportViolation] Call stack overflow." << std::endl;
		return;
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	env.setScriptId(m_scriptId, m_interface);

	lua_State* L = m_interface->getState();
	m_interface->pushFunction(m_scriptId);

	lua_pushnumber(L, env.addThing(player));
	lua_pushnumber(L, type);
	lua_pushnumber(L, reason);
	otx::lua::pushString(L, name);
	otx::lua::pushString(L, comment);
	otx::lua::pushString(L, translation);
	lua_pushnumber(L, statementId);
	otx::lua::callVoidFunction(L, 7);
}

bool CreatureEvent::executeChannelRequest(Player* player, const std::string& channel, bool isPrivate, bool custom)
{
	// onChannelRequest(cid, channel, custom)
	if (!otx::lua::reserveScriptEnv()) {
		std::clog << "[Error - CreatureEvent::executeChannelRequest] Call stack overflow." << std::endl;
		return false;
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	env.setScriptId(m_scriptId, m_interface);

	lua_State* L = m_interface->getState();
	m_interface->pushFunction(m_scriptId);

	lua_pushnumber(L, env.addThing(player));
	if (!isPrivate) {
		lua_pushnumber(L, atoi(channel.c_str()));
	} else {
		otx::lua::pushString(L, channel);
	}

	lua_pushboolean(L, custom);
	return otx::lua::callFunction(L, 3);
}

bool CreatureEvent::executePush(Player* player, Creature* target, Tile* tile)
{
	// onPush(cid, target, ground, position)
	if (!otx::lua::reserveScriptEnv()) {
		std::clog << "[Error - CreatureEvent::executePush] Call stack overflow." << std::endl;
		return false;
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	env.setScriptId(m_scriptId, m_interface);

	lua_State* L = m_interface->getState();
	m_interface->pushFunction(m_scriptId);

	lua_pushnumber(L, env.addThing(player));
	lua_pushnumber(L, env.addThing(target));
	otx::lua::pushThing(L, tile->ground, env.addThing(tile->ground));
	otx::lua::pushPosition(L, tile->getPosition(), 0);
	return otx::lua::callFunction(L, 4);
}

bool CreatureEvent::executeThrow(Player* player, Item* item, const Position& fromPosition, const Position& toPosition)
{
	// onThrow(cid, item, fromPosition, toPosition)
	if (!otx::lua::reserveScriptEnv()) {
		std::clog << "[Error - CreatureEvent::executeThrow] Call stack overflow." << std::endl;
		return false;
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	env.setScriptId(m_scriptId, m_interface);

	lua_State* L = m_interface->getState();
	m_interface->pushFunction(m_scriptId);

	otx::lua::pushThingId(L, player);
	otx::lua::pushThing(L, item);
	otx::lua::pushPosition(L, fromPosition);
	otx::lua::pushPosition(L, toPosition);
	return otx::lua::callFunction(L, 4);
}

bool CreatureEvent::executeAction(Creature* creature, Creature* target)
{
	// on[Target/Follow/Attack](cid, target)
	if (!otx::lua::reserveScriptEnv()) {
		std::clog << "[Error - CreatureEvent::executeAction] Call stack overflow." << std::endl;
		return false;
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	env.setScriptId(m_scriptId, m_interface);

	lua_State* L = m_interface->getState();
	m_interface->pushFunction(m_scriptId);

	lua_pushnumber(L, env.addThing(creature));
	lua_pushnumber(L, env.addThing(target));
	return otx::lua::callFunction(L, 2);
}

void CreatureEvent::executeExtendedOpcode(Creature* creature, uint8_t opcode, const std::string& buffer)
{
	// onExtendedOpcode(cid, opcode, buffer)
	if (!otx::lua::reserveScriptEnv()) {
		std::clog << "[Error - CreatureEvent::executeExtendedOpcode] Call stack overflow." << std::endl;
		return;
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	env.setScriptId(m_scriptId, m_interface);

	lua_State* L = m_interface->getState();
	m_interface->pushFunction(m_scriptId);

	lua_pushnumber(L, env.addThing(creature));
	lua_pushnumber(L, opcode);
	lua_pushlstring(L, buffer.c_str(), buffer.length());
	otx::lua::callVoidFunction(L, 3);
}

bool CreatureEvent::executeMoveItem(Creature* actor, Item* item, const Position& frompos, const Position& pos)
{
	UNUSED(actor);

	// onMoveItem(moveItem, frompos, position, cid)
	if (!otx::lua::reserveScriptEnv()) {
		std::clog << "[Error - CreatureEvent::executeMoveItem] Call stack overflow." << std::endl;
		return false;
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	env.setScriptId(m_scriptId, m_interface);

	lua_State* L = m_interface->getState();
	m_interface->pushFunction(m_scriptId);

	otx::lua::pushThing(L, item);
	otx::lua::pushPosition(L, frompos);
	otx::lua::pushPosition(L, pos);
	otx::lua::pushThingId(L, item);
	return otx::lua::callFunction(L, 4);
}

bool CreatureEvent::executeNoCountFragArea(Creature* creature, Creature* target)
{
	// noCountFragArea(cid, target)
	if (!otx::lua::reserveScriptEnv()) {
		std::clog << "[Error - CreatureEvent::executeNoCountFragArea] Call stack overflow." << std::endl;
		return false;
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	env.setScriptId(m_scriptId, m_interface);

	lua_State* L = m_interface->getState();
	m_interface->pushFunction(m_scriptId);

	lua_pushnumber(L, env.addThing(creature));
	lua_pushnumber(L, env.addThing(target));
	return otx::lua::callFunction(L, 2);
}
