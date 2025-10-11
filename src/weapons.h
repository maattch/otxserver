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
#include "combat.h"
#include "const.h"
#include "item.h"
#include "lua_definitions.h"
#include "player.h"

class Weapon;
class WeaponMelee;
class WeaponDistance;
class WeaponWand;

using WeaponPtr = std::unique_ptr<Weapon>;

class Weapons final : public BaseEvents
{
public:
	Weapons() = default;

	void init();
	void terminate();

	void loadDefaults();
	const Weapon* getWeapon(const Item* item) const;

	static int32_t getMaxMeleeDamage(int32_t attackSkill, int32_t attackValue);
	static int32_t getMaxWeaponDamage(int32_t level, int32_t attackSkill, int32_t attackValue, float attackFactor);

private:
	std::string getScriptBaseName() const override { return "weapons"; }
	void clear() override;

	EventPtr getEvent(const std::string& nodeName) override;
	void registerEvent(EventPtr event, xmlNodePtr p) override;

	LuaInterface* getInterface() override { return m_interface.get(); }

	std::map<uint16_t, WeaponPtr> weapons;
	LuaInterfacePtr m_interface;
};

extern Weapons g_weapons;

class Weapon : public Event
{
public:
	Weapon(LuaInterface* luaInterface);
	virtual ~Weapon() = default;

	static bool useFist(Player* player, Creature* target);

	bool loadFunction(const std::string& functionName) override final;
	bool configureEvent(xmlNodePtr p) override;

	virtual bool configureWeapon(const ItemType& it);

	virtual int32_t playerWeaponCheck(Player* player, Creature* target) const;

	uint16_t getID() const { return id; }
	const CombatParams& getCombatParam() const { return params; }

	bool interruptSwing() const { return !swing; }

	virtual bool useWeapon(Player* player, Item* item, Creature* target) const;
	virtual int32_t getWeaponDamage(const Player* player, const Creature* target, const Item* item, bool& isCritical, bool maxDamage = false) const = 0;
	virtual int32_t getWeaponElementDamage(const Player*, const Item*, bool = false) const { return 0; }

	uint32_t getReqLevel() const { return level; }
	uint32_t getReqMagLv() const { return magLevel; }
	bool hasExhaustion() const { return exhaustion != 0; }
	bool isPremium() const { return premium; }
	bool isWieldedUnproperly() const { return wieldUnproperly; }

protected:
	bool executeUseWeapon(Player* player, const LuaVariant& var) const;

	bool internalUseWeapon(Player* player, Item* item, Creature* target, int32_t damageModifier) const;
	bool internalUseWeapon(Player* player, Item* item, Tile* tile) const;

	void onUsedWeapon(Player* player, Item* item, Tile* destTile) const;
	virtual void onUsedAmmo(Player* player, Item* item, Tile* destTile) const;
	virtual bool getSkillType(const Player*, const Item*, skills_t&, uint64_t&) const { return false; }

	int32_t getManaCost(const Player* player) const;

	CombatParams params;
	uint32_t exhaustion = 0;
	int32_t level = 0;
	int32_t magLevel = 0;
	int32_t mana = 0;
	int32_t manaPercent = 0;
	int32_t soul = 0;
	uint16_t id = 0;
	AmmoAction_t ammoAction = AMMOACTION_NONE;
	bool enabled = true;
	bool premium = false;
	bool wieldUnproperly = false;
	bool swing = true;

private:
	std::string getScriptEventName() const override final { return "onUseWeapon"; }

	VocationMap vocWeaponMap;
};

class WeaponMelee final : public Weapon
{
public:
	WeaponMelee(LuaInterface* luaInterface);

	bool useWeapon(Player* player, Item* item, Creature* target) const override;
	int32_t getWeaponDamage(const Player* player, const Creature* target, const Item* item, bool& isCritical, bool maxDamage = false) const override;
	int32_t getWeaponElementDamage(const Player* player, const Item* item, bool maxDamage = false) const override;

private:
	bool getSkillType(const Player* player, const Item* item, skills_t& skill, uint64_t& skillPoint) const override;
};

class WeaponDistance final : public Weapon
{
public:
	WeaponDistance(LuaInterface* luaInterface);

	bool configureWeapon(const ItemType& it) override;

	int32_t playerWeaponCheck(Player* player, Creature* target) const override;

	bool useWeapon(Player* player, Item* item, Creature* target) const override;
	int32_t getWeaponDamage(const Player* player, const Creature* target, const Item* item, bool& isCritical, bool maxDamage = false) const override;

private:
	void onUsedAmmo(Player* player, Item* item, Tile* destTile) const override;
	bool getSkillType(const Player* player, const Item* item, skills_t& skill, uint64_t& skillPoint) const override;

	int32_t hitChance = -1;
	int32_t maxHitChance = 0;
	int32_t breakChance = 0;
	int32_t attack = 0;
};

class WeaponWand final : public Weapon
{
public:
	WeaponWand(LuaInterface* luaInterface);

	bool configureEvent(xmlNodePtr p) override;
	bool configureWeapon(const ItemType& it) override;

	int32_t getWeaponDamage(const Player* player, const Creature* target, const Item* item, bool& isCritical, bool maxDamage = false) const override;

private:
	int32_t minChange = 0;
	int32_t maxChange = 0;
};
