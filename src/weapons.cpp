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

#include "weapons.h"

#include "configmanager.h"
#include "game.h"
#include "tools.h"

#include "otx/util.hpp"

Weapons g_weapons;

void Weapons::init()
{
	m_interface = std::make_unique<LuaInterface>("Weapons Interface");
	m_interface->initState();
	g_lua.addScriptInterface(m_interface.get());
}

void Weapons::terminate()
{
	weapons.clear();

	g_lua.removeScriptInterface(m_interface.get());
	m_interface.reset();
}

const Weapon* Weapons::getWeapon(const Item* item) const
{
	if (!item) {
		return nullptr;
	}

	auto it = weapons.find(item->getID());
	if (it != weapons.end()) {
		return it->second.get();
	}
	return nullptr;
}

void Weapons::clear()
{
	weapons.clear();
	m_interface->reInitState();
}

void Weapons::loadDefaults()
{
	for (uint16_t i = 0; i <= Item::items.size(); ++i) {
		const ItemType& it = Item::items.getItemType(i);
		if (it.id == 0 || weapons.find(it.id) != weapons.end()) {
			continue;
		}

		switch (it.weaponType) {
			case WEAPON_AXE:
			case WEAPON_SWORD:
			case WEAPON_CLUB:
			case WEAPON_FIST: {
				auto weapon = std::make_unique<WeaponMelee>(getInterface());
				weapon->configureWeapon(it);
				weapons.emplace(i, std::move(weapon));
				break;
			}

			case WEAPON_AMMO:
			case WEAPON_DIST: {
				if (it.weaponType == WEAPON_DIST && it.ammoType != AMMO_NONE) {
					continue;
				}

				auto weapon = std::make_unique<WeaponDistance>(getInterface());
				weapon->configureWeapon(it);
				weapons.emplace(i, std::move(weapon));
				break;
			}

			case WEAPON_NONE:
			default:
				break;
		}
	}
}

EventPtr Weapons::getEvent(const std::string& nodeName)
{
	const std::string tmpNodeName = otx::util::as_lower_string(nodeName);
	if (tmpNodeName == "melee") {
		return EventPtr(new WeaponMelee(getInterface()));
	} else if (tmpNodeName == "distance" || tmpNodeName == "ammunition" || tmpNodeName == "ammo") {
		return EventPtr(new WeaponDistance(getInterface()));
	} else if (tmpNodeName == "wand" || tmpNodeName == "rod") {
		return EventPtr(new WeaponWand(getInterface()));
	}
	return nullptr;
}

void Weapons::registerEvent(EventPtr event, xmlNodePtr)
{
	// it is guaranteed to be weapon
	auto weapon = static_cast<Weapon*>(event.release());

	const uint16_t weaponID = weapon->getID();
	if (!weapons.emplace(weaponID, WeaponPtr(weapon)).second) {
		std::clog << "[Warning - Weapons::registerEvent] Duplicate registered item with id: " << weaponID << std::endl;
	}
}

int32_t Weapons::getMaxMeleeDamage(int32_t attackSkill, int32_t attackValue)
{
	return std::ceil((attackSkill * (attackValue * 0.05)) + (attackValue * 0.5));
}

int32_t Weapons::getMaxWeaponDamage(int32_t level, int32_t attackSkill, int32_t attackValue, float attackFactor)
{
	return std::ceil((2 * (attackValue * (attackSkill + 5.8) / 25 + (level - 1) / 10.)) / attackFactor);
}

Weapon::Weapon(LuaInterface* luaInterface) :
	Event(luaInterface)
{
	params.blockedByArmor = true;
	params.blockedByShield = true;
	params.combatType = COMBAT_PHYSICALDAMAGE;
}

bool Weapon::configureEvent(xmlNodePtr p)
{
	int32_t intValue, wieldInfo = 0;
	std::string strValue;
	if (!readXMLInteger(p, "id", intValue)) {
		std::clog << "Error: [Weapon::configureEvent] Weapon without id." << std::endl;
		return false;
	}

	id = intValue;
	if (readXMLInteger(p, "lv", intValue) || readXMLInteger(p, "lvl", intValue) || readXMLInteger(p, "level", intValue)) {
		level = intValue;
		if (level > 0) {
			wieldInfo |= WIELDINFO_LEVEL;
		}
	}

	if (readXMLInteger(p, "maglv", intValue) || readXMLInteger(p, "maglvl", intValue) || readXMLInteger(p, "maglevel", intValue)) {
		magLevel = intValue;
		if (magLevel > 0) {
			wieldInfo |= WIELDINFO_MAGLV;
		}
	}

	if (readXMLInteger(p, "mana", intValue)) {
		mana = intValue;
	}

	if (readXMLInteger(p, "manapercent", intValue)) {
		manaPercent = intValue;
	}

	if (readXMLInteger(p, "soul", intValue)) {
		soul = intValue;
	}

	if (readXMLInteger(p, "exhaust", intValue) || readXMLInteger(p, "exhaustion", intValue)) {
		exhaustion = intValue;
	}

	if (readXMLString(p, "prem", strValue) || readXMLString(p, "premium", strValue)) {
		premium = booleanString(strValue);
		if (premium) {
			wieldInfo |= WIELDINFO_PREMIUM;
		}
	}

	if (readXMLString(p, "enabled", strValue)) {
		enabled = booleanString(strValue);
	}

	if (readXMLString(p, "unproperly", strValue)) {
		wieldUnproperly = booleanString(strValue);
	}

	if (readXMLString(p, "swing", strValue)) {
		swing = booleanString(strValue);
	}

	if (readXMLString(p, "type", strValue)) {
		params.combatType = getCombatType(strValue);
	}

	std::string error;
	std::vector<std::string> vocStringVec;

	xmlNodePtr vocationNode = p->children;
	while (vocationNode) {
		if (!parseVocationNode(vocationNode, vocWeaponMap, vocStringVec, error)) {
			std::clog << "[Warning - Weapon::configureEvent] " << error << std::endl;
		}

		vocationNode = vocationNode->next;
	}

	if (!vocWeaponMap.empty()) {
		wieldInfo |= WIELDINFO_VOCREQ;
	}

	if (wieldInfo) {
		ItemType& it = Item::items.getItemType(id);
		it.minReqMagicLevel = getReqMagLv();
		it.minReqLevel = getReqLevel();

		it.wieldInfo = wieldInfo;
		it.vocationString = parseVocationString(vocStringVec);
	}

	configureWeapon(Item::items[getID()]);
	return true;
}

bool Weapon::loadFunction(const std::string& functionName)
{
	std::string tmpFunctionName = otx::util::as_lower_string(functionName);
	if (tmpFunctionName == "internalloadweapon" || tmpFunctionName == "default") {
		m_scripted = false;
		configureWeapon(Item::items[getID()]);
		return true;
	}
	return false;
}

void Weapon::configureWeapon(const ItemType& it)
{
	id = it.id;
}

int32_t Weapon::playerWeaponCheck(Player* player, Creature* target) const
{
	const Position& playerPos = player->getPosition();
	const Position& targetPos = target->getPosition();
	if (playerPos.z != targetPos.z) {
		return 0;
	}

	const ItemType& it = Item::items[id];
	int32_t range = it.shootRange;
	if (it.weaponType == WEAPON_AMMO) {
		if (Item* item = player->getWeapon(true)) {
			range = item->getShootRange();
		}
	}

	if (std::max(std::abs(playerPos.x - targetPos.x), std::abs(playerPos.y - targetPos.y)) > range) {
		return 0;
	}

	if (player->hasFlag(PlayerFlag_IgnoreEquipCheck)) {
		return 100;
	}

	if (!enabled) {
		return 0;
	}

	if (player->getMana() < getManaCost(player)) {
		return 0;
	}

	if (player->getSoul() < soul) {
		return 0;
	}

	if (isPremium() && !player->isPremium()) {
		return 0;
	}

	if (!vocWeaponMap.empty() && vocWeaponMap.find(player->getVocationId()) == vocWeaponMap.end()) {
		return 0;
	}

	int32_t modifier = 100;
	if (player->getLevel() < getReqLevel()) {
		if (!isWieldedUnproperly()) {
			return 0;
		}

		double penalty = (getReqLevel() - player->getLevel()) * 0.02;
		if (penalty > 0.5) {
			penalty = 0.5;
		}

		modifier -= (modifier * penalty);
	}

	if (player->getMagicLevel() < getReqMagLv()) {
		if (!isWieldedUnproperly()) {
			return 0;
		}

		double penalty = (getReqMagLv() - player->getMagicLevel()) * 0.02;
		if (penalty > 0.5) {
			penalty = 0.5;
		}

		modifier -= (modifier * penalty);
	}

	return modifier;
}

bool Weapon::useWeapon(Player* player, Item* item, Creature* target) const
{
	int32_t modifier = playerWeaponCheck(player, target);
	if (!modifier) {
		return false;
	}
	return internalUseWeapon(player, item, target, modifier);
}

bool Weapon::useFist(Player* player, Creature* target)
{
	const Position& playerPos = player->getPosition();
	const Position& targetPos = target->getPosition();
	if (!Position::areInRange<1, 1>(playerPos, targetPos)) {
		return false;
	}

	float attackFactor = player->getAttackFactor();
	int32_t attackSkill = player->getSkillLevel(SKILL_FIST), attackValue = otx::config::getInteger(otx::config::FIST_BASE_ATTACK);

	double maxDamage = Weapons::getMaxWeaponDamage(player->getLevel(), attackSkill, attackValue, attackFactor);

	const Vocation* vocation = player->getVocation();
	if (vocation && vocation->formulaMultipliers[MULTIPLIER_MELEE] != 1.0) {
		maxDamage *= vocation->formulaMultipliers[MULTIPLIER_MELEE];
	}

	maxDamage = std::floor(maxDamage);

	CombatDamage damage;
	damage.primary.type = COMBAT_PHYSICALDAMAGE;
	damage.primary.value = random_range(0, maxDamage, DISTRO_NORMAL);

	CombatParams params;
	params.blockedByArmor = true;
	params.blockedByShield = true;
	params.combatType = COMBAT_PHYSICALDAMAGE;

	Combat::doTargetCombat(player, target, damage, params);
	if (!player->hasFlag(PlayerFlag_NotGainSkill) && player->getAddAttackSkill()) {
		player->addSkillAdvance(SKILL_FIST, 1);
	}
	return true;
}

bool Weapon::internalUseWeapon(Player* player, Item* item, Creature* target, int32_t modifier) const
{
	if (isScripted()) {
		LuaVariant var;
		var.type = VARIANT_NUMBER;
		var.number = target->getID();
		executeUseWeapon(player, var);
	} else {
		CombatDamage damage;
		damage.primary.type = params.combatType;
		damage.primary.value = (getWeaponDamage(player, item, false) * modifier) / 100;
		damage.secondary.type = item->getElementType();
		damage.secondary.value = getWeaponElementDamage(player, item);
		Combat::doTargetCombat(player, target, damage, params);
	}

	onUsedWeapon(player, item, target->getTile());
	return true;
}

bool Weapon::internalUseWeapon(Player* player, Item* item, Tile* tile) const
{
	if (isScripted()) {
		LuaVariant var;
		var.type = VARIANT_TARGETPOSITION;
		var.pos = tile->getPosition();
		executeUseWeapon(player, var);
	} else {
		Combat::postCombatEffects(player, tile->getPosition(), params);
		g_game.addMagicEffect(tile->getPosition(), MAGIC_EFFECT_POFF);
	}

	onUsedWeapon(player, item, tile);
	return true;
}

void Weapon::onUsedWeapon(Player* player, Item* item, Tile* tile) const
{
	if (!player->hasFlag(PlayerFlag_NotGainSkill)) {
		Skills_t skillType;
		uint64_t skillPoint = 0;
		if (getSkillType(player, item, skillType, skillPoint)) {
			player->addSkillAdvance(skillType, skillPoint);
		}
	}

	if (!player->hasFlag(PlayerFlag_HasNoExhaustion) && exhaustion > 0) {
		player->addExhaust(exhaustion, EXHAUST_MELEE);
	}

	int32_t manaCost = getManaCost(player);
	if (manaCost > 0) {
		player->changeMana(-manaCost);
		if (!player->hasFlag(PlayerFlag_NotGainMana) && (player->getZone() != ZONE_HARDCORE || otx::config::getBoolean(otx::config::PVPZONE_ADDMANASPENT))) {
			player->addManaSpent(manaCost);
		}
	}

	if (!player->hasFlag(PlayerFlag_HasInfiniteSoul) && soul > 0) {
		player->changeSoul(-soul);
	}

	if (otx::config::getBoolean(otx::config::REMOVE_WEAPON_AMMO)) {
		if (ammoAction == AMMOACTION_MOVEBACK && m_breakChance > 0 && random_range(1, 100) <= m_breakChance) {
			g_game.transformItem(item, item->getID(), std::max<int32_t>(0, item->getItemCount() - 1));
			return;
		}

		switch (ammoAction) {
			case AMMOACTION_MOVEBACK:
				break;
			case AMMOACTION_REMOVECOUNT:
				g_game.transformItem(item, item->getID(), std::max<int32_t>(0, item->getItemCount()) - 1);
				break;
			case AMMOACTION_REMOVECHARGE:
				g_game.transformItem(item, item->getID(), std::max<int32_t>(0, item->getCharges()) - 1);
				break;
			case AMMOACTION_MOVE:
				g_game.internalMoveItem(player, item->getParent(), tile, INDEX_WHEREEVER, item, 1, nullptr, FLAG_NOLIMIT);
				break;

			default: {
				if (item->hasCharges()) {
					g_game.transformItem(item, item->getID(), std::max<int32_t>(0, item->getCharges()) - 1);
				}
				break;
			}
		}
	}
}

int32_t Weapon::getManaCost(const Player* player) const
{
	if (mana) {
		return mana;
	}

	return manaPercent ? (player->getMaxMana() * manaPercent) / 100 : 0;
}

bool Weapon::executeUseWeapon(Player* player, const LuaVariant& var) const
{
	// onUseWeapon(cid, var)
	if (!otx::lua::reserveScriptEnv()) {
		std::clog << "[Error - Weapon::executeUseWeapon] Call stack overflow." << std::endl;
		return false;
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	env.setScriptId(m_scriptId, m_interface);

	lua_State* L = m_interface->getState();
	m_interface->pushFunction(m_scriptId);

	lua_pushnumber(L, env.addThing(player));
	otx::lua::pushVariant(L, var);
	return otx::lua::callFunction(L, 2);
}

WeaponMelee::WeaponMelee(LuaInterface* luaInterface) :
	Weapon(luaInterface)
{
	//
}

void WeaponMelee::configureWeapon(const ItemType& it)
{
	params.distanceEffect = it.shootType;
	if (it.abilities) {
		m_elementType = it.abilities->elementType;
		m_elementDamage = it.abilities->elementDamage;
		params.isAggressive = true;
		params.useCharges = true;
	} else {
		m_elementType = COMBAT_NONE;
		m_elementDamage = 0;
	}
	Weapon::configureWeapon(it);
}

bool WeaponMelee::useWeapon(Player* player, Item* item, Creature* target) const
{
	int32_t modifier = playerWeaponCheck(player, target);
	if (!modifier) {
		return false;
	}

	return internalUseWeapon(player, item, target, modifier);
}

bool WeaponMelee::getSkillType(const Player* player, const Item* item,
	Skills_t& skill, uint64_t& skillPoint) const
{
	skillPoint = 0;
	if (player->getAddAttackSkill()) {
		switch (player->getLastAttackBlockType()) {
			case BLOCK_ARMOR:
			case BLOCK_NONE:
				skillPoint = 1;
				break;

			case BLOCK_DEFENSE:
			default:
				break;
		}
	}

	switch (item->getWeaponType()) {
		case WEAPON_SWORD: {
			skill = SKILL_SWORD;
			return true;
		}

		case WEAPON_CLUB: {
			skill = SKILL_CLUB;
			return true;
		}

		case WEAPON_AXE: {
			skill = SKILL_AXE;
			return true;
		}

		case WEAPON_FIST: {
			skill = SKILL_FIST;
			return true;
		}

		default:
			break;
	}

	return false;
}

int32_t WeaponMelee::getWeaponDamage(const Player* player, const Item* item, bool maxDamage /*= false*/) const
{
	int32_t attackSkill = player->getWeaponSkill(item), attackValue = std::max<int32_t>(0, (item->getAttack() + item->getExtraAttack()) - item->getElementDamage());
	float attackFactor = player->getAttackFactor();

	double maxValue = Weapons::getMaxWeaponDamage(player->getLevel(), attackSkill, attackValue, attackFactor);

	const Vocation* vocation = player->getVocation();
	if (vocation->formulaMultipliers[MULTIPLIER_MELEE] != 1.f) {
		maxValue *= vocation->formulaMultipliers[MULTIPLIER_MELEE];
	}

	const int32_t ret = std::floor(maxValue);
	if (maxDamage) {
		return ret;
	}
	return random_range(std::round(ret / 3.75f), std::round(ret * 0.85), DISTRO_NORMAL);
}

int32_t WeaponMelee::getWeaponElementDamage(const Player* player, const Item* item, bool maxDamage /* = false*/) const
{
	if (m_elementType == COMBAT_NONE) {
		return 0;
	}

	int32_t attackSkill = player->getWeaponSkill(item), attackValue = item->getElementDamage();
	float attackFactor = player->getAttackFactor();

	double maxValue = Weapons::getMaxWeaponDamage(player->getLevel(), attackSkill, attackValue, attackFactor);

	const Vocation* vocation = player->getVocation();
	if (vocation && vocation->formulaMultipliers[MULTIPLIER_MELEE] != 1.0) {
		maxValue *= vocation->formulaMultipliers[MULTIPLIER_MELEE];
	}

	int32_t ret = std::floor(maxValue);
	if (maxDamage) {
		return -ret;
	}
	return -random_range(0, ret, DISTRO_NORMAL);
}

WeaponDistance::WeaponDistance(LuaInterface* luaInterface) :
	Weapon(luaInterface)
{
	swing = params.blockedByShield = false;
}

void WeaponDistance::configureWeapon(const ItemType& it)
{
	m_breakChance = it.breakChance;
	ammoAction = it.ammoAction;

	params.distanceEffect = it.shootType;
	if (it.abilities) {
		m_elementType = it.abilities->elementType;
		m_elementDamage = it.abilities->elementDamage;
		params.isAggressive = true;
		params.useCharges = true;
	} else {
		m_elementType = COMBAT_NONE;
		m_elementDamage = 0;
	}
	Weapon::configureWeapon(it);
}

int32_t WeaponDistance::playerWeaponCheck(Player* player, Creature* target) const
{
	const ItemType& it = Item::items[id];
	if (it.weaponType == WEAPON_AMMO) {
		if (Item* item = player->getWeapon(true)) {
			if (const Weapon* weapon = g_weapons.getWeapon(item)) {
				return weapon->playerWeaponCheck(player, target);
			}
		}
	}

	return Weapon::playerWeaponCheck(player, target);
}

bool WeaponDistance::useWeapon(Player* player, Item* item, Creature* target) const
{
	int32_t modifier = playerWeaponCheck(player, target);
	if (!modifier) {
		return false;
	}

	const ItemType& iType = Item::items[id];

	int32_t chance = iType.hitChance;
	if (chance == -1) {
		// hit chance is based on distance to target and distance skill
		const Position& playerPos = player->getPosition();
		const Position& targetPos = target->getPosition();

		uint32_t distance = std::max(std::abs(playerPos.x - targetPos.x), std::abs(playerPos.y - targetPos.y)), skill = player->getSkillLevel(SKILL_DIST);
		if (iType.maxHitChance == 75) {
			// chance for one-handed weapons
			switch (distance) {
				case 1:
				case 5:
					chance = std::min<int32_t>(skill, 74) + 1;
					break;
				case 2:
					chance = (std::min<int32_t>(skill, 28) * 2.4f) + 8;
					break;
				case 3:
					chance = (std::min<int32_t>(skill, 45) * 1.55f) + 6;
					break;
				case 4:
					chance = (std::min<int32_t>(skill, 58) * 1.25f) + 3;
					break;
				case 6:
					chance = (std::min<int32_t>(skill, 90) * 0.8f) + 3;
					break;
				case 7:
					chance = (std::min<int32_t>(skill, 104) * 0.7f) + 2;
					break;
				default:
					chance = iType.hitChance;
					break;
			}
		} else if (iType.maxHitChance == 90) {
			// formula for two-handed weapons
			switch (distance) {
				case 1:
				case 5:
					chance = (std::min<int32_t>(skill, 74) * 1.2f) + 1;
					break;
				case 2:
					chance = std::min<int32_t>(skill, 28) * 3.2f;
					break;
				case 3:
					chance = std::min<int32_t>(skill, 45) * 2.f;
					break;
				case 4:
					chance = std::min<int32_t>(skill, 58) * 1.55f;
					break;
				case 6:
				case 7:
					chance = std::min<int32_t>(skill, 90);
					break;

				default:
					chance = iType.hitChance;
					break;
			}
		} else if (iType.maxHitChance == 100) {
			switch (distance) {
				case 1:
					chance = (std::min<int32_t>(skill, 73) * 1.35f) + 1;
					break;
				case 2:
					chance = (std::min<int32_t>(skill, 30) * 3.2f) + 4;
					break;
				case 3:
					chance = (std::min<int32_t>(skill, 48) * 2.05f) + 2;
					break;
				case 4:
					chance = (std::min<int32_t>(skill, 65) * 1.5f) + 2;
					break;
				case 5:
					chance = (std::min<int32_t>(skill, 73) * 1.35f) + 1;
					break;
				case 6:
					chance = (std::min<int32_t>(skill, 87) * 1.2f) - 4;
					break;
				case 7:
					chance = (std::min<int32_t>(skill, 90) * 1.1f) + 1;
					break;

				default:
					chance = iType.hitChance;
					break;
			}
		} else {
			chance = iType.maxHitChance;
		}
	}

	if (item->getWeaponType() == WEAPON_AMMO) {
		Item* bow = player->getWeapon(true);
		if (bow && bow->getHitChance() != -1) {
			chance += bow->getHitChance();
		}
	}

	if (random_range(1, 100) > chance) {
		// we failed attack, miss!
		Tile* destTile = target->getTile();
		if (!Position::areInRange<1, 1, 0>(player->getPosition(), target->getPosition())) {
			std::vector<std::pair<int32_t, int32_t>> destList;
			destList.push_back(std::make_pair(-1, -1));
			destList.push_back(std::make_pair(-1, 0));
			destList.push_back(std::make_pair(-1, 1));
			destList.push_back(std::make_pair(0, -1));
			destList.push_back(std::make_pair(0, 0));
			destList.push_back(std::make_pair(0, 1));
			destList.push_back(std::make_pair(1, -1));
			destList.push_back(std::make_pair(1, 0));
			destList.push_back(std::make_pair(1, 1));

			std::shuffle(destList.begin(), destList.end(), getRandomGenerator());
			Position destPos = target->getPosition();

			Tile* tmpTile = nullptr;
			for (std::vector<std::pair<int32_t, int32_t>>::iterator it = destList.begin(); it != destList.end(); ++it) {
				if (!(tmpTile = g_game.getTile(destPos.x + it->first, destPos.y + it->second, destPos.z))
					|| tmpTile->hasProperty(IMMOVABLEBLOCKSOLID) || !tmpTile->ground) {
					continue;
				}

				destTile = tmpTile;
				break;
			}
		}

		Weapon::internalUseWeapon(player, item, destTile);
	} else {
		Weapon::internalUseWeapon(player, item, target, modifier);
	}
	return true;
}

int32_t WeaponDistance::getWeaponDamage(const Player* player, const Item* item, bool maxDamage /*= false*/) const
{
	int32_t attackValue = item->getAttack() + item->getExtraAttack();
	if (item->getWeaponType() == WEAPON_AMMO) {
		if (Item* bow = const_cast<Player*>(player)->getWeapon(true)) {
			attackValue += bow->getAttack() + bow->getExtraAttack();
		}
	}

	int32_t attackSkill = player->getSkillLevel(SKILL_DIST);
	float attackFactor = player->getAttackFactor();

	double maxValue = Weapons::getMaxWeaponDamage(player->getLevel(), attackSkill, attackValue, attackFactor);

	const Vocation* vocation = player->getVocation();
	if (vocation->formulaMultipliers[MULTIPLIER_DISTANCE] != 1.0) {
		maxValue *= vocation->formulaMultipliers[MULTIPLIER_DISTANCE];
	}

	const int32_t ret = std::floor(maxValue);
	if (maxDamage) {
		return ret;
	}
	return random_range(std::round(ret / 3.75f), std::round(ret * 0.85), DISTRO_NORMAL);
}

bool WeaponDistance::getSkillType(const Player* player, const Item*,
	Skills_t& skill, uint64_t& skillPoint) const
{
	skill = SKILL_DIST;
	skillPoint = 0;
	if (player->getAddAttackSkill()) {
		switch (player->getLastAttackBlockType()) {
			case BLOCK_NONE:
				skillPoint = 2;
				break;

			case BLOCK_ARMOR:
				skillPoint = 1;
				break;

			case BLOCK_DEFENSE:
			default:
				break;
		}
	}

	return true;
}

WeaponWand::WeaponWand(LuaInterface* luaInterface) :
	Weapon(luaInterface)
{
	params.blockedByArmor = false;
	params.blockedByShield = false;
}

bool WeaponWand::configureEvent(xmlNodePtr p)
{
	if (!Weapon::configureEvent(p)) {
		return false;
	}

	int32_t intValue;
	if (readXMLInteger(p, "min", intValue)) {
		minChange = intValue;
	}

	if (readXMLInteger(p, "max", intValue)) {
		maxChange = intValue;
	}
	return true;
}

void WeaponWand::configureWeapon(const ItemType& it)
{
	params.distanceEffect = it.shootType;
	Weapon::configureWeapon(it);
}

int32_t WeaponWand::getWeaponDamage(const Player* player, const Item*, bool maxDamage /* = false*/) const
{
	float multiplier = 1.f;
	if (const Vocation* vocation = player->getVocation()) {
		multiplier = vocation->formulaMultipliers[MULTIPLIER_WAND];
	}

	if (maxDamage) {
		return maxChange * multiplier;
	}
	return random_range(minChange * multiplier, maxChange * multiplier, DISTRO_NORMAL);
}
