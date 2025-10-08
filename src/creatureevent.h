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
#include "enums.h"
#include "tile.h"

enum CreatureEventType_t : uint8_t
{
	CREATURE_EVENT_NONE,
	CREATURE_EVENT_LOGIN,
	CREATURE_EVENT_LOGOUT,
	CREATURE_EVENT_SPAWN_SINGLE,
	CREATURE_EVENT_SPAWN_GLOBAL,
	CREATURE_EVENT_CHANNEL_JOIN,
	CREATURE_EVENT_CHANNEL_LEAVE,
	CREATURE_EVENT_CHANNEL_REQUEST,
	CREATURE_EVENT_ADVANCE,
	CREATURE_EVENT_LOOK,
	CREATURE_EVENT_DIRECTION,
	CREATURE_EVENT_OUTFIT,
	CREATURE_EVENT_MAIL_SEND,
	CREATURE_EVENT_MAIL_RECEIVE,
	CREATURE_EVENT_TRADE_REQUEST,
	CREATURE_EVENT_TRADE_ACCEPT,
	CREATURE_EVENT_TEXTEDIT,
	CREATURE_EVENT_HOUSEEDIT,
	CREATURE_EVENT_REPORTBUG,
	CREATURE_EVENT_REPORTVIOLATION,
	CREATURE_EVENT_THINK,
	CREATURE_EVENT_STATSCHANGE,
	CREATURE_EVENT_COMBAT_AREA,
	CREATURE_EVENT_THROW,
	CREATURE_EVENT_PUSH,
	CREATURE_EVENT_TARGET,
	CREATURE_EVENT_FOLLOW,
	CREATURE_EVENT_COMBAT,
	CREATURE_EVENT_ATTACK,
	CREATURE_EVENT_CAST,
	CREATURE_EVENT_KILL,
	CREATURE_EVENT_DEATH,
	CREATURE_EVENT_PREPAREDEATH,
	CREATURE_EVENT_EXTENDED_OPCODE, // otclient additional network opcodes
	CREATURE_EVENT_MOVEITEM,
	CREATURE_EVENT_NOCOUNTFRAG
};

enum StatsChange_t : uint8_t
{
	STATSCHANGE_HEALTHGAIN,
	STATSCHANGE_HEALTHLOSS,
	STATSCHANGE_MANAGAIN,
	STATSCHANGE_MANALOSS
};

class Monster;
class CreatureEvent;
using CreatureEventPtr = std::unique_ptr<CreatureEvent>;

struct DeathEntry;
typedef std::vector<DeathEntry> DeathList;

typedef std::map<uint32_t, Player*> UsersMap;

class CreatureEvents final : public BaseEvents
{
public:
	CreatureEvents() = default;

	void init();
	void terminate();

	bool playerLogin(Player* player);
	bool playerLogout(Player* player, bool forceLogout);
	bool monsterSpawn(Monster* monster);
	bool executeMoveItems(Creature* actor, Item* item, const Position& frompos, const Position& pos);

	CreatureEvent* getEventByName(const std::string& name, bool forceLoaded = true);
	CreatureEventType_t getType(const std::string& type);

private:
	std::string getScriptBaseName() const override { return "creaturescripts"; }
	void clear() override;

	EventPtr getEvent(const std::string& nodeName) override;
	void registerEvent(EventPtr event, xmlNodePtr p) override;

	LuaInterface* getInterface() override { return m_interface.get(); }

	LuaInterfacePtr m_interface;

	std::map<std::string, CreatureEvent> m_creatureEvents;
};

class CreatureEvent final : public Event
{
public:
	CreatureEvent(LuaInterface* luaInterface) : Event(luaInterface) {}

	bool configureEvent(xmlNodePtr p) override;

	const std::string& getName() const { return m_eventName; }
	CreatureEventType_t getEventType() const { return m_type; }
	bool isLoaded() const { return m_loaded; }

	void copyEvent(CreatureEvent* creatureEvent);
	void clearEvent();

	bool executeLogin(Player* player);
	bool executeLogout(Player* player, bool forceLogout);
	bool executeSpawn(Monster* monster);
	void executeChannel(Player* player, uint16_t channelId, const UsersMap& usersMap);
	bool executeChannelRequest(Player* player, const std::string& channel, bool isPrivate, bool custom);
	void executeAdvance(Player* player, skills_t skill, uint32_t oldLevel, uint32_t newLevel);
	bool executeLook(Player* player, Thing* thing, const Position& position, int16_t stackpos, int32_t lookDistance);
	bool executeMail(Player* player, Player* target, Item* item, bool openBox);
	bool executeTradeRequest(Player* player, Player* target, Item* item);
	bool executeTradeAccept(Player* player, Player* target, Item* item, Item* targetItem);
	bool executeTextEdit(Player* player, Item* item, const std::string& newText);
	bool executeHouseEdit(Player* player, uint32_t houseId, uint32_t listId, const std::string& text);
	void executeReportBug(Player* player, const std::string& comment);
	void executeReportViolation(Player* player, ReportType_t type, uint8_t reason, const std::string& name, const std::string& comment, const std::string& translation, uint32_t statementId);
	void executeThink(Creature* creature, uint32_t interval);
	bool executeDirection(Creature* creature, Direction old, Direction current);
	bool executeOutfit(Creature* creature, const Outfit_t& old, const Outfit_t& current);
	bool executeStatsChange(Creature* creature, Creature* attacker, StatsChange_t type, CombatType_t combat, int32_t value);
	bool executeCombatArea(Creature* creature, Tile* tile, bool isAggressive);
	bool executePush(Player* player, Creature* target, Tile* tile);
	bool executeThrow(Player* player, Item* item, const Position& fromPosition, const Position& toPosition);
	bool executeCombat(Creature* creature, Creature* target, bool aggressive);
	bool executeAction(Creature* creature, Creature* target);
	bool executeCast(Creature* creature, Creature* target = nullptr);
	bool executeKill(Creature* creature, Creature* target, const DeathEntry& entry);
	bool executeDeath(Creature* creature, Item* corpse, const DeathList& deathList);
	bool executePrepareDeath(Creature* creature, const DeathList& deathList);
	void executeExtendedOpcode(Creature* creature, uint8_t opcode, const std::string& buffer);
	bool executeMoveItem(Creature* actor, Item* item, const Position& frompos, const Position& pos);
	bool executeNoCountFragArea(Creature* creature, Creature* target); // by feetads

private:
	std::string getScriptEventName() const override;

	std::string m_eventName;
	CreatureEventType_t m_type = CREATURE_EVENT_NONE;
	bool m_loaded = false;
};

extern CreatureEvents g_creatureEvents;
