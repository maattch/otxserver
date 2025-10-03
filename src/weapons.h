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
#include "luascript.h"
#include "player.h"

class Weapon;
class WeaponMelee;
class WeaponDistance;
class WeaponWand;

using WeaponPtr = std::unique_ptr<Weapon>;

class Weapons final : public BaseEvents
{
public:
	Weapons();

	void loadDefaults();
	const Weapon* getWeapon(const Item* item) const;

	static int32_t getMaxMeleeDamage(int32_t attackSkill, int32_t attackValue);
	static int32_t getMaxWeaponDamage(int32_t level, int32_t attackSkill, int32_t attackValue, float attackFactor);

protected:
	std::string getScriptBaseName() const override { return "weapons"; }
	void clear() override;

	Event* getEvent(const std::string& nodeName) override;
	bool registerEvent(Event* event, xmlNodePtr p, bool override) override;

	LuaInterface& getInterface() override { return m_interface; }

	LuaInterface m_interface;

	std::map<uint16_t, WeaponPtr> weapons;
};

class Weapon : public Event
{
public:
	Weapon(LuaInterface* _interface);
	virtual ~Weapon() {}

	static bool useFist(Player* player, Creature* target);

	virtual bool loadFunction(const std::string& functionName);
	virtual bool configureEvent(xmlNodePtr p);
	virtual bool configureWeapon(const ItemType& it);

	virtual int32_t playerWeaponCheck(Player* player, Creature* target) const;

	uint16_t getID() const { return id; }
	virtual bool interruptSwing() const { return !swing; }
	CombatParams getCombatParam() const { return params; }

	virtual bool useWeapon(Player* player, Item* item, Creature* target) const;
	virtual int32_t getWeaponDamage(const Player* player, const Creature* target, const Item* item, bool& isCritical, bool maxDamage = false) const = 0;
	virtual int32_t getWeaponElementDamage(const Player*, const Item*, bool = false) const { return 0; }

	uint32_t getReqLevel() const { return level; }
	uint32_t getReqMagLv() const { return magLevel; }
	bool hasExhaustion() const { return exhaustion != 0; }
	bool isPremium() const { return premium; }
	bool isWieldedUnproperly() const { return wieldUnproperly; }

protected:
	virtual std::string getScriptEventName() const { return "onUseWeapon"; }
	virtual std::string getScriptEventParams() const { return "cid, var"; }

	bool executeUseWeapon(Player* player, const LuaVariant& var) const;

	bool internalUseWeapon(Player* player, Item* item, Creature* target, int32_t damageModifier) const;
	bool internalUseWeapon(Player* player, Item* item, Tile* tile) const;

	virtual void onUsedWeapon(Player* player, Item* item, Tile* destTile) const;
	virtual void onUsedAmmo(Player* player, Item* item, Tile* destTile) const;
	virtual bool getSkillType(const Player*, const Item*, skills_t&, uint64_t&) const { return false; }

	int32_t getManaCost(const Player* player) const;

	uint16_t id;
	uint32_t exhaustion;
	bool enabled, premium, wieldUnproperly, swing;
	int32_t level, magLevel, mana, manaPercent, soul;

	AmmoAction_t ammoAction;
	CombatParams params;

private:
	VocationMap vocWeaponMap;
};

class WeaponMelee : public Weapon
{
public:
	WeaponMelee(LuaInterface* _interface);
	virtual ~WeaponMelee() {}

	virtual bool useWeapon(Player* player, Item* item, Creature* target) const;
	virtual int32_t getWeaponDamage(const Player* player, const Creature* target, const Item* item, bool& isCritical, bool maxDamage = false) const;
	virtual int32_t getWeaponElementDamage(const Player* player, const Item* item, bool maxDamage = false) const;

protected:
	virtual bool getSkillType(const Player* player, const Item* item, skills_t& skill, uint64_t& skillPoint) const;
};

class WeaponDistance : public Weapon
{
public:
	WeaponDistance(LuaInterface* _interface);
	virtual ~WeaponDistance() {}

	virtual bool configureWeapon(const ItemType& it);

	virtual int32_t playerWeaponCheck(Player* player, Creature* target) const;

	virtual bool useWeapon(Player* player, Item* item, Creature* target) const;
	virtual int32_t getWeaponDamage(const Player* player, const Creature* target, const Item* item, bool& isCritical, bool maxDamage = false) const;

protected:
	virtual void onUsedAmmo(Player* player, Item* item, Tile* destTile) const;
	virtual bool getSkillType(const Player* player, const Item* item, skills_t& skill, uint64_t& skillPoint) const;

	int32_t hitChance, maxHitChance, breakChance, attack;
};

class WeaponWand : public Weapon
{
public:
	WeaponWand(LuaInterface* _interface);
	virtual ~WeaponWand() {}

	virtual bool configureEvent(xmlNodePtr p);
	virtual bool configureWeapon(const ItemType& it);

	virtual int32_t getWeaponDamage(const Player* player, const Creature* target, const Item* item, bool& isCritical, bool maxDamage = false) const;

protected:
	virtual bool getSkillType(const Player*, const Item*, skills_t&, uint64_t&) const { return false; }

	int32_t minChange, maxChange;
};
