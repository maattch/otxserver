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

#include "creature.h"

#include "combat.h"
#include "condition.h"
#include "configmanager.h"
#include "game.h"
#include "player.h"

#include "otx/util.hpp"

#include <iomanip>

Creature::CountBlock_t::CountBlock_t(uint32_t points) :
	start(otx::util::mstime()),
	ticks(start),
	total(points)
{
	//
}

Creature::Creature()
{
	m_id = 0;
	m_tile = nullptr;
	m_direction = SOUTH;
	m_master = nullptr;
	m_lootDrop = LOOT_DROP_FULL;
	m_skillLoss = true;
	m_hideName = m_hideHealth = m_cannotMove = false;
	m_speakType = MSG_NONE;
	m_skull = SKULL_NONE;
	m_partyShield = SHIELD_NONE;
	m_guildEmblem = GUILDEMBLEM_NONE;

	m_health = 1000;
	m_healthMax = 1000;

	m_lastStep = 0;
	m_lastStepCost = 1;
	m_baseSpeed = 220;
	m_varSpeed = 0;

	m_masterRadius = -1;
	m_masterPosition = Position();

	m_followCreature = nullptr;
	m_hasFollowPath = false;
	m_removed = false;
	m_eventWalk = 0;
	m_cancelNextWalk = false;
	m_forceUpdateFollowPath = false;
	m_isMapLoaded = false;
	m_isUpdatingPath = false;
	m_checked = false;
	memset(localMapCache, false, sizeof(localMapCache));

	m_attackedCreature = nullptr;
	m_lastHitCreature = 0;
	m_lastDamageSource = COMBAT_NONE;
	m_blockCount = 0;
	m_blockTicks = 0;
	m_walkUpdateTicks = 0;
	m_checkVector = -1;
	m_lastFailedFollow = 0;

	onIdleStatus();
}

Creature::~Creature()
{
	m_attackedCreature = nullptr;
	removeConditions(CONDITIONEND_CLEANUP, false);
	for (std::list<Creature*>::iterator cit = m_summons.begin(); cit != m_summons.end(); ++cit) {
		(*cit)->setAttackedCreature(nullptr);
		(*cit)->setMaster(nullptr);
		(*cit)->unRef();
	}

	m_summons.clear();
	m_conditions.clear();
	m_eventsList.clear();
}

bool Creature::canSee(const Position& myPos, const Position& pos, uint32_t viewRangeX, uint32_t viewRangeY)
{
	if (myPos.z <= 7) {
		// we are on ground level or above (7 -> 0)
		// view is from 7 -> 0
		if (pos.z > 7) {
			return false;
		} else if (std::abs(myPos.z - pos.z) > 1) {
			return false;
		}
	} else if (myPos.z >= 8) {
		// we are underground (8 -> 15)
		// view is +/- 2 from the floor we stand on
		if (std::abs(myPos.z - pos.z) > 1) {
			return false;
		}
	}

	int32_t offsetz = myPos.z - pos.z;
	return (((uint32_t)pos.x >= myPos.x - viewRangeX + offsetz) && ((uint32_t)pos.x <= myPos.x + viewRangeX + offsetz) && ((uint32_t)pos.y >= myPos.y - viewRangeY + offsetz) && ((uint32_t)pos.y <= myPos.y + viewRangeY + offsetz));
}

bool Creature::canSee(const Position& pos) const
{
	return canSee(getPosition(), pos, Map::maxViewportX, Map::maxViewportY);
}

bool Creature::canSeeCreature(const Creature* creature) const
{
	return creature == this || (!creature->isGhost() && (!creature->isInvisible() || canSeeInvisibility()));
}

bool Creature::canWalkthrough(const Creature* creature) const
{
	if (creature == this) {
		return true;
	}

	if (const Creature* _master = creature->getMaster()) {
		if (_master != this && canWalkthrough(_master)) {
			return true;
		}
	}

	return creature->isGhost() || creature->isWalkable() || (m_master && m_master != creature && m_master->canWalkthrough(creature));
}

int64_t Creature::getTimeSinceLastMove() const
{
	if (m_lastStep) {
		return otx::util::mstime() - m_lastStep;
	}

	return 0x7FFFFFFFFFFFFFFFLL;
}

int32_t Creature::getWalkDelay(Direction dir) const
{
	if (m_lastStep) {
		return getStepDuration(dir) - (otx::util::mstime() - m_lastStep);
	}

	return 0;
}

int32_t Creature::getWalkDelay() const
{
	if (m_lastStep) {
		return getStepDuration() - (otx::util::mstime() - m_lastStep);
	}

	return 0;
}

void Creature::onThink(uint32_t interval)
{
	if (!m_isMapLoaded && useCacheMap()) {
		m_isMapLoaded = true;
		updateMapCache();
	}

	if (m_followCreature && m_master != m_followCreature && !canSeeCreature(m_followCreature)) {
		internalCreatureDisappear(m_followCreature, false);
	}

	if (m_attackedCreature && m_master != m_attackedCreature && !canSeeCreature(m_attackedCreature)) {
		internalCreatureDisappear(m_attackedCreature, false);
	}

	m_blockTicks += interval;
	if (m_blockTicks >= 1000) {
		m_blockCount = std::min((uint32_t)m_blockCount + 1, (uint32_t)2);
		m_blockTicks = 0;
	}

	if (m_followCreature) {
		m_walkUpdateTicks += interval;
		if (m_forceUpdateFollowPath || m_walkUpdateTicks >= 2000) {
			m_walkUpdateTicks = 0;
			m_forceUpdateFollowPath = false;
			m_isUpdatingPath = true;
		}
	}

	if (m_isUpdatingPath) {
		m_isUpdatingPath = false;
		goToFollowCreature();
	}

#ifndef __GROUPED_ATTACKS__
	onAttacking(interval / EVENT_CREATURECOUNT);
#else
	onAttacking(interval);
#endif
	executeConditions(interval);

	CreatureEventList thinkEvents = getCreatureEvents(CREATURE_EVENT_THINK);
	for (CreatureEventList::iterator it = thinkEvents.begin(); it != thinkEvents.end(); ++it) {
		(*it)->executeThink(this, interval);
	}
}

void Creature::onAttacking(uint32_t interval)
{
	if (!m_attackedCreature || m_attackedCreature->getHealth() < 1 || interval < 100) {
		return;
	}

	bool deny = false;
	CreatureEventList attackEvents = getCreatureEvents(CREATURE_EVENT_ATTACK);
	for (CreatureEventList::iterator it = attackEvents.begin(); it != attackEvents.end(); ++it) {
		if (!(*it)->executeAction(this, m_attackedCreature) && !deny) {
			deny = true;
		}
	}

	if (deny) {
		setAttackedCreature(nullptr);
	}

	if (!m_attackedCreature) {
		return;
	}

	onAttacked();
	m_attackedCreature->onAttacked();
	if (g_game.isSightClear(getPosition(), m_attackedCreature->getPosition(), true)) {
		doAttacking(interval);
	}
}

void Creature::onWalk()
{
	if (getWalkDelay() <= 0) {
		Direction dir;
		uint32_t flags = FLAG_IGNOREFIELDDAMAGE;
		if (!getNextStep(dir, flags)) {
			if (m_listWalkDir.empty()) {
				onWalkComplete();
			}

			stopEventWalk();
		} else if (g_game.internalMoveCreature(this, dir, flags) != RET_NOERROR) {
			m_forceUpdateFollowPath = true;
		}
	}

	if (m_cancelNextWalk) {
		m_cancelNextWalk = false;
		m_listWalkDir.clear();
		onWalkAborted();
	}

	if (m_eventWalk) {
		m_eventWalk = 0;
		addEventWalk();
	}
}

void Creature::onWalk(Direction& dir)
{
	int32_t drunk = -1;
	if (!isSuppress(CONDITION_DRUNK)) {
		Condition* condition = nullptr;
		for (ConditionList::const_iterator it = m_conditions.begin(); it != m_conditions.end(); ++it) {
			if (!(condition = *it) || condition->getType() != CONDITION_DRUNK) {
				continue;
			}

			int32_t subId = condition->getSubId();
			if ((!condition->getEndTime() || condition->getEndTime() >= otx::util::mstime()) && subId > drunk) {
				drunk = subId;
			}
		}
	}

	if (drunk < 0) {
		return;
	}

	drunk += 25;
	int32_t r = random_range(1, 100);
	if (r > drunk) {
		return;
	}

	int32_t tmp = (drunk / 5);
	if (r <= tmp) {
		dir = NORTH;
	} else if (r <= (tmp * 2)) {
		dir = WEST;
	} else if (r <= (tmp * 3)) {
		dir = SOUTH;
	} else if (r <= (tmp * 4)) {
		dir = EAST;
	}

	g_game.internalCreatureSay(this, MSG_SPEAK_MONSTER_SAY, "Hicks!", isGhost());
}

bool Creature::getNextStep(Direction& dir, uint32_t&)
{
	if (m_listWalkDir.empty()) {
		return false;
	}

	dir = m_listWalkDir.back();
	m_listWalkDir.pop_back();
	onWalk(dir);
	return true;
}

void Creature::startAutoWalk()
{
	addEventWalk(m_listWalkDir.size() == 1);
}

void Creature::startAutoWalk(Direction dir)
{
	m_listWalkDir.clear();
	m_listWalkDir.push_back(dir);
	addEventWalk(true);
}

void Creature::startAutoWalk(const std::vector<Direction>& listDir)
{
	m_listWalkDir = listDir;
	addEventWalk(listDir.size() == 1);
}

void Creature::addEventWalk(bool firstStep /* = false*/)
{
	m_cancelNextWalk = false;
	if (getStepSpeed() < 1 || m_eventWalk) {
		return;
	}

	int64_t ticks = getEventStepTicks(firstStep);
	if (ticks < 1) {
		return;
	}

	if (ticks == 1) {
		g_game.checkCreatureWalk(getID());
	}

	m_eventWalk = addSchedulerTask(
		std::max<uint32_t>(SCHEDULER_MINTICKS, ticks),
		[creatureID = getID()]() { g_game.checkCreatureWalk(creatureID); });
}

void Creature::stopEventWalk()
{
	if (!m_eventWalk) {
		return;
	}

	g_scheduler.stopEvent(m_eventWalk);
	m_eventWalk = 0;
}

void Creature::updateMapCache()
{
	const Position& pos = getPosition();
	Position dest(0, 0, pos.z);

	Tile* tile = nullptr;
	for (int32_t y = -((mapWalkHeight - 1) / 2); y <= ((mapWalkHeight - 1) / 2); ++y) {
		for (int32_t x = -((mapWalkWidth - 1) / 2); x <= ((mapWalkWidth - 1) / 2); ++x) {
			dest.x = pos.x + x;
			dest.y = pos.y + y;
			if ((tile = g_game.getTile(dest))) {
				updateTileCache(tile, dest);
			}
		}
	}
}

#ifdef __DEBUG__
void Creature::validateMapCache()
{
	const Position& myPos = getPosition();
	for (int32_t y = -((mapWalkHeight - 1) / 2); y <= ((mapWalkHeight - 1) / 2); ++y) {
		for (int32_t x = -((mapWalkWidth - 1) / 2); x <= ((mapWalkWidth - 1) / 2); ++x) {
			getWalkCache(Position(myPos.x + x, myPos.y + y, myPos.z));
		}
	}
}
#endif

void Creature::updateTileCache(const Tile* tile, int32_t dx, int32_t dy)
{
	if ((std::abs(dx) <= (mapWalkWidth - 1) / 2) && (std::abs(dy) <= (mapWalkHeight - 1) / 2)) {
		int32_t x = (mapWalkWidth - 1) / 2 + dx, y = (mapWalkHeight - 1) / 2 + dy;
		localMapCache[y][x] = (tile && tile->__queryAdd(0, this, 1, FLAG_PATHFINDING | FLAG_IGNOREFIELDDAMAGE) == RET_NOERROR);
	}
#ifdef __DEBUG__
	else
		std::clog << "Creature::updateTileCache out of range." << std::endl;
#endif
}

void Creature::updateTileCache(const Tile* tile, const Position& pos)
{
	const Position& myPos = getPosition();
	if (pos.z == myPos.z) {
		updateTileCache(tile, pos.x - myPos.x, pos.y - myPos.y);
	}
}

void Creature::updateTileCache(const Tile* tile)
{
	if (m_isMapLoaded && tile->getPosition().z == getPosition().z) {
		updateTileCache(tile, tile->getPosition());
	}
}

int32_t Creature::getWalkCache(const Position& pos) const
{
	if (!useCacheMap()) {
		return 2;
	}

	const Position& myPos = getPosition();
	if (myPos.z != pos.z) {
		return 0;
	}

	if (pos == myPos) {
		return 1;
	}

	int32_t dx = pos.x - myPos.x, dy = pos.y - myPos.y;
	if ((std::abs(dx) <= (mapWalkWidth - 1) / 2) && (std::abs(dy) <= (mapWalkHeight - 1) / 2)) {
		int32_t x = (mapWalkWidth - 1) / 2 + dx, y = (mapWalkHeight - 1) / 2 + dy;
#ifdef __DEBUG__
		// testing
		Tile* tile = g_game.getTile(pos);
		if (tile && (tile->__queryAdd(0, this, 1, FLAG_PATHFINDING | FLAG_IGNOREFIELDDAMAGE) == RET_NOERROR)) {
			if (!localMapCache[y][x]) {
				std::clog << "Wrong cache value" << std::endl;
			}
		} else if (localMapCache[y][x]) {
			std::clog << "Wrong cache value" << std::endl;
		}

#endif
		if (localMapCache[y][x]) {
			return 1;
		}

		return 0;
	}

	// out of range
	return 2;
}

void Creature::onAddTileItem(const Tile* tile, const Position& pos, const Item*)
{
	if (m_isMapLoaded && pos.z == getPosition().z) {
		updateTileCache(tile, pos);
	}
}

void Creature::onUpdateTileItem(const Tile* tile, const Position& pos, const Item*,
	const ItemType& oldType, const Item*, const ItemType& newType)
{
	if (m_isMapLoaded && (oldType.blockSolid || oldType.blockPathFind || newType.blockPathFind || newType.blockSolid) && pos.z == getPosition().z) {
		updateTileCache(tile, pos);
	}
}

void Creature::onRemoveTileItem(const Tile* tile, const Position& pos, const ItemType& iType, const Item*)
{
	if (m_isMapLoaded && (iType.blockSolid || iType.blockPathFind || iType.isGroundTile()) && pos.z == getPosition().z) {
		updateTileCache(tile, pos);
	}
}

void Creature::onCreatureAppear(const Creature* creature)
{
	if (creature == this) {
		if (useCacheMap()) {
			m_isMapLoaded = true;
			updateMapCache();
		}
	} else if (m_isMapLoaded && creature->getPosition().z == getPosition().z) {
		updateTileCache(creature->getTile(), creature->getPosition());
	}
}

void Creature::internalCreatureDisappear(const Creature* creature, bool isLogout)
{
	if (m_attackedCreature == creature) {
		setAttackedCreature(nullptr);
		onTargetDisappear(isLogout);
	}

	if (m_followCreature == creature) {
		setFollowCreature(nullptr);
		onFollowCreatureDisappear(isLogout);
	}
}

void Creature::onChangeZone(ZoneType_t zone)
{
	if (m_attackedCreature && zone == ZONE_PROTECTION) {
		internalCreatureDisappear(m_attackedCreature, false);
	}
}

void Creature::onTargetChangeZone(ZoneType_t zone)
{
	if (zone == ZONE_PROTECTION) {
		internalCreatureDisappear(m_attackedCreature, false);
	}
}

void Creature::onCreatureMove(const Creature* creature, const Tile* newTile, const Position& newPos,
	const Tile* oldTile, const Position& oldPos, bool teleport)
{
	if (creature == this) {
		if (!oldTile->floorChange() && !oldTile->positionChange()) {
			setLastPosition(oldPos);
		}

		m_lastStep = otx::util::mstime();
		m_lastStepCost = 1;
		if (!teleport) {
			if (oldPos.z != newPos.z) {
				m_lastStepCost = 2;
			} else if (std::abs(newPos.x - oldPos.x) >= 1 && std::abs(newPos.y - oldPos.y) >= 1) {
				m_lastStepCost = 3;
			}
		} else {
			stopEventWalk();
		}

		if (!m_summons.empty() && (!g_config.getBool(ConfigManager::TELEPORT_SUMMONS) || (g_config.getBool(ConfigManager::TELEPORT_PLAYER_SUMMONS) && !getPlayer()))) {
			std::list<Creature*>::iterator cit;
			std::list<Creature*> despawnList;
			for (cit = m_summons.begin(); cit != m_summons.end(); ++cit) {
				const Position pos = (*cit)->getPosition();
				if ((std::abs(pos.z - newPos.z) > 2) || (std::max(std::abs((newPos.x) - pos.x), std::abs((newPos.y - 1) - pos.y)) > 30)) {
					despawnList.push_back(*cit);
				}
			}

			for (cit = despawnList.begin(); cit != despawnList.end(); ++cit) {
				g_game.removeCreature((*cit), true);
			}
		}

		if (newTile->getZone() != oldTile->getZone()) {
			onChangeZone(getZone());
		}

		// update map cache
		if (m_isMapLoaded) {
			if (!teleport && oldPos.z == newPos.z) {
				Tile* tile = nullptr;
				const Position& myPos = getPosition();
				if (oldPos.y > newPos.y) // north
				{
					// shift y south
					for (int32_t y = mapWalkHeight - 1 - 1; y >= 0; --y) {
						memcpy(localMapCache[y + 1], localMapCache[y], sizeof(localMapCache[y]));
					}

					// update 0
					for (int32_t x = -((mapWalkWidth - 1) / 2); x <= ((mapWalkWidth - 1) / 2); ++x) {
						tile = g_game.getTile(myPos.x + x, myPos.y - ((mapWalkHeight - 1) / 2), myPos.z);
						updateTileCache(tile, x, -((mapWalkHeight - 1) / 2));
					}
				} else if (oldPos.y < newPos.y) // south
				{
					// shift y north
					for (int32_t y = 0; y <= mapWalkHeight - 1 - 1; ++y) {
						memcpy(localMapCache[y], localMapCache[y + 1], sizeof(localMapCache[y]));
					}

					// update mapWalkHeight - 1
					for (int32_t x = -((mapWalkWidth - 1) / 2); x <= ((mapWalkWidth - 1) / 2); ++x) {
						tile = g_game.getTile(myPos.x + x, myPos.y + ((mapWalkHeight - 1) / 2), myPos.z);
						updateTileCache(tile, x, (mapWalkHeight - 1) / 2);
					}
				}

				if (oldPos.x < newPos.x) // east
				{
					// shift y west
					int32_t starty = 0, endy = mapWalkHeight - 1, dy = (oldPos.y - newPos.y);
					if (dy < 0) {
						endy = endy + dy;
					} else if (dy > 0) {
						starty = starty + dy;
					}

					for (int32_t y = starty; y <= endy; ++y) {
						for (int32_t x = 0; x <= mapWalkWidth - 1 - 1; ++x) {
							localMapCache[y][x] = localMapCache[y][x + 1];
						}
					}

					// update mapWalkWidth - 1
					for (int32_t y = -((mapWalkHeight - 1) / 2); y <= ((mapWalkHeight - 1) / 2); ++y) {
						tile = g_game.getTile(myPos.x + ((mapWalkWidth - 1) / 2), myPos.y + y, myPos.z);
						updateTileCache(tile, (mapWalkWidth - 1) / 2, y);
					}
				} else if (oldPos.x > newPos.x) // west
				{
					// shift y east
					int32_t starty = 0, endy = mapWalkHeight - 1, dy = (oldPos.y - newPos.y);
					if (dy < 0) {
						endy = endy + dy;
					} else if (dy > 0) {
						starty = starty + dy;
					}

					for (int32_t y = starty; y <= endy; ++y) {
						for (int32_t x = mapWalkWidth - 1 - 1; x >= 0; --x) {
							localMapCache[y][x + 1] = localMapCache[y][x];
						}
					}

					// update 0
					for (int32_t y = -((mapWalkHeight - 1) / 2); y <= ((mapWalkHeight - 1) / 2); ++y) {
						tile = g_game.getTile(myPos.x - ((mapWalkWidth - 1) / 2), myPos.y + y, myPos.z);
						updateTileCache(tile, -((mapWalkWidth - 1) / 2), y);
					}
				}

				updateTileCache(oldTile, oldPos);
#ifdef __DEBUG__
				validateMapCache();
#endif
			} else {
				updateMapCache();
			}
		}
	} else if (m_isMapLoaded) {
		const Position& myPos = getPosition();
		if (newPos.z == myPos.z) {
			updateTileCache(newTile, newPos);
		}

		if (oldPos.z == myPos.z) {
			updateTileCache(oldTile, oldPos);
		}
	}

	if (creature == m_followCreature || (creature == this && m_followCreature)) {
		if (m_hasFollowPath) {
			if (getWalkDelay() <= 0) {
				m_isUpdatingPath = false;
				addDispatcherTask([creatureID = getID()]() { g_game.updateCreatureWalk(creatureID); });
			} else {
				m_isUpdatingPath = true;
			}
		}

		if (newPos.z != oldPos.z || !canSee(m_followCreature->getPosition())) {
			internalCreatureDisappear(m_followCreature, false);
		}
	}

	if (creature == m_attackedCreature || (creature == this && m_attackedCreature)) {
		if (newPos.z == oldPos.z && canSee(m_attackedCreature->getPosition())) {
			if (hasExtraSwing()) { // our target is moving lets see if we can get in hit
				addDispatcherTask([creatureID = getID()]() { g_game.checkCreatureAttack(creatureID); });
			}

			if (newTile->getZone() != oldTile->getZone()) {
				onTargetChangeZone(m_attackedCreature->getZone());
			}
		} else {
			internalCreatureDisappear(m_attackedCreature, false);
		}
	}
}

bool Creature::onDeath()
{
	DeathList deathList = getKillers();
	bool deny = false;

	CreatureEventList prepareDeathEvents = getCreatureEvents(CREATURE_EVENT_PREPAREDEATH);
	for (CreatureEventList::iterator it = prepareDeathEvents.begin(); it != prepareDeathEvents.end(); ++it) {
		if (!(*it)->executePrepareDeath(this, deathList) && !deny) {
			deny = true;
		}
	}

	if (deny) {
		return false;
	}

	int32_t i = 0, size = deathList.size(), limit = g_config.getNumber(ConfigManager::DEATH_ASSISTS) + 1;
	if (limit > 0 && size > limit) {
		size = limit;
	}

	Creature* tmp = nullptr;
	CreatureVector justifyVec;
	for (DeathList::iterator it = deathList.begin(); it != deathList.end(); ++it, ++i) {
		if (it->isNameKill()) {
			continue;
		}

		if (it == deathList.begin()) {
			it->setLast();
		}

		if (i < size) {
			if (it->getKillerCreature()->getPlayer()) {
				tmp = it->getKillerCreature();
			} else if (it->getKillerCreature()->getPlayerMaster()) {
				tmp = it->getKillerCreature()->getMaster();
			}
		}

		if (tmp) {
			if (std::find(justifyVec.begin(), justifyVec.end(), tmp) == justifyVec.end()) {
				it->setJustify();
				justifyVec.push_back(tmp);
			}

			tmp = nullptr;
		}

		if (!it->getKillerCreature()->onKilledCreature(this, (*it)) && it->isLast()) {
			return false;
		}
	}

	for (CountMap::iterator it = m_damageMap.begin(); it != m_damageMap.end(); ++it) {
		if ((tmp = g_game.getCreatureByID(it->first))) {
			tmp->onTargetKilled(this);
		}
	}

	dropCorpse(deathList);
	if (m_master) {
		m_master->removeSummon(this);
	}

	return true;
}

void Creature::dropCorpse(DeathList deathList)
{
	if (m_master) {
		g_game.addMagicEffect(getPosition(), MAGIC_EFFECT_POFF);
		return;
	}

	Item* corpse = createCorpse(deathList);
	if (corpse) {
		corpse->setParent(VirtualCylinder::virtualCylinder);
	}

	bool deny = false;
	CreatureEventList deathEvents = getCreatureEvents(CREATURE_EVENT_DEATH);
	for (CreatureEventList::iterator it = deathEvents.begin(); it != deathEvents.end(); ++it) {
		if (!(*it)->executeDeath(this, corpse, deathList) && !deny) {
			deny = true;
		}
	}

	if (!corpse) {
		return;
	}

	corpse->setParent(nullptr);
	if (deny) {
		return;
	}

	Tile* tile = getTile();
	if (!tile) {
		return;
	}

	Item* splash = nullptr;
	switch (getRace()) {
		case RACE_VENOM:
			splash = Item::CreateItem(ITEM_FULLSPLASH, FLUID_GREEN);
			break;

		case RACE_BLOOD:
			splash = Item::CreateItem(ITEM_FULLSPLASH, FLUID_BLOOD);
			break;

		default:
			break;
	}

	if (splash) {
		g_game.internalAddItem(nullptr, tile, splash, INDEX_WHEREEVER, FLAG_NOLIMIT);
		g_game.startDecay(splash);
	}

	g_game.internalAddItem(nullptr, tile, corpse, INDEX_WHEREEVER, FLAG_NOLIMIT);
	dropLoot(corpse->getContainer());
	g_game.startDecay(corpse);
}

DeathList Creature::getKillers()
{
	DeathList list;
	CountMap::const_iterator it;

	Creature* lhc = nullptr;
	if ((lhc = g_game.getCreatureByID(m_lastHitCreature))) {
		int32_t damage = 0;
		if ((it = m_damageMap.find(m_lastHitCreature)) != m_damageMap.end()) {
			damage = it->second.total;
		}

		list.push_back(DeathEntry(lhc, damage));
	} else {
		list.push_back(DeathEntry(getCombatName(m_lastDamageSource), 0));
	}

	int32_t requiredTime = g_config.getNumber(ConfigManager::DEATHLIST_REQUIRED_TIME);
	int64_t now = otx::util::mstime();

	CountBlock_t cb;
	for (it = m_damageMap.begin(); it != m_damageMap.end(); ++it) {
		cb = it->second;
		if ((now - cb.ticks) > requiredTime) {
			continue;
		}

		Creature* mdc = g_game.getCreatureByID(it->first);
		if (!mdc || mdc == lhc || (lhc && (mdc->getMaster() == lhc || lhc->getMaster() == mdc))) {
			continue;
		}

		bool deny = false;
		for (DeathList::iterator fit = list.begin(); fit != list.end(); ++fit) {
			if (fit->isNameKill()) {
				continue;
			}

			Creature* tmp = fit->getKillerCreature();
			if (!(mdc->getName() == tmp->getName() && mdc->getMaster() == tmp->getMaster()) && (!mdc->getMaster() || (mdc->getMaster() != tmp && mdc->getMaster() != tmp->getMaster()))
				&& (mdc->getSummonCount() <= 0 || tmp->getMaster() != mdc)) {
				continue;
			}

			deny = true;
			break;
		}

		if (!deny) {
			list.push_back(DeathEntry(mdc, cb.total));
		}
	}

	if (list.size() > 1) {
		std::sort(list.begin() + 1, list.end(), DeathLessThan());
	}

	return list;
}

bool Creature::hasBeenAttacked(uint32_t attackerId) const
{
	CountMap::const_iterator it = m_damageMap.find(attackerId);
	if (it != m_damageMap.end()) {
		return (otx::util::mstime() - it->second.ticks) <= g_config.getNumber(ConfigManager::PZ_LOCKED);
	}

	return false;
}

Item* Creature::createCorpse(DeathList)
{
	return Item::CreateItem(getLookCorpse());
}

void Creature::changeHealth(int32_t healthChange)
{
	if (healthChange > 0) {
		m_health += std::min(healthChange, getMaxHealth() - m_health);
	} else {
		m_health = std::max((int32_t)0, m_health + healthChange);
	}

	g_game.addCreatureHealth(this);
}

void Creature::changeMaxHealth(int32_t healthChange)
{
	m_healthMax = healthChange;
	if (m_health > m_healthMax) {
		m_health = m_healthMax;
		g_game.addCreatureHealth(this);
	}
}

bool Creature::getStorage(const std::string& key, std::string& value) const
{
	auto it = m_storageMap.find(key);
	if (it == m_storageMap.end()) {
		value = "-1";
		return false;
	}
	value = it->second;
	return true;
}

bool Creature::setStorage(const std::string& key, const std::string& value)
{
	m_storageMap[key] = value;
	return true;
}

void Creature::gainHealth(Creature* caster, int32_t healthGain)
{
	if (healthGain > 0) {
		int32_t prevHealth = getHealth();
		changeHealth(healthGain);

		int32_t effectiveGain = getHealth() - prevHealth;
		if (caster) {
			caster->onTargetGainHealth(this, effectiveGain);
		}
	} else {
		changeHealth(healthGain);
	}
}

void Creature::drainHealth(Creature* attacker, CombatType_t combatType, int32_t damage)
{
	m_lastDamageSource = combatType;
	onAttacked();

	changeHealth(-damage);
	if (attacker) {
		attacker->onTargetDrainHealth(this, damage);
	}
}

void Creature::drainMana(Creature* attacker, CombatType_t combatType, int32_t damage)
{
	m_lastDamageSource = combatType;
	onAttacked();

	changeMana(-damage);
	if (attacker) {
		attacker->onTargetDrainMana(this, damage);
	}
}

BlockType_t Creature::blockHit(Creature* attacker, CombatType_t combatType, int32_t& damage,
	bool checkDefense /* = false*/, bool checkArmor /* = false*/, bool /* reflect = true*/, bool /* field = false*/, bool /* element = false*/)
{
	BlockType_t blockType = BLOCK_NONE;
	if (isImmune(combatType)) {
		damage = 0;
		blockType = BLOCK_IMMUNITY;
	} else if (checkDefense || checkArmor) {
		bool hasDefense = false;
		if (m_blockCount > 0) {
			--m_blockCount;
			hasDefense = true;
		}

		if (checkDefense && hasDefense) {
			int32_t maxDefense = getDefense(), minDefense = maxDefense / 2;
			damage -= random_range(minDefense, maxDefense);
			if (damage <= 0) {
				damage = 0;
				blockType = BLOCK_DEFENSE;
				checkArmor = false;
			}
		}

		if (checkArmor) {
			int32_t armorValue = getArmor(), minArmorReduction = 0,
					maxArmorReduction = 0;
			if (armorValue > 1) {
				minArmorReduction = (int32_t)std::ceil(armorValue * 0.475);
				maxArmorReduction = (int32_t)std::ceil(
					((armorValue * 0.475) - 1) + minArmorReduction);
			} else if (armorValue == 1) {
				minArmorReduction = 1;
				maxArmorReduction = 1;
			}

			damage -= random_range(minArmorReduction, maxArmorReduction);
			if (damage <= 0) {
				damage = 0;
				blockType = BLOCK_ARMOR;
			}
		}

		if (hasDefense && blockType != BLOCK_NONE) {
			onBlockHit(blockType);
		}
	}

	if (attacker) {
		attacker->onTarget(this);
		attacker->onTargetBlockHit(this, blockType);
	}

	onAttacked();
	return blockType;
}

bool Creature::setAttackedCreature(Creature* creature)
{
	if (creature) {
		const Position& creaturePos = creature->getPosition();
		if (creaturePos.z != getPosition().z || !canSee(creaturePos)) {
			m_attackedCreature = nullptr;
			return false;
		}
	}

	m_attackedCreature = creature;
	if (m_attackedCreature) {
		onTarget(m_attackedCreature);
		m_attackedCreature->onAttacked();
	}

	for (std::list<Creature*>::iterator cit = m_summons.begin(); cit != m_summons.end(); ++cit) {
		(*cit)->setAttackedCreature(creature);
	}

	Condition* condition = getCondition(CONDITION_LOGINPROTECTION, CONDITIONID_DEFAULT);
	if (condition) {
		removeCondition(condition);
	}

	return true;
}

void Creature::getPathSearchParams(const Creature*, FindPathParams& fpp) const
{
	fpp.fullPathSearch = !m_hasFollowPath;
	fpp.clearSight = true;
	fpp.maxSearchDist = 12;
	fpp.minTargetDist = fpp.maxTargetDist = 1;
}

void Creature::goToFollowCreature()
{
	if (getPlayer() && (otx::util::mstime() - m_lastFailedFollow <= g_config.getNumber(ConfigManager::FOLLOW_EXHAUST))) {
		return;
	}

	if (m_followCreature) {
		FindPathParams fpp;
		getPathSearchParams(m_followCreature, fpp);
		if (g_game.getPathToEx(this, m_followCreature->getPosition(), m_listWalkDir, fpp)) {
			m_hasFollowPath = true;
			startAutoWalk(m_listWalkDir);
		} else {
			m_hasFollowPath = false;
			m_lastFailedFollow = otx::util::mstime();
		}
	}

	onFollowCreatureComplete(m_followCreature);
}

bool Creature::setFollowCreature(Creature* creature, bool /*fullPathSearch = false*/)
{
	if (creature) {
		if (m_followCreature == creature) {
			return true;
		}

		const Position& creaturePos = creature->getPosition();
		if (creaturePos.z != getPosition().z || !canSee(creaturePos)) {
			m_followCreature = nullptr;
			return false;
		}

		if (!m_listWalkDir.empty()) {
			m_listWalkDir.clear();
			onWalkAborted();
		}

		m_hasFollowPath = m_forceUpdateFollowPath = false;
		m_followCreature = creature;
		m_isUpdatingPath = true;
	} else {
		m_isUpdatingPath = false;
		m_followCreature = nullptr;
	}

	// g_game.updateCreatureWalk(id);
	onFollowCreature(creature);
	return true;
}

double Creature::getDamageRatio(Creature* attacker) const
{
	double totalDamage = 0, attackerDamage = 0;
	for (CountMap::const_iterator it = m_damageMap.begin(); it != m_damageMap.end(); ++it) {
		totalDamage += it->second.total;
		if (it->first == attacker->getID()) {
			attackerDamage += it->second.total;
		}
	}

	return (totalDamage ? attackerDamage / totalDamage : 0);
}

double Creature::getGainedExperience(Creature* attacker) const
{
	return getDamageRatio(attacker) * (double)getLostExperience();
}

void Creature::addDamagePoints(Creature* attacker, int32_t damagePoints)
{
	if (damagePoints < 0) {
		return;
	}

	uint32_t attackerId = 0;
	if (attacker) {
		attackerId = attacker->getID();
	}

	CountMap::iterator it = m_damageMap.find(attackerId);
	if (it != m_damageMap.end()) {
		it->second.ticks = otx::util::mstime();
		if (damagePoints > 0) {
			it->second.total += damagePoints;
		}
	} else {
		m_damageMap[attackerId] = CountBlock_t(damagePoints);
	}

	if (damagePoints > 0) {
		m_lastHitCreature = attackerId;
	}
}

void Creature::addHealPoints(Creature* caster, int32_t healthPoints)
{
	if (healthPoints <= 0) {
		return;
	}

	uint32_t casterId = 0;
	if (caster) {
		casterId = caster->getID();
	}

	CountMap::iterator it = m_healMap.find(casterId);
	if (it != m_healMap.end()) {
		it->second.ticks = otx::util::mstime();
		it->second.total += healthPoints;
	} else {
		m_healMap[casterId] = CountBlock_t(healthPoints);
	}
}

void Creature::onAddCondition(ConditionType_t type, bool hadCondition)
{
	switch (type) {
		case CONDITION_INVISIBLE: {
			if (!hadCondition) {
				g_game.internalCreatureChangeVisible(this, VISIBLE_DISAPPEAR);
			}

			break;
		}

		case CONDITION_PARALYZE: {
			if (hasCondition(CONDITION_HASTE, -1, false)) {
				removeCondition(CONDITION_HASTE);
			}

			break;
		}

		case CONDITION_HASTE: {
			if (hasCondition(CONDITION_PARALYZE, -1, false)) {
				removeCondition(CONDITION_PARALYZE);
			}

			break;
		}

		default:
			break;
	}
}

void Creature::onEndCondition(ConditionType_t type, ConditionId_t)
{
	if (type == CONDITION_INVISIBLE && !hasCondition(CONDITION_INVISIBLE, -1, false)) {
		g_game.internalCreatureChangeVisible(this, VISIBLE_APPEAR);
	}
}

void Creature::onTickCondition(ConditionType_t type, ConditionId_t, int32_t, bool& _remove)
{
	if (const MagicField* field = getTile()->getFieldItem()) {
		switch (type) {
			case CONDITION_FIRE:
				_remove = field->getCombatType() != COMBAT_FIREDAMAGE;
				break;
			case CONDITION_ENERGY:
				_remove = field->getCombatType() != COMBAT_ENERGYDAMAGE;
				break;
			case CONDITION_POISON:
				_remove = field->getCombatType() != COMBAT_EARTHDAMAGE;
				break;
			case CONDITION_FREEZING:
				_remove = field->getCombatType() != COMBAT_ICEDAMAGE;
				break;
			case CONDITION_DAZZLED:
				_remove = field->getCombatType() != COMBAT_HOLYDAMAGE;
				break;
			case CONDITION_CURSED:
				_remove = field->getCombatType() != COMBAT_DEATHDAMAGE;
				break;
			case CONDITION_DROWN:
				_remove = field->getCombatType() != COMBAT_DROWNDAMAGE;
				break;
			case CONDITION_BLEEDING:
				_remove = field->getCombatType() != COMBAT_PHYSICALDAMAGE;
				break;
			default:
				break;
		}
	}
}

void Creature::onCombatRemoveCondition(const Creature*, Condition* condition)
{
	removeCondition(condition);
}

void Creature::onIdleStatus()
{
	if (getHealth() > 0) {
		m_healMap.clear();
		m_damageMap.clear();
	}
}

void Creature::onTargetDrainHealth(Creature* target, int32_t points)
{
	onTargetDrain(target, points);
}

void Creature::onTargetDrainMana(Creature* target, int32_t points)
{
	onTargetDrain(target, points);
}

void Creature::onTargetDrain(Creature* target, int32_t points)
{
	if (points >= 0) {
		target->addDamagePoints(this, points);
	}
}

void Creature::onTargetGainHealth(Creature* target, int32_t points)
{
	onTargetGain(target, points);
}

void Creature::onTargetGainMana(Creature* target, int32_t points)
{
	onTargetGain(target, points);
}

void Creature::onTargetGain(Creature* target, int32_t points)
{
	if (points > 0) {
		target->addHealPoints(this, points);
	}
}

void Creature::onTargetKilled(Creature* target)
{
	if (target == this) {
		return;
	}

	double exp = target->getGainedExperience(this);
	onGainExperience(exp, target, false);
}

bool Creature::onKilledCreature(Creature* target, DeathEntry& entry)
{
	bool ret = true;
	if (m_master) {
		ret = m_master->onKilledCreature(target, entry);
	}

	CreatureEventList killEvents = getCreatureEvents(CREATURE_EVENT_KILL);
	if (!entry.isLast()) {
		for (CreatureEventList::iterator it = killEvents.begin(); it != killEvents.end(); ++it) {
			(*it)->executeKill(this, target, entry);
		}

		return true;
	}

	for (CreatureEventList::iterator it = killEvents.begin(); it != killEvents.end(); ++it) {
		if (!(*it)->executeKill(this, target, entry) && ret) {
			ret = false;
		}
	}

	return ret;
}

std::string Creature::TransformExpToString(double& gainExp)
{
	std::stringstream sss;
	const long long TRILHAO = 1000000000000LL;
	const long long BILHAO = 1000000000LL;
	const long long MILHAO = 1000000LL;
	const long long MIL = 1000LL;

	sss << "+" << std::fixed << std::setprecision(2); // Defina a precisão de todos os casos

	if ((uint64_t)gainExp >= TRILHAO) {
		sss << (double)(gainExp / TRILHAO) << " Tri Exp";
	} else if ((uint64_t)gainExp >= BILHAO) {
		sss << (double)(gainExp / BILHAO) << " Bi Exp";
	} else if ((uint64_t)gainExp >= MILHAO) {
		sss << (double)(gainExp / MILHAO) << " Mi Exp";
	} else if ((uint64_t)gainExp >= MIL) {
		sss << (double)(gainExp / MIL) << "K Exp";
	} else {
		sss << (double)(gainExp) << " Exp";
	}

	return sss.str();
}

void Creature::onGainExperience(double& gainExp, Creature* target, bool multiplied)
{
	if (gainExp <= 0) {
		return;
	}

	if (m_master) {
		gainExp = gainExp / 2;
		m_master->onGainExperience(gainExp, target, multiplied);
	} else if (!multiplied) {
		gainExp *= g_config.getDouble(ConfigManager::RATE_EXPERIENCE);
	}

	int16_t color = g_config.getNumber(ConfigManager::EXPERIENCE_COLOR);
	if (color < 0) {
		color = random_range(0, 255);
	}

	const Position& targetPos = getPosition();

	SpectatorVec list;
	g_game.getSpectators(list, targetPos, false, true, Map::maxViewportX, Map::maxViewportX,
		Map::maxViewportY, Map::maxViewportY);

	std::ostringstream ss;
	ss << ucfirst(getNameDescription()) << " gained " << (uint64_t)gainExp << " experience points.";

	SpectatorVec textList;
	for (Creature* spectator : list) {
		if (spectator != this) {
			textList.insert(spectator);
		}
	}

	std::string sss;
	if (g_config.getBool(ConfigManager::USEEXP_IN_K)) {
		sss = TransformExpToString((double&)gainExp);
	}

	MessageDetails* details = nullptr;
	if (!g_config.getBool(ConfigManager::USEEXP_IN_K)) {
		details = new MessageDetails((uint32_t)gainExp, (Color_t)color);
		g_game.addStatsMessage(textList, MSG_EXPERIENCE_OTHERS, ss.str(), targetPos, details);
	} else {
		uint16_t colorExp = g_config.getNumber(ConfigManager::EXPERIENCE_COLOR);
		g_game.addAnimatedText(targetPos, (Color_t)colorExp, sss);
	}

	if (Player* player = getPlayer()) {
		ss.str("");
		ss << "You gained " << (uint64_t)gainExp << " experience points.";
		player->sendStatsMessage(MSG_EXPERIENCE, ss.str(), targetPos, details);
	}

	if (details) {
		delete details;
	}
}

void Creature::onGainSharedExperience(double& gainExp, Creature* target, bool multiplied)
{
	if (gainExp <= 0) {
		return;
	}

	if (m_master) {
		gainExp = gainExp / 2;
		m_master->onGainSharedExperience(gainExp, target, multiplied);
	} else if (!multiplied) {
		gainExp *= g_config.getDouble(ConfigManager::RATE_EXPERIENCE);
	}

	int16_t color = g_config.getNumber(ConfigManager::EXPERIENCE_COLOR);
	if (color < 0) {
		color = random_range(0, 255);
	}

	const Position& targetPos = getPosition();

	SpectatorVec list;
	g_game.getSpectators(list, targetPos, false, true, Map::maxViewportX, Map::maxViewportX,
		Map::maxViewportY, Map::maxViewportY);

	std::ostringstream ss;
	ss << ucfirst(getNameDescription()) << " gained " << (uint64_t)gainExp << " experience points.";

	SpectatorVec textList;
	for (Creature* spectator : list) {
		if (spectator != this) {
			textList.insert(spectator);
		}
	}

	std::string sss;
	if (g_config.getBool(ConfigManager::USEEXP_IN_K)) {
		sss = TransformExpToString((double&)gainExp);
	}

	MessageDetails* details = nullptr;
	if (!g_config.getBool(ConfigManager::USEEXP_IN_K)) {
		details = new MessageDetails((uint32_t)gainExp, (Color_t)color);
		g_game.addStatsMessage(textList, MSG_EXPERIENCE_OTHERS, ss.str(), targetPos, details);
	} else {
		uint16_t colorExp = g_config.getNumber(ConfigManager::EXPERIENCE_COLOR);
		g_game.addAnimatedText(targetPos, (Color_t)colorExp, sss);
	}

	if (Player* player = getPlayer()) {
		ss.str("");
		ss << "You gained " << (uint64_t)gainExp << " experience points.";
		player->sendStatsMessage(MSG_EXPERIENCE, ss.str(), targetPos, details);
	}

	if (details) {
		delete details;
	}
}

void Creature::addSummon(Creature* creature)
{
	creature->setDropLoot(LOOT_DROP_NONE);
	creature->setLossSkill(false);

	creature->setMaster(this);
	creature->addRef();
	m_summons.push_back(creature);
}

void Creature::removeSummon(const Creature* creature)
{
	std::list<Creature*>::iterator it = std::find(m_summons.begin(), m_summons.end(), creature);
	if (it == m_summons.end()) {
		return;
	}

	(*it)->setMaster(nullptr);
	(*it)->unRef();
	m_summons.erase(it);
}

void Creature::destroySummons()
{
	for (std::list<Creature*>::iterator it = m_summons.begin(); it != m_summons.end(); ++it) {
		(*it)->setAttackedCreature(nullptr);
		(*it)->changeHealth(-(*it)->getHealth());

		(*it)->setMaster(nullptr);
		(*it)->unRef();
	}

	m_summons.clear();
}

bool Creature::addCondition(Condition* condition)
{
	if (!condition) {
		return false;
	}

	bool hadCondition = hasCondition(condition->getType(), -1, false);
	if (Condition* previous = getCondition(condition->getType(), condition->getId(), condition->getSubId())) {
		previous->addCondition(this, condition);
		delete condition;
		return true;
	}

	if (condition->startCondition(this)) {
		m_conditions.push_back(condition);
		onAddCondition(condition->getType(), hadCondition);
		return true;
	}

	delete condition;
	return false;
}

bool Creature::addCombatCondition(Condition* condition)
{
	bool hadCondition = hasCondition(condition->getType(), -1, false);
	// Caution: condition variable could be deleted after the call to addCondition
	ConditionType_t type = condition->getType();
	if (!addCondition(condition)) {
		return false;
	}

	onAddCombatCondition(type, hadCondition);
	return true;
}

void Creature::removeCondition(ConditionType_t type)
{
	for (ConditionList::iterator it = m_conditions.begin(); it != m_conditions.end();) {
		if ((*it)->getType() != type) {
			++it;
			continue;
		}

		Condition* condition = *it;
		it = m_conditions.erase(it);

		condition->endCondition(this, CONDITIONEND_ABORT);
		onEndCondition(condition->getType(), condition->getId());
		delete condition;
	}
}

void Creature::removeCondition(ConditionType_t type, ConditionId_t conditionId)
{
	for (ConditionList::iterator it = m_conditions.begin(); it != m_conditions.end();) {
		if ((*it)->getType() != type || (*it)->getId() != conditionId) {
			++it;
			continue;
		}

		Condition* condition = *it;
		it = m_conditions.erase(it);

		condition->endCondition(this, CONDITIONEND_ABORT);
		onEndCondition(condition->getType(), condition->getId());
		delete condition;
	}
}

void Creature::removeCondition(Condition* condition)
{
	ConditionList::iterator it = std::find(m_conditions.begin(), m_conditions.end(), condition);
	if (it != m_conditions.end()) {
		Condition* tmpCondition = *it;
		it = m_conditions.erase(it);

		tmpCondition->endCondition(this, CONDITIONEND_ABORT);
		onEndCondition(tmpCondition->getType(), tmpCondition->getId());
		delete tmpCondition;
	}
}

void Creature::removeCondition(const Creature* attacker, ConditionType_t type)
{
	ConditionList tmpList = m_conditions;
	for (ConditionList::iterator it = tmpList.begin(); it != tmpList.end(); ++it) {
		if ((*it)->getType() == type) {
			onCombatRemoveCondition(attacker, *it);
		}
	}
}

void Creature::removeConditions(ConditionEnd_t reason, bool onlyPersistent /* = true*/)
{
	for (ConditionList::iterator it = m_conditions.begin(); it != m_conditions.end();) {
		if (onlyPersistent && !(*it)->isPersistent()) {
			++it;
			continue;
		}

		Condition* condition = *it;
		it = m_conditions.erase(it);

		condition->endCondition(this, reason);
		onEndCondition(condition->getType(), condition->getId());
		delete condition;
	}
}

Condition* Creature::getCondition(ConditionType_t type, ConditionId_t conditionId, uint32_t subId /* = 0*/) const
{
	for (ConditionList::const_iterator it = m_conditions.begin(); it != m_conditions.end(); ++it) {
		if ((*it)->getType() == type && (*it)->getId() == conditionId && (*it)->getSubId() == subId) {
			return *it;
		}
	}

	return nullptr;
}

void Creature::executeConditions(uint32_t interval)
{
	for (ConditionList::iterator it = m_conditions.begin(); it != m_conditions.end();) {
		if ((*it)->executeCondition(this, interval)) {
			++it;
			continue;
		}

		Condition* condition = *it;
		it = m_conditions.erase(it);

		condition->endCondition(this, CONDITIONEND_TICKS);
		onEndCondition(condition->getType(), condition->getId());
		delete condition;
	}
}

bool Creature::hasCondition(ConditionType_t type, int32_t subId /* = 0*/, bool checkTime /* = true*/) const
{
	if (isSuppress(type)) {
		return false;
	}

	Condition* condition = nullptr;
	for (ConditionList::const_iterator it = m_conditions.begin(); it != m_conditions.end(); ++it) {
		if (!(condition = *it) || condition->getType() != type || (subId != -1 && condition->getSubId() != (uint32_t)subId)) {
			continue;
		}

		if (!checkTime || !condition->getEndTime() || condition->getEndTime() >= otx::util::mstime()) {
			return true;
		}
	}

	return false;
}

bool Creature::isImmune(CombatType_t type) const
{
	return ((getDamageImmunities() & (uint32_t)type) == (uint32_t)type);
}

bool Creature::isImmune(ConditionType_t type) const
{
	return ((getConditionImmunities() & (uint32_t)type) == (uint32_t)type);
}

bool Creature::isSuppress(ConditionType_t type) const
{
	return ((getConditionSuppressions() & (uint32_t)type) == (uint32_t)type);
}

std::string Creature::getDescription(int32_t) const
{
	return "a creature";
}

int64_t Creature::getStepDuration(Direction dir) const
{
	if (dir == NORTHWEST || dir == NORTHEAST || dir == SOUTHWEST || dir == SOUTHEAST) {
		return getStepDuration() << 1;
	}

	return getStepDuration();
}

int64_t Creature::getStepDuration() const
{
	if (m_removed) {
		return 0;
	}

	uint32_t stepSpeed = getStepSpeed();
	if (!stepSpeed) {
		return 0;
	}

	const Tile* tile = getTile();
	if (!tile || !tile->ground) {
		return 0;
	}

	return ((1000 * Item::items[tile->ground->getID()].speed) / stepSpeed) * m_lastStepCost;
}

int64_t Creature::getEventStepTicks(bool onlyDelay /* = false*/) const
{
	int64_t ret = getWalkDelay();
	if (ret > 0) {
		return ret;
	}

	if (!onlyDelay) {
		return getStepDuration();
	}

	return 1;
}

void Creature::getCreatureLight(LightInfo& light) const
{
	light = m_internalLight;
}

void Creature::resetLight()
{
	m_internalLight.level = m_internalLight.color = 0;
}

bool Creature::registerCreatureEvent(const std::string& name)
{
	CreatureEvent* newEvent = g_creatureEvents.getEventByName(name);
	if (!newEvent || !newEvent->isLoaded()) { // check for existance
		return false;
	}

	for (const auto& creatureEventType : m_eventsList) {
		for (const auto& creatureEvent : creatureEventType.second) {
			if (creatureEvent == newEvent) {
				return false;
			}
		}
	}

	m_eventsList[newEvent->getEventType()].push_back(newEvent);
	return true;
}

bool Creature::unregisterCreatureEvent(const std::string& name)
{
	CreatureEvent* event = g_creatureEvents.getEventByName(name);
	if (!event || !event->isLoaded()) { // check for existance
		return false;
	}

	for (auto& creatureEventType : m_eventsList) {
		for (CreatureEventList::iterator it = creatureEventType.second.begin(); it != creatureEventType.second.end(); ++it) {
			if ((*it) != event) {
				continue;
			}
			creatureEventType.second.erase(it);
			return true; // we shouldn't have a duplicate
		}
	}

	return false;
}

void Creature::unregisterCreatureEvent(CreatureEventType_t type)
{
	m_eventsList.erase(type);
}

CreatureEventList Creature::getCreatureEvents(CreatureEventType_t type)
{
	return m_eventsList[type];
}

FrozenPathingConditionCall::FrozenPathingConditionCall(const Position& _targetPos)
{
	targetPos = _targetPos;
}

bool FrozenPathingConditionCall::isInRange(const Position& startPos, const Position& testPos,
	const FindPathParams& fpp) const
{
	if (fpp.fullPathSearch) {
		if (testPos.x > targetPos.x + fpp.maxTargetDist) {
			return false;
		}

		if (testPos.x < targetPos.x - fpp.maxTargetDist) {
			return false;
		}

		if (testPos.y > targetPos.y + fpp.maxTargetDist) {
			return false;
		}

		if (testPos.y < targetPos.y - fpp.maxTargetDist) {
			return false;
		}
	} else {
		int_fast32_t dx = Position::getOffsetX(startPos, targetPos);

		int32_t dxMax = (dx >= 0 ? fpp.maxTargetDist : 0);
		if (testPos.x > targetPos.x + dxMax) {
			return false;
		}

		int32_t dxMin = (dx <= 0 ? fpp.maxTargetDist : 0);
		if (testPos.x < targetPos.x - dxMin) {
			return false;
		}

		int_fast32_t dy = Position::getOffsetY(startPos, targetPos);

		int32_t dyMax = (dy >= 0 ? fpp.maxTargetDist : 0);
		if (testPos.y > targetPos.y + dyMax) {
			return false;
		}

		int32_t dyMin = (dy <= 0 ? fpp.maxTargetDist : 0);
		if (testPos.y < targetPos.y - dyMin) {
			return false;
		}
	}
	return true;
}

bool FrozenPathingConditionCall::operator()(const Position& startPos, const Position& testPos,
	const FindPathParams& fpp, int32_t& bestMatchDist) const
{
	if (!isInRange(startPos, testPos, fpp)) {
		return false;
	}

	if (fpp.clearSight && !g_game.isSightClear(testPos, targetPos, true)) {
		return false;
	}

	int32_t testDist = std::max<int32_t>(Position::getDistanceX(targetPos, testPos), Position::getDistanceY(targetPos, testPos));
	if (fpp.maxTargetDist == 1) {
		if (testDist < fpp.minTargetDist || testDist > fpp.maxTargetDist) {
			return false;
		}

		return true;
	} else if (testDist <= fpp.maxTargetDist) {
		if (testDist < fpp.minTargetDist) {
			return false;
		}

		if (testDist == fpp.maxTargetDist) {
			bestMatchDist = 0;
			return true;
		} else if (testDist > bestMatchDist) {
			// not quite what we want, but the best so far
			bestMatchDist = testDist;
			return true;
		}
	}
	return false;
}
