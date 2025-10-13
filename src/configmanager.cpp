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

namespace
{
	std::string string_array[otx::config::LAST_STRING_CONFIG];
	double double_array[otx::config::LAST_DOUBLE_CONFIG] = {};
	int64_t integer_array[otx::config::LAST_INTEGER_CONFIG] = {};
	bool bool_array[otx::config::LAST_BOOL_CONFIG] = {};

	uint32_t IP_NUMBER = 0;

	bool loaded = false;

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

bool otx::config::load()
{
	if (string_array[CONFIG_FILE].empty()) {
		string_array[CONFIG_FILE] = getFilePath(FILE_TYPE_CONFIG, "config.lua");
	}

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

	if (!loaded) {
		// info that must be loaded one time- unless we reset the modules involved
		if (string_array[IP].empty()) {
			string_array[IP] = getConfigString(L, "ip", "127.0.0.1");
		}

		// Convert ip string to number
		std::string ipString = string_array[IP];
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

		if (string_array[RUNFILE].empty()) {
			string_array[RUNFILE] = getConfigString(L, "runFile", "");
		}

		if (string_array[OUTPUT_LOG].empty()) {
			string_array[OUTPUT_LOG] = getConfigString(L, "outputLog", "");
		}

		if (string_array[DATA_DIRECTORY].empty()) {
			string_array[DATA_DIRECTORY] = getConfigString(L, "dataDirectory", "data/");
		}

		if (string_array[LOGS_DIRECTORY].empty()) {
			string_array[LOGS_DIRECTORY] = getConfigString(L, "logsDirectory", "logs/");
		}

		if (integer_array[LOGIN_PORT] == 0) {
			integer_array[LOGIN_PORT] = getConfigInteger(L, "loginPort", 7171);
		}

		if (integer_array[GAME_PORT] == 0) {
			// backwards-compatibility
			std::string port = getConfigString(L, "gamePort", "7172");
			integer_array[GAME_PORT] = otx::util::cast<int64_t>(port.data());
		}

		if (integer_array[STATUS_PORT] == 0) {
			integer_array[STATUS_PORT] = getConfigInteger(L, "statusPort", 7171);
		}

		string_array[MAP_NAME] = getConfigString(L, "mapName", "forgotten.otbm");
		string_array[HOUSE_RENT_PERIOD] = getConfigString(L, "houseRentPeriod", "monthly");
		string_array[SQL_HOST] = getConfigString(L, "sqlHost", "localhost");
		string_array[SQL_USER] = getConfigString(L, "sqlUser", "root");
		string_array[SQL_PASS] = getConfigString(L, "sqlPass", "");
		string_array[SQL_DB] = getConfigString(L, "sqlDatabase", "theotxserver");
		string_array[DEFAULT_PRIORITY] = getConfigString(L, "defaultPriority", "high");

		integer_array[SQL_PORT] = getConfigInteger(L, "sqlPort", 3306);
		integer_array[GLOBALSAVE_H] = getConfigInteger(L, "globalSaveHour", 8);
		integer_array[GLOBALSAVE_M] = getConfigInteger(L, "globalSaveMinute", 0);

		bool_array[GLOBALSAVE_ENABLED] = getConfigBoolean(L, "globalSaveEnabled", true);
		bool_array[LOGIN_ONLY_LOGINSERVER] = getConfigBoolean(L, "loginOnlyWithLoginServer", false);
		bool_array[OPTIMIZE_DATABASE] = getConfigBoolean(L, "startupDatabaseOptimization", true);
		bool_array[STORE_TRASH] = getConfigBoolean(L, "storeTrash", true);
		bool_array[TRUNCATE_LOG] = getConfigBoolean(L, "truncateLogOnStartup", true);
		bool_array[GUILD_HALLS] = getConfigBoolean(L, "guildHalls", false);
		bool_array[BIND_ONLY_GLOBAL_ADDRESS] = getConfigBoolean(L, "bindOnlyGlobalAddress", false);
	}

	string_array[HOUSE_STORAGE] = getConfigString(L, "houseDataStorage", "binary");
	string_array[SERVER_NAME] = getConfigString(L, "serverName", "");
	string_array[OWNER_NAME] = getConfigString(L, "ownerName", "");
	string_array[OWNER_EMAIL] = getConfigString(L, "ownerEmail", "");
	string_array[URL] = getConfigString(L, "url", "");
	string_array[LOCATION] = getConfigString(L, "location", "");
	string_array[MOTD] = getConfigString(L, "motd", "");
	string_array[WORLD_TYPE] = getConfigString(L, "worldType", "open");
	string_array[MAP_AUTHOR] = getConfigString(L, "mapAuthor", "Unknown");
	string_array[PREFIX_CHANNEL_LOGS] = getConfigString(L, "prefixChannelLogs", "");
	string_array[CORES_USED] = getConfigString(L, "coresUsed", "-1");
	string_array[MAILBOX_DISABLED_TOWNS] = getConfigString(L, "mailboxDisabledTowns", "");
	string_array[FORBIDDEN_NAMES] = getConfigString(L, "forbiddenNames", "gm;adm;cm;support;god;tutor");
	string_array[ADVERTISING_BLOCK] = getConfigString(L, "advertisingBlock", "");
	string_array[VERSION_MSG] = getConfigString(L, "versionMsg", "Only clients with protocol " CLIENT_VERSION_STRING " allowed!");

	double_array[RATE_EXPERIENCE] = getConfigDouble(L, "rateExperience", 1.0);
	double_array[RATE_SKILL] = getConfigDouble(L, "rateSkill", 1.0);
	double_array[RATE_SKILL_OFFLINE] = getConfigDouble(L, "rateSkillOffline", 0.5);
	double_array[RATE_MAGIC] = getConfigDouble(L, "rateMagic", 1.0);
	double_array[RATE_MAGIC_OFFLINE] = getConfigDouble(L, "rateMagicOffline", 0.5);
	double_array[RATE_LOOT] = getConfigDouble(L, "rateLoot", 1.0);
	double_array[TWO_VOCATION_PARTY] = getConfigDouble(L, "twoVocationExpMultiplier", 1.4);
	double_array[THREE_VOCATION_PARTY] = getConfigDouble(L, "threeVocationExpMultiplier", 1.6);
	double_array[FOUR_VOCATION_PARTY] = getConfigDouble(L, "fourVocationExpMultiplier", 2.0);
	double_array[PARTY_DIFFERENCE] = getConfigDouble(L, "experienceShareLevelDifference", 2.0 / 3.0);
	double_array[RATE_STAMINA_GAIN] = getConfigDouble(L, "rateStaminaGain", 3.0);
	double_array[RATE_STAMINA_THRESHOLD] = getConfigDouble(L, "rateStaminaThresholdGain", 12.0);
	double_array[RATE_STAMINA_ABOVE] = getConfigDouble(L, "rateStaminaAboveNormal", 1.5);
	double_array[RATE_STAMINA_UNDER] = getConfigDouble(L, "rateStaminaUnderNormal", 0.5);
	double_array[EFP_MIN_THRESHOLD] = getConfigDouble(L, "minLevelThresholdForKilledPlayer", 0.9);
	double_array[EFP_MAX_THRESHOLD] = getConfigDouble(L, "maxLevelThresholdForKilledPlayer", 1.1);
	double_array[RATE_PVP_EXPERIENCE] = getConfigDouble(L, "rateExperienceFromPlayers", 0.0);
	double_array[RATE_MONSTER_HEALTH] = getConfigDouble(L, "rateMonsterHealth", 1.0);
	double_array[RATE_MONSTER_MANA] = getConfigDouble(L, "rateMonsterMana", 1.0);
	double_array[RATE_MONSTER_ATTACK] = getConfigDouble(L, "rateMonsterAttack", 1.0);
	double_array[MAX_ABSORB_PERCENT] = getConfigDouble(L, "maxAbsorbPercent", 80.0);
	double_array[RATE_MONSTER_DEFENSE] = getConfigDouble(L, "rateMonsterDefense", 1.0);
	double_array[FORMULA_LEVEL] = getConfigDouble(L, "formulaLevel", 5.0);
	double_array[FORMULA_MAGIC] = getConfigDouble(L, "formulaMagic", 1.0);

	integer_array[LOGIN_TRIES] = getConfigInteger(L, "loginTries", 3);
	integer_array[MYSQL_RECONNECTION_ATTEMPTS] = getConfigInteger(L, "mysqlReconnectionAttempts", 3);
	integer_array[RETRY_TIMEOUT] = getConfigInteger(L, "retryTimeout", 30000);
	integer_array[LOGIN_TIMEOUT] = getConfigInteger(L, "loginTimeout", 5000);
	integer_array[MAX_PLAYERS] = getConfigInteger(L, "maxPlayers", 1000);
	integer_array[PZ_LOCKED] = getConfigInteger(L, "pzLocked", 60000);
	integer_array[EXHAUST_POTION] = getConfigInteger(L, "exhaustPotionMiliSeconds", 1500);
	integer_array[EXHAUST_SPECTATOR_SAY] = getConfigInteger(L, "exhaust_spectatorSay", 3);
	integer_array[HUNTING_DURATION] = getConfigInteger(L, "huntingDuration", 60000);
	integer_array[DEFAULT_DESPAWNRANGE] = getConfigInteger(L, "deSpawnRange", 2);
	integer_array[DEFAULT_DESPAWNRADIUS] = getConfigInteger(L, "deSpawnRadius", 50);
	integer_array[RATE_SPAWN_MIN] = getConfigInteger(L, "rateSpawnMin", 1);
	integer_array[RATE_SPAWN_MAX] = getConfigInteger(L, "rateSpawnMax", 1);
	integer_array[SPAWNPOS_X] = getConfigInteger(L, "newPlayerSpawnPosX", 100);
	integer_array[SPAWNPOS_Y] = getConfigInteger(L, "newPlayerSpawnPosY", 100);
	integer_array[SPAWNPOS_Z] = getConfigInteger(L, "newPlayerSpawnPosZ", 7);
	integer_array[SPAWNTOWN_ID] = getConfigInteger(L, "newPlayerTownId", 1);
	integer_array[START_LEVEL] = getConfigInteger(L, "newPlayerLevel", 1);
	integer_array[START_MAGICLEVEL] = getConfigInteger(L, "newPlayerMagicLevel", 0);
	integer_array[HOUSE_PRICE] = getConfigInteger(L, "housePriceEachSquare", 1000);
	integer_array[MAX_MESSAGEBUFFER] = getConfigInteger(L, "maxMessageBuffer", 4);
	integer_array[ACTIONS_DELAY_INTERVAL] = getConfigInteger(L, "timeBetweenActions", 200);
	integer_array[EX_ACTIONS_DELAY_INTERVAL] = getConfigInteger(L, "timeBetweenExActions", 1000);
	integer_array[CUSTOM_ACTIONS_DELAY_INTERVAL] = getConfigInteger(L, "timeBetweenCustomActions", 500);
	integer_array[PROTECTION_LEVEL] = getConfigInteger(L, "protectionLevel", 1);
	integer_array[STATUSQUERY_TIMEOUT] = getConfigInteger(L, "statusTimeout", 300000);
	integer_array[LEVEL_TO_FORM_GUILD] = getConfigInteger(L, "levelToFormGuild", 8);
	integer_array[MIN_GUILDNAME] = getConfigInteger(L, "guildNameMinLength", 4);
	integer_array[MAX_GUILDNAME] = getConfigInteger(L, "guildNameMaxLength", 20);
	integer_array[LEVEL_TO_BUY_HOUSE] = getConfigInteger(L, "levelToBuyHouse", 1);
	integer_array[HOUSES_PER_ACCOUNT] = getConfigInteger(L, "housesPerAccount", 0);
	integer_array[WHITE_SKULL_TIME] = getConfigInteger(L, "whiteSkullTime", 900000);
	integer_array[RED_SKULL_LENGTH] = getConfigInteger(L, "redSkullLength", 2592000);
	integer_array[BLACK_SKULL_LENGTH] = getConfigInteger(L, "blackSkullLength", 3888000);
	integer_array[MAX_VIOLATIONCOMMENT_SIZE] = getConfigInteger(L, "maxViolationCommentSize", 60);
	integer_array[NOTATIONS_TO_BAN] = getConfigInteger(L, "notationsToBan", 3);
	integer_array[WARNINGS_TO_FINALBAN] = getConfigInteger(L, "warningsToFinalBan", 4);
	integer_array[WARNINGS_TO_DELETION] = getConfigInteger(L, "warningsToDeletion", 5);
	integer_array[BAN_LENGTH] = getConfigInteger(L, "banLength", 604800);
	integer_array[KILLS_BAN_LENGTH] = getConfigInteger(L, "killsBanLength", 604800);
	integer_array[FINALBAN_LENGTH] = getConfigInteger(L, "finalBanLength", 2592000);
	integer_array[IPBAN_LENGTH] = getConfigInteger(L, "ipBanLength", 86400);
	integer_array[MAX_PLAYER_SUMMONS] = getConfigInteger(L, "maxPlayerSummons", 2);
	integer_array[FIELD_OWNERSHIP] = getConfigInteger(L, "fieldOwnershipDuration", 5000);
	integer_array[EXTRA_PARTY_PERCENT] = getConfigInteger(L, "extraPartyExperiencePercent", 5);
	integer_array[EXTRA_PARTY_LIMIT] = getConfigInteger(L, "extraPartyExperienceLimit", 20);
	integer_array[PARTY_RADIUS_X] = getConfigInteger(L, "experienceShareRadiusX", 30);
	integer_array[PARTY_RADIUS_Y] = getConfigInteger(L, "experienceShareRadiusY", 30);
	integer_array[PARTY_RADIUS_Z] = getConfigInteger(L, "experienceShareRadiusZ", 1);
	integer_array[LOGIN_PROTECTION] = getConfigInteger(L, "loginProtectionPeriod", 10000);
	integer_array[PLAYER_DEEPNESS] = getConfigInteger(L, "playerQueryDeepness", -1);
	integer_array[STAIRHOP_DELAY] = getConfigInteger(L, "stairhopDelay", 2 * 1000);
	integer_array[RATE_STAMINA_LOSS] = getConfigInteger(L, "rateStaminaLoss", 1);
	integer_array[STAMINA_LIMIT_TOP] = getConfigInteger(L, "staminaRatingLimitTop", 2460);
	integer_array[STAMINA_LIMIT_BOTTOM] = getConfigInteger(L, "staminaRatingLimitBottom", 840);
	integer_array[BLESS_REDUCTION_BASE] = getConfigInteger(L, "blessingReductionBase", 30);
	integer_array[BLESS_REDUCTION_DECREMENT] = getConfigInteger(L, "blessingReductionDecrement", 5);
	integer_array[BLESS_REDUCTION] = getConfigInteger(L, "eachBlessReduction", 8);
	integer_array[NICE_LEVEL] = getConfigInteger(L, "niceLevel", 5);
	integer_array[EXPERIENCE_COLOR] = getConfigInteger(L, "gainExperienceColor", COLOR_WHITE);
	integer_array[GUILD_PREMIUM_DAYS] = getConfigInteger(L, "premiumDaysToFormGuild", 0);
	integer_array[PUSH_CREATURE_DELAY] = getConfigInteger(L, "pushCreatureDelay", 2000);
	integer_array[DEATH_CONTAINER] = getConfigInteger(L, "deathContainerId", 1987);
	integer_array[MAXIMUM_DOOR_LEVEL] = getConfigInteger(L, "maximumDoorLevel", 500);
	integer_array[DEATH_ASSISTS] = getConfigInteger(L, "deathAssistCount", 1);
	integer_array[FRAG_LIMIT] = getConfigInteger(L, "fragsLimit", 86400);
	integer_array[FRAG_SECOND_LIMIT] = getConfigInteger(L, "fragsSecondLimit", 604800);
	integer_array[FRAG_THIRD_LIMIT] = getConfigInteger(L, "fragsThirdLimit", 2592000);
	integer_array[RED_LIMIT] = getConfigInteger(L, "fragsToRedSkull", 3);
	integer_array[RED_SECOND_LIMIT] = getConfigInteger(L, "fragsSecondToRedSkull", 5);
	integer_array[RED_THIRD_LIMIT] = getConfigInteger(L, "fragsThirdToRedSkull", 10);
	integer_array[BLACK_LIMIT] = getConfigInteger(L, "fragsToBlackSkull", integer_array[RED_LIMIT]);
	integer_array[BLACK_SECOND_LIMIT] = getConfigInteger(L, "fragsSecondToBlackSkull", integer_array[RED_SECOND_LIMIT]);
	integer_array[BLACK_THIRD_LIMIT] = getConfigInteger(L, "fragsThirdToBlackSkull", integer_array[RED_THIRD_LIMIT]);
	integer_array[BAN_LIMIT] = getConfigInteger(L, "fragsToBanishment", integer_array[RED_LIMIT]);
	integer_array[BAN_SECOND_LIMIT] = getConfigInteger(L, "fragsSecondToBanishment", integer_array[RED_SECOND_LIMIT]);
	integer_array[BAN_THIRD_LIMIT] = getConfigInteger(L, "fragsThirdToBanishment", integer_array[RED_THIRD_LIMIT]);
	integer_array[BLACK_SKULL_DEATH_HEALTH] = getConfigInteger(L, "blackSkulledDeathHealth", 40);
	integer_array[BLACK_SKULL_DEATH_MANA] = getConfigInteger(L, "blackSkulledDeathMana", 0);
	integer_array[DEATHLIST_REQUIRED_TIME] = getConfigInteger(L, "deathListRequiredTime", 60000);
	integer_array[EXPERIENCE_SHARE_ACTIVITY] = getConfigInteger(L, "experienceShareActivity", 120000);
	integer_array[TILE_LIMIT] = getConfigInteger(L, "tileLimit", 0);
	integer_array[PROTECTION_TILE_LIMIT] = getConfigInteger(L, "protectionTileLimit", 0);
	integer_array[HOUSE_TILE_LIMIT] = getConfigInteger(L, "houseTileLimit", 0);
	integer_array[TRADE_LIMIT] = getConfigInteger(L, "tradeLimit", 100);
	integer_array[SQUARE_COLOR] = getConfigInteger(L, "squareColor", 0);
	integer_array[LOOT_MESSAGE] = getConfigInteger(L, "monsterLootMessage", 3);
	integer_array[LOOT_MESSAGE_TYPE] = getConfigInteger(L, "monsterLootMessageType", 19);
	integer_array[NAME_REPORT_TYPE] = getConfigInteger(L, "violationNameReportActionType", 2);
	integer_array[HOUSE_CLEAN_OLD] = getConfigInteger(L, "houseCleanOld", 0);
	integer_array[MAX_IP_CONNECTIONS] = getConfigInteger(L, "MaxIpConnections", 4);
	integer_array[CAST_EXP_PERCENT] = getConfigInteger(L, "expPercentIncast", 5.0);
	integer_array[VIPLIST_DEFAULT_LIMIT] = getConfigInteger(L, "vipListDefaultLimit", 20);
	integer_array[VIPLIST_DEFAULT_PREMIUM_LIMIT] = getConfigInteger(L, "vipListDefaultPremiumLimit", 100);
	integer_array[STAMINA_DESTROY_LOOT] = getConfigInteger(L, "staminaLootLimit", 840);
	integer_array[FIST_BASE_ATTACK] = getConfigInteger(L, "fistBaseAttack", 7);
	integer_array[PVP_BLESSING_THRESHOLD] = getConfigInteger(L, "pvpBlessingThreshold", 40);
	integer_array[FAIRFIGHT_TIMERANGE] = getConfigInteger(L, "fairFightTimeRange", 60);
	integer_array[DEFAULT_DEPOT_SIZE_PREMIUM] = getConfigInteger(L, "defaultDepotSizePremium", 2000);
	integer_array[DEFAULT_DEPOT_SIZE] = getConfigInteger(L, "defaultDepotSize", 2000);
	integer_array[MAIL_ATTEMPTS_FADE] = getConfigInteger(L, "mailAttemptsFadeTime", 600000);
	integer_array[MAIL_ATTEMPTS] = getConfigInteger(L, "mailMaxAttempts", 20);
	integer_array[MAIL_BLOCK] = getConfigInteger(L, "mailBlockPeriod", 3600000);
	integer_array[FOLLOW_EXHAUST] = getConfigInteger(L, "playerFollowExhaust", 2000);
	integer_array[MAX_PACKETS_PER_SECOND] = getConfigInteger(L, "packetsPerSecond", 50);
	integer_array[VERSION_MIN] = getConfigInteger(L, "versionMin", CLIENT_VERSION_MIN);
	integer_array[VERSION_MAX] = getConfigInteger(L, "versionMax", CLIENT_VERSION_MAX);
	integer_array[HIGHSCORES_TOP] = getConfigInteger(L, "highscoreDisplayPlayers", 10);
	integer_array[HIGHSCORES_UPDATETIME] = getConfigInteger(L, "updateHighscoresAfterMinutes", 60);
	integer_array[LOGIN_PROTECTION_TIME] = getConfigInteger(L, "loginProtectionTime", 10);

	bool_array[MONSTER_ATTACK_MONSTER] = getConfigBoolean(L, "monsterAttacksOnlyDamagePlayers", true);
	bool_array[START_CHOOSEVOC] = getConfigBoolean(L, "newPlayerChooseVoc", false);
	bool_array[ON_OR_OFF_CHARLIST] = getConfigBoolean(L, "displayOnOrOffAtCharlist", false);
	bool_array[PARTY_VOCATION_MULT] = getConfigBoolean(L, "enablePartyVocationBonus", false);
	bool_array[ONE_PLAYER_ON_ACCOUNT] = getConfigBoolean(L, "onePlayerOnlinePerAccount", true);
	bool_array[REMOVE_WEAPON_AMMO] = getConfigBoolean(L, "removeWeaponAmmunition", true);
	bool_array[REMOVE_WEAPON_CHARGES] = getConfigBoolean(L, "removeWeaponCharges", true);
	bool_array[REMOVE_RUNE_CHARGES] = getConfigBoolean(L, "removeRuneCharges", true);
	bool_array[SHUTDOWN_AT_GLOBALSAVE] = getConfigBoolean(L, "shutdownAtGlobalSave", false);
	bool_array[CLEAN_MAP_AT_GLOBALSAVE] = getConfigBoolean(L, "cleanMapAtGlobalSave", true);
	bool_array[FREE_PREMIUM] = getConfigBoolean(L, "freePremium", false);
	bool_array[GENERATE_ACCOUNT_NUMBER] = getConfigBoolean(L, "generateAccountNumber", false);
	bool_array[GENERATE_ACCOUNT_SALT] = getConfigBoolean(L, "generateAccountSalt", false);
	bool_array[BANK_SYSTEM] = getConfigBoolean(L, "bankSystem", true);
	bool_array[ADD_FRAG_SAMEIP] = getConfigBoolean(L, "addFragToSameIp", false);
	bool_array[INIT_PREMIUM_UPDATE] = getConfigBoolean(L, "updatePremiumStateAtStartup", true);
	bool_array[TELEPORT_SUMMONS] = getConfigBoolean(L, "teleportAllSummons", false);
	bool_array[TELEPORT_PLAYER_SUMMONS] = getConfigBoolean(L, "teleportPlayerSummons", false);
	bool_array[PVP_TILE_IGNORE_PROTECTION] = getConfigBoolean(L, "pvpTileIgnoreLevelAndVocationProtection", true);
	bool_array[POTION_CAN_EXHAUST_ITEM] = getConfigBoolean(L, "exhaustItemAtUsePotion", false);
	bool_array[CLEAN_PROTECTED_ZONES] = getConfigBoolean(L, "cleanProtectedZones", true);
	bool_array[SPELL_NAME_INSTEAD_WORDS] = getConfigBoolean(L, "spellNameInsteadOfWords", false);
	bool_array[EMOTE_SPELLS] = getConfigBoolean(L, "emoteSpells", false);
	bool_array[DISPLAY_BROADCAST] = getConfigBoolean(L, "displayBroadcastLog", true);
	bool_array[REPLACE_KICK_ON_LOGIN] = getConfigBoolean(L, "replaceKickOnLogin", true);
	bool_array[PREMIUM_FOR_PROMOTION] = getConfigBoolean(L, "premiumForPromotion", true);
	bool_array[SHOW_HEALTH_CHANGE] = getConfigBoolean(L, "showHealthChange", true);
	bool_array[SHOW_MANA_CHANGE] = getConfigBoolean(L, "showManaChange", true);
	bool_array[BROADCAST_BANISHMENTS] = getConfigBoolean(L, "broadcastBanishments", true);
	bool_array[USE_MAX_ABSORBALL] = getConfigBoolean(L, "useMaxAbsorbAll", false);
	bool_array[SAVE_GLOBAL_STORAGE] = getConfigBoolean(L, "saveGlobalStorage", true);
	bool_array[SAVE_PLAYER_DATA] = getConfigBoolean(L, "savePlayerData", true);
	bool_array[INGAME_GUILD_MANAGEMENT] = getConfigBoolean(L, "ingameGuildManagement", true);
	bool_array[USEDAMAGE_IN_K] = getConfigBoolean(L, "modifyDamageInK", false);
	bool_array[USEEXP_IN_K] = getConfigBoolean(L, "modifyExperienceInK", false);
	bool_array[EXTERNAL_GUILD_WARS_MANAGEMENT] = getConfigBoolean(L, "externalGuildWarsManagement", false);
	bool_array[HOUSE_BUY_AND_SELL] = getConfigBoolean(L, "buyableAndSellableHouses", true);
	bool_array[HOUSE_NEED_PREMIUM] = getConfigBoolean(L, "houseNeedPremium", true);
	bool_array[HOUSE_RENTASPRICE] = getConfigBoolean(L, "houseRentAsPrice", false);
	bool_array[HOUSE_PRICEASRENT] = getConfigBoolean(L, "housePriceAsRent", false);
	bool_array[ACCOUNT_MANAGER] = getConfigBoolean(L, "accountManager", true);
	bool_array[NAMELOCK_MANAGER] = getConfigBoolean(L, "namelockManager", false);
	bool_array[ALLOW_CHANGEOUTFIT] = getConfigBoolean(L, "allowChangeOutfit", true);
	bool_array[CANNOT_ATTACK_SAME_LOOKFEET] = getConfigBoolean(L, "noDamageToSameLookfeet", false);
	bool_array[AIMBOT_HOTKEY_ENABLED] = getConfigBoolean(L, "hotkeyAimbotEnabled", true);
	bool_array[FORCE_CLOSE_SLOW_CONNECTION] = getConfigBoolean(L, "forceSlowConnectionsToDisconnect", false);
	bool_array[EXPERIENCE_STAGES] = getConfigBoolean(L, "experienceStages", false);
	bool_array[BLESSINGS] = getConfigBoolean(L, "blessings", true);
	bool_array[BLESSING_ONLY_PREMIUM] = getConfigBoolean(L, "blessingOnlyPremium", true);
	bool_array[BED_REQUIRE_PREMIUM] = getConfigBoolean(L, "bedsRequirePremium", true);
	bool_array[ALLOW_CHANGECOLORS] = getConfigBoolean(L, "allowChangeColors", true);
	bool_array[STOP_ATTACK_AT_EXIT] = getConfigBoolean(L, "stopAttackingAtExit", false);
	bool_array[DISABLE_OUTFITS_PRIVILEGED] = getConfigBoolean(L, "disableOutfitsForPrivilegedPlayers", false);
	bool_array[STORE_DIRECTION] = getConfigBoolean(L, "storePlayerDirection", false);
	bool_array[DISPLAY_LOGGING] = getConfigBoolean(L, "displayPlayersLogging", true);
	bool_array[STAMINA_BONUS_PREMIUM] = getConfigBoolean(L, "staminaThresholdOnlyPremium", true);
	bool_array[ALLOW_CHANGEADDONS] = getConfigBoolean(L, "allowChangeAddons", true);
	bool_array[GHOST_INVISIBLE_EFFECT] = getConfigBoolean(L, "ghostModeInvisibleEffect", false);
	bool_array[SHOW_HEALTH_CHANGE_MONSTER] = getConfigBoolean(L, "showHealthChangeForMonsters", true);
	bool_array[SHOW_MANA_CHANGE_MONSTER] = getConfigBoolean(L, "showManaChangeForMonsters", true);
	bool_array[CHECK_CORPSE_OWNER] = getConfigBoolean(L, "checkCorpseOwner", true);
	bool_array[BUFFER_SPELL_FAILURE] = getConfigBoolean(L, "bufferMutedOnSpellFailure", false);
	bool_array[PREMIUM_SKIP_WAIT] = getConfigBoolean(L, "premiumPlayerSkipWaitList", false);
	bool_array[DEATH_LIST] = getConfigBoolean(L, "deathListEnabled", true);
	bool_array[GHOST_SPELL_EFFECTS] = getConfigBoolean(L, "ghostModeSpellEffects", true);
	bool_array[PVPZONE_ADDMANASPENT] = getConfigBoolean(L, "addManaSpentInPvPZone", true);
	bool_array[PVPZONE_RECOVERMANA] = getConfigBoolean(L, "recoverManaAfterDeathInPvPZone", false);
	bool_array[USE_BLACK_SKULL] = getConfigBoolean(L, "useBlackSkull", true);
	bool_array[USE_FRAG_HANDLER] = getConfigBoolean(L, "useFragHandler", true);
	bool_array[VIPLIST_PER_PLAYER] = getConfigBoolean(L, "separateVipListPerCharacter", false);
	bool_array[DELETE_PLAYER_MONSTER_NAME] = getConfigBoolean(L, "deletePlayersWithMonsterName", false);
	bool_array[ADDONS_PREMIUM] = getConfigBoolean(L, "addonsOnlyPremium", true);
	bool_array[OPTIONAL_WAR_ATTACK_ALLY] = getConfigBoolean(L, "optionalWarAttackableAlly", false);
	bool_array[UNIFIED_SPELLS] = getConfigBoolean(L, "unifiedSpells", true);
	bool_array[MONSTER_SPAWN_WALKBACK] = getConfigBoolean(L, "monsterSpawnWalkback", true);
	bool_array[USE_CAPACITY] = getConfigBoolean(L, "useCapacity", true);
	bool_array[DAEMONIZE] = getConfigBoolean(L, "daemonize", false);
	bool_array[SKIP_ITEMS_VERSION] = getConfigBoolean(L, "skipItemsVersionCheck", false);
	bool_array[SILENT_LUA] = getConfigBoolean(L, "disableLuaErrors", false);
	bool_array[HOUSE_SKIP_INIT_RENT] = getConfigBoolean(L, "houseSkipInitialRent", true);
	bool_array[HOUSE_PROTECTION] = getConfigBoolean(L, "houseProtection", false);
	bool_array[HOUSE_OWNED_BY_ACCOUNT] = getConfigBoolean(L, "houseOwnedByAccount", true);
	bool_array[FAIRFIGHT_REDUCTION] = getConfigBoolean(L, "useFairfightReduction", true);
	bool_array[ALLOW_BLOCK_SPAWN] = getConfigBoolean(L, "allowBlockSpawn", true);
	bool_array[MULTIPLE_NAME] = getConfigBoolean(L, "multipleNames", false);
	bool_array[SAVE_STATEMENT] = getConfigBoolean(L, "logPlayersStatements", true);
	bool_array[MANUAL_ADVANCED_CONFIG] = getConfigBoolean(L, "manualVersionConfig", false);
	bool_array[USE_RUNE_REQUIREMENTS] = getConfigBoolean(L, "useRunesRequirements", true);
	bool_array[CLASSIC_EQUIPMENT_SLOTS] = getConfigBoolean(L, "classicEquipmentSlots", false);
	bool_array[NO_ATTACKHEALING_SIMULTANEUS] = getConfigBoolean(L, "noAttackHealingSimultaneus", false);
	bool_array[OPTIONAL_PROTECTION] = getConfigBoolean(L, "optionalProtection", true);
	bool_array[ALLOW_CORPSE_BLOCK] = getConfigBoolean(L, "allowCorpseBlock", false);
	bool_array[ALLOW_INDEPENDENT_PUSH] = getConfigBoolean(L, "allowIndependentCreaturePush", true);
	bool_array[DIAGONAL_PUSH] = getConfigBoolean(L, "diagonalPush", true);
	bool_array[PZLOCK_ON_ATTACK_SKULLED_PLAYERS] = getConfigBoolean(L, "pzlockOnAttackSkulledPlayers", false);
	bool_array[MAXIP_USECONECT] = getConfigBoolean(L, "UseMaxIpConnect", false);
	bool_array[CAST_EXP_ENABLED] = getConfigBoolean(L, "expInCast", false);
	bool_array[PUSH_IN_PZ] = getConfigBoolean(L, "pushInProtectZone", true);

	loaded = true;
	return true;
}

bool otx::config::reload()
{
	if (!loaded) {
		return false;
	}

	const int64_t oldHousePrice = integer_array[HOUSE_PRICE];
	if (!load()) {
		return false;
	}

	if (integer_array[HOUSE_PRICE] == oldHousePrice) {
		return true;
	}

	for (const auto& it : Houses::getInstance()->getHouses()) {
		uint32_t price = it.second->getTilesCount() * static_cast<uint32_t>(integer_array[HOUSE_PRICE]);
		if (bool_array[HOUSE_RENTASPRICE]) {
			const uint32_t rent = it.second->getRent();
			if (!bool_array[HOUSE_PRICEASRENT] && it.second->getPrice() != rent) {
				price = rent;
			}
		}

		it.second->setPrice(price);
		if (bool_array[HOUSE_PRICEASRENT]) {
			it.second->setRent(price);
		}

		if (it.second->getOwner() == 0) {
			it.second->updateDoorDescription();
		}
	}
	return true;
}

uint32_t otx::config::getIPNumber()
{
	return IP_NUMBER;
}

const std::string& otx::config::getString(ConfigString_t option)
{
	if (option <= LAST_STRING_CONFIG) {
		return string_array[option];
	}

	static const std::string emptyString;
	std::clog << "[Warning - otx::config::getString] Invalid option " << static_cast<int>(option) << std::endl;
	return emptyString;
}

bool otx::config::setString(ConfigString_t option, const std::string& value)
{
	if (option < LAST_STRING_CONFIG) {
		string_array[option] = value;
		return true;
	}

	std::clog << "[Warning - otx::config::setString] Invalid option " << static_cast<int>(option) << std::endl;
	return false;
}

double otx::config::getDouble(ConfigDouble_t option)
{
	if (option < LAST_DOUBLE_CONFIG) {
		return double_array[option];
	}

	std::clog << "[Warning - otx::config::getDouble] Invalid option " << static_cast<int>(option) << std::endl;
	return 0;
}

bool otx::config::setDouble(ConfigDouble_t option, double value)
{
	if (option < LAST_DOUBLE_CONFIG) {
		integer_array[option] = value;
		return true;
	}

	std::clog << "[Warning - otx::config::setDouble] Invalid option " << static_cast<int>(option) << std::endl;
	return false;
}

int64_t otx::config::getInteger(ConfigInteger_t option)
{
	if (option < LAST_INTEGER_CONFIG) {
		return integer_array[option];
	}

	std::clog << "[Warning - otx::config::getInteger] Invalid option " << static_cast<int>(option) << std::endl;
	return 0;
}

bool otx::config::setInteger(ConfigInteger_t option, int64_t value)
{
	if (option < LAST_INTEGER_CONFIG) {
		integer_array[option] = value;
		return true;
	}

	std::clog << "[Warning - otx::config::setInteger] Invalid option " << static_cast<int>(option) << std::endl;
	return false;
}

bool otx::config::getBoolean(ConfigBoolean_t option)
{
	if (option < LAST_BOOL_CONFIG) {
		return bool_array[option];
	}

	std::clog << "[Warning - otx::config::getBoolean] Invalid option " << static_cast<int>(option) << std::endl;
	return false;
}

bool otx::config::setBoolean(ConfigBoolean_t option, bool value)
{
	if (option < LAST_BOOL_CONFIG) {
		bool_array[option] = value;
		return true;
	}

	std::clog << "[Warning - otx::config::setBoolean] Invalid option " << static_cast<int>(option) << std::endl;
	return false;
}
