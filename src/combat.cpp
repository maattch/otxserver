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

#include "combat.h"

#include "configmanager.h"
#include "const.h"
#include "creature.h"
#include "game.h"
#include "player.h"
#include "tile.h"
#include "weapons.h"

#include "otx/util.hpp"

namespace
{
	std::vector<Tile*> getList(const MatrixArea& area, const Position& targetPos, const Direction dir)
	{
		auto casterPos = getNextPosition(dir, targetPos);

		std::vector<Tile*> vec;

		auto&& [centerX, centerY] = area.getCenter();

		Position tmpPos(targetPos.x - centerX, targetPos.y - centerY, targetPos.z);
		for (uint32_t row = 0; row < area.getRows(); ++row, ++tmpPos.y) {
			for (uint32_t col = 0; col < area.getCols(); ++col, ++tmpPos.x) {
				if (area(row, col)) {
					if (g_game.isSightClear(casterPos, tmpPos, true)) {
						Tile* tile = g_game.getMap()->getTile(tmpPos);
						if (!tile) {
							tile = new StaticTile(tmpPos.x, tmpPos.y, tmpPos.z);
							g_game.getMap()->setTile(tmpPos, tile);
						}
						vec.push_back(tile);
					}
				}
			}
			tmpPos.x -= area.getCols();
		}
		return vec;
	}

	std::vector<Tile*> getCombatArea(const Position& centerPos, const Position& targetPos, const CombatArea* area)
	{
		if (targetPos.z >= MAP_MAX_LAYERS) {
			return {};
		}

		if (area) {
			return getList(area->getArea(centerPos, targetPos), targetPos, getDirectionTo(targetPos, centerPos));
		}

		Tile* tile = g_game.getMap()->getTile(targetPos);
		if (!tile) {
			tile = new StaticTile(targetPos.x, targetPos.y, targetPos.z);
			g_game.getMap()->setTile(targetPos, tile);
		}
		return { tile };
	}

	MatrixArea createArea(const std::vector<uint8_t>& vec, uint32_t rows)
	{
		uint32_t cols;
		if (rows == 0) {
			cols = 0;
		} else {
			cols = vec.size() / rows;
		}

		MatrixArea area(rows, cols);

		uint32_t x = 0, y = 0;
		for (uint8_t value : vec) {
			if (value == 1 || value == 3) {
				area(y, x) = 1;
			}

			if (value == 2 || value == 3) {
				area.setCenter(y, x);
			}

			++x;
			if (cols == x) {
				x = 0;
				++y;
			}
		}
		return area;
	}


} // namespace

Combat::~Combat()
{
	for (const Condition* condition : m_params.conditionList) {
		delete condition;
	}
	m_params.conditionList.clear();
}

CombatDamage Combat::getCombatDamage(Creature* creature) const
{
	CombatDamage damage;
	damage.primary.type = m_params.combatType;

	if (m_formulaType == FORMULA_VALUE) {
		damage.min = static_cast<int32_t>(m_mina);
		damage.max = static_cast<int32_t>(m_maxa);
		damage.primary.value = otx::util::abs(random_range(damage.min, damage.max));
	} else if (creature) {
		if (creature->getCombatValues(damage.min, damage.max)) {
			damage.primary.value = otx::util::abs(random_range(damage.min, damage.max));
		} else if (Player* player = creature->getPlayer()) {
			if (m_params.valueCallback) {
				m_params.valueCallback->getMinMaxValues(player, damage, m_params.useCharges);
			} else if (m_formulaType == FORMULA_LEVELMAGIC) {
				damage.min = (player->getLevel() / m_minl + player->getMagicLevel() * m_minm) * m_mina + m_minb;
				damage.max = (player->getLevel() / m_maxl + player->getMagicLevel() * m_maxm) * m_maxa + m_maxb;
				if (m_minc != 0 && otx::util::abs(damage.min) < otx::util::abs(m_minc)) {
					damage.min = m_minc;
				}

				if (m_maxc != 0 && otx::util::abs(damage.max) < otx::util::abs(m_maxc)) {
					damage.max = m_maxc;
				}

				player->increaseCombatValues(damage.min, damage.max, m_params.useCharges, true);
				damage.primary.value = otx::util::abs(random_range(damage.min, damage.max));
			} else if (m_formulaType == FORMULA_SKILL) {
				Item* tool = player->getWeapon(false);
				if (const Weapon* weapon = g_weapons.getWeapon(tool)) {
					damage.max = weapon->getWeaponDamage(player, tool, true) * m_maxa + m_maxb;
					damage.secondary.type = tool->getElementType();
					damage.secondary.value = weapon->getWeaponElementDamage(player, tool) * m_maxa + m_maxb;
				} else {
					damage.max = static_cast<int32_t>(m_maxb);
				}

				damage.min = static_cast<int32_t>(m_minb);
				if (m_maxc != 0 && otx::util::abs(damage.max) < otx::util::abs(m_maxc)) {
					damage.max = m_maxc;
				}
				damage.primary.value = otx::util::abs(random_range(damage.min, damage.max));
			}
		}
	}
	return damage;
}

CombatType_t Combat::ConditionToDamageType(ConditionType_t type)
{
	switch (type) {
		case CONDITION_FIRE:     return COMBAT_FIREDAMAGE;
		case CONDITION_ENERGY:   return COMBAT_ENERGYDAMAGE;
		case CONDITION_POISON:   return COMBAT_EARTHDAMAGE;
		case CONDITION_FREEZING: return COMBAT_ICEDAMAGE;
		case CONDITION_DAZZLED:  return COMBAT_HOLYDAMAGE;
		case CONDITION_CURSED:   return COMBAT_DEATHDAMAGE;
		case CONDITION_DROWN:    return COMBAT_DROWNDAMAGE;
		case CONDITION_BLEEDING: return COMBAT_PHYSICALDAMAGE;

		default:
			return COMBAT_NONE;
	}
}

ConditionType_t Combat::DamageToConditionType(CombatType_t type)
{
	switch (type) {
		case COMBAT_FIREDAMAGE:     return CONDITION_FIRE;
		case COMBAT_ENERGYDAMAGE:   return CONDITION_ENERGY;
		case COMBAT_EARTHDAMAGE:    return CONDITION_POISON;
		case COMBAT_ICEDAMAGE:      return CONDITION_FREEZING;
		case COMBAT_HOLYDAMAGE:     return CONDITION_DAZZLED;
		case COMBAT_DEATHDAMAGE:    return CONDITION_CURSED;
		case COMBAT_PHYSICALDAMAGE: return CONDITION_BLEEDING;

		default:
			return CONDITION_NONE;
	}
}

ReturnValue Combat::canDoCombat(Creature* caster, Tile* tile, bool isAggressive)
{
	if (tile->hasProperty(BLOCKPROJECTILE) || tile->floorChange() || tile->getTeleportItem()) {
		return RET_NOTENOUGHROOM;
	}

	if (caster) {
		bool success = true;
		if (caster->hasEventRegistered(CREATURE_EVENT_COMBAT_AREA)) {
			for (CreatureEvent* it : caster->getCreatureEvents(CREATURE_EVENT_COMBAT_AREA)) {
				if (!it->executeCombatArea(caster, tile, isAggressive)) {
					success = false;
				}
			}
		}

		if (!success) {
			return RET_NOTPOSSIBLE;
		}

		if (caster->getPosition().z < tile->getPosition().z) {
			return RET_FIRSTGODOWNSTAIRS;
		}

		if (caster->getPosition().z > tile->getPosition().z) {
			return RET_FIRSTGOUPSTAIRS;
		}

		if (!isAggressive) {
			return RET_NOERROR;
		}

		const Player* player = caster->getPlayer();
		if (player && player->hasFlag(PlayerFlag_IgnoreProtectionZone)) {
			return RET_NOERROR;
		}
	}
	return isAggressive && tile->hasFlag(TILESTATE_PROTECTIONZONE) ? RET_ACTIONNOTPERMITTEDINPROTECTIONZONE : RET_NOERROR;
}

ReturnValue Combat::canDoCombat(Creature* attacker, Creature* target, bool isAggressive)
{
	if (!attacker) {
		return RET_NOERROR;
	}

	bool success = true;
	if (attacker->hasEventRegistered(CREATURE_EVENT_COMBAT)) {
		for (CreatureEvent* it : attacker->getCreatureEvents(CREATURE_EVENT_COMBAT)) {
			if (!it->executeCombat(attacker, target, isAggressive)) {
				success = false;
			}
		}
	}

	if (!success) {
		return RET_NOTPOSSIBLE;
	}

	if (otx::config::getBoolean(otx::config::MONSTER_ATTACK_MONSTER)) {
		if (target->getType() == CREATURE_TYPE_MONSTER && attacker->getType() == CREATURE_TYPE_MONSTER && !target->isPlayerSummon() && !attacker->isPlayerSummon()) {
			return RET_NOTPOSSIBLE;
		}

		if (!attacker->isSummon() && !target->isSummon()) {
			if (attacker->getType() == CREATURE_TYPE_MONSTER && target->getType() == CREATURE_TYPE_MONSTER) {
				return RET_NOTPOSSIBLE;
			}
		}
	}

	if (attacker->isSummon()) {
		if (target == attacker->getMaster()) {
			return RET_NOTPOSSIBLE;
		}
	}

	// can't attack at login
	if (const Player* attackerP = attacker->getPlayer()) {
		if (attackerP->checkLoginDelay()) {
			return RET_YOUMAYNOTATTACKIMMEDIATELYAFTERLOGGINGIN;
		}
	}

	bool checkZones = false;
	if (const Player* targetPlayer = target->getPlayer()) {
		// can't attack players in login delay
		if (targetPlayer->checkLoginDelay()) {
			return RET_YOUMAYNOTATTACKIMMEDIATELYAFTERLOGGINGIN;
		}

		if (!targetPlayer->isAttackable()) {
			return RET_YOUMAYNOTATTACKTHISPLAYER;
		}

		const Player* attackerPlayer = nullptr;
		if ((attackerPlayer = attacker->getPlayer()) || (attackerPlayer = attacker->getPlayerMaster())) {
			checkZones = true;
			if ((g_game.getWorldType() == WORLDTYPE_OPTIONAL && !Combat::isInPvpZone(attacker, target)
					&& !attackerPlayer->isEnemy(targetPlayer, true))
				|| isProtected(const_cast<Player*>(attackerPlayer),
					const_cast<Player*>(targetPlayer))
				|| (otx::config::getBoolean(otx::config::CANNOT_ATTACK_SAME_LOOKFEET)
					&& attackerPlayer->getDefaultOutfit().lookFeet == targetPlayer->getDefaultOutfit().lookFeet)
				|| !attackerPlayer->canSeeCreature(targetPlayer)) {
				return RET_YOUMAYNOTATTACKTHISPLAYER;
			}
		}
	} else if (target->getMonster()) {
		if (!target->isAttackable()) {
			return RET_YOUMAYNOTATTACKTHISCREATURE;
		}

		const Player* attackerPlayer = nullptr;
		if ((attackerPlayer = attacker->getPlayer()) || (attackerPlayer = attacker->getPlayerMaster())) {
			if (attackerPlayer->hasFlag(PlayerFlag_CannotAttackMonster)) {
				return RET_YOUMAYNOTATTACKTHISCREATURE;
			}

			if (target->isPlayerSummon()) {
				checkZones = true;
				if (g_game.getWorldType() == WORLDTYPE_OPTIONAL && !Combat::isInPvpZone(attacker, target) && !attackerPlayer->isEnemy(target->getPlayerMaster(), true)) {
					return RET_YOUMAYNOTATTACKTHISCREATURE;
				}
			}
		}
	}
	return checkZones && (target->getTile()->hasFlag(TILESTATE_OPTIONALZONE) || (attacker->getTile()->hasFlag(TILESTATE_OPTIONALZONE) && !target->getTile()->hasFlag(TILESTATE_OPTIONALZONE) && !target->getTile()->hasFlag(TILESTATE_PROTECTIONZONE))) ? RET_ACTIONNOTPERMITTEDINANOPVPZONE : RET_NOERROR;
}

ReturnValue Combat::canTargetCreature(Player* player, Creature* target)
{
	if (player == target) {
		return RET_YOUMAYNOTATTACKTHISPLAYER;
	}

	bool deny = false;
	if (player->hasEventRegistered(CREATURE_EVENT_TARGET)) {
		for (CreatureEvent* it : player->getCreatureEvents(CREATURE_EVENT_TARGET)) {
			if (!it->executeAction(player, target)) {
				deny = true;
			}
		}
	}

	if (deny) {
		return RET_NEEDEXCHANGE;
	}

	if (!player->hasFlag(PlayerFlag_IgnoreProtectionZone)) {
		if (player->getZone() == ZONE_PROTECTION) {
			return RET_YOUMAYNOTATTACKAPERSONWHILEINPROTECTIONZONE;
		}

		if (target->getZone() == ZONE_PROTECTION) {
			return RET_YOUMAYNOTATTACKAPERSONINPROTECTIONZONE;
		}

		if (target->getPlayer() || target->isPlayerSummon()) {
			if (player->getZone() == ZONE_OPTIONAL) {
				return RET_ACTIONNOTPERMITTEDINANOPVPZONE;
			}

			if (target->getZone() == ZONE_OPTIONAL) {
				return RET_YOUMAYNOTATTACKAPERSONINPROTECTIONZONE;
			}
		}
	}

	if (player->hasFlag(PlayerFlag_CannotUseCombat)) {
		return target->getPlayer() ? RET_YOUMAYNOTATTACKTHISPLAYER : RET_YOUMAYNOTATTACKTHISCREATURE;
	}

	if (target->getPlayer() && !Combat::isInPvpZone(player, target) && player->getSkullType(target->getPlayer()) == SKULL_NONE) {
		if (player->getSecureMode()) {
			return RET_TURNSECUREMODETOATTACKUNMARKEDPLAYERS;
		}

		if (otx::config::getBoolean(otx::config::USE_BLACK_SKULL)) {
			if (player->getSkull() == SKULL_BLACK) {
				return RET_YOUMAYNOTATTACKTHISPLAYER;
			}
		}
	}

	if (target->getNpc()) {
		return RET_YOUMAYNOTATTACKTHISCREATURE;
	}
	return Combat::canDoCombat(player, target, true);
}

bool Combat::isInPvpZone(const Creature* attacker, const Creature* target)
{
	return attacker->getZone() == ZONE_HARDCORE && target->getZone() == ZONE_HARDCORE;
}

bool Combat::isProtected(Player* attacker, Player* target)
{
	if (attacker->hasFlag(PlayerFlag_CannotAttackPlayer)) {
		return true;
	}

	if (attacker->hasCustomFlag(PlayerCustomFlag_GamemasterPrivileges)) {
		return false;
	}

	if (attacker->getZone() == ZONE_HARDCORE && target->getZone() == ZONE_HARDCORE && otx::config::getBoolean(otx::config::PVP_TILE_IGNORE_PROTECTION)) {
		return false;
	}

	if (target->checkLoginDelay()) {
		return true;
	}

	return target->isProtected() || attacker->isProtected() || (attacker->checkLoginDelay() && !attacker->hasBeenAttacked(target->getID()));
}

void Combat::setPlayerCombatValues(FormulaType_t type, double mina,
	double minb, double maxa, double maxb, double minl,
	double maxl, double minm, double maxm, int32_t minc, int32_t maxc)
{
	m_formulaType = type;
	m_mina = mina;
	m_minb = minb;
	m_maxa = maxa;
	m_maxb = maxb;
	m_minl = minl;
	m_maxl = maxl;
	m_minm = minm;
	m_maxm = maxm;
	m_minc = minc;
	m_maxc = maxc;
}

bool Combat::setParam(CombatParam_t param, uint32_t value)
{
	switch (param) {
		case COMBATPARAM_COMBATTYPE:
			m_params.combatType = (CombatType_t)value;
			return true;
		case COMBATPARAM_EFFECT:
			m_params.impactEffect = (MagicEffect_t)value;
			return true;
		case COMBATPARAM_DISTANCEEFFECT:
			m_params.distanceEffect = (ShootEffect_t)value;
			return true;
		case COMBATPARAM_BLOCKEDBYARMOR:
			m_params.blockedByArmor = (value != 0);
			return true;
		case COMBATPARAM_BLOCKEDBYSHIELD:
			m_params.blockedByShield = (value != 0);
			return true;
		case COMBATPARAM_TARGETCASTERORTOPMOST:
			m_params.targetCasterOrTopMost = (value != 0);
			return true;
		case COMBATPARAM_TARGETPLAYERSORSUMMONS:
			m_params.targetPlayersOrSummons = (value != 0);
			return true;
		case COMBATPARAM_DIFFERENTAREADAMAGE:
			m_params.differentAreaDamage = (value != 0);
			return true;
		case COMBATPARAM_CREATEITEM:
			m_params.itemId = value;
			return true;
		case COMBATPARAM_AGGRESSIVE:
			m_params.isAggressive = (value != 0);
			return true;
		case COMBATPARAM_DISPEL:
			m_params.dispelType = (ConditionType_t)value;
			return true;
		case COMBATPARAM_USECHARGES:
			m_params.useCharges = (value != 0);
			return true;
		case COMBATPARAM_HITEFFECT:
			m_params.hitEffect = (MagicEffect_t)value;
			return true;
		case COMBATPARAM_HITCOLOR:
			m_params.hitColor = (Color_t)value;
			return true;
		case COMBATPARAM_ELEMENTDAMAGE:
			m_params.elementDamage = value;
			return true;
		case COMBATPARAM_ELEMENTTYPE:
			m_params.elementType = (CombatType_t)value;
			return true;

		default:
			return false;
	}
}

bool Combat::setCallback(CallBackParam_t key)
{
	switch (key) {
		case CALLBACKPARAM_LEVELMAGICVALUE: {
			m_params.valueCallback.reset(new ValueCallback(FORMULA_LEVELMAGIC));
			return true;
		}
		case CALLBACKPARAM_SKILLVALUE: {
			m_params.valueCallback.reset(new ValueCallback(FORMULA_SKILL));
			return true;
		}
		case CALLBACKPARAM_TARGETTILECALLBACK: {
			m_params.tileCallback.reset(new TileCallback);
			return true;
		}
		case CALLBACKPARAM_TARGETCREATURECALLBACK: {
			m_params.targetCallback.reset(new TargetCallback);
			return true;
		}

		default: {
			std::clog << "[Warning - Combat::setCallback] Invalid callback type: " << static_cast<int>(key) << std::endl;
			return false;
		}
	}
}

CallBack* Combat::getCallback(CallBackParam_t key)
{
	switch (key) {
		case CALLBACKPARAM_LEVELMAGICVALUE:
		case CALLBACKPARAM_SKILLVALUE:
			return m_params.valueCallback.get();
		case CALLBACKPARAM_TARGETTILECALLBACK:
			return m_params.tileCallback.get();
		case CALLBACKPARAM_TARGETCREATURECALLBACK:
			return m_params.targetCallback.get();

		default:
			return nullptr;
	}
}

void Combat::combatTileEffects(const SpectatorVec& list, Creature* caster, Tile* tile, const CombatParams& params)
{
	if (!tile) {
		return;
	}

	if (params.itemId) {
		Player* player = nullptr;
		if (caster) {
			if (caster->getPlayer()) {
				player = caster->getPlayer();
			} else if (caster->isPlayerSummon()) {
				player = caster->getPlayerMaster();
			}
		}

		uint32_t itemId = params.itemId;
		if (player) {
			bool pzLock = false;
			if (((g_game.getWorldType() == WORLDTYPE_OPTIONAL || otx::config::getBoolean(otx::config::OPTIONAL_PROTECTION))
					&& !tile->hasFlag(TILESTATE_HARDCOREZONE))
				|| tile->hasFlag(TILESTATE_OPTIONALZONE)) {
				switch (itemId) {
					case ITEM_FIREFIELD:
						itemId = ITEM_FIREFIELD_SAFE;
						break;
					case ITEM_POISONFIELD:
						itemId = ITEM_POISONFIELD_SAFE;
						break;
					case ITEM_ENERGYFIELD:
						itemId = ITEM_ENERGYFIELD_SAFE;
						break;
					case ITEM_MAGICWALL:
						itemId = ITEM_MAGICWALL_SAFE;
						break;
					case ITEM_WILDGROWTH:
						itemId = ITEM_WILDGROWTH_SAFE;
						break;
					default:
						break;
				}
			} else if (params.isAggressive && !Item::items[itemId].blockPathFind) {
				pzLock = true;
			}

			player->addInFightTicks(pzLock);
		}

		if (Item* item = Item::CreateItem(itemId)) {
			if (caster) {
				item->setOwner(caster->getID());
			}

			if (g_game.internalAddItem(caster, tile, item) == RET_NOERROR) {
				g_game.startDecay(item);
			} else {
				delete item;
			}
		}
	}

	if (params.tileCallback) {
		params.tileCallback->onTileCombat(caster, tile);
	}

	if (params.impactEffect != MAGIC_EFFECT_NONE && (!caster || !caster->isGhost() || otx::config::getBoolean(otx::config::GHOST_SPELL_EFFECTS))) {
		g_game.addMagicEffect(list, tile->getPosition(), params.impactEffect);
	}
}

void Combat::postCombatEffects(Creature* caster, const Position& pos, const CombatParams& params)
{
	if (caster && params.distanceEffect != SHOOT_EFFECT_NONE) {
		addDistanceEffect(caster, caster->getPosition(), pos, params.distanceEffect);
	}
}

void Combat::addDistanceEffect(Creature* caster, const Position& fromPos, const Position& toPos, ShootEffect_t effect)
{
	if (effect == SHOOT_EFFECT_WEAPONTYPE) {
		switch (caster->getWeaponType()) {
			case WEAPON_AXE:
				effect = SHOOT_EFFECT_WHIRLWINDAXE;
				break;
			case WEAPON_SWORD:
				effect = SHOOT_EFFECT_WHIRLWINDSWORD;
				break;
			case WEAPON_CLUB:
				effect = SHOOT_EFFECT_WHIRLWINDCLUB;
				break;
			case WEAPON_FIST:
				effect = SHOOT_EFFECT_LARGEROCK;
				break;

			default:
				effect = SHOOT_EFFECT_NONE;
				break;
		}
	}

	if (caster && effect != SHOOT_EFFECT_NONE) {
		g_game.addDistanceEffect(fromPos, toPos, effect);
	}
}

bool Combat::doCombat(Creature* caster, Creature* target) const
{
	// target combat callback function
	if (m_params.combatType != COMBAT_NONE) {
		if (!m_params.isAggressive || (caster != target && Combat::canDoCombat(caster, target, true) == RET_NOERROR)) {
			CombatDamage damage = getCombatDamage(caster);
			Combat::doTargetCombat(caster, target, damage, m_params);
			return true;
		} else if (caster == target && m_params.impactEffect != MAGIC_EFFECT_NONE) {
			g_game.addMagicEffect(target->getPosition(), m_params.impactEffect);
			return true;
		}
	} else {
		if (!m_params.isAggressive || (caster != target && Combat::canDoCombat(caster, target, true) == RET_NOERROR)) {
			for (const Condition* condition : m_params.conditionList) {
				if (caster == target || !target->isImmune(condition->getType())) {
					auto conditionClone = condition->clone();
					if (caster) {
						conditionClone->setParam(CONDITIONPARAM_OWNER, caster->getID());
					}

					//TODO: infight condition until all aggressive conditions has ended [?]
					target->addCombatCondition(std::move(conditionClone));
				}
			}

			if (m_params.dispelType != CONDITION_NONE) {
				target->removeCondition(caster, m_params.dispelType);
			}

			SpectatorVec spectators;
			g_game.getSpectators(spectators, target->getPosition(), true, true);

			Combat::combatTileEffects(spectators, caster, target->getTile(), m_params);

			if (m_params.targetCallback) {
				m_params.targetCallback->onTargetCombat(caster, target);
			}

			if (m_params.impactEffect != MAGIC_EFFECT_NONE && (!caster || !caster->isGhost() || otx::config::getBoolean(otx::config::GHOST_SPELL_EFFECTS))) {
				g_game.addMagicEffect(spectators, target->getPosition(), m_params.impactEffect);
			}

			if (caster && m_params.distanceEffect != SHOOT_EFFECT_NONE) {
				g_game.addDistanceEffect(spectators, caster->getPosition(), target->getPosition(), m_params.distanceEffect);
			}
			return true;
		}
	}
	return false;

	//// target combat callback function
	//if (m_params.combatType != COMBAT_NONE) {
	//	if (m_params.isAggressive && (caster == target || Combat::canDoCombat(caster, target, true) != RET_NOERROR)) {
	//		return;
	//	}

	//	int32_t minChange = 0, maxChange = 0;
	//	CombatParams params = m_params;

	//	getMinMaxValues(caster, target, params, minChange, maxChange);
	//	if (m_params.combatType != COMBAT_MANADRAIN) {
	//		doCombatHealth(caster, target, minChange, maxChange, params, false);
	//	} else {
	//		doCombatMana(caster, target, minChange, maxChange, params, false);
	//	}
	//} else {
	//	doCombatDefault(caster, target, m_params);
	//}
}

void Combat::doCombat(Creature* caster, const Position& pos) const
{
	// area combat callback function
	if (m_params.combatType != COMBAT_NONE) {
		CombatDamage damage = getCombatDamage(caster);
		Combat::doAreaCombat(caster, pos, m_area.get(), damage, m_params);
	} else {
		auto tiles = (caster ? getCombatArea(caster->getPosition(), pos, m_area.get()) : getCombatArea(pos, pos, m_area.get()));

		int32_t maxX = 0, maxY = 0;
		// calculate the max viewable range
		for (Tile* tile : tiles) {
			const Position& tilePos = tile->getPosition();
			maxX = std::max(maxX, Position::getDistanceX(tilePos, pos));
			maxY = std::max(maxY, Position::getDistanceY(tilePos, pos));
		}

		maxX += Map::maxViewportX;
		maxY += Map::maxViewportY;

		SpectatorVec spectators;
		g_game.getSpectators(spectators, pos, true, true, maxX, maxX, maxY, maxY);

		Combat::postCombatEffects(caster, pos, m_params);

		for (Tile* tile : tiles) {
			if (Combat::canDoCombat(caster, tile, m_params.isAggressive) != RET_NOERROR) {
				continue;
			}

			Combat::combatTileEffects(spectators, caster, tile, m_params);

			if (CreatureVector* creatures = tile->getCreatures()) {
				const Creature* topCreature = tile->getTopCreature();
				for (Creature* creature : *creatures) {
					if (m_params.targetPlayersOrSummons && !creature->getPlayer() && !creature->isPlayerSummon()) {
						continue;
					}

					if (m_params.targetCasterOrTopMost) {
						if (caster && caster->getTile() == tile) {
							if (creature != caster) {
								continue;
							}
						} else if (creature != topCreature) {
							continue;
						}
					}

					if (!m_params.isAggressive || (caster != creature && Combat::canDoCombat(caster, creature, true) == RET_NOERROR)) {
						for (const Condition* condition : m_params.conditionList) {
							if (caster == creature || !creature->isImmune(condition->getType())) {
								Condition* conditionClone = condition->clone();
								if (caster) {
									conditionClone->setParam(CONDITIONPARAM_OWNER, caster->getID());
								}

								//TODO: infight condition until all aggressive conditions has ended [?]
								creature->addCombatCondition(conditionClone);
							}
						}

						if (m_params.dispelType != CONDITION_NONE) {
							creature->removeCondition(caster, m_params.dispelType);
						}

						if (m_params.targetCallback) {
							m_params.targetCallback->onTargetCombat(caster, creature);
						}

						if (m_params.targetCasterOrTopMost) {
							break;
						}
					}
				}
			}
		}
	}

	//// area combat callback function
	//if (m_params.combatType != COMBAT_NONE) {
	//	int32_t minChange = 0, maxChange = 0;
	//	CombatParams params = m_params;

	//	getMinMaxValues(caster, nullptr, params, minChange, maxChange);
	//	if (m_params.combatType != COMBAT_MANADRAIN) {
	//		doCombatHealth(caster, pos, m_area, minChange, maxChange, params);
	//	} else {
	//		doCombatMana(caster, pos, m_area, minChange, maxChange, params);
	//	}
	//} else {
	//	CombatFunc(caster, pos, m_area, m_params, CombatNullFunc, nullptr);
	//}
}

void Combat::doTargetCombat(Creature* caster, Creature* target, CombatDamage& damage, const CombatParams& params)
{
	if (params.impactEffect != MAGIC_EFFECT_NONE && (!caster || !caster->isGhost() || otx::config::getBoolean(otx::config::GHOST_SPELL_EFFECTS))) {
		g_game.addMagicEffect(target->getPosition(), params.impactEffect);
	}

	if (caster && params.distanceEffect != MAGIC_EFFECT_NONE) {
		g_game.addDistanceEffect(caster->getPosition(), target->getPosition(), params.distanceEffect);
	}

	if (g_game.combatBlockHit(damage, caster, target, params.blockedByShield, params.blockedByArmor, params.itemId != 0)) {
		return;
	}

	Player* casterPlayer = (caster ? caster->getPlayer() : nullptr);
	if (casterPlayer && caster != target && target->getPlayer() && target->getSkull() != SKULL_BLACK && params.combatType != COMBAT_HEALING) {
		damage.primary.value /= 2;
		damage.secondary.value /= 2;
	}

	bool success;
	if (params.combatType != COMBAT_MANADRAIN) {
		success = g_game.combatChangeHealth(caster, target, damage, params.hitEffect, params.hitColor);
	} else {
		success = g_game.combatChangeMana(caster, target, damage);
	}

	if (success) {
		for (const Condition* condition : params.conditionList) {
			if (caster == target || !target->isImmune(condition->getType())) {
				Condition* conditionClone = condition->clone();
				if (caster) {
					conditionClone->setParam(CONDITIONPARAM_OWNER, caster->getID());
				}

				//TODO: infight condition until all aggressive conditions has ended?
				target->addCombatCondition(conditionClone);
			}
		}

		if (params.dispelType != CONDITION_NONE) {
			target->removeCondition(caster, params.dispelType);
		}
	}

	if (params.targetCallback) {
		params.targetCallback->onTargetCombat(caster, target);
	}
}

void Combat::doTargetCombat(Creature* caster, Creature* target, const CombatParams& params)
{
	if (params.impactEffect != MAGIC_EFFECT_NONE && (!caster || !caster->isGhost() || otx::config::getBoolean(otx::config::GHOST_SPELL_EFFECTS))) {
		g_game.addMagicEffect(target->getPosition(), params.impactEffect);
	}

	if (caster && params.distanceEffect != MAGIC_EFFECT_NONE) {
		g_game.addDistanceEffect(caster->getPosition(), target->getPosition(), params.distanceEffect);
	}

	for (const Condition* condition : params.conditionList) {
		if (caster == target || !target->isImmune(condition->getType())) {
			Condition* conditionClone = condition->clone();
			if (caster) {
				conditionClone->setParam(CONDITIONPARAM_OWNER, caster->getID());
			}

			//TODO: infight condition until all aggressive conditions has ended?
			target->addCombatCondition(conditionClone);
		}
	}

	if (params.dispelType != CONDITION_NONE) {
		target->removeCondition(caster, params.dispelType);
	}

	if (params.targetCallback) {
		params.targetCallback->onTargetCombat(caster, target);
	}
}

void Combat::doAreaCombat(Creature* caster, const Position& pos,
	const CombatArea* area, CombatDamage& damage, const CombatParams& params)
{
	const auto& tiles = (caster ? getCombatArea(caster->getPosition(), pos, area) : getCombatArea(pos, pos, area));

	int32_t maxX = 0, maxY = 0;
	// calculate the max viewable range
	for (Tile* tile : tiles) {
		const Position& tilePos = tile->getPosition();
		maxX = std::max(maxX, Position::getDistanceX(tilePos, pos));
		maxY = std::max(maxY, Position::getDistanceY(tilePos, pos));
	}

	maxX += Map::maxViewportX;
	maxY += Map::maxViewportY;

	SpectatorVec spectators;
	g_game.getSpectators(spectators, pos, true, true, maxX, maxX, maxY, maxY);

	Combat::postCombatEffects(caster, pos, params);

	std::vector<Creature*> toDamageCreatures;
	toDamageCreatures.reserve(20);

	for (Tile* tile : tiles) {
		if (Combat::canDoCombat(caster, tile, params.isAggressive) != RET_NOERROR) {
			continue;
		}

		Combat::combatTileEffects(spectators, caster, tile, params);

		if (CreatureVector* creatures = tile->getCreatures()) {
			const Creature* topCreature = tile->getTopCreature();
			for (Creature* creature : *creatures) {
				if (params.targetPlayersOrSummons && !creature->getPlayer() && !creature->isPlayerSummon()) {
					continue;
				}

				if (params.targetCasterOrTopMost) {
					if (caster && caster->getTile() == tile) {
						if (creature != caster) {
							continue;
						}
					} else if (creature != topCreature) {
						continue;
					}
				}

				if (!params.isAggressive || (caster != creature && Combat::canDoCombat(caster, creature, true) == RET_NOERROR)) {
					toDamageCreatures.push_back(creature);
					if (params.targetCasterOrTopMost) {
						break;
					}
				}
			}
		}
	}

	if (!params.differentAreaDamage) {
		damage.primary.value = otx::util::abs(random_range(damage.min, damage.max));
	}

	for (Creature* creature : toDamageCreatures) {
		CombatDamage damageCopy = damage;
		if (params.differentAreaDamage) {
			damageCopy.primary.value = otx::util::abs(random_range(damage.min, damage.max));
		}

		if (g_game.combatBlockHit(damageCopy, caster, creature, params.blockedByShield, params.blockedByArmor, params.itemId != 0)) {
			continue;
		}

		if (caster && caster != creature && caster->getPlayer() && creature->getPlayer() && creature->getSkull() != SKULL_BLACK && damageCopy.primary.type != COMBAT_HEALING) {
			damageCopy.primary.value /= 2;
			damageCopy.secondary.value /= 2;
		}

		bool success;
		if (params.combatType != COMBAT_MANADRAIN) {
			success = g_game.combatChangeHealth(caster, creature, damageCopy, params.hitEffect, params.hitColor);
		} else {
			success = g_game.combatChangeMana(caster, creature, damageCopy);
		}

		if (success) {
			for (const Condition* condition : params.conditionList) {
				if (caster == creature || !creature->isImmune(condition->getType())) {
					Condition* conditionClone = condition->clone();
					if (caster) {
						conditionClone->setParam(CONDITIONPARAM_OWNER, caster->getID());
					}

					//TODO: infight condition until all aggressive conditions has ended [?]
					creature->addCombatCondition(conditionClone);
				}
			}

			if (params.dispelType != CONDITION_NONE) {
				creature->removeCondition(caster, params.dispelType);
			}
		}

		if (params.targetCallback) {
			params.targetCallback->onTargetCombat(caster, creature);
		}
	}
}

void Combat::doAreaCombat(Creature* caster, const Position& pos,
	const CombatArea* area, const CombatParams& params)
{
	const auto& tiles = (caster ? getCombatArea(caster->getPosition(), pos, area) : getCombatArea(pos, pos, area));

	int32_t maxX = 0, maxY = 0;
	// calculate the max viewable range
	for (Tile* tile : tiles) {
		const Position& tilePos = tile->getPosition();
		maxX = std::max(maxX, Position::getDistanceX(tilePos, pos));
		maxY = std::max(maxY, Position::getDistanceY(tilePos, pos));
	}

	maxX += Map::maxViewportX;
	maxY += Map::maxViewportY;

	SpectatorVec spectators;
	g_game.getSpectators(spectators, pos, true, true, maxX, maxX, maxY, maxY);

	Combat::postCombatEffects(caster, pos, params);

	std::vector<Creature*> toDamageCreatures;
	toDamageCreatures.reserve(20);

	for (Tile* tile : tiles) {
		if (Combat::canDoCombat(caster, tile, params.isAggressive) != RET_NOERROR) {
			continue;
		}

		Combat::combatTileEffects(spectators, caster, tile, params);

		if (CreatureVector* creatures = tile->getCreatures()) {
			const Creature* topCreature = tile->getTopCreature();
			for (Creature* creature : *creatures) {
				if (params.targetPlayersOrSummons && !creature->getPlayer() && !creature->isPlayerSummon()) {
					continue;
				}

				if (params.targetCasterOrTopMost) {
					if (caster && caster->getTile() == tile) {
						if (creature != caster) {
							continue;
						}
					} else if (creature != topCreature) {
						continue;
					}
				}

				if (!params.isAggressive || (caster != creature && Combat::canDoCombat(caster, creature, true) == RET_NOERROR)) {
					toDamageCreatures.push_back(creature);
					if (params.targetCasterOrTopMost) {
						break;
					}
				}
			}
		}
	}

	for (Creature* creature : toDamageCreatures) {
		for (const Condition* condition : params.conditionList) {
			if (caster == creature || !creature->isImmune(condition->getType())) {
				Condition* conditionClone = condition->clone();
				if (caster) {
					conditionClone->setParam(CONDITIONPARAM_OWNER, caster->getID());
				}

				//TODO: infight condition until all aggressive conditions has ended [?]
				creature->addCombatCondition(conditionClone);
			}
		}

		if (params.dispelType != CONDITION_NONE) {
			creature->removeCondition(caster, params.dispelType);
		}

		if (params.targetCallback) {
			params.targetCallback->onTargetCombat(caster, creature);
		}
	}
}

//**********************************************************

void ValueCallback::getMinMaxValues(Player* player, CombatDamage& damage, bool useCharges) const
{
	//"onGetPlayerMinMaxValues"(cid, ...)
	if (!otx::lua::reserveScriptEnv()) {
		std::clog << "[Error - ValueCallback::getMinMaxValues] Callstack overflow." << std::endl;
		return;
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	if (!env.setCallbackId(m_scriptId, m_interface)) {
		return;
	}

	m_interface->pushFunction(m_scriptId);
	lua_State* L = m_interface->getState();
	lua_pushnumber(L, env.addThing(player));

	int parameters = 1;
	switch (m_type) {
		case FORMULA_LEVELMAGIC: {
			//"onGetPlayerMinMaxValues"(cid, level, magLevel)
			lua_pushnumber(L, player->getLevel());
			lua_pushnumber(L, player->getMagicLevel());
			parameters += 2;
			break;
		}

		case FORMULA_SKILL: {
			//"onGetPlayerMinMaxValues"(cid, level, skill, attack, element, factor)
			lua_pushnumber(L, player->getLevel());
			if (Item* weapon = player->getWeapon(false)) {
				lua_pushnumber(L, player->getWeaponSkill(weapon));
				if (useCharges && weapon->hasCharges() && otx::config::getBoolean(otx::config::REMOVE_WEAPON_CHARGES)) {
					g_game.transformItem(weapon, weapon->getID(), std::max(0, weapon->getCharges() - 1));
				}

				uint32_t attack = weapon->getAttack() + weapon->getExtraAttack();
				if (weapon->getWeaponType() == WEAPON_AMMO) {
					if (Item* bow = player->getWeapon(true)) {
						attack += bow->getAttack() + bow->getExtraAttack();
					}
				}

				/*Fix infinite damage at physical*/
				if (weapon->getElementType() != COMBAT_NONE) {
					int32_t absAttack = std::abs(static_cast<int>(attack));
					if (weapon->getElementDamage() >= absAttack) {
						attack = weapon->getElementDamage() - absAttack;
					} else {
						attack -= weapon->getElementDamage();
					}

					lua_pushnumber(L, attack);
					lua_pushnumber(L, weapon->getElementDamage());
					damage.secondary.type = weapon->getElementType();
				} else {
					lua_pushnumber(L, attack);
					lua_pushnumber(L, 0);
				}
			} else {
				lua_pushnumber(L, player->getSkillLevel(SKILL_FIST));
				lua_pushnumber(L, otx::config::getInteger(otx::config::FIST_BASE_ATTACK));
				lua_pushnumber(L, 0);
			}

			lua_pushnumber(L, player->getAttackFactor());
			parameters += 5;
			break;
		}

		default: {
			std::clog << "[Warning - ValueCallback::getMinMaxValues] Unknown callback type" << std::endl;
			return;
		}
	}

	const int args = lua_gettop(L);
	if (!otx::lua::protectedCall(L, parameters, 3)) {
		damage.secondary.value = otx::lua::popNumber(L);
		damage.max = otx::lua::popNumber(L);
		damage.min = otx::lua::popNumber(L);
		player->increaseCombatValues(damage.min, damage.max, useCharges, m_type != FORMULA_SKILL);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::popString(L));
	}

	if ((lua_gettop(L) + parameters + 1) != args) {
		otx::lua::reportErrorEx(L, "Stack size changed!");
	}

	env.resetCallback();
	otx::lua::resetScriptEnv();
}

//**********************************************************

void TileCallback::onTileCombat(Creature* creature, Tile* tile) const
{
	//"onTileCombat"(cid, pos)
	if (!otx::lua::reserveScriptEnv()) {
		std::clog << "[Error - TileCallback::onTileCombat] Call stack overflow." << std::endl;
		return;
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	if (!env.setCallbackId(m_scriptId, m_interface)) {
		return;
	}

	lua_State* L = m_interface->getState();
	m_interface->pushFunction(m_scriptId);

	lua_pushnumber(L, env.addThing(creature));
	otx::lua::pushPosition(L, tile->getPosition());
	otx::lua::callVoidFunction(L, 2);

	env.resetCallback();
}

//**********************************************************

void TargetCallback::onTargetCombat(Creature* creature, Creature* target) const
{
	//"onTargetCombat"(cid, target)
	if (!otx::lua::reserveScriptEnv()) {
		std::clog << "[Error - TargetCallback::onTargetCombat] Call stack overflow." << std::endl;
		return;
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	if (!env.setCallbackId(m_scriptId, m_interface)) {
		return;
	}

	lua_State* L = m_interface->getState();
	m_interface->pushFunction(m_scriptId);

	lua_pushnumber(L, env.addThing(creature));
	lua_pushnumber(L, env.addThing(target));
	otx::lua::callVoidFunction(L, 2);

	env.resetCallback();
}

//
// MatrixArea
//
MatrixArea MatrixArea::construct(MatrixOperation_t operation) const
{
	switch (operation) {
		case MATRIXOPERATION_ROTATE90: {
			std::vector<uint8_t> newArr(m_array.size());
			for (uint32_t y = 0; y < m_rows; ++y) {
				for (uint32_t x = 0; x < m_cols; ++x) {
					newArr[x * m_rows + (m_rows - y - 1)] = m_array[y * m_cols + x];
				}
			}
			return { m_rows - m_centerY - 1, m_centerX, m_cols, m_rows, std::move(newArr) };
		}
		case MATRIXOPERATION_ROTATE180: {
			std::vector<uint8_t> newArr(m_array.size());
			std::reverse_copy(std::begin(m_array), std::end(m_array), std::begin(newArr));
			return { m_cols - m_centerX - 1, m_rows - m_centerY - 1, m_rows, m_cols, std::move(newArr) };
		}
		case MATRIXOPERATION_ROTATE270: {
			std::vector<uint8_t> newArr(m_array.size());
			for (uint32_t y = 0; y < m_rows; ++y) {
				for (uint32_t x = 0; x < m_cols; ++x) {
					newArr[(m_cols - 1 - x) * m_rows + y] = m_array[y * m_cols + x];
				}
			}
			return { m_centerY, m_cols - m_centerX - 1, m_cols, m_rows, std::move(newArr) };
		}

		default:
			return *this;
	}
}

//
// CombatArea
//
const MatrixArea& CombatArea::getArea(const Position& centerPos, const Position& targetPos) const
{
	const int32_t dx = targetPos.x - centerPos.x;
	const int32_t dy = targetPos.y - centerPos.y;

	Direction dir = NORTH;
	if (dx < 0) {
		dir = WEST;
	} else if (dx > 0) {
		dir = EAST;
	} else if (dy < 0) {
		dir = NORTH;
	} else {
		dir = SOUTH;
	}

	if (m_hasExtArea) {
		if (dx < 0 && dy < 0) {
			dir = NORTHWEST;
		} else if (dx > 0 && dy < 0) {
			dir = NORTHEAST;
		} else if (dx < 0 && dy > 0) {
			dir = SOUTHWEST;
		} else if (dx > 0 && dy > 0) {
			dir = SOUTHEAST;
		}
	}
	return getAreaByDirection(dir);
}

const MatrixArea& CombatArea::getAreaByDirection(Direction dir) const
{
	if (dir >= m_areas.size()) {
		static const MatrixArea emptyMatrix;
		return emptyMatrix;
	}
	return m_areas[dir];
}

void CombatArea::setupArea(const std::vector<uint8_t>& vec, uint32_t rows)
{
	MatrixArea area = createArea(vec, rows);
	if (m_areas.empty()) {
		m_areas.resize(4);
	}

	m_areas[EAST] = area.construct(MATRIXOPERATION_ROTATE90);
	m_areas[SOUTH] = area.construct(MATRIXOPERATION_ROTATE180);
	m_areas[WEST] = area.construct(MATRIXOPERATION_ROTATE270);
	m_areas[NORTH] = std::move(area);
}

void CombatArea::setupArea(uint8_t length, uint8_t spread)
{
	uint32_t rows = length;
	int32_t cols = 1;
	if (spread != 0) {
		cols = (((length - length % spread) / spread) << 1) + 1;
	}

	std::vector<uint8_t> vec;
	vec.reserve(static_cast<size_t>(rows) * cols);

	int32_t colSpread = cols;
	for (uint32_t y = 1; y <= rows; ++y) {
		int32_t mincol = cols - colSpread + 1;
		int32_t maxcol = cols - (cols - colSpread);
		for (int32_t x = 1; x <= cols; ++x) {
			if (y == rows && x == ((cols - cols % 2) / 2) + 1) {
				vec.push_back(3);
			} else if (x >= mincol && x <= maxcol) {
				vec.push_back(1);
			} else {
				vec.push_back(0);
			}
		}

		if (spread > 0 && y % spread == 0) {
			--colSpread;
		}
	}

	setupArea(vec, rows);
}

void CombatArea::setupArea(uint8_t radius)
{
	static constexpr uint8_t area[13][13] = {
		{ 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 8, 8, 7, 8, 8, 0, 0, 0, 0 },
		{ 0, 0, 0, 8, 7, 6, 6, 6, 7, 8, 0, 0, 0 },
		{ 0, 0, 8, 7, 6, 5, 5, 5, 6, 7, 8, 0, 0 },
		{ 0, 8, 7, 6, 5, 4, 4, 4, 5, 6, 7, 8, 0 },
		{ 0, 8, 6, 5, 4, 3, 2, 3, 4, 5, 6, 8, 0 },
		{ 8, 7, 6, 5, 4, 2, 1, 2, 4, 5, 6, 7, 8 },
		{ 0, 8, 6, 5, 4, 3, 2, 3, 4, 5, 6, 8, 0 },
		{ 0, 8, 7, 6, 5, 4, 4, 4, 5, 6, 7, 8, 0 },
		{ 0, 0, 8, 7, 6, 5, 5, 5, 6, 7, 8, 0, 0 },
		{ 0, 0, 0, 8, 7, 6, 6, 6, 7, 8, 0, 0, 0 },
		{ 0, 0, 0, 0, 8, 8, 7, 8, 8, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0 }
	};

	std::vector<uint8_t> vec;
	vec.reserve(169); // 13 * 13

	for (auto& row : area) {
		for (uint8_t cell : row) {
			if (cell == 1) {
				vec.push_back(3);
			} else if (cell != 0 && cell <= radius) {
				vec.push_back(1);
			} else {
				vec.push_back(0);
			}
		}
	}

	setupArea(vec, 13);
}

void CombatArea::setupExtArea(const std::vector<uint8_t>& vec, uint32_t rows)
{
	if (vec.empty()) {
		return;
	}

	MatrixArea area = createArea(vec, rows);
	m_hasExtArea = true;

	m_areas.resize(8);
	m_areas[NORTHEAST] = area.construct(MATRIXOPERATION_ROTATE90);
	m_areas[SOUTHEAST] = area.construct(MATRIXOPERATION_ROTATE180);
	m_areas[SOUTHWEST] = area.construct(MATRIXOPERATION_ROTATE270);
	m_areas[NORTHWEST] = std::move(area);
}

//
// MagicField
//
MagicField::MagicField(uint16_t type) :
	Item(type),
	createTime(otx::util::mstime())
{
	//
}

bool MagicField::isBlocking(const Creature* creature) const
{
	if (!isUnstepable()) {
		return Item::isBlocking(creature);
	}

	if (!creature || !creature->getPlayer()) {
		return true;
	}

	const uint32_t ownerId = getOwner();
	if (ownerId == 0) {
		return false;
	}

	if (Creature* owner = g_game.getCreatureByID(ownerId)) {
		return creature->getPlayer()->getGuildEmblem(owner) != GUILDEMBLEM_NONE;
	}
	return false;
}

void MagicField::onStepInField(Creature* creature)
{
	if (!creature) {
		return;
	}

	// remove magic walls/wild growth
	if (isUnstepable() || m_id == ITEM_MAGICWALL || m_id == ITEM_WILDGROWTH || m_id == ITEM_MAGICWALL_SAFE || m_id == ITEM_WILDGROWTH_SAFE || isBlocking(creature)) {
		if (!creature->isGhost()) {
			g_game.internalRemoveItem(creature, this, 1);
		}
		return;
	}

	const ItemType& it = items[m_id];
	if (!it.condition) {
		return;
	}

	uint32_t ownerId = getOwner();
	Tile* tile = getTile();

	Condition* condition = it.condition->clone();
	if (ownerId && !tile->hasFlag(TILESTATE_HARDCOREZONE)) {
		if (Creature* owner = g_game.getCreatureByID(ownerId)) {
			Player* ownerPlayer = owner->getPlayer();
			if (!ownerPlayer && owner->isPlayerSummon()) {
				ownerPlayer = owner->getPlayerMaster();
			}

			bool harmful = true;
			if (ownerPlayer && (tile->hasFlag(TILESTATE_OPTIONALZONE) || g_game.getWorldType() == WORLDTYPE_OPTIONAL)) {
				harmful = false;
			} else if (Player* player = creature->getPlayer()) {
				if (ownerPlayer && Combat::isProtected(ownerPlayer, player)) {
					harmful = false;
				}
			}

			if (!harmful || (otx::util::mstime() - createTime) <= (uint32_t)otx::config::getInteger(otx::config::FIELD_OWNERSHIP) || creature->hasBeenAttacked(ownerId)) {
				condition->setParam(CONDITIONPARAM_OWNER, ownerId);
			}
		}
	}

	creature->addCondition(condition);
}
