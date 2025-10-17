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
#include "itemloader.h"
#include "position.h"

class Condition;

enum ItemTypes_t
{
	ITEM_TYPE_NONE = 0,
	ITEM_TYPE_DEPOT,
	ITEM_TYPE_MAILBOX,
	ITEM_TYPE_TRASHHOLDER,
	ITEM_TYPE_CONTAINER,
	ITEM_TYPE_DOOR,
	ITEM_TYPE_MAGICFIELD,
	ITEM_TYPE_TELEPORT,
	ITEM_TYPE_BED,
	ITEM_TYPE_KEY,
	ITEM_TYPE_RUNE,
	ITEM_TYPE_LAST
};

enum FloorChange_t
{
	CHANGE_PRE_FIRST = 0,
	CHANGE_DOWN = CHANGE_PRE_FIRST,
	CHANGE_FIRST = 1,
	CHANGE_NORTH = CHANGE_FIRST,
	CHANGE_EAST = 2,
	CHANGE_SOUTH = 3,
	CHANGE_WEST = 4,
	CHANGE_FIRST_EX = 5,
	CHANGE_NORTH_EX = CHANGE_FIRST_EX,
	CHANGE_EAST_EX = 6,
	CHANGE_SOUTH_EX = 7,
	CHANGE_WEST_EX = 8,
	CHANGE_NONE = 9,
	CHANGE_PRE_LAST = 8,
	CHANGE_LAST = CHANGE_NONE
};

struct Abilities
{
	Abilities()
	{
		memset(skills, 0, sizeof(skills));
		memset(skillsPercent, 0, sizeof(skillsPercent));
		memset(stats, 0, sizeof(stats));
		memset(statsPercent, 0, sizeof(statsPercent));

		memset(absorb, 0, sizeof(absorb));
		memset(fieldAbsorb, 0, sizeof(fieldAbsorb));
		memset(increment, 0, sizeof(increment));
		memset(reflect[REFLECT_PERCENT], 0, sizeof(reflect[REFLECT_PERCENT]));
		memset(reflect[REFLECT_CHANCE], 0, sizeof(reflect[REFLECT_CHANCE]));

		elementType = COMBAT_NONE;
		manaShield = invisible = regeneration = preventLoss = preventDrop = false;
		speed = healthGain = healthTicks = manaGain = manaTicks = elementDamage = conditionSuppressions = 0;
	};

	bool manaShield, invisible, regeneration, preventLoss, preventDrop;
	CombatType_t elementType;

	int16_t elementDamage;
	int16_t absorb[COMBATINDEX_LAST + 1];
	int16_t increment[INCREMENT_LAST + 1];
	int16_t reflect[REFLECT_LAST + 1][COMBATINDEX_LAST + 1];
	int16_t fieldAbsorb[COMBATINDEX_LAST + 1];

	int32_t skills[SKILL_LAST + 1], skillsPercent[SKILL_LAST + 1], stats[STAT_LAST + 1], statsPercent[STAT_LAST + 1],
		speed, healthGain, healthTicks, manaGain, manaTicks, conditionSuppressions;
};

class ItemType final
{
public:
	ItemType() = default;

	// non-copyable
	ItemType(const ItemType&) = delete;
	ItemType& operator=(const ItemType&) = delete;

	// moveable
	ItemType(ItemType&&) = default;
	ItemType& operator=(ItemType&&) = default;

	Abilities& getAbilities()
	{
		if (!abilities) {
			abilities.reset(new Abilities);
		}
		return *abilities;
	}

	bool isGroundTile() const { return (group == ITEM_GROUP_GROUND); }
	bool isContainer() const { return (group == ITEM_GROUP_CONTAINER); }
	bool isSplash() const { return (group == ITEM_GROUP_SPLASH); }
	bool isFluidContainer() const { return (group == ITEM_GROUP_FLUID); }

	bool isDoor() const { return (type == ITEM_TYPE_DOOR); }
	bool isMagicField() const { return (type == ITEM_TYPE_MAGICFIELD); }
	bool isTeleport() const { return (type == ITEM_TYPE_TELEPORT); }
	bool isKey() const { return (type == ITEM_TYPE_KEY); }
	bool isDepot() const { return (type == ITEM_TYPE_DEPOT); }
	bool isMailbox() const { return (type == ITEM_TYPE_MAILBOX); }
	bool isTrashHolder() const { return (type == ITEM_TYPE_TRASHHOLDER); }
	bool isRune() const { return (type == ITEM_TYPE_RUNE); }
	bool isBed() const { return (type == ITEM_TYPE_BED); }

	bool hasSubType() const { return (isFluidContainer() || isSplash() || stackable || charges); }
	bool hasAbilities() const { return abilities != nullptr; }

	std::string name;
	std::string pluralName;
	std::string article;
	std::string description;
	std::string text;
	std::string writer;
	std::string runeSpellName;
	std::string vocationString;

	std::unique_ptr<Condition> condition;
	std::unique_ptr<Abilities> abilities;

	uint32_t shootRange = 1;
	uint32_t charges = 0;
	uint32_t decayTime = 0;
	uint32_t attackSpeed = 0;
	uint32_t wieldInfo = 0;
	uint32_t minReqLevel = 0;
	uint32_t minReqMagicLevel = 0;
	uint32_t worth = 0;
	uint32_t levelDoor = 0;
	uint32_t date = 0;
	uint32_t runeLevel = 0;
	uint32_t runeMagLevel = 0;

	int32_t attack = 0;
	int32_t extraAttack = 0;
	int32_t defense = 0;
	int32_t extraDefense = 0;
	int32_t armor = 0;
	int32_t breakChance = -1;
	int32_t hitChance = -1;
	int32_t maxHitChance = -1;
	int32_t lightLevel = 0;
	int32_t lightColor = 0;
	int32_t decayTo = -1;
	int32_t rotateTo = 0;
	int32_t alwaysOnTopOrder = 0;
	int32_t extraAttackChance = 0;
	int32_t extraDefenseChance = 0;
	int32_t attackSpeedChance = 0;

	uint16_t id = 0;
	uint16_t clientId = 100;
	uint16_t maxItems = 8;
	uint16_t slotPosition = SLOTP_HAND;
	uint16_t wieldPosition = SLOT_HAND;
	uint16_t speed = 0;
	uint16_t transformUseTo = 0;
	uint16_t transformEquipTo = 0;
	uint16_t transformDeEquipTo = 0;
	uint16_t maxTextLength = 0;
	uint16_t writeOnceItemId = 0;
	uint16_t wareId = 0;
	uint16_t premiumDays = 0;
	uint16_t transformBed[PLAYERSEX_MALE + 1] = {};

	itemgroup_t group = ITEM_GROUP_NONE;
	ItemTypes_t type = ITEM_TYPE_NONE;
	float weight = 0.f;

	MagicEffect_t magicEffect = MAGIC_EFFECT_NONE;
	FluidTypes_t fluidSource = FLUID_NONE;
	WeaponType_t weaponType = WEAPON_NONE;
	Direction bedPartnerDir = NORTH;
	AmmoAction_t ammoAction = AMMOACTION_NONE;
	CombatType_t combatType = COMBAT_NONE;
	RaceType_t corpseType = RACE_NONE;
	ShootEffect_t shootType = SHOOT_EFFECT_NONE;
	Ammo_t ammoType = AMMO_NONE;

	bool loaded = false;
	bool stopTime = false;
	bool showCount = true;
	bool stackable = false;
	bool showDuration = false;
	bool showCharges = false;
	bool showAttributes = false;
	bool dualWield = false;
	bool allowDistRead = false;
	bool canReadText = false;
	bool canWriteText = false;
	bool forceSerialize = false;
	bool isVertical = false;
	bool isHorizontal = false;
	bool isHangable = false;
	bool usable = false;
	bool movable = true;
	bool pickupable = false;
	bool rotable = false;
	bool replacable = true;
	bool lookThrough = false;
	bool walkStack = true;
	bool hasHeight = false;
	bool blockSolid = false;
	bool blockPickupable = false;
	bool blockProjectile = false;
	bool blockPathFind = false;
	bool allowPickupable = false;
	bool alwaysOnTop = false;
	bool isAnimation = false;
	bool specialDoor = false;
	bool closingDoor = false;
	bool cache = false;
	bool floorChange[CHANGE_LAST] = {};
};

typedef std::map<int32_t, int32_t> IntegerMap;

class Items final
{
public:
	Items();

	// non-copyable
	Items(const Items&) = delete;
	Items& operator=(const Items&) = delete;

	bool loadFromOtb(const std::string& file);
	bool loadFromXml();
	void parseItemNode(xmlNodePtr itemNode, uint16_t id);

	bool reload();
	void clear();

	ItemType& getItemType(uint16_t id);
	const ItemType& getItemType(uint16_t id) const;
	const ItemType& operator[](uint16_t id) const { return getItemType(id); }

	uint16_t getItemIdByName(const std::string& name);
	const ItemType& getItemIdByClientId(uint16_t spriteId) const;

	uint32_t size() const noexcept { return items.size(); }
	const IntegerMap& getMoneyMap() const { return moneyMap; }

	static uint32_t dwMajorVersion;
	static uint32_t dwMinorVersion;
	static uint32_t dwBuildNumber;

private:
	std::unordered_map<std::string, uint16_t> nameToItems;
	std::vector<ItemType> items;
	std::vector<uint16_t> clientIdToServerId;
	IntegerMap moneyMap;
};
