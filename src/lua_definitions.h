#pragma once

#include "position.h"

#include <optional>

#include <luajit/lua.hpp>

class Item;
class Creature;
class Player;
class Monster;
class Npc;
class Container;
class Thing;
class Combat;
class CombatArea;
class Condition;
class LuaInterface;
class ScriptEnvironment;

struct Outfit_t;

using LuaInterfacePtr = std::unique_ptr<LuaInterface>;
using CombatPtr = std::unique_ptr<Combat>;
using CombatAreaPtr = std::unique_ptr<CombatArea>;
using ConditionPtr = std::unique_ptr<Condition>;

enum
{
	EVENT_ID_LOADING = 1,
	EVENT_ID_USER = 1000,
};

enum LuaErrorCode_t : uint8_t
{
	LUA_ERROR_PLAYER_NOT_FOUND,
	LUA_ERROR_MONSTER_NOT_FOUND,
	LUA_ERROR_NPC_NOT_FOUND,
	LUA_ERROR_CREATURE_NOT_FOUND,
	LUA_ERROR_ITEM_NOT_FOUND,
	LUA_ERROR_THING_NOT_FOUND,
	LUA_ERROR_TILE_NOT_FOUND,
	LUA_ERROR_HOUSE_NOT_FOUND,
	LUA_ERROR_COMBAT_NOT_FOUND,
	LUA_ERROR_CONDITION_NOT_FOUND,
	LUA_ERROR_AREA_NOT_FOUND,
	LUA_ERROR_CONTAINER_NOT_FOUND,
	LUA_ERROR_VARIANT_NOT_FOUND,
	LUA_ERROR_VARIANT_UNKNOWN,
	LUA_ERROR_SPELL_NOT_FOUND
};

enum LuaVariantType_t : uint8_t
{
	VARIANT_NONE,
	VARIANT_NUMBER,
	VARIANT_POSITION,
	VARIANT_TARGETPOSITION,
	VARIANT_STRING
};

enum Recursive_t : int8_t
{
	RECURSE_FIRST = -1,
	RECURSE_NONE = 0,
	RECURSE_ALL = 1
};

struct LuaVariant
{
	std::string text;
	Position pos;
	uint32_t number = 0;
	LuaVariantType_t type = VARIANT_NONE;
};

struct LuaTimerEvent
{
	std::string script;
	std::vector<int> parameters;
	Npc* npc;
	uint32_t eventId;
	int scriptId;
	int funcRef;
};

namespace otx::lua
{
	#define reportErrorEx(L, desc) reportError(__FUNCTION__, desc, L, true)

	void reportError(const char* function, std::string_view desc, lua_State* L = nullptr, bool stack_trace = false);

	// Is
	bool isNil(lua_State* L, int index);
	bool isNumber(lua_State* L, int index);
	bool isString(lua_State* L, int index);
	bool isBoolean(lua_State* L, int index);
	bool isTable(lua_State* L, int index);
	bool isFunction(lua_State* L, int index);
	bool isNone(lua_State* L, int index);
	bool isNoneOrNil(lua_State* L, int index);
	bool isLightUserdata(lua_State* L, int index);
	bool isUserdata(lua_State* L, int index);

	// Pop
	bool popBoolean(lua_State* L);
	int64_t popNumber(lua_State* L);
	std::string popString(lua_State* L);

	// Get
	template<typename T>
	std::enable_if_t<std::is_integral_v<T> || std::is_floating_point_v<T>, T>
		getNumber(lua_State* L, int index)
	{
#if STRICT_LUA > 0
		double num = luaL_checknumber(L, index);
		if (num < static_cast<double>(std::numeric_limits<T>::lowest()) ||
			num > static_cast<double>(std::numeric_limits<T>::max())) {
			index = (index < 0 ? (lua_gettop(L) - (index - 1)) : index);
			reportError(nullptr, "argument #" + std::to_string(index) + " has out-of-range value for " + typeid(T).name() + ": " + std::to_string(num), L, true);
		}
		return static_cast<T>(num);
#else
		return static_cast<T>(lua_tonumber(L, index));
#endif
	}

	template<typename T> T getNumber(lua_State* L, int index, T def)
	{
#if STRICT_LUA > 0
		double num = luaL_optnumber(L, index, def);
		if (num < static_cast<double>(std::numeric_limits<T>::lowest()) ||
			num > static_cast<double>(std::numeric_limits<T>::max())) {
			index = (index < 0 ? (lua_gettop(L) - (index - 1)) : index);
			reportError(nullptr, "argument #" + std::to_string(index) + " has out-of-range value for " + typeid(T).name() + ": " + std::to_string(num), L, true);
		}
		return static_cast<T>(num);
#else
		if (!isNumber(L, index)) {
			return def;
		}
		return getNumber<T>(L, index);
#endif
	}

	bool getBoolean(lua_State* L, int index);
	bool getBoolean(lua_State* L, int index, bool def);

	std::string getString(lua_State* L, int index);
	Position getPosition(lua_State* L, int index);
	Position getPosition(lua_State* L, int index, int32_t& stackpos);
	Outfit_t getOutfit(lua_State* L, int index);
	LuaVariant getVariant(lua_State* L, int index);
	Creature* getCreature(lua_State* L, int index);
	Thing* getThing(lua_State* L, int index);
	Player* getPlayer(lua_State* L, int index);
	Monster* getMonster(lua_State* L, int index);
	Npc* getNpc(lua_State* L, int index);
	Item* getItem(lua_State* L, int index);
	Container* getContainer(lua_State* L, int index);

	std::string getSource(lua_State* L, int level);

	// Push
	void pushString(lua_State* L, std::string_view s);
	void pushThing(lua_State* L, Thing* thing, uint32_t id = 0, Recursive_t recursive = RECURSE_NONE, int narr = 0, int nrec = 0);
	void pushVariant(lua_State* L, const LuaVariant& var);
	void pushPosition(lua_State* L, const Position& position, int32_t stackpos = 0);
	void pushOutfit(lua_State* L, const Outfit_t& outfit);
	void pushThingId(lua_State* L, Thing* thing);

	// get field template
	template<typename T>
	std::enable_if_t<std::is_integral_v<T> || std::is_floating_point_v<T>, T>
		getTableNumber(lua_State* L, int index, const char* field, T def = {})
	{
		lua_getfield(L, index, field);
		T value = getNumber<T>(L, -1, def);
		lua_pop(L, 1);
		return value;
	}
	std::string getTableString(lua_State* L, int index, const char* field);

	bool getTableBool(lua_State* L, int index, const char* field);
	bool getTableBool(lua_State* L, int index, const char* field, bool def);

	template<typename T>
	std::enable_if_t<std::is_same_v<T, bool>, void>
		setTableValue(lua_State* L, const char* key, T value)
	{
		lua_pushboolean(L, value);
		lua_setfield(L, -2, key);
	}
	void setTableValue(lua_State* L, const char* key, lua_Number value);
	void setTableValue(lua_State* L, const char* key, const std::string& value);
	void setTableValue(lua_State* L, const char* key, const Position& value);

	template<typename T>
	std::optional<T> getTableOptValue(lua_State* L, int index, const char* field) {
		lua_getfield(L, index, field);
		if (isNoneOrNil(L, -1)) {
			lua_pop(L, 1);
			return std::nullopt;
		}

		T v;
		if constexpr(std::is_integral_v<T> || std::is_floating_point_v<T>) {
			v = getNumber<T>(L, -1);
		} else if constexpr (std::is_same_v<T, bool>) {
			v = getBoolean(L, -1);
		} else {
			v = getString(L, -1);
		}

		lua_pop(L, 1);
		return v;
	}

	void addTempItem(ScriptEnvironment* env, Item* item);
	void removeTempItem(Item* item);

	ScriptEnvironment& getScriptEnv();
	bool reserveScriptEnv();
	void resetScriptEnv();

	bool protectedCall(lua_State* L, int nargs, int nresults);

	constexpr std::string_view getErrorDesc(LuaErrorCode_t code)
	{
		switch (code) {
			case LUA_ERROR_PLAYER_NOT_FOUND:
				return "Player not found";
			case LUA_ERROR_MONSTER_NOT_FOUND:
				return "Monster not found";
			case LUA_ERROR_NPC_NOT_FOUND:
				return "Npc not found";
			case LUA_ERROR_CREATURE_NOT_FOUND:
				return "Creature not found";
			case LUA_ERROR_ITEM_NOT_FOUND:
				return "Item not found";
			case LUA_ERROR_THING_NOT_FOUND:
				return "Thing not found";
			case LUA_ERROR_TILE_NOT_FOUND:
				return "Tile not found";
			case LUA_ERROR_HOUSE_NOT_FOUND:
				return "House not found";
			case LUA_ERROR_COMBAT_NOT_FOUND:
				return "Combat not found";
			case LUA_ERROR_CONDITION_NOT_FOUND:
				return "Condition not found";
			case LUA_ERROR_AREA_NOT_FOUND:
				return "Area not found";
			case LUA_ERROR_CONTAINER_NOT_FOUND:
				return "Container not found";
			case LUA_ERROR_VARIANT_NOT_FOUND:
				return "Variant not found";
			case LUA_ERROR_VARIANT_UNKNOWN:
				return "Unknown variant type";
			case LUA_ERROR_SPELL_NOT_FOUND:
				return "Spell not found";

			default:
				return "Invalid error code!";
		}
	}

	bool callFunction(lua_State* L, int nargs, bool releaseEnv = true);
	void callVoidFunction(lua_State* L, int nargs, bool releaseEnv = true);

} // otx::lua

class ScriptEnvironment final
{
public:
	ScriptEnvironment() = default;

	int getScriptId() const { return m_scriptId; };
	void setScriptId(int scriptId, LuaInterface* interface)
	{
		m_scriptId = scriptId;
		m_interface = interface;
	}

	void reset();

	int32_t getCallbackId() const { return m_callbackId; };
	bool setCallbackId(int32_t callbackId, LuaInterface* interface);

	Npc* getNpc() const { return m_curNpc; }
	void setNpc(Npc* npc) { m_curNpc = npc; }

	Thing* getThingByUID(uint32_t uid);
	Item* getItemByUID(uint32_t uid);
	Container* getContainerByUID(uint32_t uid);

	uint32_t addThing(Thing* thing);
	void insertItem(uint32_t uid, Item* item);
	void removeItem(uint32_t uid);

	uint32_t addCombat(Combat* combat);
	uint32_t addCombatArea(CombatArea* area);
	uint32_t addCondition(Condition* condition);

	LuaInterface* getInterface() { return m_interface; }

	void getInfo(int& scriptId, LuaInterface*& interface, int& callbackId, std::string*& timerScript);
	void resetCallback() { m_callbackId = 0; }

	void setTimerScript(std::string* script) { m_timerScript = script; }

private:
	std::unordered_map<uint32_t, Item*> m_localItemMap;

	LuaInterface* m_interface = nullptr;
	std::string* m_timerScript = nullptr;
	Npc* m_curNpc = nullptr;

	uint32_t m_lastUID = 0xFFFF;
	int m_scriptId = -1;
	int m_callbackId = -1;
};

class LuaInterface
{
public:
	LuaInterface(std::string interfaceName);
	virtual ~LuaInterface();

	virtual bool initState();
	virtual bool closeState();
	bool reInitState();

	bool loadFile(const std::string& file, Npc* npc = nullptr);
	bool loadDirectory(std::string dir, bool recursively, bool loadSystems, Npc* npc = nullptr);

	const std::string& getScript(int32_t scriptId);
	const std::string& getName() const { return m_interfaceName; };
	const std::string& getLastError() const { return m_lastError; }

	int getEvent(const std::string& eventName);
	lua_State* getState() { return m_luaState; }

	bool pushFunction(int functionId);

protected:
	lua_State* m_luaState = nullptr;
	int eventTableRef = -1;
	int m_runningEvent;

private:
	std::string m_lastError;
	std::string m_loadingFile;
	std::string m_interfaceName;

	std::map<int, std::string> m_cacheFiles;

	friend class LuaEnvironment;
};

class LuaEnvironment final
{
public:
	LuaEnvironment() = default;

	// non-copyable
	LuaEnvironment(const LuaEnvironment&) = delete;
	LuaEnvironment& operator=(const LuaEnvironment&) = delete;

	// non-moveable
	LuaEnvironment(LuaEnvironment&&) = delete;
	LuaEnvironment& operator=(LuaEnvironment&&) = delete;

	void init();
	void reload();
	void terminate();

	void addScriptInterface(LuaInterface* luaInterface);
	void removeScriptInterface(LuaInterface* luaInterface);

	uint32_t addTimerEvent(LuaTimerEvent&& timerEvent, uint32_t delay);
	bool stopTimerEvent(lua_State* L, uint32_t eventId);
	bool executeTimerEvent(uint32_t eventIndex);

	uint32_t addCondition(Condition* condition);
	Condition* getConditionById(uint32_t id);
	void removeConditionById(uint32_t id);

	uint32_t addCombat(Combat* combat);
	Combat* getCombatById(uint32_t id);
	void removeCombatById(uint32_t id);

	uint32_t addCombatArea(CombatArea* area);
	CombatArea* getCombatAreaById(uint32_t id);
	void removeCombatAreaById(uint32_t id);

	uint32_t getLastCombatId() const { return m_lastCombatId; }

	lua_State* getLuaState() const { return m_L; }
	LuaInterface* getMainInterface() const { return m_mainInterface.get(); }

	void unref(int ref) const;

	void collectGarbage() const;

	bool isErrorsEnabled() const { return m_errors; }
	void setErrorsEnabled(bool enabled) { m_errors = enabled; }

private:
	std::unordered_map<uint32_t, CombatPtr> m_combats;
	std::unordered_map<uint32_t, CombatAreaPtr> m_combatAreas;
	std::unordered_map<uint32_t, ConditionPtr> m_conditions;
	std::unordered_map<uint32_t, LuaTimerEvent> m_timerEvents;
	std::list<LuaInterface*> m_scriptInterfaces;
	lua_State* m_L = nullptr;
	LuaInterfacePtr m_mainInterface;
	uint32_t m_lastCombatId = 0;
	uint32_t m_lastCombatAreaId = 0;
	uint32_t m_lastConditionId = 0;
	uint32_t m_lastTimerEventId = 0;
	bool m_errors = true;
};

extern LuaEnvironment g_lua;
