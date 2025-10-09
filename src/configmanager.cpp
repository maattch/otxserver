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

#include "configmanager.h"

#include "house.h"
#include "lua_definitions.h"
#include "otx/cast.hpp"

ConfigManager g_config;

namespace
{

	std::string getConfigString(lua_State* L, const char* key, const std::string& def)
	{
		lua_getglobal(L, "g_config");
		if (!otx::lua::isTable(L, -1)) {
			lua_pop(L, 1);
			return def;
		}

		std::string result;
		lua_getfield(L, -1, key);
		if (otx::lua::isString(L, -1)) {
			result = otx::lua::getString(L, -1);
		} else {
			result = def;
		}

		lua_pop(L, 2);
		return result;
	}

	double getConfigDouble(lua_State* L, const char* key, double def)
	{
		lua_getglobal(L, "g_config");
		if (!otx::lua::isTable(L, -1)) {
			lua_pop(L, 1);
			return def;
		}

		double result;
		lua_getfield(L, -1, key);
		if (otx::lua::isNumber(L, -1)) {
			result = otx::lua::getNumber<double>(L, -1);
		} else {
			result = def;
		}

		lua_pop(L, 2);
		return result;
	}

	int64_t getConfigInteger(lua_State* L, const char* key, int64_t def)
	{
		lua_getglobal(L, "g_config");
		if (!otx::lua::isTable(L, -1)) {
			lua_pop(L, 1);
			return def;
		}

		int64_t result;
		lua_getfield(L, -1, key);
		if (otx::lua::isNumber(L, -1)) {
			result = otx::lua::getNumber<int64_t>(L, -1);
		} else {
			result = def;
		}

		lua_pop(L, 2);
		return result;
	}

	bool getConfigBoolean(lua_State* L, const char* key, bool def)
	{
		lua_getglobal(L, "g_config");
		if (!otx::lua::isTable(L, -1)) {
			lua_pop(L, 1);
			return def;
		}

		bool result;
		lua_getfield(L, -1, key);
		if (otx::lua::isBoolean(L, -1)) {
			result = otx::lua::getBoolean(L, -1);
		} else {
			result = def;
		}

		lua_pop(L, 2);
		return result;
	}

}; //namespace

ConfigManager::ConfigManager()
{
	m_confString[CONFIG_FILE] = getFilePath(FILE_TYPE_CONFIG, "config.lua");
}

bool ConfigManager::load()
{
	lua_State* L = g_lua.getLuaState();

	if (luaL_loadfile(L, "config.lua") != 0) {
		std::clog << "[Error - ConfigManager::load] " << otx::lua::popString(L) << std::endl;
		return false;
	}

	lua_createtable(L, 0, 0);
	lua_pushvalue(L, -1);
	lua_setglobal(L, "g_config");
	lua_setfenv(L, -2);

	if (lua_pcall(L, 0, 0, 0) != 0) {
		std::clog << "[Error - ConfigManager::load] " << otx::lua::popString(L) << std::endl;
		return false;
	}

	if (!m_loaded) {
		// info that must be loaded one time- unless we reset the modules involved
		if (m_confString[IP] == "") {
			m_confString[IP] = getConfigString(L, "ip", "127.0.0.1");
		}

		// Convert ip string to number
		std::string ipString = m_confString[IP];
		IP_NUMBER = inet_addr(ipString.data());
		if (IP_NUMBER == INADDR_NONE) {
			if (hostent* hostname = gethostbyname(ipString.data())) {
				ipString = std::string(inet_ntoa(**reinterpret_cast<in_addr**>(hostname->h_addr_list)));
				IP_NUMBER = inet_addr(ipString.data());
			}
		}

		if (IP_NUMBER == INADDR_NONE) {
			std::cout << "[Error - ConfigManager::load] Cannot resolve given ip address, make sure you typed correct IP or hostname." << std::endl;
			return false;
		}

		if (m_confString[DATA_DIRECTORY] == "") {
			m_confString[DATA_DIRECTORY] = getConfigString(L, "dataDirectory", "data/");
		}

		if (m_confString[LOGS_DIRECTORY] == "") {
			m_confString[LOGS_DIRECTORY] = getConfigString(L, "logsDirectory", "logs/");
		}

		if (m_confNumber[LOGIN_PORT] == 0) {
			m_confNumber[LOGIN_PORT] = getConfigInteger(L, "loginPort", 7171);
		}

		if (m_confNumber[GAME_PORT] == 0) {
			// backwards-compatibility
			std::string port = getConfigString(L, "gamePort", "7172");
			m_confNumber[GAME_PORT] = otx::util::cast<int64_t>(port.data());
		}

		if (m_confNumber[STATUS_PORT] == 0) {
			m_confNumber[STATUS_PORT] = getConfigInteger(L, "statusPort", 7171);
		}

		if (m_confString[RUNFILE] == "") {
			m_confString[RUNFILE] = getConfigString(L, "runFile", "");
		}

		if (m_confString[OUTPUT_LOG] == "") {
			m_confString[OUTPUT_LOG] = getConfigString(L, "outputLog", "");
		}

		m_confBool[TRUNCATE_LOG] = getConfigBoolean(L, "truncateLogOnStartup", true);
		m_confString[SQL_HOST] = getConfigString(L, "sqlHost", "localhost");
		m_confNumber[SQL_PORT] = getConfigInteger(L, "sqlPort", 3306);
		m_confString[SQL_DB] = getConfigString(L, "sqlDatabase", "theotxserver");
		m_confString[SQL_USER] = getConfigString(L, "sqlUser", "root");
		m_confString[SQL_PASS] = getConfigString(L, "sqlPass", "");
		m_confString[SQL_FILE] = getConfigString(L, "sqlFile", "otxserver.s3db");
		m_confNumber[SQL_KEEPALIVE] = getConfigInteger(L, "sqlKeepAlive", 0);
		m_confNumber[MYSQL_READ_TIMEOUT] = getConfigInteger(L, "mysqlReadTimeout", 10);
		m_confNumber[MYSQL_WRITE_TIMEOUT] = getConfigInteger(L, "mysqlWriteTimeout", 10);
		m_confBool[OPTIMIZE_DATABASE] = getConfigBoolean(L, "startupDatabaseOptimization", true);
		m_confString[MAP_NAME] = getConfigString(L, "mapName", "forgotten.otbm");
		m_confBool[GLOBALSAVE_ENABLED] = getConfigBoolean(L, "globalSaveEnabled", true);
		m_confNumber[GLOBALSAVE_H] = getConfigInteger(L, "globalSaveHour", 8);
		m_confNumber[GLOBALSAVE_M] = getConfigInteger(L, "globalSaveMinute", 0);
		m_confString[HOUSE_RENT_PERIOD] = getConfigString(L, "houseRentPeriod", "monthly");
		m_confBool[STORE_TRASH] = getConfigBoolean(L, "storeTrash", true);
		m_confString[DEFAULT_PRIORITY] = getConfigString(L, "defaultPriority", "high");
		m_confBool[BIND_ONLY_GLOBAL_ADDRESS] = getConfigBoolean(L, "bindOnlyGlobalAddress", false);
		m_confBool[GUILD_HALLS] = getConfigBoolean(L, "guildHalls", false);
		m_confBool[LOGIN_ONLY_LOGINSERVER] = getConfigBoolean(L, "loginOnlyWithLoginServer", false);
	}

	m_confBool[CAST_EXP_ENABLED] = getConfigBoolean(L, "expInCast", false); // this edit by feetads
	m_confNumber[CAST_EXP_PERCENT] = getConfigInteger(L, "expPercentIncast", 5.0);
	m_confBool[MAXIP_USECONECT] = getConfigBoolean(L, "UseMaxIpConnect", false); // this edit by feetads

	m_confBool[ADD_FRAG_SAMEIP] = getConfigBoolean(L, "addFragToSameIp", false);

	m_confBool[USE_MAX_ABSORBALL] = getConfigBoolean(L, "useMaxAbsorbAll", false);
	m_confDouble[MAX_ABSORB_PERCENT] = getConfigDouble(L, "maxAbsorbPercent", 80.0);

	m_confString[FORBIDDEN_NAMES] = getConfigString(L, "forbiddenNames", "gm;adm;cm;support;god;tutor");

	m_confBool[DELETE_PLAYER_MONSTER_NAME] = getConfigBoolean(L, "deletePlayersWithMonsterName", false);

	m_confBool[POTION_CAN_EXHAUST_ITEM] = getConfigBoolean(L, "exhaustItemAtUsePotion", false);

	m_confBool[USEDAMAGE_IN_K] = getConfigBoolean(L, "modifyDamageInK", false);
	m_confBool[USEEXP_IN_K] = getConfigBoolean(L, "modifyExperienceInK", false);

	m_confBool[DISPLAY_BROADCAST] = getConfigBoolean(L, "displayBroadcastLog", true); // by kizuno18

	m_confNumber[EXHAUST_POTION] = getConfigInteger(L, "exhaustPotionMiliSeconds", 1500);
	// exhaust to spectator say
	m_confNumber[EXHAUST_SPECTATOR_SAY] = getConfigInteger(L, "exhaust_spectatorSay", 3);

	m_confString[MAP_AUTHOR] = getConfigString(L, "mapAuthor", "Unknown");
	m_confNumber[LOGIN_TRIES] = getConfigInteger(L, "loginTries", 3);
	m_confNumber[RETRY_TIMEOUT] = getConfigInteger(L, "retryTimeout", 30000);
	m_confNumber[LOGIN_TIMEOUT] = getConfigInteger(L, "loginTimeout", 5000);
	m_confNumber[MAX_MESSAGEBUFFER] = getConfigInteger(L, "maxMessageBuffer", 4);
	m_confNumber[MAX_PLAYERS] = getConfigInteger(L, "maxPlayers", 1000);
	m_confNumber[DEFAULT_DESPAWNRANGE] = getConfigInteger(L, "deSpawnRange", 2);
	m_confNumber[DEFAULT_DESPAWNRADIUS] = getConfigInteger(L, "deSpawnRadius", 50);
	m_confNumber[PZ_LOCKED] = getConfigInteger(L, "pzLocked", 60000);
	m_confNumber[HUNTING_DURATION] = getConfigInteger(L, "huntingDuration", 60000);
	m_confString[SERVER_NAME] = getConfigString(L, "serverName", "");
	m_confString[OWNER_NAME] = getConfigString(L, "ownerName", "");
	m_confString[OWNER_EMAIL] = getConfigString(L, "ownerEmail", "");
	m_confString[URL] = getConfigString(L, "url", "");
	m_confString[LOCATION] = getConfigString(L, "location", "");
	m_confString[MOTD] = getConfigString(L, "motd", "");
	m_confBool[EXPERIENCE_STAGES] = getConfigBoolean(L, "experienceStages", false);
	m_confDouble[RATE_EXPERIENCE] = getConfigDouble(L, "rateExperience", 1);
	m_confDouble[RATE_SKILL] = getConfigDouble(L, "rateSkill", 1);
	m_confDouble[RATE_SKILL_OFFLINE] = getConfigDouble(L, "rateSkillOffline", 0.5);
	m_confDouble[RATE_MAGIC] = getConfigDouble(L, "rateMagic", 1);
	m_confDouble[RATE_MAGIC_OFFLINE] = getConfigDouble(L, "rateMagicOffline", 0.5);
	m_confDouble[RATE_LOOT] = getConfigDouble(L, "rateLoot", 1);
	m_confNumber[RATE_SPAWN_MIN] = getConfigInteger(L, "rateSpawnMin", 1);
	m_confNumber[RATE_SPAWN_MAX] = getConfigInteger(L, "rateSpawnMax", 1);
	m_confNumber[PARTY_RADIUS_X] = getConfigInteger(L, "experienceShareRadiusX", 30);
	m_confNumber[PARTY_RADIUS_Y] = getConfigInteger(L, "experienceShareRadiusY", 30);
	m_confNumber[PARTY_RADIUS_Z] = getConfigInteger(L, "experienceShareRadiusZ", 1);
	m_confDouble[PARTY_DIFFERENCE] = getConfigDouble(L, "experienceShareLevelDifference", (double)2 / 3);
	m_confNumber[SPAWNPOS_X] = getConfigInteger(L, "newPlayerSpawnPosX", 100);
	m_confNumber[SPAWNPOS_Y] = getConfigInteger(L, "newPlayerSpawnPosY", 100);
	m_confNumber[SPAWNPOS_Z] = getConfigInteger(L, "newPlayerSpawnPosZ", 7);
	m_confNumber[SPAWNTOWN_ID] = getConfigInteger(L, "newPlayerTownId", 1);
	m_confString[WORLD_TYPE] = getConfigString(L, "worldType", "open");
	m_confBool[ACCOUNT_MANAGER] = getConfigBoolean(L, "accountManager", true);
	m_confBool[NAMELOCK_MANAGER] = getConfigBoolean(L, "namelockManager", false);
	m_confNumber[START_LEVEL] = getConfigInteger(L, "newPlayerLevel", 1);
	m_confNumber[START_MAGICLEVEL] = getConfigInteger(L, "newPlayerMagicLevel", 0);
	m_confBool[START_CHOOSEVOC] = getConfigBoolean(L, "newPlayerChooseVoc", false);
	m_confNumber[HOUSE_PRICE] = getConfigInteger(L, "housePriceEachSquare", 1000);
	m_confNumber[WHITE_SKULL_TIME] = getConfigInteger(L, "whiteSkullTime", 900000);
	m_confBool[ON_OR_OFF_CHARLIST] = getConfigBoolean(L, "displayOnOrOffAtCharlist", false);
	m_confBool[ALLOW_CHANGEOUTFIT] = getConfigBoolean(L, "allowChangeOutfit", true);
	m_confBool[ONE_PLAYER_ON_ACCOUNT] = getConfigBoolean(L, "onePlayerOnlinePerAccount", true);
	m_confBool[CANNOT_ATTACK_SAME_LOOKFEET] = getConfigBoolean(L, "noDamageToSameLookfeet", false);
	m_confBool[AIMBOT_HOTKEY_ENABLED] = getConfigBoolean(L, "hotkeyAimbotEnabled", true);
	m_confNumber[ACTIONS_DELAY_INTERVAL] = getConfigInteger(L, "timeBetweenActions", 200);
	m_confNumber[EX_ACTIONS_DELAY_INTERVAL] = getConfigInteger(L, "timeBetweenExActions", 1000);
	m_confNumber[CUSTOM_ACTIONS_DELAY_INTERVAL] = getConfigInteger(L, "timeBetweenCustomActions", 500);
	m_confNumber[CRITICAL_HIT_CHANCE] = getConfigInteger(L, "criticalHitChance", 5);
	m_confBool[REMOVE_WEAPON_AMMO] = getConfigBoolean(L, "removeWeaponAmmunition", true);
	m_confBool[REMOVE_WEAPON_CHARGES] = getConfigBoolean(L, "removeWeaponCharges", true);
	m_confBool[REMOVE_RUNE_CHARGES] = getConfigBoolean(L, "removeRuneCharges", true);
	m_confDouble[RATE_PVP_EXPERIENCE] = getConfigDouble(L, "rateExperienceFromPlayers", 0);
	m_confDouble[EFP_MIN_THRESHOLD] = getConfigDouble(L, "minLevelThresholdForKilledPlayer", 0.9);
	m_confDouble[EFP_MAX_THRESHOLD] = getConfigDouble(L, "maxLevelThresholdForKilledPlayer", 1.1);
	m_confBool[SHUTDOWN_AT_GLOBALSAVE] = getConfigBoolean(L, "shutdownAtGlobalSave", false);
	m_confBool[CLEAN_MAP_AT_GLOBALSAVE] = getConfigBoolean(L, "cleanMapAtGlobalSave", true);
	m_confBool[FREE_PREMIUM] = getConfigBoolean(L, "freePremium", false);
	m_confNumber[PROTECTION_LEVEL] = getConfigInteger(L, "protectionLevel", 1);
	m_confNumber[STATUSQUERY_TIMEOUT] = getConfigInteger(L, "statusTimeout", 300000);
	m_confBool[BROADCAST_BANISHMENTS] = getConfigBoolean(L, "broadcastBanishments", true);
	m_confBool[GENERATE_ACCOUNT_NUMBER] = getConfigBoolean(L, "generateAccountNumber", false);
	m_confBool[GENERATE_ACCOUNT_SALT] = getConfigBoolean(L, "generateAccountSalt", false);
	m_confBool[INGAME_GUILD_MANAGEMENT] = getConfigBoolean(L, "ingameGuildManagement", true);
	m_confBool[EXTERNAL_GUILD_WARS_MANAGEMENT] = getConfigBoolean(L, "externalGuildWarsManagement", false);
	m_confNumber[LEVEL_TO_FORM_GUILD] = getConfigInteger(L, "levelToFormGuild", 8);
	m_confNumber[MIN_GUILDNAME] = getConfigInteger(L, "guildNameMinLength", 4);
	m_confNumber[MAX_GUILDNAME] = getConfigInteger(L, "guildNameMaxLength", 20);
	m_confNumber[LEVEL_TO_BUY_HOUSE] = getConfigInteger(L, "levelToBuyHouse", 1);
	m_confNumber[HOUSES_PER_ACCOUNT] = getConfigInteger(L, "housesPerAccount", 0);
	m_confBool[HOUSE_BUY_AND_SELL] = getConfigBoolean(L, "buyableAndSellableHouses", true);
	m_confBool[REPLACE_KICK_ON_LOGIN] = getConfigBoolean(L, "replaceKickOnLogin", true);
	m_confBool[HOUSE_NEED_PREMIUM] = getConfigBoolean(L, "houseNeedPremium", true);
	m_confBool[HOUSE_RENTASPRICE] = getConfigBoolean(L, "houseRentAsPrice", false);
	m_confBool[HOUSE_PRICEASRENT] = getConfigBoolean(L, "housePriceAsRent", false);
	m_confString[HOUSE_STORAGE] = getConfigString(L, "houseDataStorage", "binary");
	m_confNumber[RED_SKULL_LENGTH] = getConfigInteger(L, "redSkullLength", 2592000);
	m_confNumber[MAX_VIOLATIONCOMMENT_SIZE] = getConfigInteger(L, "maxViolationCommentSize", 60);
	m_confNumber[BLACK_SKULL_LENGTH] = getConfigInteger(L, "blackSkullLength", 3888000);
	m_confNumber[NOTATIONS_TO_BAN] = getConfigInteger(L, "notationsToBan", 3);
	m_confNumber[WARNINGS_TO_FINALBAN] = getConfigInteger(L, "warningsToFinalBan", 4);
	m_confNumber[WARNINGS_TO_DELETION] = getConfigInteger(L, "warningsToDeletion", 5);
	m_confNumber[BAN_LENGTH] = getConfigInteger(L, "banLength", 604800);
	m_confNumber[KILLS_BAN_LENGTH] = getConfigInteger(L, "killsBanLength", 604800);
	m_confNumber[FINALBAN_LENGTH] = getConfigInteger(L, "finalBanLength", 2592000);
	m_confNumber[IPBAN_LENGTH] = getConfigInteger(L, "ipBanLength", 86400);
	m_confBool[BANK_SYSTEM] = getConfigBoolean(L, "bankSystem", true);
	m_confBool[PREMIUM_FOR_PROMOTION] = getConfigBoolean(L, "premiumForPromotion", true);
	m_confBool[INIT_PREMIUM_UPDATE] = getConfigBoolean(L, "updatePremiumStateAtStartup", true);
	m_confBool[SHOW_HEALTH_CHANGE] = getConfigBoolean(L, "showHealthChange", true);
	m_confBool[SHOW_MANA_CHANGE] = getConfigBoolean(L, "showManaChange", true);
	m_confBool[TELEPORT_SUMMONS] = getConfigBoolean(L, "teleportAllSummons", false);
	m_confBool[TELEPORT_PLAYER_SUMMONS] = getConfigBoolean(L, "teleportPlayerSummons", false);
	m_confBool[PVP_TILE_IGNORE_PROTECTION] = getConfigBoolean(L, "pvpTileIgnoreLevelAndVocationProtection", true);
	m_confBool[DISPLAY_CRITICAL_HIT] = getConfigBoolean(L, "displayCriticalHitNotify", false);
	m_confBool[CLEAN_PROTECTED_ZONES] = getConfigBoolean(L, "cleanProtectedZones", true);
	m_confBool[SPELL_NAME_INSTEAD_WORDS] = getConfigBoolean(L, "spellNameInsteadOfWords", false);
	m_confBool[EMOTE_SPELLS] = getConfigBoolean(L, "emoteSpells", false);
	m_confNumber[MAX_PLAYER_SUMMONS] = getConfigInteger(L, "maxPlayerSummons", 2);
	m_confBool[SAVE_GLOBAL_STORAGE] = getConfigBoolean(L, "saveGlobalStorage", true);
	m_confBool[SAVE_PLAYER_DATA] = getConfigBoolean(L, "savePlayerData", true);
	m_confBool[FORCE_CLOSE_SLOW_CONNECTION] = getConfigBoolean(L, "forceSlowConnectionsToDisconnect", false);
	m_confBool[BLESSINGS] = getConfigBoolean(L, "blessings", true);
	m_confBool[BLESSING_ONLY_PREMIUM] = getConfigBoolean(L, "blessingOnlyPremium", true);
	m_confBool[BED_REQUIRE_PREMIUM] = getConfigBoolean(L, "bedsRequirePremium", true);
	m_confNumber[FIELD_OWNERSHIP] = getConfigInteger(L, "fieldOwnershipDuration", 5000);
	m_confBool[ALLOW_CHANGECOLORS] = getConfigBoolean(L, "allowChangeColors", true);
	m_confBool[STOP_ATTACK_AT_EXIT] = getConfigBoolean(L, "stopAttackingAtExit", false);
	m_confNumber[EXTRA_PARTY_PERCENT] = getConfigInteger(L, "extraPartyExperiencePercent", 5);
	m_confNumber[EXTRA_PARTY_LIMIT] = getConfigInteger(L, "extraPartyExperienceLimit", 20);
	m_confBool[DISABLE_OUTFITS_PRIVILEGED] = getConfigBoolean(L, "disableOutfitsForPrivilegedPlayers", false);
	m_confNumber[LOGIN_PROTECTION] = getConfigInteger(L, "loginProtectionPeriod", 10000);
	m_confBool[STORE_DIRECTION] = getConfigBoolean(L, "storePlayerDirection", false);
	m_confNumber[PLAYER_DEEPNESS] = getConfigInteger(L, "playerQueryDeepness", -1);
	m_confDouble[CRITICAL_HIT_MUL] = getConfigDouble(L, "criticalHitMultiplier", 1);
	m_confNumber[STAIRHOP_DELAY] = getConfigInteger(L, "stairhopDelay", 2 * 1000);
	m_confNumber[RATE_STAMINA_LOSS] = getConfigInteger(L, "rateStaminaLoss", 1);
	m_confDouble[RATE_STAMINA_GAIN] = getConfigDouble(L, "rateStaminaGain", 3);
	m_confDouble[RATE_STAMINA_THRESHOLD] = getConfigDouble(L, "rateStaminaThresholdGain", 12);
	m_confDouble[RATE_STAMINA_ABOVE] = getConfigDouble(L, "rateStaminaAboveNormal", 1.5);
	m_confDouble[RATE_STAMINA_UNDER] = getConfigDouble(L, "rateStaminaUnderNormal", 0.5);
	m_confNumber[STAMINA_LIMIT_TOP] = getConfigInteger(L, "staminaRatingLimitTop", 2460);
	m_confNumber[STAMINA_LIMIT_BOTTOM] = getConfigInteger(L, "staminaRatingLimitBottom", 840);
	m_confBool[DISPLAY_LOGGING] = getConfigBoolean(L, "displayPlayersLogging", true);
	m_confBool[STAMINA_BONUS_PREMIUM] = getConfigBoolean(L, "staminaThresholdOnlyPremium", true);
	m_confNumber[BLESS_REDUCTION_BASE] = getConfigInteger(L, "blessingReductionBase", 30);
	m_confNumber[BLESS_REDUCTION_DECREMENT] = getConfigInteger(L, "blessingReductionDecrement", 5);
	m_confBool[ALLOW_CHANGEADDONS] = getConfigBoolean(L, "allowChangeAddons", true);
	m_confNumber[BLESS_REDUCTION] = getConfigInteger(L, "eachBlessReduction", 8);
	m_confDouble[FORMULA_LEVEL] = getConfigDouble(L, "formulaLevel", 5.0);
	m_confDouble[FORMULA_MAGIC] = getConfigDouble(L, "formulaMagic", 1.0);
	m_confString[PREFIX_CHANNEL_LOGS] = getConfigString(L, "prefixChannelLogs", "");
	m_confBool[GHOST_INVISIBLE_EFFECT] = getConfigBoolean(L, "ghostModeInvisibleEffect", false);
	m_confString[CORES_USED] = getConfigString(L, "coresUsed", "-1");
	m_confNumber[NICE_LEVEL] = getConfigInteger(L, "niceLevel", 5);
	m_confNumber[EXPERIENCE_COLOR] = getConfigInteger(L, "gainExperienceColor", COLOR_WHITE);
	m_confBool[SHOW_HEALTH_CHANGE_MONSTER] = getConfigBoolean(L, "showHealthChangeForMonsters", true);
	m_confBool[SHOW_MANA_CHANGE_MONSTER] = getConfigBoolean(L, "showManaChangeForMonsters", true);
	m_confBool[CHECK_CORPSE_OWNER] = getConfigBoolean(L, "checkCorpseOwner", true);
	m_confBool[BUFFER_SPELL_FAILURE] = getConfigBoolean(L, "bufferMutedOnSpellFailure", false);
	m_confNumber[GUILD_PREMIUM_DAYS] = getConfigInteger(L, "premiumDaysToFormGuild", 0);
	m_confNumber[PUSH_CREATURE_DELAY] = getConfigInteger(L, "pushCreatureDelay", 2000);
	m_confNumber[DEATH_CONTAINER] = getConfigInteger(L, "deathContainerId", 1987);
	m_confBool[PREMIUM_SKIP_WAIT] = getConfigBoolean(L, "premiumPlayerSkipWaitList", false);
	m_confNumber[MAXIMUM_DOOR_LEVEL] = getConfigInteger(L, "maximumDoorLevel", 500);
	m_confBool[DEATH_LIST] = getConfigBoolean(L, "deathListEnabled", true);
	m_confNumber[DEATH_ASSISTS] = getConfigInteger(L, "deathAssistCount", 1);
	m_confNumber[FRAG_LIMIT] = getConfigInteger(L, "fragsLimit", 86400);
	m_confNumber[FRAG_SECOND_LIMIT] = getConfigInteger(L, "fragsSecondLimit", 604800);
	m_confNumber[FRAG_THIRD_LIMIT] = getConfigInteger(L, "fragsThirdLimit", 2592000);
	m_confNumber[RED_LIMIT] = getConfigInteger(L, "fragsToRedSkull", 3);
	m_confNumber[RED_SECOND_LIMIT] = getConfigInteger(L, "fragsSecondToRedSkull", 5);
	m_confNumber[RED_THIRD_LIMIT] = getConfigInteger(L, "fragsThirdToRedSkull", 10);
	m_confNumber[BLACK_LIMIT] = getConfigInteger(L, "fragsToBlackSkull", m_confNumber[RED_LIMIT]);
	m_confNumber[BLACK_SECOND_LIMIT] = getConfigInteger(L, "fragsSecondToBlackSkull", m_confNumber[RED_SECOND_LIMIT]);
	m_confNumber[BLACK_THIRD_LIMIT] = getConfigInteger(L, "fragsThirdToBlackSkull", m_confNumber[RED_THIRD_LIMIT]);
	m_confNumber[BAN_LIMIT] = getConfigInteger(L, "fragsToBanishment", m_confNumber[RED_LIMIT]);
	m_confNumber[BAN_SECOND_LIMIT] = getConfigInteger(L, "fragsSecondToBanishment", m_confNumber[RED_SECOND_LIMIT]);
	m_confNumber[BAN_THIRD_LIMIT] = getConfigInteger(L, "fragsThirdToBanishment", m_confNumber[RED_THIRD_LIMIT]);
	m_confNumber[BLACK_SKULL_DEATH_HEALTH] = getConfigInteger(L, "blackSkulledDeathHealth", 40);
	m_confNumber[BLACK_SKULL_DEATH_MANA] = getConfigInteger(L, "blackSkulledDeathMana", 0);
	m_confNumber[DEATHLIST_REQUIRED_TIME] = getConfigInteger(L, "deathListRequiredTime", 60000);
	m_confNumber[EXPERIENCE_SHARE_ACTIVITY] = getConfigInteger(L, "experienceShareActivity", 120000);
	m_confBool[GHOST_SPELL_EFFECTS] = getConfigBoolean(L, "ghostModeSpellEffects", true);
	m_confBool[PVPZONE_ADDMANASPENT] = getConfigBoolean(L, "addManaSpentInPvPZone", true);
	m_confBool[PVPZONE_RECOVERMANA] = getConfigBoolean(L, "recoverManaAfterDeathInPvPZone", false);
	m_confNumber[TILE_LIMIT] = getConfigInteger(L, "tileLimit", 0);
	m_confNumber[PROTECTION_TILE_LIMIT] = getConfigInteger(L, "protectionTileLimit", 0);
	m_confNumber[HOUSE_TILE_LIMIT] = getConfigInteger(L, "houseTileLimit", 0);
	m_confNumber[TRADE_LIMIT] = getConfigInteger(L, "tradeLimit", 100);
	m_confString[MAILBOX_DISABLED_TOWNS] = getConfigString(L, "mailboxDisabledTowns", "");
	m_confNumber[SQUARE_COLOR] = getConfigInteger(L, "squareColor", 0);
	m_confBool[USE_BLACK_SKULL] = getConfigBoolean(L, "useBlackSkull", true);
	m_confBool[USE_FRAG_HANDLER] = getConfigBoolean(L, "useFragHandler", true);
	m_confNumber[LOOT_MESSAGE] = getConfigInteger(L, "monsterLootMessage", 3);
	m_confNumber[LOOT_MESSAGE_TYPE] = getConfigInteger(L, "monsterLootMessageType", 19);
	m_confNumber[NAME_REPORT_TYPE] = getConfigInteger(L, "violationNameReportActionType", 2);
	m_confNumber[HOUSE_CLEAN_OLD] = getConfigInteger(L, "houseCleanOld", 0);
	m_confNumber[MAX_IP_CONNECTIONS] = getConfigInteger(L, "MaxIpConnections", 4); // Swatt'
	m_confBool[VIPLIST_PER_PLAYER] = getConfigBoolean(L, "separateVipListPerCharacter", false);
	m_confDouble[RATE_MONSTER_HEALTH] = getConfigDouble(L, "rateMonsterHealth", 1);
	m_confDouble[RATE_MONSTER_MANA] = getConfigDouble(L, "rateMonsterMana", 1);
	m_confDouble[RATE_MONSTER_ATTACK] = getConfigDouble(L, "rateMonsterAttack", 1);
	m_confDouble[RATE_MONSTER_DEFENSE] = getConfigDouble(L, "rateMonsterDefense", 1);
	m_confBool[ADDONS_PREMIUM] = getConfigBoolean(L, "addonsOnlyPremium", true);
	m_confBool[UNIFIED_SPELLS] = getConfigBoolean(L, "unifiedSpells", true);
	m_confBool[OPTIONAL_WAR_ATTACK_ALLY] = getConfigBoolean(L, "optionalWarAttackableAlly", false);
	m_confNumber[VIPLIST_DEFAULT_LIMIT] = getConfigInteger(L, "vipListDefaultLimit", 20);
	m_confNumber[VIPLIST_DEFAULT_PREMIUM_LIMIT] = getConfigInteger(L, "vipListDefaultPremiumLimit", 100);
	m_confNumber[STAMINA_DESTROY_LOOT] = getConfigInteger(L, "staminaLootLimit", 840);
	m_confNumber[FIST_BASE_ATTACK] = getConfigInteger(L, "fistBaseAttack", 7);
	m_confBool[MONSTER_SPAWN_WALKBACK] = getConfigBoolean(L, "monsterSpawnWalkback", true);
	m_confNumber[PVP_BLESSING_THRESHOLD] = getConfigInteger(L, "pvpBlessingThreshold", 40);
	m_confNumber[FAIRFIGHT_TIMERANGE] = getConfigInteger(L, "fairFightTimeRange", 60);
	m_confNumber[DEFAULT_DEPOT_SIZE_PREMIUM] = getConfigInteger(L, "defaultDepotSizePremium", 2000);
	m_confNumber[DEFAULT_DEPOT_SIZE] = getConfigInteger(L, "defaultDepotSize", 2000);
	m_confBool[USE_CAPACITY] = getConfigBoolean(L, "useCapacity", true);
	m_confBool[DAEMONIZE] = getConfigBoolean(L, "daemonize", false);
	m_confBool[SKIP_ITEMS_VERSION] = getConfigBoolean(L, "skipItemsVersionCheck", false);
	m_confBool[SILENT_LUA] = getConfigBoolean(L, "disableLuaErrors", false);
	m_confNumber[MAIL_ATTEMPTS] = getConfigInteger(L, "mailMaxAttempts", 20);
	m_confNumber[MAIL_BLOCK] = getConfigInteger(L, "mailBlockPeriod", 3600000);
	m_confNumber[MAIL_ATTEMPTS_FADE] = getConfigInteger(L, "mailAttemptsFadeTime", 600000);
	m_confBool[HOUSE_SKIP_INIT_RENT] = getConfigBoolean(L, "houseSkipInitialRent", true);
	m_confBool[HOUSE_PROTECTION] = getConfigBoolean(L, "houseProtection", false);
	m_confBool[HOUSE_OWNED_BY_ACCOUNT] = getConfigBoolean(L, "houseOwnedByAccount", true);
	m_confBool[FAIRFIGHT_REDUCTION] = getConfigBoolean(L, "useFairfightReduction", true);
	m_confNumber[MYSQL_RECONNECTION_ATTEMPTS] = getConfigInteger(L, "mysqlReconnectionAttempts", 3);
	m_confBool[ALLOW_BLOCK_SPAWN] = getConfigBoolean(L, "allowBlockSpawn", true);
	m_confNumber[FOLLOW_EXHAUST] = getConfigInteger(L, "playerFollowExhaust", 2000);
	m_confBool[MULTIPLE_NAME] = getConfigBoolean(L, "multipleNames", false);
	m_confNumber[MAX_PACKETS_PER_SECOND] = getConfigInteger(L, "packetsPerSecond", 50);
	m_confString[ADVERTISING_BLOCK] = getConfigString(L, "advertisingBlock", "");
	m_confBool[SAVE_STATEMENT] = getConfigBoolean(L, "logPlayersStatements", true);
	m_confBool[MANUAL_ADVANCED_CONFIG] = getConfigBoolean(L, "manualVersionConfig", false);
	m_confNumber[VERSION_MIN] = getConfigInteger(L, "versionMin", CLIENT_VERSION_MIN);
	m_confNumber[VERSION_MAX] = getConfigInteger(L, "versionMax", CLIENT_VERSION_MAX);
	m_confString[VERSION_MSG] = getConfigString(L, "versionMsg", "Only clients with protocol " CLIENT_VERSION_STRING " allowed!");
	m_confBool[USE_RUNE_REQUIREMENTS] = getConfigBoolean(L, "useRunesRequirements", true);
	m_confNumber[HIGHSCORES_TOP] = getConfigInteger(L, "highscoreDisplayPlayers", 10);
	m_confNumber[HIGHSCORES_UPDATETIME] = getConfigInteger(L, "updateHighscoresAfterMinutes", 60);
	m_confNumber[LOGIN_PROTECTION_TIME] = getConfigInteger(L, "loginProtectionTime", 10);
	m_confBool[CLASSIC_EQUIPMENT_SLOTS] = getConfigBoolean(L, "classicEquipmentSlots", false);
	m_confBool[PARTY_VOCATION_MULT] = getConfigBoolean(L, "enablePartyVocationBonus", false);
	m_confDouble[TWO_VOCATION_PARTY] = getConfigDouble(L, "twoVocationExpMultiplier", 1.4);
	m_confDouble[THREE_VOCATION_PARTY] = getConfigDouble(L, "threeVocationExpMultiplier", 1.6);
	m_confDouble[FOUR_VOCATION_PARTY] = getConfigDouble(L, "fourVocationExpMultiplier", 2.0);
	m_confBool[NO_ATTACKHEALING_SIMULTANEUS] = getConfigBoolean(L, "noAttackHealingSimultaneus", false);
	m_confBool[OPTIONAL_PROTECTION] = getConfigBoolean(L, "optionalProtection", true);
	m_confBool[MONSTER_ATTACK_MONSTER] = getConfigBoolean(L, "monsterAttacksOnlyDamagePlayers", true);
	m_confBool[ALLOW_CORPSE_BLOCK] = getConfigBoolean(L, "allowCorpseBlock", false);
	m_confBool[ALLOW_INDEPENDENT_PUSH] = getConfigBoolean(L, "allowIndependentCreaturePush", true);
	m_confBool[DIAGONAL_PUSH] = getConfigBoolean(L, "diagonalPush", true);
	m_confBool[PZLOCK_ON_ATTACK_SKULLED_PLAYERS] = getConfigBoolean(L, "pzlockOnAttackSkulledPlayers", false);
	m_confBool[PUSH_IN_PZ] = getConfigBoolean(L, "pushInProtectZone", true);

	m_loaded = true;
	return true;
}

bool ConfigManager::reload()
{
	if (!m_loaded) {
		return false;
	}

	uint32_t tmp = m_confNumber[HOUSE_PRICE];
	if (!load()) {
		return false;
	}

	if ((uint32_t)m_confNumber[HOUSE_PRICE] == tmp) {
		return true;
	}

	for (const auto& it : Houses::getInstance()->getHouses()) {
		uint32_t price = it.second->getTilesCount() * m_confNumber[HOUSE_PRICE];
		if (m_confBool[HOUSE_RENTASPRICE]) {
			uint32_t rent = it.second->getRent();
			if (!m_confBool[HOUSE_PRICEASRENT] && it.second->getPrice() != rent) {
				price = rent;
			}
		}

		it.second->setPrice(price);
		if (m_confBool[HOUSE_PRICEASRENT]) {
			it.second->setRent(price);
		}

		if (!it.second->getOwner()) {
			it.second->updateDoorDescription();
		}
	}
	return true;
}

const std::string& ConfigManager::getString(uint32_t _what) const
{
	if ((m_loaded && _what < LAST_STRING_CONFIG) || _what <= CONFIG_FILE) {
		return m_confString[_what];
	}

	if (!m_startup) {
		std::clog << "[Warning - ConfigManager::getString] " << _what << std::endl;
	}
	return m_confString[DUMMY_STR];
}

bool ConfigManager::getBool(uint32_t _what) const
{
	if (m_loaded && _what < LAST_BOOL_CONFIG) {
		return m_confBool[_what];
	}

	if (!m_startup) {
		std::clog << "[Warning - ConfigManager::getBool] " << _what << std::endl;
	}
	return false;
}

int64_t ConfigManager::getNumber(uint32_t _what) const
{
	if (m_loaded && _what < LAST_NUMBER_CONFIG) {
		return m_confNumber[_what];
	}

	if (!m_startup) {
		std::clog << "[Warning - ConfigManager::getNumber] " << _what << std::endl;
	}
	return 0;
}

double ConfigManager::getDouble(uint32_t _what) const
{
	if (m_loaded && _what < LAST_DOUBLE_CONFIG) {
		return m_confDouble[_what];
	}

	if (!m_startup) {
		std::clog << "[Warning - ConfigManager::getDouble] " << _what << std::endl;
	}
	return 0;
}

bool ConfigManager::setString(uint32_t _what, const std::string& _value)
{
	if (_what < LAST_STRING_CONFIG) {
		m_confString[_what] = _value;
		return true;
	}

	std::clog << "[Warning - ConfigManager::setString] " << _what << std::endl;
	return false;
}

bool ConfigManager::setNumber(uint32_t _what, int64_t _value)
{
	if (_what < LAST_NUMBER_CONFIG) {
		m_confNumber[_what] = _value;
		return true;
	}

	std::clog << "[Warning - ConfigManager::setNumber] " << _what << std::endl;
	return false;
}

bool ConfigManager::setBool(uint32_t _what, bool _value)
{
	if (_what < LAST_BOOL_CONFIG) {
		m_confBool[_what] = _value;
		return true;
	}

	std::clog << "[Warning - ConfigManager::setBool] " << _what << std::endl;
	return false;
}
