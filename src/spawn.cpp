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

#include "spawn.h"

#include "configmanager.h"
#include "game.h"
#include "npc.h"
#include "player.h"
#include "scheduler.h"
#include "tools.h"

#include "otx/util.hpp"

#define MINSPAWN_INTERVAL 1000

Spawns::Spawns()
{
	loaded = started = false;
}

Spawns::~Spawns()
{
	if (started) {
		clear();
	}
}

bool Spawns::loadFromXml(const std::string& _filename)
{
	if (isLoaded()) {
		return true;
	}

	filename = _filename;
	xmlDocPtr doc = xmlParseFile(filename.c_str());
	if (!doc) {
		std::clog << "[Warning - Spawns::loadFromXml] Cannot open spawns file." << std::endl;
		std::clog << getLastXMLError() << std::endl;
		return false;
	}

	xmlNodePtr root = xmlDocGetRootElement(doc);
	if (xmlStrcmp(root->name, (const xmlChar*)"spawns")) {
		std::clog << "[Error - Spawns::loadFromXml] Malformed spawns file." << std::endl;
		xmlFreeDoc(doc);
		return false;
	}

	for (xmlNodePtr p = root->children; p; p = p->next) {
		parseSpawnNode(p, false);
	}

	xmlFreeDoc(doc);
	loaded = true;
	return true;
}

bool Spawns::parseSpawnNode(xmlNodePtr p, bool checkDuplicate)
{
	if (xmlStrcmp(p->name, (const xmlChar*)"spawn")) {
		return false;
	}

	int32_t intValue;
	std::string strValue;

	Position centerPos;
	if (!readXMLString(p, "centerpos", strValue)) {
		if (!readXMLInteger(p, "centerx", intValue)) {
			return false;
		}

		centerPos.x = intValue;
		if (!readXMLInteger(p, "centery", intValue)) {
			return false;
		}

		centerPos.y = intValue;
		if (!readXMLInteger(p, "centerz", intValue)) {
			return false;
		}

		centerPos.z = intValue;
	} else {
		IntegerVec posVec = vectorAtoi(explodeString(strValue, ","));
		if (posVec.size() < 3) {
			return false;
		}

		centerPos = Position(posVec[0], posVec[1], posVec[2]);
	}

	if (!readXMLInteger(p, "radius", intValue)) {
		return false;
	}

	int32_t radius = intValue;
	Spawn* spawn = new Spawn(centerPos, radius);
	if (checkDuplicate) {
		for (SpawnList::iterator it = spawnList.begin(); it != spawnList.end(); ++it) {
			if ((*it)->getPosition() == centerPos) {
				delete *it;
			}
		}
	}

	spawnList.push_back(spawn);
	for (xmlNodePtr tmpNode = p->children; tmpNode; tmpNode = tmpNode->next) {
		if (!xmlStrcmp(tmpNode->name, (const xmlChar*)"monster")) {
			if (!readXMLString(tmpNode, "name", strValue)) {
				continue;
			}

			std::string name = strValue;
			int32_t interval = MINSPAWN_INTERVAL / 1000;
			if (readXMLInteger(tmpNode, "spawntime", intValue) || readXMLInteger(tmpNode, "interval", intValue)) {
				if (intValue <= interval) {
					std::clog << "[Warning - Spawns::loadFromXml] " << name << " " << centerPos << " spawntime cannot"
							  << " be less or equal than " << interval << " seconds." << std::endl;
					continue;
				}

				interval = intValue;
			}

			interval *= 1000;
			interval /= 2;
			Position placePos = centerPos;
			if (readXMLInteger(tmpNode, "x", intValue)) {
				placePos.x += intValue;
			}

			if (readXMLInteger(tmpNode, "y", intValue)) {
				placePos.y += intValue;
			}

			if (readXMLInteger(tmpNode, "z", intValue)) {
				placePos.z /*+*/ = intValue;
			}

			Direction direction = NORTH;
			if (readXMLInteger(tmpNode, "direction", intValue) && direction >= EAST && direction <= WEST) {
				direction = (Direction)intValue;
			}

			spawn->addMonster(name, placePos, direction, interval);
		} else if (!xmlStrcmp(tmpNode->name, (const xmlChar*)"npc")) {
			if (!readXMLString(tmpNode, "name", strValue)) {
				continue;
			}

			std::string name = strValue;
			Position placePos = centerPos;
			if (readXMLInteger(tmpNode, "x", intValue)) {
				placePos.x += intValue;
			}

			if (readXMLInteger(tmpNode, "y", intValue)) {
				placePos.y += intValue;
			}

			if (readXMLInteger(tmpNode, "z", intValue)) {
				placePos.z /*+*/ = intValue;
			}

			Direction direction = NORTH;
			if (readXMLInteger(tmpNode, "direction", intValue) && direction >= EAST && direction <= WEST) {
				direction = (Direction)intValue;
			}

			Npc* npc = Npc::createNpc(name);
			if (!npc) {
				continue;
			}

			npc->setMasterPosition(placePos, radius);
			npc->setDirection(direction);
			npcList.push_back(npc);
		}
	}

	return true;
}

void Spawns::startup()
{
	if (!isLoaded() || isStarted()) {
		return;
	}

	for (NpcList::iterator it = npcList.begin(); it != npcList.end(); ++it) {
		g_game.placeCreature((*it), (*it)->getMasterPosition(), false, true);
	}

	npcList.clear();
	for (SpawnList::iterator it = spawnList.begin(); it != spawnList.end(); ++it) {
		(*it)->startup();
	}

	started = true;
}

void Spawns::clear()
{
	started = false;
	for (SpawnList::iterator it = spawnList.begin(); it != spawnList.end(); ++it) {
		delete (*it);
	}

	spawnList.clear();
	loaded = false;
	filename = std::string();
}

bool Spawns::isInZone(const Position& centerPos, int32_t radius, const Position& pos)
{
	if (radius == -1) {
		return true;
	}

	return ((pos.x >= centerPos.x - radius) && (pos.x <= centerPos.x + radius) && (pos.y >= centerPos.y - radius) && (pos.y <= centerPos.y + radius));
}

void Spawn::startEvent(MonsterType* mType)
{
	if (m_checkSpawnEvent == 0) {
		m_checkSpawnEvent = addSchedulerTask(getInterval() / g_game.spawnDivider(mType), [this]() { checkSpawn(); });
	}
}

Spawn::~Spawn()
{
	stopEvent();
	for (const auto& it : m_spawnedMap) {
		it.second->setSpawn(nullptr);
		if (!it.second->isRemoved()) {
			g_game.freeThing(it.second);
		}
	}

	m_spawnedMap.clear();
	m_spawnMap.clear();
}

bool Spawn::findPlayer(const Position& pos, bool blocked)
{
	SpectatorVec list;
	g_game.getSpectators(list, pos, false, true);
	for (Creature* spectator : list) {
		if (blocked && !spectator->getPlayer()->hasFlag(PlayerFlag_IgnoredByMonsters)) {
			return true;
		} else if (!blocked && (spectator->getPlayer()->getSkull() == SKULL_WHITE)) {
			// If allowBlockSpawn is false, monsters can be summoned on screen,
			// this part is preventing infinite PK situations.
			return true;
		}
	}
	return false;
}

bool Spawn::spawnMonster(uint32_t spawnId, MonsterType* mType, const Position& pos, Direction dir, bool startup /*= false*/)
{
	Monster* monster = Monster::createMonster(mType);
	if (!monster) {
		return false;
	}

	if (startup) {
		// No need to send out events to the surrounding since there is no one out there to listen!
		if (!g_game.internalPlaceCreature(monster, pos, false, true)) {
			delete monster;
			return false;
		}
	} else if (!g_game.placeCreature(monster, pos, false, true)) {
		delete monster;
		return false;
	}

	monster->setSpawn(this);
	monster->setMasterPosition(pos, m_radius);
	monster->setDirection(dir);

	monster->addRef();
	m_spawnedMap.insert(SpawnedPair(spawnId, monster));
	m_spawnMap[spawnId].lastSpawn = otx::util::mstime();
	return true;
}

void Spawn::startup()
{
	for (const auto& [spawnId, sb] : m_spawnMap) {
		spawnMonster(spawnId, sb.mType, sb.pos, sb.direction, true);
	}
}

void Spawn::checkSpawn()
{
#ifdef __DEBUG_SPAWN__
	std::clog << "[Notice] Spawn::checkSpawn " << this << std::endl;
#endif
	const int64_t timeNow = otx::util::mstime();

	m_checkSpawnEvent = 0;
	for (auto it = m_spawnedMap.begin(); it != m_spawnedMap.end(); ) {
		if (it->second->isRemoved()) {
			if (it->first != 0) {
				m_spawnMap[it->first].lastSpawn = timeNow;
			}

			it->second->unRef();
			it = m_spawnedMap.erase(it);
		} else {
			++it;
		}
	}

	uint32_t spawnCount = 0;
	uint32_t interval = 1;

	for (auto& [spawnId, sb] : m_spawnMap) {
		interval = g_game.spawnDivider(sb.mType);
		if (m_spawnedMap.count(spawnId) != 0) {
			continue;
		}

		if (timeNow < sb.lastSpawn + (sb.interval / interval)) {
			continue;
		}

		bool block = otx::config::getBoolean(otx::config::ALLOW_BLOCK_SPAWN);
		if (findPlayer(sb.pos, block)) {
			sb.lastSpawn = timeNow;
			continue;
		}

		spawnMonster(spawnId, sb.mType, sb.pos, sb.direction);
		uint32_t minSpawnCount = otx::config::getInteger(otx::config::RATE_SPAWN_MIN),
				 maxSpawnCount = otx::config::getInteger(otx::config::RATE_SPAWN_MAX);
		if (++spawnCount >= (uint32_t)random_range(minSpawnCount, maxSpawnCount)) {
			break;
		}
	}

	if (m_spawnedMap.size() < m_spawnMap.size()) {
		m_checkSpawnEvent = addSchedulerTask(getInterval() / interval, [this]() { checkSpawn(); });
	}
#ifdef __DEBUG_SPAWN__
	else {
		std::clog << "[Notice] Spawn::checkSpawn stopped " << this << std::endl;
	}
#endif
}

bool Spawn::addMonster(const std::string& _name, const Position& _pos, Direction _dir, uint32_t _interval)
{
	if (!g_game.getTile(_pos)) {
		std::clog << "[Spawn::addMonster] nullptr tile at spawn position (" << _pos << ")" << std::endl;
		return false;
	}

	MonsterType* mType = g_monsters.getMonsterType(_name);
	if (!mType) {
		std::clog << "[Spawn::addMonster] Cannot find \"" << _name << "\"" << std::endl;
		return false;
	}

	if (_interval < m_interval) {
		m_interval = _interval;
	}

	spawnBlock_t sb;
	sb.mType = mType;
	sb.pos = _pos;
	sb.direction = _dir;
	sb.interval = _interval;
	sb.lastSpawn = 0;

	uint32_t spawnId = (int32_t)m_spawnMap.size() + 1;
	m_spawnMap[spawnId] = sb;
	return true;
}

void Spawn::removeMonster(Monster* monster)
{
	for (auto it = m_spawnedMap.begin(); it != m_spawnedMap.end(); ++it) {
		if (it->second != monster) {
			continue;
		}

		monster->unRef();
		m_spawnedMap.erase(it);
		break;
	}
}

void Spawn::stopEvent()
{
	if (m_checkSpawnEvent == 0) {
		return;
	}

	g_scheduler.stopEvent(m_checkSpawnEvent);
	m_checkSpawnEvent = 0;
}
