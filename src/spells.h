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

#include "actions.h"
#include "baseevents.h"
#include "lua_definitions.h"
#include "player.h"
#include "talkaction.h"

class InstantSpell;
class ConjureSpell;
class RuneSpell;
class Spell;

using InstantSpellFuncPtr = bool(*)(const InstantSpell* spell, Creature* creature, const std::string& param);
using ConjureSpellFuncPtr = bool(*)(const ConjureSpell* spell, Creature* creature);
using RuneSpellFuncPtr = bool(*)(const RuneSpell* spell, Creature* creature, const Position& posTo);

class Spells final : public BaseEvents
{
public:
	Spells() = default;

	void init();
	void terminate();

	Spell* getSpellByName(const std::string& name);

	RuneSpell* getRuneSpell(uint32_t id);
	RuneSpell* getRuneSpellByName(const std::string& name);

	InstantSpell* getInstantSpell(const std::string& words);
	InstantSpell* getInstantSpellByName(const std::string& name);
	InstantSpell* getInstantSpellByIndex(const Player* player, uint32_t index);

	uint32_t getInstantSpellCount(const Player* player);
	ReturnValue onPlayerSay(Player* player, const std::string& words);

	static Position getCasterPosition(Creature* creature, Direction dir);

	std::string getScriptBaseName() const override { return "spells"; }

private:
	void clear() override;

	EventPtr getEvent(const std::string& nodeName) override;
	void registerEvent(EventPtr event, xmlNodePtr p) override;

	LuaInterface* getInterface() override { return m_interface.get(); }

	LuaInterfacePtr m_interface;

	std::map<uint32_t, RuneSpell> runes;
	std::map<std::string, InstantSpell> instants;
	std::map<std::string, InstantSpell*> instantsWithParam;

	friend class CombatSpell;
};

extern Spells g_spells;

class BaseSpell
{
public:
	BaseSpell() = default;
	virtual ~BaseSpell() = default;

	virtual bool castSpell(Creature* creature);
	virtual bool castSpell(Creature* creature, Creature* target);
};

class CombatSpell : public Event, public BaseSpell
{
public:
	CombatSpell(Combat* combat, bool needTarget, bool needDirection);
	virtual ~CombatSpell();

	bool castSpell(Creature* creature) override;
	bool castSpell(Creature* creature, Creature* target) override;
	bool configureEvent(xmlNodePtr) override { return true; }

	// scripting
	bool executeCastSpell(Creature* creature, const LuaVariant& var);

	bool loadScriptCombat();
	Combat* getCombat() { return combat; }

private:
	std::string getScriptEventName() const override { return "onCastSpell"; }

	Combat* combat;
	bool needTarget;
	bool needDirection;
};

class Spell : public BaseSpell
{
public:
	Spell() = default;
	virtual ~Spell() = default;

	bool configureSpell(xmlNodePtr xmlspell);
	const std::string& getName() const { return name; }

	void postSpell(Player* player) const;
	void postSpell(Player* player, uint32_t manaCost, uint32_t soulCost) const;

	int32_t getManaCost(const Player* player) const;
	int32_t getSoulCost() const { return soul; }
	uint32_t getLevel() const { return level; }
	int32_t getMagicLevel() const { return magLevel; }
	int32_t getMana() const { return mana; }
	int32_t getManaPercent() const { return manaPercent; }
	uint32_t getExhaustion() const { return exhaustion; }
	const auto& getGroupExhaustions() const { return groupExhaustions; }

	bool isEnabled() const { return enabled; }
	bool isPremium() const { return premium; }

	bool isLearnable() const { return learnable; }

	virtual bool isInstant() const = 0;

	static ReturnValue CreateIllusion(Creature* creature, const Outfit_t& outfit, int32_t time);
	static ReturnValue CreateIllusion(Creature* creature, const std::string& name, int32_t time);
	static ReturnValue CreateIllusion(Creature* creature, uint32_t itemId, int32_t time);

protected:
	bool checkSpell(Player* player) const;
	bool checkInstantSpell(Player* player, Creature* creature);
	bool checkInstantSpell(Player* player, const Position& toPos);
	bool checkRuneSpell(Player* player, const Position& toPos);

	StringVec vocStringVec;
	VocationMap vocSpellMap;
	int32_t range = -1;
	uint32_t level = 0;
	uint32_t magLevel = 0;
	bool isAggressive = true;
	bool needTarget = false;
	bool selfTarget = false;

private:
	std::string name;
	std::map<SpellGroup_t, uint32_t> groupExhaustions;
	int32_t mana = 0;
	int32_t manaPercent = 0;
	int32_t soul = 0;
	int32_t skills[SKILL_LAST + 1] = { 10, 10, 10, 10, 10, 10, 10 };
	uint32_t exhaustion = 1000;
	Exhaust_t exhaustedGroup = EXHAUST_SPELLGROUP_NONE;
	bool needWeapon = false;
	bool blockingSolid = false;
	bool blockingCreature = false;
	bool premium = false;
	bool learnable = false;
	bool enabled = true;
};

class InstantSpell : public TalkAction, public Spell
{
public:
	InstantSpell(LuaInterface* luaInterface) : TalkAction(luaInterface) {}

	bool configureEvent(xmlNodePtr p) override;
	bool loadFunction(const std::string& functionName) override;

	virtual bool castInstant(Player* player, const std::string& param);

	bool castSpell(Creature* creature) override;
	bool castSpell(Creature* creature, Creature* target) override;

	// scripting
	bool executeCastSpell(Creature* creature, const LuaVariant& var);

	bool isInstant() const override final { return true; }
	bool getHasParam() const { return hasParam; }
	bool canCast(const Player* player) const;
	bool canThrowSpell(const Creature* creature, const Creature* target) const;

private:
	std::string getScriptEventName() const override final { return "onCastSpell"; }

	static bool SearchPlayer(const InstantSpell* spell, Creature* creature, const std::string& param);
	static bool SummonMonster(const InstantSpell* spell, Creature* creature, const std::string& param);
	static bool Levitate(const InstantSpell* spell, Creature* creature, const std::string& param);
	static bool Illusion(const InstantSpell* spell, Creature* creature, const std::string& param);

	bool internalCastSpell(Creature* creature, const LuaVariant& var);

	InstantSpellFuncPtr function = nullptr;
	uint8_t limitRange = 0;
	bool needDirection = false;
	bool hasParam = false;
	bool checkLineOfSight = true;
	bool casterTargetOrDirection = false;
};

class ConjureSpell final : public InstantSpell
{
public:
	ConjureSpell(LuaInterface* luaInterface);

	bool configureEvent(xmlNodePtr p) override;
	bool loadFunction(const std::string& functionName) override;

	bool castInstant(Player* player, const std::string& param) override;

	bool castSpell(Creature*) override { return false; }
	bool castSpell(Creature*, Creature*) override { return false; }

	uint32_t getConjureId() const { return conjureId; }
	uint32_t getConjureCount() const { return conjureCount; }
	uint32_t getReagentId() const { return conjureReagentId; }

private:
	bool internalCastSpell(Creature* creature, const LuaVariant& var);

	static ReturnValue internalConjureItem(Player* player, uint32_t conjureId, uint32_t conjureCount, bool transform = false, uint32_t reagentId = 0);
	static bool ConjureItem(const ConjureSpell* spell, Creature* creature);

	ConjureSpellFuncPtr function = nullptr;
	uint32_t conjureId = 0;
	uint32_t conjureCount = 1;
	uint32_t conjureReagentId = 0;
};

class RuneSpell final : public Action, public Spell
{
public:
	RuneSpell(LuaInterface* luaInterface);

	bool configureEvent(xmlNodePtr p) override;
	bool loadFunction(const std::string& functionName) override;

	ReturnValue canExecuteAction(const Player* player, const Position& toPos) override;
	bool hasOwnErrorHandler() override { return true; }

	bool executeUse(Player* player, Item* item, const PositionEx& posFrom, const PositionEx& posTo, bool extendedUse, uint32_t creatureId) override;

	bool castSpell(Creature* creature) override;
	bool castSpell(Creature* creature, Creature* target) override;

	uint32_t getRuneItemId() const { return runeId; }

	// scripting
	bool executeCastSpell(Creature* creature, const LuaVariant& var);
	bool isInstant() const override { return false; }

private:
	std::string getScriptEventName() const override { return "onCastSpell"; }

	bool internalCastSpell(Creature* creature, const LuaVariant& var);

	static bool Illusion(const RuneSpell* spell, Creature* creature, const Position& posTo);
	static bool Convince(const RuneSpell* spell, Creature* creature, const Position& posTo);
	static bool Soulfire(const RuneSpell* spell, Creature* creature, const Position& posTo);

	RuneSpellFuncPtr function = nullptr;
	uint32_t runeId = 0;
	bool hasCharges = true;
};
