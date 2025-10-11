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

#include "const.h"
#include "creature.h"
#include "lua_definitions.h"

class Npc;
struct NpcType
{
	std::string name, file, nameDescription, script;
	Outfit_t outfit;
};

class Npcs
{
public:
	Npcs() {}
	virtual ~Npcs();
	void reload();

	bool loadFromXml(bool reloading = false);
	bool parseNpcNode(xmlNodePtr node, FileType_t path, bool reloading = false);

	NpcType* getType(const std::string& name) const;
	bool setType(std::string name, NpcType* nType);

private:
	typedef std::map<std::string, NpcType*> DataMap;
	DataMap data;
};

extern Npcs g_npcs;

class NpcScript final : public LuaInterface
{
public:
	NpcScript();
	virtual ~NpcScript() {}

private:
	virtual void registerFunctions();

	static int32_t luaActionFocus(lua_State* L);
	static int32_t luaActionSay(lua_State* L);
	static int32_t luaActionFollow(lua_State* L);

	static int32_t luaGetNpcId(lua_State* L);
	static int32_t luaGetNpcParameter(lua_State* L);

	static int32_t luaOpenShopWindow(lua_State* L);
	static int32_t luaCloseShopWindow(lua_State* L);
	static int32_t luaGetShopOwner(lua_State* L);

private:
	bool initState() final;
	bool closeState() final;
};

class Player;
class NpcEvents
{
public:
	NpcEvents(std::string file, Npc* npc);
	virtual ~NpcEvents() {}

	virtual void onCreatureAppear(const Creature* creature);
	virtual void onCreatureDisappear(const Creature* creature);

	virtual void onCreatureMove(const Creature* creature, const Position& oldPos, const Position& newPos);
	virtual void onCreatureSay(const Creature* creature, MessageClasses, const std::string& text, Position* pos = nullptr);

	virtual void onPlayerTrade(const Player* player, int32_t callback, uint16_t itemid,
		uint8_t count, uint8_t amount, bool ignore, bool inBackpacks);
	virtual void onPlayerEndTrade(const Player* player);
	virtual void onPlayerCloseChannel(const Player* player);

	virtual void onThink();
	bool isLoaded() const { return m_loaded; }

private:
	Npc* m_npc;
	bool m_loaded;
	NpcScript* m_interface;
	int32_t m_onCreatureAppear, m_onCreatureDisappear, m_onCreatureMove, m_onCreatureSay,
		m_onPlayerCloseChannel, m_onPlayerEndTrade, m_onThink;
};

struct Voice
{
	bool randomSpectator;
	MessageClasses type;
	uint32_t interval, margin;
	std::string text;
};

class Npc final : public Creature
{
public:
#if ENABLE_SERVER_DIAGNOSTIC > 0
	static uint32_t npcCount;
#endif
	virtual ~Npc();

	static uint32_t npcAutoID;
	void setID() override
	{
		if (m_id == 0) {
			m_id = npcAutoID++;
		}
	}

	static Npc* createNpc(NpcType* nType);
	static Npc* createNpc(const std::string& name);

	virtual Npc* getNpc() { return this; }
	virtual const Npc* getNpc() const { return this; }

	CreatureType_t getType() const override { return CREATURE_TYPE_NPC; }

	void addList() override;
	void removeList() override;

	virtual bool isPushable() const { return false; }
	virtual bool isAttackable() const { return attackable; }
	virtual bool isWalkable() const { return walkable; }

	virtual bool canSee(const Position& pos) const;
	virtual bool canSeeInvisibility() const { return true; }

	bool isLoaded() { return loaded; }
	bool load();
	void reload();

	void setNpcPath(const std::string& _name, bool fromXmlFile = false);

	virtual const std::string& getName() const { return nType->name; }
	virtual const std::string& getNameDescription() const { return nType->nameDescription; }

	void doSay(const std::string& text, MessageClasses type, Player* player);

	void onPlayerTrade(Player* player, int32_t callback, uint16_t itemId, uint8_t count,
		uint8_t amount, bool ignore = false, bool inBackpacks = false);
	void onPlayerEndTrade(Player* player, int32_t buyCallback,
		int32_t sellCallback);
	void onPlayerCloseChannel(const Player* player);

	void setCreatureFocus(Creature* creature);
	NpcScript* getInterface();

private:
	Npc(NpcType* _nType);
	NpcType* nType;
	bool loaded;

	void reset();
	bool loadFromXml();

	virtual void onCreatureAppear(const Creature* creature);
	virtual void onCreatureDisappear(const Creature* creature, bool isLogout);
	virtual void onCreatureMove(const Creature* creature, const Tile* newTile, const Position& newPos,
		const Tile* oldTile, const Position& oldPos, bool teleport);
	virtual void onCreatureSay(const Creature* creature, MessageClasses type, const std::string& text, Position* pos = nullptr);
	virtual void onThink(uint32_t interval);

	bool isImmune(CombatType_t) const { return true; }
	bool isImmune(ConditionType_t) const { return true; }

	virtual std::string getDescription(int32_t) const { return nType->nameDescription + "."; }
	virtual bool getNextStep(Direction& dir, uint32_t& flags);
	bool getRandomStep(Direction& dir);
	bool canWalkTo(const Position& fromPos, Direction dir);

	void onPlayerLeave(Player* player);

	void addShopPlayer(Player* player);
	void removeShopPlayer(const Player* player);
	void closeAllShopWindows();

	bool floorChange, attackable, walkable, isIdle;
	Direction baseDirection;

	int32_t talkRadius, idleTime, idleInterval, focusCreature;
	uint32_t walkTicks;
	int64_t lastVoice;

	typedef std::map<std::string, std::string> ParametersMap;
	ParametersMap m_parameters;

	typedef std::list<Player*> ShopPlayerList;
	ShopPlayerList shopPlayerList;

	typedef std::list<Voice> VoiceList;
	VoiceList voiceList;

	NpcEvents* m_npcEventHandler;
	static NpcScript* m_interface;

	friend class Npcs;
	friend class NpcScript;
};
