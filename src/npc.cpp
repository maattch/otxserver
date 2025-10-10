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

#include "npc.h"

#include "game.h"
#include "lua_definitions.h"
#include "position.h"
#include "spawn.h"
#include "spells.h"

#include "otx/util.hpp"

Npcs g_npcs;

uint32_t Npc::npcAutoID = 0x80000000;

#if ENABLE_SERVER_DIAGNOSTIC > 0
uint32_t Npc::npcCount = 0;
#endif

NpcScript* Npc::m_interface = nullptr;

Npcs::~Npcs()
{
	for (DataMap::iterator it = data.begin(); it != data.end(); ++it) {
		delete it->second;
	}

	data.clear();
}

bool Npcs::loadFromXml(bool reloading /* = false*/)
{
	xmlDocPtr doc = xmlParseFile(getFilePath(FILE_TYPE_OTHER, "npc/npcs.xml").c_str());
	if (!doc) {
		std::clog << "[Warning - Npcs::loadFromXml] Cannot load npcs file." << std::endl;
		std::clog << getLastXMLError() << std::endl;
		return false;
	}

	xmlNodePtr root = xmlDocGetRootElement(doc);
	if (xmlStrcmp(root->name, (const xmlChar*)"npcs")) {
		std::clog << "[Error - Npcs::loadFromXml] Malformed npcs file." << std::endl;
		return false;
	}

	for (xmlNodePtr p = root->children; p; p = p->next) {
		if (p->type != XML_ELEMENT_NODE) {
			continue;
		}

		if (xmlStrcmp(p->name, (const xmlChar*)"npc")) {
			std::clog << "[Warning - Npcs::loadFromXml] Unknown node name: " << p->name << "." << std::endl;
			continue;
		} else {
			parseNpcNode(p, FILE_TYPE_OTHER, reloading);
		}
	}

	return true;
}

bool Npcs::parseNpcNode(xmlNodePtr node, FileType_t path, bool reloading /* = false*/)
{
	std::string name;
	if (!readXMLString(node, "name", name)) {
		std::clog << "[Warning - Npcs::parseNpcNode] Missing npc name!" << std::endl;
		return false;
	}

	bool new_nType = false;
	NpcType* nType = nullptr;
	if (!(nType = getType(name))) {
		new_nType = true;
	} else if (reloading) {
		std::clog << "[Warning - Npcs::parseNpcNode] Duplicate registered npc with name: " << name << "." << std::endl;
		return false;
	}

	std::string strValue;
	if (!readXMLString(node, "file", strValue) && !readXMLString(node, "path", strValue)) {
		std::clog << "[Warning - Npcs::loadFromXml] Missing file path for npc with name " << name << "." << std::endl;
		return false;
	}

	if (new_nType) {
		nType = new NpcType();
	}

	nType->name = name;
	otx::util::to_lower_string(name);

	nType->file = getFilePath(path, "npc/" + strValue);
	if (readXMLString(node, "nameDescription", strValue) || readXMLString(node, "namedescription", strValue)) {
		nType->nameDescription = strValue;
	}

	if (readXMLString(node, "script", strValue)) {
		nType->script = strValue;
	}

	for (xmlNodePtr q = node->children; q; q = q->next) {
		if (!xmlStrcmp(q->name, (const xmlChar*)"look")) {
			int32_t intValue;
			if (readXMLInteger(q, "type", intValue)) {
				nType->outfit.lookType = intValue;
				if (readXMLInteger(q, "head", intValue)) {
					nType->outfit.lookHead = intValue;
				}

				if (readXMLInteger(q, "body", intValue)) {
					nType->outfit.lookBody = intValue;
				}

				if (readXMLInteger(q, "legs", intValue)) {
					nType->outfit.lookLegs = intValue;
				}

				if (readXMLInteger(q, "feet", intValue)) {
					nType->outfit.lookFeet = intValue;
				}

				if (readXMLInteger(q, "addons", intValue)) {
					nType->outfit.lookAddons = intValue;
				}
			} else if (readXMLInteger(q, "typeex", intValue)) {
				nType->outfit.lookTypeEx = intValue;
			}
		}
	}

	if (new_nType) {
		data[name] = nType;
	}

	return true;
}

void Npcs::reload()
{
	for (const auto& it : g_game.getNpcs()) {
		it.second->closeAllShopWindows();
	}

	delete Npc::m_interface;
	Npc::m_interface = nullptr;

	if (fileExists(getFilePath(FILE_TYPE_OTHER, "npc/npcs.xml").c_str())) {
		DataMap tmp = data;
		if (!loadFromXml()) {
			data = tmp;
		}

		tmp.clear();
	}

	for (const auto& it : g_game.getNpcs()) {
		it.second->reload();
	}
}

NpcType* Npcs::getType(const std::string& name) const
{
	DataMap::const_iterator it = data.find(otx::util::as_lower_string(name));
	if (it == data.end()) {
		return nullptr;
	}
	return it->second;
}

bool Npcs::setType(std::string name, NpcType* nType)
{
	otx::util::to_lower_string(name);
	DataMap::const_iterator it = data.find(name);
	if (it != data.end()) {
		return false;
	}

	data[name] = nType;
	return true;
}

Npc* Npc::createNpc(NpcType* nType)
{
	Npc* npc = new Npc(nType);
	if (!npc) {
		return nullptr;
	}

	if (npc->load()) {
		return npc;
	}

	delete npc;
	return nullptr;
}

Npc* Npc::createNpc(const std::string& name)
{
	NpcType* nType = nullptr;
	if (!(nType = g_npcs.getType(name))) {
		nType = new NpcType();
		nType->file = getFilePath(FILE_TYPE_OTHER, "npc/" + name + ".xml");
		if (!fileExists(nType->file.c_str())) {
			std::clog << "[Warning - Npc::createNpc] Cannot find npc with name: " << name << "." << std::endl;
			return nullptr;
		}

		nType->name = name;
		g_npcs.setType(name, nType);
	}

	return createNpc(nType);
}

Npc::Npc(NpcType* _nType) :
	Creature(),
	m_npcEventHandler(nullptr)
{
#if ENABLE_SERVER_DIAGNOSTIC > 0
	++Npc::npcCount;
#endif
	nType = _nType;

	m_npcEventHandler = nullptr;
	reset();
}

Npc::~Npc()
{
	reset();
#if ENABLE_SERVER_DIAGNOSTIC > 0
	--Npc::npcCount;
#endif
}

void Npc::reset()
{
	loaded = false;
	walkTicks = 1500;
	floorChange = false;
	attackable = false;
	walkable = false;
	focusCreature = 0;
	isIdle = true;
	talkRadius = 2;
	idleTime = 0;
	idleInterval = 5 * 60;
	lastVoice = otx::util::mstime();
	baseDirection = SOUTH;
	if (m_npcEventHandler) {
		delete m_npcEventHandler;
	}

	m_npcEventHandler = nullptr;
	m_parameters.clear();
	shopPlayerList.clear();
	voiceList.clear();
}

void Npc::addList()
{
	g_game.addNpc(this);
}

void Npc::removeList()
{
	g_game.removeNpc(this);
}

bool Npc::load()
{
	if (isLoaded()) {
		return true;
	}

	if (nType->file.empty()) {
		std::clog << "[Warning - Npc::load] Cannot load npc with name: " << nType->name << "." << std::endl;
		return false;
	}

	if (!m_interface) {
		m_interface = new NpcScript();
		m_interface->loadDirectory(getFilePath(FILE_TYPE_OTHER, "npc/lib"), false, true, this);
	}

	loaded = loadFromXml();
	m_defaultOutfit = m_currentOutfit = nType->outfit;
	return isLoaded();
}

void Npc::reload()
{
	reset();
	load();
	// Simulate that the creature is placed on the map again.
	if (m_npcEventHandler) {
		m_npcEventHandler->onCreatureAppear(this);
	}

	if (walkTicks) {
		addEventWalk();
	}
}

bool Npc::loadFromXml()
{
	xmlDocPtr doc = xmlParseFile(nType->file.c_str());
	if (!doc) {
		std::clog << "[Warning - Npc::loadFromXml] Cannot load npc file: " << nType->file << "." << std::endl;
		std::clog << getLastXMLError() << std::endl;
		return false;
	}

	xmlNodePtr root = xmlDocGetRootElement(doc);
	if (xmlStrcmp(root->name, (const xmlChar*)"npc")) {
		std::clog << "[Warning - Npc::loadFromXml] Malformed npc file: " << nType->file << "." << std::endl;
		xmlFreeDoc(doc);
		return false;
	}

	int32_t intValue;
	std::string strValue;
	if (readXMLString(root, "name", strValue)) {
		nType->name = strValue;
	}

	if (readXMLString(root, "script", strValue)) {
		nType->script = strValue;
	}

	if (readXMLString(root, "namedescription", strValue) || readXMLString(root, "nameDescription", strValue)) {
		nType->nameDescription = strValue;
	}

	if (!nType->nameDescription.empty()) {
		otx::util::replace_all(nType->nameDescription, "|NAME|", nType->name);
	} else {
		nType->nameDescription = nType->name;
	}

	if (readXMLString(root, "hidename", strValue) || readXMLString(root, "hideName", strValue)) {
		m_hideName = booleanString(strValue);
	}

	if (readXMLString(root, "hidehealth", strValue) || readXMLString(root, "hideHealth", strValue)) {
		m_hideHealth = booleanString(strValue);
	}

	m_baseSpeed = 100;
	if (readXMLInteger(root, "speed", intValue)) {
		m_baseSpeed = intValue;
	}

	if (readXMLString(root, "attackable", strValue)) {
		attackable = booleanString(strValue);
	}

	if (readXMLString(root, "walkable", strValue)) {
		walkable = booleanString(strValue);
	}

	if (readXMLInteger(root, "autowalk", intValue)) {
		std::clog << "[Notice - Npc::Npc] NPC: " << nType->name << " - autowalk attribute has been deprecated, use walkinterval instead." << std::endl;
	}

	if (readXMLInteger(root, "walkinterval", intValue)) {
		walkTicks = intValue;
	}

	if (readXMLInteger(root, "direction", intValue) && intValue >= NORTH && intValue <= WEST) {
		m_direction = (Direction)intValue;
		baseDirection = m_direction;
	}

	if (readXMLString(root, "floorchange", strValue)) {
		floorChange = booleanString(strValue);
	}

	if (readXMLString(root, "skull", strValue)) {
		setSkull(getSkulls(strValue));
	}

	if (readXMLString(root, "shield", strValue)) {
		setShield(getShields(strValue));
	}

	if (readXMLString(root, "emblem", strValue)) {
		setEmblem(getEmblems(strValue));
	}

	for (xmlNodePtr p = root->children; p; p = p->next) {
		if (!xmlStrcmp(p->name, (const xmlChar*)"health")) {
			if (readXMLInteger(p, "now", intValue)) {
				m_health = intValue;
			} else {
				m_health = 100;
			}

			if (readXMLInteger(p, "max", intValue)) {
				m_healthMax = intValue;
			} else {
				m_healthMax = 100;
			}
		} else if (!xmlStrcmp(p->name, (const xmlChar*)"look")) {
			if (readXMLInteger(p, "type", intValue)) {
				nType->outfit.lookType = intValue;
				if (readXMLInteger(p, "head", intValue)) {
					nType->outfit.lookHead = intValue;
				}

				if (readXMLInteger(p, "body", intValue)) {
					nType->outfit.lookBody = intValue;
				}

				if (readXMLInteger(p, "legs", intValue)) {
					nType->outfit.lookLegs = intValue;
				}

				if (readXMLInteger(p, "feet", intValue)) {
					nType->outfit.lookFeet = intValue;
				}

				if (readXMLInteger(p, "addons", intValue)) {
					nType->outfit.lookAddons = intValue;
				}
			} else if (readXMLInteger(p, "typeex", intValue)) {
				nType->outfit.lookTypeEx = intValue;
			}
		} else if (!xmlStrcmp(p->name, (const xmlChar*)"voices")) {
			for (xmlNodePtr q = p->children; q != nullptr; q = q->next) {
				if (!xmlStrcmp(q->name, (const xmlChar*)"voice")) {
					if (!readXMLString(q, "text", strValue)) {
						continue;
					}

					Voice voice;
					voice.text = strValue;
					if (readXMLInteger(q, "interval2", intValue)) {
						voice.interval = intValue;
					} else {
						voice.interval = 60;
					}

					if (readXMLInteger(q, "margin", intValue)) {
						voice.margin = intValue;
					} else {
						voice.margin = 0;
					}

					voice.type = MSG_SPEAK_SAY;
					if (readXMLInteger(q, "type", intValue)) {
						voice.type = (MessageClasses)intValue;
					} else if (readXMLString(q, "yell", strValue) && booleanString(strValue)) {
						voice.type = MSG_SPEAK_YELL;
					}

					if (readXMLString(q, "randomspectator", strValue) || readXMLString(q, "randomSpectator", strValue)) {
						voice.randomSpectator = booleanString(strValue);
					} else {
						voice.randomSpectator = false;
					}

					voiceList.push_back(voice);
				}
			}
		} else if (!xmlStrcmp(p->name, (const xmlChar*)"parameters")) {
			for (xmlNodePtr q = p->children; q != nullptr; q = q->next) {
				if (!xmlStrcmp(q->name, (const xmlChar*)"parameter")) {
					std::string paramKey, paramValue;
					if (!readXMLString(q, "key", paramKey)) {
						continue;
					}

					if (!readXMLString(q, "value", paramValue)) {
						continue;
					}

					m_parameters[paramKey] = paramValue;
				}
			}
		}
	}

	xmlFreeDoc(doc);
	if (nType->script.empty()) {
		return true;
	}

	if (nType->script.find("/") != std::string::npos) {
		otx::util::replace_all(nType->script, "|DATA|", getFilePath(FILE_TYPE_OTHER, "npc/scripts"));
	} else {
		nType->script = getFilePath(FILE_TYPE_OTHER, "npc/scripts/" + nType->script);
	}

	m_npcEventHandler = new NpcEvents(nType->script, this);
	return m_npcEventHandler->isLoaded();
}

bool Npc::canSee(const Position& pos) const
{
	Position tmp = getPosition();
	if (pos.z != tmp.z) {
		return false;
	}
	return Creature::canSee(tmp, pos, 3, 3);
}

void Npc::onCreatureAppear(const Creature* creature)
{
	Creature::onCreatureAppear(creature);
	if (creature == this) {
		if (walkTicks) {
			addEventWalk();
		}

		if (m_npcEventHandler) {
			m_npcEventHandler->onCreatureAppear(creature);
		}
		return;
	}

	if (m_npcEventHandler) {
		m_npcEventHandler->onCreatureAppear(creature);
	}
}

void Npc::onCreatureDisappear(const Creature* creature, bool isLogout)
{
	Creature::onCreatureDisappear(creature, isLogout);
	if (creature == this) {
		closeAllShopWindows();
		return;
	}

	if (m_npcEventHandler) {
		m_npcEventHandler->onCreatureDisappear(creature);
	}
}

void Npc::onCreatureMove(const Creature* creature, const Tile* newTile, const Position& newPos,
	const Tile* oldTile, const Position& oldPos, bool teleport)
{
	Creature::onCreatureMove(creature, newTile, newPos, oldTile, oldPos, teleport);
	if (m_npcEventHandler) {
		m_npcEventHandler->onCreatureMove(creature, oldPos, newPos);
	}
}

void Npc::onCreatureSay(const Creature* creature, MessageClasses type, const std::string& text, Position* pos /* = nullptr*/)
{
	if (m_npcEventHandler) {
		m_npcEventHandler->onCreatureSay(creature, type, text, pos);
	}
}

void Npc::onPlayerCloseChannel(const Player* player)
{
	if (m_npcEventHandler) {
		m_npcEventHandler->onPlayerCloseChannel(player);
	}
}

void Npc::onPlayerLeave(Player* player)
{
	if (player->getShopOwner() == this) {
		player->closeShopWindow();
	}
}

void Npc::onThink(uint32_t interval)
{
	Creature::onThink(interval);
	if (m_npcEventHandler) {
		m_npcEventHandler->onThink();
	}

	std::vector<Player*> list;
	Player* tmpPlayer = nullptr;

	SpectatorVec tmpList;
	g_game.getSpectators(tmpList, getPosition(), true, true);

	if (tmpList.size()) // loop only if there's at least one spectator
	{
		for (SpectatorVec::const_iterator it = tmpList.begin(); it != tmpList.end(); ++it) {
			if ((tmpPlayer = (*it)->getPlayer()) && !tmpPlayer->isRemoved()) {
				list.push_back(tmpPlayer);
			}
		}
	}

	if (list.size()) // loop only if there's at least one player
	{
		int64_t now = otx::util::mstime();
		for (VoiceList::iterator it = voiceList.begin(); it != voiceList.end(); ++it) {
			if (now < (lastVoice + it->margin)) {
				continue;
			}

			if ((uint32_t)(MAX_RAND_RANGE / it->interval) < (uint32_t)random_range(0, MAX_RAND_RANGE)) {
				continue;
			}

			tmpPlayer = nullptr;
			if (it->randomSpectator) {
				size_t random = random_range(0, (int32_t)list.size());
				if (random < list.size()) { // 1 slot chance to make it public
					tmpPlayer = list[random];
				}
			}

			doSay(it->text, it->type, tmpPlayer);
			lastVoice = now;
			break;
		}
	}

	if (!isIdle && getTimeSinceLastMove() >= walkTicks) {
		addEventWalk();
	}
}

void Npc::doSay(const std::string& text, MessageClasses type, Player* player)
{
	if (!player) {
		std::string tmp = text;
		otx::util::replace_all(tmp, "{", "");
		otx::util::replace_all(tmp, "}", "");
		g_game.internalCreatureSay(this, type, tmp, player && player->isGhost());
	} else {
		player->sendCreatureSay(this, type, text);
		player->onCreatureSay(this, type, text);
	}
}

void Npc::onPlayerTrade(Player* player, int32_t callback, uint16_t itemId, uint8_t count,
	uint8_t amount, bool ignore /* = false*/, bool inBackpacks /* = false*/)
{
	if (m_npcEventHandler) {
		m_npcEventHandler->onPlayerTrade(player, callback, itemId, count, amount, ignore, inBackpacks);
	}
	player->sendGoods();
}

void Npc::onPlayerEndTrade(Player* player, int32_t buyCallback, int32_t sellCallback)
{
	if (buyCallback != -1) {
		g_lua.unref(buyCallback);
	}

	if (sellCallback != -1) {
		g_lua.unref(sellCallback);
	}

	removeShopPlayer(player);

	if (m_npcEventHandler) {
		m_npcEventHandler->onPlayerEndTrade(player);
	}
}

bool Npc::getNextStep(Direction& dir, uint32_t& flags)
{
	if (Creature::getNextStep(dir, flags)) {
		return true;
	}

	if (!walkTicks || !isIdle || focusCreature || getTimeSinceLastMove() < walkTicks) {
		return false;
	}
	return getRandomStep(dir);
}

bool Npc::canWalkTo(const Position& fromPos, Direction dir)
{
	if (m_cannotMove) {
		return false;
	}

	Position toPos = getNextPosition(dir, fromPos);
	if (!Spawns::getInstance()->isInZone(m_masterPosition, m_masterRadius, toPos)) {
		return false;
	}

	Tile* tile = g_game.getTile(toPos);
	if (!tile || g_game.isSwimmingPool(nullptr, getTile(), false) != g_game.isSwimmingPool(nullptr, tile, false) || (!floorChange && (tile->floorChange() || tile->positionChange()))) {
		return false;
	}

	return tile->__queryAdd(0, this, 1, FLAG_PATHFINDING) == RET_NOERROR;
}

bool Npc::getRandomStep(Direction& dir)
{
	std::vector<Direction> dirList;
	for (int32_t i = NORTH; i < SOUTHWEST; ++i) {
		if (canWalkTo(getPosition(), (Direction)i)) {
			dirList.push_back((Direction)i);
		}
	}

	if (dirList.empty()) {
		return false;
	}

	std::shuffle(dirList.begin(), dirList.end(), getRandomGenerator());
	dir = dirList[random_range(0, dirList.size() - 1)];
	return true;
}

void Npc::setCreatureFocus(Creature* creature)
{
	if (!creature) {
		if (!walkTicks) {
			g_game.internalCreatureTurn(this, baseDirection);
		}

		focusCreature = 0;
		return;
	}

	Position pos = creature->getPosition(), _pos = getPosition();
	int32_t dx = _pos.x - pos.x, dy = _pos.y - pos.y;

	float tan = 10;
	if (dx != 0) {
		tan = dy / dx;
	}

	Direction dir = SOUTH;
	if (std::abs(tan) < 1) {
		if (dx > 0) {
			dir = WEST;
		} else {
			dir = EAST;
		}
	} else if (dy > 0) {
		dir = NORTH;
	}

	g_game.internalCreatureTurn(this, dir);
	focusCreature = creature->getID();
}

void Npc::addShopPlayer(Player* player)
{
	ShopPlayerList::iterator it = std::find(shopPlayerList.begin(), shopPlayerList.end(), player);
	if (it == shopPlayerList.end()) {
		shopPlayerList.push_back(player);
	}
}

void Npc::removeShopPlayer(const Player* player)
{
	ShopPlayerList::iterator it = std::find(shopPlayerList.begin(), shopPlayerList.end(), player);
	if (it != shopPlayerList.end()) {
		shopPlayerList.erase(it);
	}
}

void Npc::closeAllShopWindows()
{
	ShopPlayerList closeList = shopPlayerList;
	for (ShopPlayerList::iterator it = closeList.begin(); it != closeList.end(); ++it) {
		(*it)->closeShopWindow();
	}
}

NpcScript* Npc::getInterface()
{
	return m_interface;
}

NpcScript::NpcScript() :
	LuaInterface("NpcScript Interface")
{
	initState();
}

bool NpcScript::initState()
{
	m_luaState = g_lua.getLuaState();
	if (!m_luaState) {
		return false;
	}

	registerFunctions();

	lua_newtable(m_luaState);
	eventTableRef = luaL_ref(m_luaState, LUA_REGISTRYINDEX);
	m_runningEvent = EVENT_ID_USER;
	return true;
}

bool NpcScript::closeState()
{
	LuaInterface::closeState();
	return true;
}

void NpcScript::registerFunctions()
{
	lua_register(m_luaState, "selfFocus", NpcScript::luaActionFocus);
	lua_register(m_luaState, "selfSay", NpcScript::luaActionSay);
	lua_register(m_luaState, "selfFollow", NpcScript::luaActionFollow);

	lua_register(m_luaState, "getNpcId", NpcScript::luaGetNpcId);
	lua_register(m_luaState, "getNpcParameter", NpcScript::luaGetNpcParameter);

	lua_register(m_luaState, "openShopWindow", NpcScript::luaOpenShopWindow);
	lua_register(m_luaState, "closeShopWindow", NpcScript::luaCloseShopWindow);
	lua_register(m_luaState, "getShopOwner", NpcScript::luaGetShopOwner);
}

int32_t NpcScript::luaActionFocus(lua_State* L)
{
	// selfFocus(cid)
	ScriptEnvironment& env = otx::lua::getScriptEnv();
	if (Npc* npc = env.getNpc()) {
		npc->setCreatureFocus(otx::lua::getCreature(L, otx::lua::popNumber(L)));
	}
	return 0;
}

int32_t NpcScript::luaActionSay(lua_State* L)
{
	// selfSay(words[, target[, type]])
	int32_t params = lua_gettop(L), target = 0;
	MessageClasses type = MSG_NONE;
	if (params > 2) {
		type = (MessageClasses)otx::lua::popNumber(L);
	}

	if (params > 1) {
		target = otx::lua::popNumber(L);
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	Npc* npc = env.getNpc();
	if (!npc) {
		return 0;
	}

	Player* player = otx::lua::getPlayer(L, target);
	if (type == MSG_NONE) {
		if (player) {
			type = MSG_NPC_FROM;
		} else {
			type = MSG_SPEAK_SAY;
		}
	}

	npc->doSay(otx::lua::popString(L), (MessageClasses)type, player);
	return 0;
}

int32_t NpcScript::luaActionFollow(lua_State* L)
{
	// selfFollow(cid)
	uint32_t cid = otx::lua::popNumber(L);
	ScriptEnvironment& env = otx::lua::getScriptEnv();

	Creature* creature = otx::lua::getCreature(L, cid);
	if (cid && !creature) {
		lua_pushboolean(L, false);
		return 1;
	}

	Npc* npc = env.getNpc();
	if (!npc) {
		lua_pushboolean(L, false);
		return 1;
	}

	lua_pushboolean(L, npc->setFollowCreature(creature, true));
	return 1;
}

int32_t NpcScript::luaGetNpcId(lua_State* L)
{
	// getNpcId()
	ScriptEnvironment& env = otx::lua::getScriptEnv();
	if (Npc* npc = env.getNpc()) {
		lua_pushnumber(L, env.addThing(npc));
	} else {
		lua_pushnil(L);
	}

	return 1;
}

int32_t NpcScript::luaGetNpcParameter(lua_State* L)
{
	// getNpcParameter(key)
	ScriptEnvironment& env = otx::lua::getScriptEnv();
	if (Npc* npc = env.getNpc()) {
		Npc::ParametersMap::iterator it = npc->m_parameters.find(otx::lua::popString(L));
		if (it != npc->m_parameters.end()) {
			otx::lua::pushString(L, it->second);
		} else {
			lua_pushnil(L);
		}
	} else {
		lua_pushnil(L);
	}

	return 1;
}

int32_t NpcScript::luaOpenShopWindow(lua_State* L)
{
	// openShopWindow(cid, items, onBuy callback, onSell callback)
	ScriptEnvironment& env = otx::lua::getScriptEnv();
	Npc* npc = env.getNpc();
	if (!npc) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	int32_t sellCallback = -1;
	if (!lua_isfunction(L, -1)) {
		lua_pop(L, 1); // skip it - use default value
	} else {
		sellCallback = luaL_ref(L, LUA_REGISTRYINDEX);
	}

	int32_t buyCallback = -1;
	if (!lua_isfunction(L, -1)) {
		lua_pop(L, 1);
	} else {
		buyCallback = luaL_ref(L, LUA_REGISTRYINDEX);
	}

	if (!lua_istable(L, -1)) {
		otx::lua::reportErrorEx(L, "Item list is not a table");
		lua_pushboolean(L, false);
		return 1;
	}

	ShopInfoList itemList;
	lua_pushnil(L);
	while (lua_next(L, -2)) {
		ShopInfo item;
		item.itemId = otx::lua::getTableNumber<uint32_t>(L, -1, "id");
		item.subType = otx::lua::getTableNumber<int32_t>(L, -1, "subType");
		item.buyPrice = otx::lua::getTableNumber<int32_t>(L, -1, "buy");
		item.sellPrice = otx::lua::getTableNumber<int32_t>(L, -1, "sell");
		item.itemName = otx::lua::getTableString(L, -1, "name");

		itemList.push_back(item);
		lua_pop(L, 1);
	}

	lua_pop(L, 1);
	Player* player = otx::lua::getPlayer(L, otx::lua::popNumber(L));
	if (!player) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	if (player->getShopOwner() == npc) {
		lua_pushboolean(L, true);
		return 1;
	}

	player->closeShopWindow(false);
	npc->addShopPlayer(player);

	player->setShopOwner(npc, buyCallback, sellCallback, itemList);
	player->openShopWindow(npc);

	lua_pushboolean(L, true);
	return 1;
}

int32_t NpcScript::luaCloseShopWindow(lua_State* L)
{
	// closeShopWindow(cid)
	Player* player = otx::lua::getPlayer(L, otx::lua::popNumber(L));
	if (!player) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	player->closeShopWindow();
	lua_pushboolean(L, true);
	return 1;
}

int32_t NpcScript::luaGetShopOwner(lua_State* L)
{
	// getShopOwner(cid)
	ScriptEnvironment& env = otx::lua::getScriptEnv();
	Player* player = otx::lua::getPlayer(L, otx::lua::popNumber(L));
	if (!player) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	if (Npc* npc = player->getShopOwner()) {
		lua_pushnumber(L, env.addThing(npc));
	} else {
		lua_pushnil(L);
	}

	return 1;
}

NpcEvents::NpcEvents(std::string file, Npc* npc) :
	m_npc(npc),
	m_interface(npc->getInterface())
{
	if (!m_interface->loadFile(file, npc)) {
		std::clog << "[Warning - NpcEvents::NpcEvents] Cannot load script: " << file
				  << std::endl
				  << m_interface->getLastError() << std::endl;
		m_loaded = false;
		return;
	}

	m_onCreatureSay = m_interface->getEvent("onCreatureSay");
	m_onCreatureDisappear = m_interface->getEvent("onCreatureDisappear");
	m_onCreatureAppear = m_interface->getEvent("onCreatureAppear");
	m_onCreatureMove = m_interface->getEvent("onCreatureMove");
	m_onPlayerCloseChannel = m_interface->getEvent("onPlayerCloseChannel");
	m_onPlayerEndTrade = m_interface->getEvent("onPlayerEndTrade");
	m_onThink = m_interface->getEvent("onThink");
	m_loaded = true;
}

void NpcEvents::onCreatureAppear(const Creature* creature)
{
	// onCreatureAppear(cid)
	if (m_onCreatureAppear == -1) {
		return;
	}

	if (!otx::lua::reserveScriptEnv()) {
		std::clog << "[Error - NpcEvents::onCreatureAppear] NPC Name: " << m_npc->getName() << " - Call stack overflow." << std::endl;
		return;
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	env.setScriptId(m_onCreatureAppear, m_interface);
	env.setNpc(m_npc);

	m_interface->pushFunction(m_onCreatureAppear);

	lua_State* L = m_interface->getState();
	lua_pushnumber(L, env.addThing(const_cast<Creature*>(creature)));
	otx::lua::callFunction(L, 1);
}

void NpcEvents::onCreatureDisappear(const Creature* creature)
{
	// onCreatureDisappear(cid)
	if (m_onCreatureDisappear == -1) {
		return;
	}

	if (!otx::lua::reserveScriptEnv()) {
		std::clog << "[Error - NpcEvents::onCreatureDisappear] NPC Name: " << m_npc->getName() << " - Call stack overflow." << std::endl;
		return;
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	env.setScriptId(m_onCreatureDisappear, m_interface);
	env.setNpc(m_npc);

	m_interface->pushFunction(m_onCreatureDisappear);

	lua_State* L = m_interface->getState();
	lua_pushnumber(L, env.addThing(const_cast<Creature*>(creature)));
	otx::lua::callFunction(L, 1);
}

void NpcEvents::onCreatureMove(const Creature* creature, const Position& oldPos, const Position& newPos)
{
	// onCreatureMove(cid, oldPos, newPos)
	if (m_onCreatureMove == -1) {
		return;
	}

	if (!otx::lua::reserveScriptEnv()) {
		std::clog << "[Error - NpcEvents::onCreatureMove] NPC Name: " << m_npc->getName() << " - Call stack overflow." << std::endl;
		return;
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	env.setScriptId(m_onCreatureMove, m_interface);
	env.setNpc(m_npc);

	m_interface->pushFunction(m_onCreatureMove);

	lua_State* L = m_interface->getState();
	lua_pushnumber(L, env.addThing(const_cast<Creature*>(creature)));
	otx::lua::pushPosition(L, oldPos);
	otx::lua::pushPosition(L, newPos);
	otx::lua::callFunction(L, 3);
}

void NpcEvents::onCreatureSay(const Creature* creature, MessageClasses type, const std::string& text, Position* /*pos = nullptr*/)
{
	// onCreatureSay(cid, type, msg)
	if (m_onCreatureSay == -1) {
		return;
	}

	if (!otx::lua::reserveScriptEnv()) {
		std::clog << "[Error - NpcEvents::onCreatureSay] NPC Name: " << m_npc->getName() << " - Call stack overflow." << std::endl;
		return;
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	env.setScriptId(m_onCreatureSay, m_interface);
	env.setNpc(m_npc);

	m_interface->pushFunction(m_onCreatureSay);

	lua_State* L = m_interface->getState();
	lua_pushnumber(L, env.addThing(const_cast<Creature*>(creature)));
	lua_pushnumber(L, type);
	otx::lua::pushString(L, text);
	otx::lua::callFunction(L, 3);
}

void NpcEvents::onPlayerTrade(const Player* player, int32_t callback, uint16_t itemid,
	uint8_t count, uint8_t amount, bool ignore, bool inBackpacks)
{
	// on"Buy/Sell"(cid, itemid, count, amount, "ignore", inBackpacks)
	if (callback == -1) {
		return;
	}

	if (!otx::lua::reserveScriptEnv()) {
		std::clog << "[Error - NpcEvents::onPlayerTrade] NPC Name: " << m_npc->getName() << " - Call stack overflow." << std::endl;
		return;
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	env.setScriptId(-1, m_interface);
	env.setNpc(m_npc);

	lua_State* L = m_interface->getState();
	lua_rawgeti(L, LUA_REGISTRYINDEX, callback);

	lua_pushnumber(L, env.addThing(const_cast<Player*>(player)));
	lua_pushnumber(L, itemid);
	lua_pushnumber(L, count);
	lua_pushnumber(L, amount);
	lua_pushboolean(L, ignore);
	lua_pushboolean(L, inBackpacks);
	otx::lua::callFunction(L, 6);
}

void NpcEvents::onPlayerEndTrade(const Player* player)
{
	// onPlayerEndTrade(cid)
	if (m_onPlayerEndTrade == -1) {
		return;
	}

	if (!otx::lua::reserveScriptEnv()) {
		std::clog << "[Error - NpcEvents::onPlayerEndTrade] NPC Name: " << m_npc->getName() << " - Call stack overflow." << std::endl;
		return;
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	env.setScriptId(m_onPlayerEndTrade, m_interface);
	env.setNpc(m_npc);

	m_interface->pushFunction(m_onPlayerEndTrade);

	lua_State* L = m_interface->getState();
	lua_pushnumber(L, env.addThing(const_cast<Player*>(player)));
	otx::lua::callFunction(L, 1);
}

void NpcEvents::onPlayerCloseChannel(const Player* player)
{
	// onPlayerCloseChannel(cid)
	if (m_onPlayerCloseChannel == -1) {
		return;
	}

	if (!otx::lua::reserveScriptEnv()) {
		std::clog << "[Error - NpcEvents::onPlayerCloseChannel] NPC Name: " << m_npc->getName() << " - Call stack overflow." << std::endl;
		return;
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	env.setScriptId(m_onPlayerCloseChannel, m_interface);
	env.setNpc(m_npc);

	m_interface->pushFunction(m_onPlayerCloseChannel);

	lua_State* L = m_interface->getState();
	lua_pushnumber(L, env.addThing(const_cast<Player*>(player)));
	otx::lua::callFunction(L, 1);
}

void NpcEvents::onThink()
{
	// onThink()
	if (m_onThink == -1) {
		return;
	}

	if (!otx::lua::reserveScriptEnv()) {
		std::clog << "[Error - NpcEvents::onThink] NPC Name: " << m_npc->getName() << " - Call stack overflow." << std::endl;
		return;
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	env.setScriptId(m_onThink, m_interface);
	env.setNpc(m_npc);

	lua_State* L = m_interface->getState();
	m_interface->pushFunction(m_onThink);
	otx::lua::callVoidFunction(L, 0);
}
