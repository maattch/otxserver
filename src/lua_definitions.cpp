#include "otpch.h"

#include "lua_definitions.h"

#include "configmanager.h"
#include "game.h"
#include "item.h"
#include "lua_functions.h"
#include "player.h"

LuaEnvironment g_lua;

namespace
{
	// TODO: make conditions and combats uservalue,
	// so lua garbage collector will handle it

	int scriptEnvIndex = -1;
	std::array<ScriptEnvironment, 18> scriptEnv;

	std::multimap<ScriptEnvironment*, Item*> tempItems;

	// temporary conditions created mid lua functions (removed on reset script environment)
	std::unordered_map<ScriptEnvironment*, std::vector<uint32_t>> envConditionIdsMap;

	// objects created on loading scripts (removed on reload script interface)
	std::unordered_map<LuaInterface*, std::vector<uint32_t>> combatIdsMap;
	std::unordered_map<LuaInterface*, std::vector<uint32_t>> combatAreaIdsMap;
	std::unordered_map<LuaInterface*, std::vector<uint32_t>> conditionIdsMap;

	void clearEnvironmentObjects(ScriptEnvironment* env)
	{
		auto it = envConditionIdsMap.find(env);
		if (it != envConditionIdsMap.end()) {
			for (uint32_t id : it->second) {
				g_lua.removeConditionById(id);
			}

			it->second.clear();
		}
	}

	void clearInterfaceObjects(LuaInterface* luaInterface)
	{
		auto it = combatIdsMap.find(luaInterface);
		if (it != combatIdsMap.end()) {
			for (uint32_t id : it->second) {
				g_lua.removeCombatById(id);
			}

			it->second.clear();
		}

		it = combatAreaIdsMap.find(luaInterface);
		if (it != combatAreaIdsMap.end()) {
			for (uint32_t id : it->second) {
				g_lua.removeCombatAreaById(id);
			}

			it->second.clear();
		}

		it = conditionIdsMap.find(luaInterface);
		if (it != conditionIdsMap.end()) {
			for (uint32_t id : it->second) {
				g_lua.removeConditionById(id);
			}

			it->second.clear();
		}
	}

	std::string getStackTrace(lua_State* L, std::string_view errorDesc, int level = 1)
	{
#ifdef LUAJIT_VERSION
		luaL_traceback(L, L, errorDesc.data(), level);
#else
		lua_getglobal(L, "debug");
		if (!isTable(L, -1)) {
			lua_pop(L, 1);
			return errorDesc;
		}

		lua_getfield(L, -1, "traceback");
		if (!isFunction(L, -1)) {
			lua_pop(L, 2);
			return errorDesc;
		}

		lua_replace(L, -2);
		lua_pushlstring(L, errorDesc.data(), errorDesc.size());
		lua_pushnumber(L, level);
		lua_call(L, 2, 1);
#endif
		return otx::lua::popString(L);
	}

	int luaErrorHandler(lua_State* L)
	{
		otx::lua::pushString(L, getStackTrace(L, otx::lua::popString(L)));
		return 1;
	}
} // namespace (end)

void otx::lua::reportError(const char* function, std::string_view desc, lua_State* L /*= nullptr*/, bool stack_trace /*= false*/)
{
	if (otx::config::getBoolean(otx::config::SILENT_LUA)) {
		return;
	}

	if (!g_lua.isErrorsEnabled()) {
		return;
	}

	int scriptId;
	int callbackId;
	std::string* timerScript;
	LuaInterface* scriptInterface;
	getScriptEnv().getInfo(scriptId, scriptInterface, callbackId, timerScript);

	std::ostringstream oss;
	oss << "Lua Script Error: ";

	if (scriptInterface) {
		oss << '[' << scriptInterface->getName() << "]\n";
		if (timerScript) {
			oss << "in a timer event called from: " << *timerScript << '\n';
		}

		if (callbackId != 0) {
			oss << "in a callback: " << scriptInterface->getScript(callbackId) << '\n';
		}

		if (!timerScript) {
			oss << scriptInterface->getScript(scriptId) << '\n';
		}
	}

	if (function) {
		oss << function << "() ";
	}

	if (L && stack_trace) {
		oss << getStackTrace(L, desc) << '\n';
	} else {
		oss << desc << '\n';
	}

	std::clog << std::endl << oss.str();
}

bool otx::lua::isNil(lua_State* L, int index) { return lua_type(L, index) == LUA_TNIL; }
bool otx::lua::isNumber(lua_State* L, int index) { return lua_type(L, index) == LUA_TNUMBER; }
bool otx::lua::isString(lua_State* L, int index) { return lua_type(L, index) == LUA_TSTRING; }
bool otx::lua::isBoolean(lua_State* L, int index) { return lua_type(L, index) == LUA_TBOOLEAN; }
bool otx::lua::isTable(lua_State* L, int index) { return lua_type(L, index) == LUA_TTABLE; }
bool otx::lua::isFunction(lua_State* L, int index) { return lua_type(L, index) == LUA_TFUNCTION; }
bool otx::lua::isNone(lua_State* L, int index) { return lua_type(L, index) == LUA_TNONE; }
bool otx::lua::isNoneOrNil(lua_State* L, int index) { return lua_type(L, index) <= LUA_TNIL; }
bool otx::lua::isLightUserdata(lua_State* L, int index) { return lua_type(L, index) == LUA_TLIGHTUSERDATA; }
bool otx::lua::isUserdata(lua_State* L, int index) { return lua_isuserdata(L, index) == 1; }

std::string otx::lua::getSource(lua_State* L, int level)
{
	lua_Debug ar;
	ar.short_src[0] = 0;
	ar.currentline = 0;
	if (lua_getstack(L, level, &ar) == 1) {
		lua_getinfo(L, "Sl", &ar);
	}
	return std::string(ar.short_src) + ":" + std::to_string(ar.currentline);
}

bool otx::lua::popBoolean(lua_State* L)
{
	bool ret = (lua_toboolean(L, -1) != 0);
	lua_pop(L, 1);
	return ret;
}

int64_t otx::lua::popNumber(lua_State* L)
{
	auto ret = static_cast<int64_t>(lua_tonumber(L, -1));
	lua_pop(L, 1);
	return ret;
}

std::string otx::lua::popString(lua_State* L)
{
	size_t len;
	const char* c_str = lua_tolstring(L, -1, &len);
	lua_pop(L, 1);

	if (!c_str || len == 0) {
		return std::string();
	}
	return std::string(c_str, len);
}

bool otx::lua::getBoolean(lua_State* L, int index)
{
#if STRICT_LUA > 0
	if (!isBoolean(L, index)) {
		luaL_typerror(L, index, "boolean");
	}
#endif
	return lua_toboolean(L, index) != 0;
}

bool otx::lua::getBoolean(lua_State* L, int index, bool def)
{
	if (isNoneOrNil(L, index)) {
		return def;
	}

#if STRICT_LUA > 0
	if (!isBoolean(L, index)) {
		luaL_typerror(L, index, "boolean");
		return def;
	}
#endif
	return lua_toboolean(L, index) != 0;
}

std::string otx::lua::getString(lua_State* L, int index)
{
	size_t len;
#if STRICT_LUA > 0
	const char* c_str = luaL_checklstring(L, index, &len);
#else
	const char* c_str = lua_tolstring(L, index, &len);
#endif
	if (!c_str || len == 0) {
		return std::string();
	}
	return std::string(c_str, len);
}

Position otx::lua::getPosition(lua_State* L, int index)
{
	Position position;
	if (isTable(L, index)) {
		position.x = getTableNumber<uint16_t>(L, index, "x");
		position.y = getTableNumber<uint16_t>(L, index, "y");
		position.z = getTableNumber<uint8_t>(L, index, "z");
	}
	return position;
}

Position otx::lua::getPosition(lua_State* L, int index, int32_t& stackpos)
{
	Position position;
	if (isTable(L, index)) {
		position.x = getTableNumber<uint16_t>(L, index, "x");
		position.y = getTableNumber<uint16_t>(L, index, "y");
		position.z = getTableNumber<uint8_t>(L, index, "z");
		stackpos = getTableNumber<int32_t>(L, index, "stackpos");
	}
	return position;
}

Outfit_t otx::lua::getOutfit(lua_State* L, int index)
{
	Outfit_t outfit;
	if (isTable(L, index)) {
		outfit.lookType = getTableNumber<uint16_t>(L, index, "lookType");
		outfit.lookTypeEx = getTableNumber<uint16_t>(L, index, "lookTypeEx");
		outfit.lookFeet = getTableNumber<uint8_t>(L, index, "lookFeet");
		outfit.lookLegs = getTableNumber<uint8_t>(L, index, "lookLegs");
		outfit.lookBody = getTableNumber<uint8_t>(L, index, "lookBody");
		outfit.lookHead = getTableNumber<uint8_t>(L, index, "lookHead");
		outfit.lookAddons = getTableNumber<uint8_t>(L, index, "lookAddons");
	}
#if STRICT_LUA > 0
	else { luaL_typerror(L, index, "table"); }
#endif
	return outfit;
}

LuaVariant otx::lua::getVariant(lua_State* L, int index)
{
	LuaVariant var;
	if (!isTable(L, index)) {
#if STRICT_LUA > 0
		luaL_typerror(L, index, "table");
#endif
		return var;
	}

	var.type = static_cast<LuaVariantType_t>(getTableNumber<uint8_t>(L, index, "type"));

	switch (var.type) {
		case VARIANT_NUMBER: {
			var.number = getTableNumber<uint32_t>(L, index, "number");
			break;
		}
		case VARIANT_STRING: {
			var.text = getTableString(L, index, "string");
			break;
		}
		case VARIANT_POSITION:
		case VARIANT_TARGETPOSITION: {
			lua_getfield(L, index, "pos");
			var.pos = getPosition(L, -1);
			lua_pop(L, 1);
			break;
		}

		case VARIANT_NONE:
		default: {
			var.type = VARIANT_NONE;
			break;
		}
	}
	return var;
}

Creature* otx::lua::getCreature(lua_State* L, int index)
{
	return g_game.getCreatureByID(getNumber<uint32_t>(L, index));
}

Thing* otx::lua::getThing(lua_State* L, int index)
{
	return getScriptEnv().getThingByUID(getNumber<uint32_t>(L, index));
}

Player* otx::lua::getPlayer(lua_State* L, int index)
{
	return g_game.getPlayerByID(getNumber<uint32_t>(L, index));
}

Monster* otx::lua::getMonster(lua_State* L, int index)
{
	return g_game.getMonsterByID(getNumber<uint32_t>(L, index));
}

Npc* otx::lua::getNpc(lua_State* L, int index)
{
	return g_game.getNpcByID(getNumber<uint32_t>(L, index));
}

Item* otx::lua::getItem(lua_State* L, int index)
{
	return getScriptEnv().getItemByUID(getNumber<uint32_t>(L, index));
}

Container* otx::lua::getContainer(lua_State* L, int index)
{
	if (Item* item = getItem(L, index)) {
		return item->getContainer();
	}
	return nullptr;
}

void otx::lua::pushString(lua_State* L, std::string_view s)
{
	lua_pushlstring(L, s.data(), s.size());
}

void otx::lua::pushThing(lua_State* L, Thing* thing,
	uint32_t id/* = 0*/, Recursive_t recursive/* = RECURSE_NONE*/,
	int narr/* = 0*/, int nrec/* = 0*/)
{
	lua_createtable(L, narr, 7 + nrec);
	if (thing && thing->getItem()) {
		const Item* item = thing->getItem();
		if (id == 0) {
			id = getScriptEnv().addThing(thing);
		}

		setTableValue(L, "uniqueid", id);
		setTableValue(L, "uid", id);
		setTableValue(L, "itemid", item->getID());
		setTableValue(L, "id", item->getID());
		if (item->hasSubType()) {
			setTableValue(L, "type", item->getSubType());
		} else {
			setTableValue(L, "type", 0);
		}

		setTableValue(L, "actionid", item->getActionId());
		setTableValue(L, "aid", item->getActionId());
		if (recursive != RECURSE_NONE) {
			if (const Container* container = item->getContainer()) {
				if (recursive == RECURSE_FIRST) {
					recursive = RECURSE_NONE;
				}

				const auto& containerItems = container->getItemList();

				int index = 0;
				lua_createtable(L, containerItems.size(), 0);
				for (Item* containerItem : containerItems) {
					pushThing(L, containerItem, 0, recursive);
					lua_rawseti(L, -2, ++index);
				}
				lua_setfield(L, -2, "items");
			}
		}
	} else if (thing && thing->getCreature()) {
		const Creature* creature = thing->getCreature();
		if (id == 0) {
			id = creature->getID();
		}

		setTableValue(L, "uniqueid", id);
		setTableValue(L, "uid", id);
		setTableValue(L, "itemid", 1);
		setTableValue(L, "id", 1);
		if (creature->getPlayer()) {
			setTableValue(L, "type", 1);
		} else if (creature->getMonster()) {
			setTableValue(L, "type", 2);
		} else {
			setTableValue(L, "type", 3);
		}

		if (const Player* player = creature->getPlayer()) {
			setTableValue(L, "actionid", player->getGUID());
			setTableValue(L, "aid", player->getGUID());
		} else {
			setTableValue(L, "actionid", 0);
			setTableValue(L, "aid", 0);
		}
	} else {
		setTableValue(L, "uid", 0);
		setTableValue(L, "uniqueid", 0);
		setTableValue(L, "itemid", 0);
		setTableValue(L, "id", 0);
		setTableValue(L, "type", 0);
		setTableValue(L, "actionid", 0);
		setTableValue(L, "aid", 0);
	}
}

void otx::lua::pushVariant(lua_State* L, const LuaVariant& var)
{
	lua_createtable(L, 0, 2);
	setTableValue(L, "type", var.type);

	switch (var.type) {
		case VARIANT_NUMBER: {
			setTableValue(L, "number", var.number);
			break;
		}
		case VARIANT_STRING: {
			setTableValue(L, "string", var.text);
			break;
		}
		case VARIANT_TARGETPOSITION:
		case VARIANT_POSITION: {
			pushPosition(L, var.pos);
			lua_setfield(L, -2, "pos");
			break;
		}

		case VARIANT_NONE:
			break;
	}
}

void otx::lua::pushPosition(lua_State* L, const Position& position, int32_t stackpos/* = 0*/)
{
	lua_createtable(L, 0, 4);
	setTableValue(L, "x", position.x);
	setTableValue(L, "y", position.y);
	setTableValue(L, "z", position.z);
	setTableValue(L, "stackpos", stackpos);
}

void otx::lua::pushOutfit(lua_State* L, const Outfit_t& outfit)
{
	lua_createtable(L, 0, 7);
	setTableValue(L, "lookType", outfit.lookType);
	setTableValue(L, "lookTypeEx", outfit.lookTypeEx);
	setTableValue(L, "lookHead", outfit.lookHead);
	setTableValue(L, "lookBody", outfit.lookBody);
	setTableValue(L, "lookLegs", outfit.lookLegs);
	setTableValue(L, "lookFeet", outfit.lookFeet);
	setTableValue(L, "lookAddons", outfit.lookAddons);
}

void otx::lua::pushThingId(lua_State* L, Thing* thing)
{
	lua_pushnumber(L, getScriptEnv().addThing(thing));
}

std::string otx::lua::getTableString(lua_State* L, int index, const char* field)
{
	lua_getfield(L, index, field);
	return popString(L);
}

bool otx::lua::getTableBool(lua_State* L, int index, const char* field)
{
	lua_getfield(L, index, field);
	return popBoolean(L);
}

bool otx::lua::getTableBool(lua_State* L, int index, const char* field, bool def)
{
	lua_getfield(L, index, field);
	bool res = getBoolean(L, -1, def);
	lua_pop(L, 1);
	return res;
}

void otx::lua::setTableValue(lua_State* L, const char* key, lua_Number value)
{
	lua_pushnumber(L, value);
	lua_setfield(L, -2, key);
}

void otx::lua::setTableValue(lua_State* L, const char* key, const std::string& value)
{
	pushString(L, value);
	lua_setfield(L, -2, key);
}

void otx::lua::setTableValue(lua_State* L, const char* key, const Position& value)
{
	pushPosition(L, value);
	lua_setfield(L, -2, key);
}

void otx::lua::addTempItem(ScriptEnvironment* env, Item* item)
{
	tempItems.emplace(env, item);
}

void otx::lua::removeTempItem(Item* item)
{
	for (auto it = tempItems.begin(), end = tempItems.end(); it != end; ++it) {
		if (it->second == item) {
			tempItems.erase(it);
			break;
		}
	}
}

ScriptEnvironment& otx::lua::getScriptEnv()
{
	assert(scriptEnvIndex >= 0 && scriptEnvIndex < static_cast<int>(scriptEnv.size()));
	return scriptEnv[scriptEnvIndex];
}

bool otx::lua::reserveScriptEnv()
{
	if ((scriptEnvIndex + 1) < static_cast<int>(scriptEnv.size())) {
		++scriptEnvIndex;
		return true;
	}
	return false;
}

void otx::lua::resetScriptEnv()
{
	assert(scriptEnvIndex >= 0);
	scriptEnv[scriptEnvIndex--].reset();
}

// Same as lua_pcall, but adds stack trace to error strings in called function.
bool otx::lua::protectedCall(lua_State* L, int nargs, int nresults)
{
	const int errorIndex = lua_gettop(L) - nargs;
	lua_pushcfunction(L, luaErrorHandler);
	lua_insert(L, errorIndex);

	bool ret = lua_pcall(L, nargs, nresults, errorIndex) == 0;
	lua_remove(L, errorIndex);
	return ret;
}

bool otx::lua::callFunction(lua_State* L, int nargs, bool releaseEnv/* = true*/)
{
	bool result = false;
	const int stackSize = lua_gettop(L);
	if (!protectedCall(L, nargs, 1)) {
		reportError(nullptr, popString(L));
	} else {
		result = popBoolean(L);
	}

	if ((lua_gettop(L) + nargs + 1) != stackSize) {
		reportError(nullptr, "Stack size changed!");
	}

	if (releaseEnv) {
		resetScriptEnv();
	}
	return result;
}

void otx::lua::callVoidFunction(lua_State* L, int nargs, bool releaseEnv/* = true*/)
{
	const int stackSize = lua_gettop(L);
	if (!protectedCall(L, nargs, 0)) {
		reportError(nullptr, popString(L));
	}

	if ((lua_gettop(L) + nargs + 1) != stackSize) {
		reportError(nullptr, "Stack size changed!");
	}

	if (releaseEnv) {
		resetScriptEnv();
	}
}

//
// ScriptEnvironment
//
void ScriptEnvironment::reset()
{
	m_scriptId = -1;
	m_callbackId = -1;
	m_interface = nullptr;
	m_timerScript = nullptr;
	m_localItemMap.clear();

	auto&& [item_iter, item_end] = tempItems.equal_range(this);
	while (item_iter != item_end) {
		Item* item = item_iter->second;
		if (item && item->getParent() == VirtualCylinder::virtualCylinder) {
			g_game.freeThing(item);
		}
		item_iter = tempItems.erase(item_iter);
	}

	clearEnvironmentObjects(this);
}

bool ScriptEnvironment::setCallbackId(int32_t callbackId, LuaInterface* interface)
{
	if (m_callbackId == 0) {
		m_callbackId = callbackId;
		m_interface = interface;
		return true;
	}

	// nested callbacks are not allowed
	if (m_interface) {
		otx::lua::reportErrorEx(m_interface->getState(), "Nested callbacks are not allowed!");
	}
	return false;
}

void ScriptEnvironment::getInfo(int& scriptId, LuaInterface*& interface,
	int& callbackId, std::string*& timerScript)
{
	scriptId = m_scriptId;
	interface = m_interface;
	callbackId = m_callbackId;
	timerScript = m_timerScript;
}

uint32_t ScriptEnvironment::addThing(Thing* thing)
{
	if (!thing || thing->isRemoved()) {
		return 0;
	}

	if (Creature* creature = thing->getCreature()) {
		return creature->getID();
	}

	Item* item = thing->getItem();
	if (!item) {
		return 0;
	}

	const uint16_t uniqueId = item->getUniqueId();
	if (uniqueId != 0) {
		return uniqueId;
	}

	for (const auto& it : m_localItemMap) {
		if (it.second == item) {
			return it.first;
		}
	}

	m_localItemMap[++m_lastUID] = item;
	return m_lastUID;
}

void ScriptEnvironment::insertItem(uint32_t uid, Item* item)
{
	if (!m_localItemMap.emplace(uid, item).second) {
		std::clog << "[Error - ScriptEnvironment::insertThing] Thing uid already taken." << std::endl;
	}
}

Thing* ScriptEnvironment::getThingByUID(uint32_t uid)
{
	if (uid >= PLAYER_ID_RANGE) {
		return g_game.getCreatureByID(uid);
	}

	if (uid <= 0xFFFF) {
		Item* item = g_game.getUniqueItem(uid);
		if (item && !item->isRemoved()) {
			return item;
		}
		return nullptr;
	}

	auto it = m_localItemMap.find(uid);
	if (it != m_localItemMap.end()) {
		Item* item = it->second;
		if (!item->isRemoved()) {
			return item;
		}
	}
	return nullptr;
}

Item* ScriptEnvironment::getItemByUID(uint32_t uid)
{
	if (Thing* thing = getThingByUID(uid)) {
		return thing->getItem();
	}
	return nullptr;
}

Container* ScriptEnvironment::getContainerByUID(uint32_t uid)
{
	if (Item* item = getItemByUID(uid)) {
		return item->getContainer();
	}
	return nullptr;
}

void ScriptEnvironment::removeItem(uint32_t uid)
{
	if (uid <= 0xFFFF) {
		g_game.removeUniqueItem(uid);
		return;
	}

	auto it = m_localItemMap.find(uid);
	if (it != m_localItemMap.end()) {
		m_localItemMap.erase(it);
	}
}

uint32_t ScriptEnvironment::addCombat(Combat* combat)
{
	uint32_t id = g_lua.addCombat(combat);
	combatIdsMap.emplace(getInterface(), id);
	return id;
}

uint32_t ScriptEnvironment::addCombatArea(CombatArea* area)
{
	uint32_t id = g_lua.addCombatArea(area);
	combatAreaIdsMap.emplace(getInterface(), id);
	return id;
}

uint32_t ScriptEnvironment::addCondition(Condition* condition)
{
	uint32_t id = g_lua.addCondition(condition);
	if (m_scriptId != EVENT_ID_LOADING) {
		envConditionIdsMap.emplace(this, id);
	} else {
		conditionIdsMap.emplace(getInterface(), id);
	}
	return id;
}

//
// LuaInterface
//
LuaInterface::LuaInterface(std::string interfaceName) :
	m_interfaceName(std::move(interfaceName))
{
	//
}

LuaInterface::~LuaInterface()
{
	closeState();
}

bool LuaInterface::reInitState()
{
	closeState();
	return initState();
}

bool LuaInterface::loadFile(const std::string& file, Npc* npc /* = nullptr*/)
{
	// loads file as a chunk at stack top
	if (luaL_loadfile(m_luaState, file.data()) != 0) {
		m_lastError = otx::lua::popString(m_luaState);
		std::clog << "[Error - LuaInterface::loadFile] " << m_lastError << std::endl;
		return false;
	}

	// check that it is loaded as a function
	if (!otx::lua::isFunction(m_luaState, -1)) {
		lua_pop(m_luaState, 1);
		return false;
	}

	m_loadingFile = file;
	if (!otx::lua::reserveScriptEnv()) {
		lua_pop(m_luaState, 1);
		return false;
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	env.setScriptId(EVENT_ID_LOADING, this);
	env.setNpc(npc);

	// execute it
	if (!otx::lua::protectedCall(m_luaState, 0, 0)) {
		otx::lua::reportError(nullptr, otx::lua::popString(m_luaState));
		otx::lua::resetScriptEnv();
		return false;
	}

	otx::lua::resetScriptEnv();
	return true;
}

bool LuaInterface::loadDirectory(std::string dir, bool recursively, bool loadSystems, Npc* npc /* = nullptr*/)
{
	if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir)) {
		std::clog << "[Error - LuaInterface::loadDirectory] Cannot load directory " << dir << std::endl;
		return false;
	}

	std::vector<std::filesystem::path> scripts;
	if (recursively) {
		for (std::filesystem::recursive_directory_iterator it(dir), end; it != end; ++it) {
			if (std::filesystem::is_regular_file(it->status()) && it->path().extension() == ".lua") {
				scripts.push_back(it->path());
			}
		}
	} else {
		for (std::filesystem::directory_iterator it(dir), end; it != end; ++it) {
			if (std::filesystem::is_regular_file(it->status()) && it->path().extension() == ".lua") {
				scripts.push_back(it->path());
			}
		}
	}

	// sort alphabetically
	std::sort(scripts.begin(), scripts.end());

	for (const auto& script : scripts) {
		if (!loadSystems && !script.filename().empty() && script.filename().native().front() == '_') {
			continue;
		}

		if (!loadFile(script.string(), npc)) {
			return false;
		}
	}
	return true;
}

int LuaInterface::getEvent(const std::string& eventName)
{
	lua_rawgeti(m_luaState, LUA_REGISTRYINDEX, eventTableRef);
	if (!lua_istable(m_luaState, -1)) {
		lua_pop(m_luaState, 1);
		return -1;
	}

	// get current event function pointer
	lua_getglobal(m_luaState, eventName.c_str());
	if (!lua_isfunction(m_luaState, -1)) {
		lua_pop(m_luaState, 1);
		return -1;
	}

	// save in our events table
	lua_pushnumber(m_luaState, m_runningEvent);
	lua_pushvalue(m_luaState, -2);

	lua_rawset(m_luaState, -4);
	lua_pop(m_luaState, 2);

	// reset global value of this event
	lua_pushnil(m_luaState);
	lua_setglobal(m_luaState, eventName.c_str());

	m_cacheFiles[m_runningEvent] = m_loadingFile + ":" + eventName;
	++m_runningEvent;
	return m_runningEvent - 1;
}

const std::string& LuaInterface::getScript(int32_t scriptId)
{
	if (scriptId == EVENT_ID_LOADING) {
		return m_loadingFile;
	}

	auto it = m_cacheFiles.find(scriptId);
	if (it != m_cacheFiles.end()) {
		return it->second;
	}

	static const std::string unkScript = "(Unknown script file)";
	return unkScript;
}

bool LuaInterface::pushFunction(int function)
{
	lua_rawgeti(m_luaState, LUA_REGISTRYINDEX, eventTableRef);
	if (!lua_istable(m_luaState, -1)) {
		return false;
	}

	lua_pushnumber(m_luaState, function);
	lua_rawget(m_luaState, -2);

	lua_remove(m_luaState, -2);
	return lua_isfunction(m_luaState, -1);
}

bool LuaInterface::initState()
{
	m_luaState = g_lua.getLuaState();
	if (!m_luaState) {
		return false;
	}

	lua_createtable(m_luaState, 0, 0);
	eventTableRef = luaL_ref(m_luaState, LUA_REGISTRYINDEX);
	m_runningEvent = EVENT_ID_USER;
	return true;
}

bool LuaInterface::closeState()
{
	if (!m_luaState) {
		return false;
	}

	clearInterfaceObjects(this);

	g_lua.unref(eventTableRef);

	eventTableRef = -1;
	m_cacheFiles.clear();
	m_luaState = nullptr;
	return true;
}

//
// LuaEnvironment
//
void LuaEnvironment::init()
{
	if (m_L) {
		return;
	}

	m_L = luaL_newstate();
	luaL_openlibs(m_L);

	m_mainInterface = std::make_unique<LuaInterface>("Main Interface");
	m_mainInterface->initState();

	otx::lua::registerFunctions();
}

void LuaEnvironment::reload()
{
	// TODO
}

void LuaEnvironment::terminate()
{
	if (!m_L) {
		return;
	}

	for (auto& timerEntry : m_timerEvents) {
		LuaTimerEvent timerEventDesc = std::move(timerEntry.second);
		g_scheduler.stopEvent(timerEventDesc.eventId);

		for (int parameter : timerEventDesc.parameters) {
			unref(parameter);
		}
		unref(timerEventDesc.funcRef);
	}

	m_timerEvents.clear();
	m_combats.clear();
	m_combatAreas.clear();
	m_conditions.clear();

	m_lastCombatId = 0;
	m_lastCombatAreaId = 0;
	m_lastConditionId = 0;
	m_lastTimerEventId = 0;

	m_mainInterface.reset();

	lua_close(m_L);
	m_L = nullptr;
}

void LuaEnvironment::addScriptInterface(LuaInterface* luaInterface)
{
	auto it = std::find(m_scriptInterfaces.begin(), m_scriptInterfaces.end(), luaInterface);
	if (it == m_scriptInterfaces.end()) {
		m_scriptInterfaces.push_back(luaInterface);
	}
}

void LuaEnvironment::removeScriptInterface(LuaInterface* luaInterface)
{
	auto it = std::find(m_scriptInterfaces.begin(), m_scriptInterfaces.end(), luaInterface);
	if (it != m_scriptInterfaces.end()) {
		m_scriptInterfaces.erase(it);
	}
}

uint32_t LuaEnvironment::addTimerEvent(LuaTimerEvent&& timerEvent, uint32_t delay)
{
	timerEvent.eventId = addSchedulerTask(delay, ([this, id = m_lastTimerEventId]() { executeTimerEvent(id); }));
	m_timerEvents.emplace(m_lastTimerEventId, std::move(timerEvent));
	return m_lastTimerEventId++;
}

bool LuaEnvironment::stopTimerEvent(lua_State* L, uint32_t eventId)
{
	UNUSED(L);

	auto it = m_timerEvents.find(eventId);
	if (it != m_timerEvents.end()) {
		g_scheduler.stopEvent(it->second.eventId);
		for (int ref : it->second.parameters) {
			unref(ref);
		}
		unref(it->second.funcRef);

		m_timerEvents.erase(it);
		return true;
	}
	return false;
}

bool LuaEnvironment::executeTimerEvent(uint32_t eventIndex)
{
	if (!m_L) {
		return false;
	}

	auto it = m_timerEvents.find(eventIndex);
	if (it == m_timerEvents.end()) {
		return false;
	}

	LuaTimerEvent timerEventDesc = std::move(it->second);
	m_timerEvents.erase(it);

	// push function
	lua_rawgeti(m_L, LUA_REGISTRYINDEX, timerEventDesc.funcRef);

	// push parameters
	for (auto rit = timerEventDesc.parameters.rbegin(); rit != timerEventDesc.parameters.rend(); ++rit) {
		lua_rawgeti(m_L, LUA_REGISTRYINDEX, *rit);
	}

	// call the function
	if (otx::lua::reserveScriptEnv()) {
		auto& env = otx::lua::getScriptEnv();
		env.setTimerScript(&timerEventDesc.script);
		env.setScriptId(timerEventDesc.scriptId, getMainInterface());
		otx::lua::callVoidFunction(m_L, timerEventDesc.parameters.size());
	} else {
		std::clog << "[Error - LuaEnvironment::executeTimerEvent] Call stack overflow." << std::endl;
	}

	// free resources
	luaL_unref(m_L, LUA_REGISTRYINDEX, timerEventDesc.funcRef);
	for (int parameter : timerEventDesc.parameters) {
		luaL_unref(m_L, LUA_REGISTRYINDEX, parameter);
	}
	return true;
}

uint32_t LuaEnvironment::addCondition(Condition* condition)
{
	m_conditions.emplace(++m_lastConditionId, ConditionPtr(condition));
	return m_lastConditionId;
}

Condition* LuaEnvironment::getConditionById(uint32_t id)
{
	auto it = m_conditions.find(id);
	if (it != m_conditions.end()) {
		return it->second.get();
	}
	return nullptr;
}

void LuaEnvironment::removeConditionById(uint32_t id)
{
	auto it = m_conditions.find(id);
	if (it != m_conditions.end()) {
		m_conditions.erase(it);
	}
}

uint32_t LuaEnvironment::addCombat(Combat* combat)
{
	m_combats.emplace(++m_lastCombatId, CombatPtr(combat));
	return m_lastCombatId;
}

Combat* LuaEnvironment::getCombatById(uint32_t id)
{
	auto it = m_combats.find(id);
	if (it != m_combats.end()) {
		return it->second.get();
	}
	return nullptr;
}

void LuaEnvironment::removeCombatById(uint32_t id)
{
	auto it = m_combats.find(id);
	if (it != m_combats.end()) {
		m_combats.erase(it);
	}
}

uint32_t LuaEnvironment::addCombatArea(CombatArea* area)
{
	m_combatAreas.emplace(++m_lastCombatAreaId, CombatAreaPtr(area));
	return m_lastCombatAreaId;
}

CombatArea* LuaEnvironment::getCombatAreaById(uint32_t id)
{
	auto it = m_combatAreas.find(id);
	if (it != m_combatAreas.end()) {
		return it->second.get();
	}
	return nullptr;
}

void LuaEnvironment::removeCombatAreaById(uint32_t id)
{
	auto it = m_combatAreas.find(id);
	if (it != m_combatAreas.end()) {
		m_combatAreas.erase(it);
	}
}

void LuaEnvironment::unref(int ref) const
{
	if (ref > 0 && m_L) {
		luaL_unref(m_L, LUA_REGISTRYINDEX, ref);
	}
}

void LuaEnvironment::collectGarbage() const
{
	if (!m_L) {
		return;
	}

	// prevents recursive collects
	static bool collecting = false;
	if (!collecting) {
		collecting = true;

		// we must collect two times because __gc metamethod
		// is called on uservalues only the second time
		lua_gc(m_L, LUA_GCCOLLECT, 0);
		lua_gc(m_L, LUA_GCCOLLECT, 0);

		collecting = false;
	}
}
