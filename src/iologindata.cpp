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

#include "iologindata.h"

#include "configmanager.h"
#include "game.h"
#include "house.h"
#include "ioguild.h"
#include "item.h"
#include "tools.h"
#include "town.h"
#include "vocation.h"

extern ConfigManager g_config;
extern Game g_game;

Account IOLoginData::loadAccount(uint32_t accountId, bool preLoad /* = false*/)
{
	std::ostringstream query;
	query << "SELECT `name`, `password`, `salt`, `premdays`, `lastday`, `key`, `warnings` FROM `accounts` WHERE `id` = " << accountId << " LIMIT 1";

	DBResultPtr result;
	if (!(result = g_database.storeQuery(query.str()))) {
		return Account();
	}

	Account account;
	account.number = accountId;
	account.name = result->getString("name");
	account.password = result->getString("password");
	account.salt = result->getString("salt");
	account.premiumDays = std::max((int32_t)0, std::min((int32_t)GRATIS_PREMIUM, result->getNumber<int32_t>("premdays")));
	account.lastDay = result->getNumber<uint32_t>("lastday");
	account.recoveryKey = result->getString("key");
	account.warnings = result->getNumber<uint16_t>("warnings");

	if (!preLoad) {
		loadCharacters(account);
	}

	return account;
}

bool IOLoginData::loadAccount(Account& account, const std::string& name)
{
	std::ostringstream query;

	query << "SELECT `id`, `password`, `salt`, `premdays`, `lastday`, `key`, `warnings` FROM `accounts` WHERE `name` = " << g_database.escapeString(name) << " LIMIT 1";
	DBResultPtr result;
	if (!(result = g_database.storeQuery(query.str()))) {
		return false;
	}

	account.number = result->getNumber<int32_t>("id");
	account.name = name;
	account.password = result->getString("password");
	account.salt = result->getString("salt");
	account.premiumDays = std::max((int32_t)0, std::min((int32_t)GRATIS_PREMIUM, result->getNumber<int32_t>("premdays")));
	account.lastDay = result->getNumber<uint32_t>("lastday");
	account.recoveryKey = result->getString("key");
	account.warnings = result->getNumber<uint16_t>("warnings");

	loadCharacters(account);
	return true;
}

void IOLoginData::loadCharacters(Account& account)
{
	std::ostringstream query;
	query << "SELECT `name` FROM `players` WHERE `account_id` = " << account.number << " AND `world_id` = " << g_config.getNumber(ConfigManager::WORLD_ID) << " AND `deleted` = 0";

	DBResultPtr result;
	if (!(result = g_database.storeQuery(query.str()))) {
		return;
	}

	do {
		account.charList.push_back(result->getString("name"));
	} while (result->next());
	account.charList.sort();
}

bool IOLoginData::saveAccount(Account account)
{
	std::ostringstream query;
	query << "UPDATE `accounts` SET `premdays` = " << account.premiumDays << ", `warnings` = " << account.warnings << ", `lastday` = " << account.lastDay << " WHERE `id` = " << account.number << " LIMIT 1;";
	return g_database.executeQuery(query.str());
}

bool IOLoginData::getAccountId(const std::string& name, uint32_t& number)
{
	if (!name.length()) {
		return false;
	}

	std::ostringstream query;
	query << "SELECT `id` FROM `accounts` WHERE `name` = " << g_database.escapeString(name) << " LIMIT 1";

	DBResultPtr result;
	if (!(result = g_database.storeQuery(query.str()))) {
		return false;
	}

	number = result->getNumber<int32_t>("id");
	return true;
}

bool IOLoginData::getAccountName(uint32_t number, std::string& name)
{
	if (number < 2) {
		name = number ? "1" : "";
		return true;
	}

	std::ostringstream query;
	query << "SELECT `name` FROM `accounts` WHERE `id` = " << number << " LIMIT 1";

	DBResultPtr result;
	if (!(result = g_database.storeQuery(query.str()))) {
		return false;
	}

	name = result->getString("name");
	return true;
}

bool IOLoginData::hasFlag(uint32_t accountId, PlayerFlags value)
{
	std::ostringstream query;
	query << "SELECT `group_id` FROM `accounts` WHERE `id` = " << accountId << " LIMIT 1";

	DBResultPtr result;
	if (!(result = g_database.storeQuery(query.str()))) {
		return false;
	}

	Group* group = Groups::getInstance()->getGroup(result->getNumber<int32_t>("group_id"));
	return group && group->hasFlag(value);
}

bool IOLoginData::hasCustomFlag(uint32_t accountId, PlayerCustomFlags value)
{
	std::ostringstream query;
	query << "SELECT `group_id` FROM `accounts` WHERE `id` = " << accountId << " LIMIT 1";

	DBResultPtr result;
	if (!(result = g_database.storeQuery(query.str()))) {
		return false;
	}

	Group* group = Groups::getInstance()->getGroup(result->getNumber<int32_t>("group_id"));
	return group && group->hasCustomFlag(value);
}

bool IOLoginData::hasFlag(PlayerFlags value, const std::string& accName)
{
	std::ostringstream query;
	query << "SELECT `group_id` FROM `accounts` WHERE `name` = " << g_database.escapeString(accName) << " LIMIT 1";

	DBResultPtr result;
	if (!(result = g_database.storeQuery(query.str()))) {
		return false;
	}

	Group* group = Groups::getInstance()->getGroup(result->getNumber<int32_t>("group_id"));
	return group && group->hasFlag(value);
}

bool IOLoginData::hasCustomFlag(PlayerCustomFlags value, const std::string& accName)
{
	std::ostringstream query;
	query << "SELECT `group_id` FROM `accounts` WHERE `name` = " << g_database.escapeString(accName) << " LIMIT 1";

	DBResultPtr result;
	if (!(result = g_database.storeQuery(query.str()))) {
		return false;
	}

	Group* group = Groups::getInstance()->getGroup(result->getNumber<int32_t>("group_id"));
	return group && group->hasCustomFlag(value);
}

bool IOLoginData::accountIdExists(uint32_t accountId)
{
	std::ostringstream query;
	query << "SELECT 1 FROM `accounts` WHERE `id` = " << accountId << " LIMIT 1";

	DBResultPtr result;
	if (!(result = g_database.storeQuery(query.str()))) {
		return false;
	}

	return true;
}

bool IOLoginData::accountNameExists(const std::string& name)
{
	std::ostringstream query;
	query << "SELECT 1 FROM `accounts` WHERE `name` = " << g_database.escapeString(name) << " LIMIT 1";

	DBResultPtr result;
	if (!(result = g_database.storeQuery(query.str()))) {
		return false;
	}

	return true;
}

bool IOLoginData::getPassword(uint32_t accountId, std::string& password, std::string& salt, std::string name /* = ""*/)
{
	std::ostringstream query;
	query << "SELECT `password`, `salt` FROM `accounts` WHERE `id` = " << accountId << " LIMIT 1";

	DBResultPtr result;
	if (!(result = g_database.storeQuery(query.str()))) {
		return false;
	}

	std::string tmpPassword = result->getString("password"), tmpSalt = result->getString("salt");
	if (name.empty() || name == "Account Manager") {
		password = tmpPassword;
		salt = tmpSalt;
		return true;
	}

	query.str("");
	query << "SELECT `name` FROM `players` WHERE `account_id` = " << accountId;
	if (!(result = g_database.storeQuery(query.str()))) {
		return false;
	}

	do {
		if (result->getString("name") != name) {
			continue;
		}

		password = tmpPassword;
		salt = tmpSalt;

		return true;
	} while (result->next());
	return false;
}

bool IOLoginData::setPassword(uint32_t accountId, std::string newPassword)
{
	std::string salt;
	if (g_config.getBool(ConfigManager::GENERATE_ACCOUNT_SALT)) {
		salt = generateRecoveryKey(2, 19, true);
		newPassword = salt + newPassword;
	}

	std::ostringstream query;
	query << "UPDATE `accounts` SET `password` = " << g_database.escapeString(transformToSHA1(newPassword)) << ", `salt` = " << g_database.escapeString(salt) << " WHERE `id` = " << accountId << " LIMIT 1;";
	return g_database.executeQuery(query.str());
}

bool IOLoginData::validRecoveryKey(uint32_t accountId, const std::string& recoveryKey)
{
	std::ostringstream query;
	query << "SELECT `id` FROM `accounts` WHERE `id` = " << accountId << " AND `key` = " << g_database.escapeString(transformToSHA1(recoveryKey)) << " LIMIT 1";
	return g_database.storeQuery(query.str()) != nullptr;
}

bool IOLoginData::setRecoveryKey(uint32_t accountId, const std::string& newRecoveryKey)
{
	std::ostringstream query;
	query << "UPDATE `accounts` SET `key` = " << g_database.escapeString(transformToSHA1(newRecoveryKey)) << " WHERE `id` = " << accountId << " LIMIT 1;";
	return g_database.executeQuery(query.str());
}

uint64_t IOLoginData::createAccount(std::string name, const std::string& password)
{
	const std::string salt = generateRecoveryKey(2, 19, true);
	std::ostringstream query;
	query << "INSERT INTO `accounts` (`id`, `name`, `password`, `salt`) VALUES (NULL, " << g_database.escapeString(name) << ", " << g_database.escapeString(transformToSHA1(salt + password)) << ", " << g_database.escapeString(salt) << ")";
	if (!g_database.executeQuery(query.str())) {
		return 0;
	}

	return g_database.getLastInsertId();
}

void IOLoginData::removePremium(Account& account)
{
	bool save = false;
	uint64_t timeNow = time(NULL);
	if (account.premiumDays != 0 && account.premiumDays != (uint16_t)GRATIS_PREMIUM) {
		if (account.lastDay == 0) {
			account.lastDay = timeNow;
			save = true;
		} else {
			uint32_t days = (timeNow - account.lastDay) / 86400;
			if (days > 0) {
				if (account.premiumDays >= days) {
					account.premiumDays -= days;
					uint32_t remainder = (timeNow - account.lastDay) % 86400;
					account.lastDay = timeNow - remainder;
				} else {
					account.premiumDays = 0;
					account.lastDay = 0;
				}
				save = true;
			}
		}
	} else if (account.lastDay != 0) {
		account.lastDay = 0;
		save = true;
	}

	if (save && !saveAccount(account)) {
		std::clog << "> ERROR: Failed to save account: " << account.name << "!" << std::endl;
	}
}

const Group* IOLoginData::getPlayerGroupByAccount(uint32_t accountId)
{
	std::ostringstream query;
	query << "SELECT `group_id` FROM `accounts` WHERE `id` = " << accountId << " LIMIT 1";

	DBResultPtr result;
	if (!(result = g_database.storeQuery(query.str()))) {
		return NULL;
	}

	Group* group = Groups::getInstance()->getGroup(result->getNumber<int32_t>("group_id"));
	return group;
}

bool IOLoginData::loadPlayer(Player* player, const std::string& name, bool preLoad /*= false*/)
{
	std::ostringstream query;
	query << "SELECT `id`, `account_id`, `group_id`, `world_id`, `sex`, `vocation`, `experience`, `level`, "
		  << "`maglevel`, `health`, `healthmax`, `blessings`, `pvp_blessing`, `mana`, `manamax`, `manaspent`, `soul`, "
		  << "`lookbody`, `lookfeet`, `lookhead`, `looklegs`, `looktype`, `lookaddons`, `posx`, `posy`, "
		  << "`posz`, `cap`, `lastlogin`, `lastlogout`, `lastip`, `conditions`, `skull`, `skulltime`, `guildnick`, "
		  << "`rank_id`, `town_id`, `balance`, `stamina`, `direction`, `loss_experience`, `loss_mana`, `loss_skills`, "
		  << "`loss_containers`, `loss_items`, `marriage`, `promotion`, `description`, `offlinetraining_time`, `offlinetraining_skill`, "
		  << "`save`";
	if (g_config.getBool(ConfigManager::RESET_SYSTEM_ENABLE)) {
		query << ", `reset`";
	}
	query << " FROM `players` WHERE "
		  << "`name` = " << g_database.escapeString(name) << " AND `deleted` = 0 LIMIT 1";

	DBResultPtr result;
	if (!(result = g_database.storeQuery(query.str()))) {
		return false;
	}

	uint32_t accountId = result->getNumber<int32_t>("account_id");
	if (accountId < 1) {
		return false;
	}

	Group* group = Groups::getInstance()->getGroup(result->getNumber<int32_t>("group_id"));
	if (!group->hasCustomFlag(PlayerCustomFlag_GamemasterPrivileges) && result->getNumber<int32_t>("world_id") != g_config.getNumber(ConfigManager::WORLD_ID)) {
		return false;
	}

	Account account = loadAccount(accountId, true);
	player->account = account.name;
	player->accountId = accountId;

	player->setGroup(group);
	player->setGUID(result->getNumber<int32_t>("id"));
	player->premiumDays = account.premiumDays;

	nameCacheMap[player->getGUID()] = name;
	guidCacheMap[name] = player->getGUID();
	if (preLoad) {
		// only loading basic info
		return true;
	}

	player->nameDescription += result->getString("description");
	player->setSex(result->getNumber<int32_t>("sex"));
	if (g_config.getBool(ConfigManager::STORE_DIRECTION)) {
		player->setDirection((Direction)result->getNumber<int32_t>("direction"));
	}

	player->level = std::max((uint32_t)1, (uint32_t)result->getNumber<int32_t>("level"));
	if (g_config.getBool(ConfigManager::RESET_SYSTEM_ENABLE)) {
		player->reset = std::max((uint32_t)0, (uint32_t)result->getNumber<int32_t>("reset")); // reset system
	}

	uint64_t currExpCount = Player::getExpForLevel(player->level), nextExpCount = Player::getExpForLevel(player->level + 1), experience = result->getNumber<uint64_t>("experience");
	if (experience < currExpCount || experience > nextExpCount) {
		experience = currExpCount;
	}

	player->experience = experience;
	player->levelPercent = 0;
	if (currExpCount < nextExpCount) {
		player->levelPercent = Player::getPercentLevel(player->experience - currExpCount, nextExpCount - currExpCount);
	}

	player->soul = result->getNumber<int32_t>("soul");
	player->capacity = result->getNumber<int32_t>("cap");
	player->setStamina(result->getNumber<uint64_t>("stamina"));
	player->marriage = result->getNumber<int32_t>("marriage");

	player->balance = result->getNumber<uint64_t>("balance");
	if (g_config.getBool(ConfigManager::BLESSINGS) && (player->isPremium() || !g_config.getBool(ConfigManager::BLESSING_ONLY_PREMIUM))) {
		player->blessings = result->getNumber<int32_t>("blessings");
		player->setPVPBlessing(result->getNumber<int32_t>("pvp_blessing") != 0);
	}

	unsigned long conditionsSize = 0;
	const char* conditions = result->getStream("conditions", conditionsSize);

	PropStream propStream;
	propStream.init(conditions, conditionsSize);

	Condition* condition;
	while ((condition = Condition::createCondition(propStream))) {
		if (condition->unserialize(propStream)) {
			player->storedConditionList.push_back(condition);
		} else {
			delete condition;
		}
	}

	player->vocationId = result->getNumber<int32_t>("vocation");
	player->setPromotionLevel(result->getNumber<int32_t>("promotion"));

	player->health = result->getNumber<int32_t>("health");
	player->healthMax = result->getNumber<int32_t>("healthmax");
	player->mana = result->getNumber<int32_t>("mana");
	player->manaMax = result->getNumber<int32_t>("manamax");

	player->magLevel = result->getNumber<int32_t>("maglevel");
	uint64_t nextManaCount = player->vocation->getReqMana(player->magLevel + 1), manaSpent = result->getNumber<uint64_t>("manaspent");
	if (manaSpent > nextManaCount) {
		manaSpent = 0;
	}

	player->manaSpent = manaSpent;
	player->magLevelPercent = Player::getPercentLevel(player->manaSpent, nextManaCount);
	if (!group || !group->getOutfit()) {
		player->defaultOutfit.lookType = result->getNumber<int32_t>("looktype");
		uint32_t outfitId = Outfits::getInstance()->getOutfitId(player->defaultOutfit.lookType);

		bool wearable = true;
		if (outfitId > 0) {
			Outfit outfit;
			wearable = Outfits::getInstance()->getOutfit(outfitId, player->getSex(true), outfit);
			if (wearable && player->defaultOutfit.lookType != outfit.lookType) {
				player->defaultOutfit.lookType = outfit.lookType;
			}
		}

		if (!wearable) // just pick the first default outfit we can find
		{
			const OutfitMap& defaultOutfits = Outfits::getInstance()->getOutfits(player->getSex(true));
			if (!defaultOutfits.empty()) {
				Outfit newOutfit = (*defaultOutfits.begin()).second;
				player->defaultOutfit.lookType = newOutfit.lookType;
			}
		}
	} else {
		player->defaultOutfit.lookType = group->getOutfit();
	}

	player->defaultOutfit.lookHead = result->getNumber<int32_t>("lookhead");
	player->defaultOutfit.lookBody = result->getNumber<int32_t>("lookbody");
	player->defaultOutfit.lookLegs = result->getNumber<int32_t>("looklegs");
	player->defaultOutfit.lookFeet = result->getNumber<int32_t>("lookfeet");
	player->defaultOutfit.lookAddons = result->getNumber<int32_t>("lookaddons");

	player->currentOutfit = player->defaultOutfit;
	Skulls_t skull = SKULL_RED;
	if (g_config.getBool(ConfigManager::USE_BLACK_SKULL)) {
		skull = (Skulls_t)result->getNumber<int32_t>("skull");
	}

	player->setSkullEnd((time_t)result->getNumber<int32_t>("skulltime"), true, skull);
	player->saving = result->getNumber<int32_t>("save") != 0;

	player->town = result->getNumber<int32_t>("town_id");
	if (Town* town = Towns::getInstance()->getTown(player->town)) {
		player->setMasterPosition(town->getPosition());
	}

	player->setLossPercent(LOSS_EXPERIENCE, result->getNumber<int32_t>("loss_experience"));
	player->setLossPercent(LOSS_MANA, result->getNumber<int32_t>("loss_mana"));
	player->setLossPercent(LOSS_SKILLS, result->getNumber<int32_t>("loss_skills"));
	player->setLossPercent(LOSS_CONTAINERS, result->getNumber<int32_t>("loss_containers"));
	player->setLossPercent(LOSS_ITEMS, result->getNumber<int32_t>("loss_items"));

	player->lastLogin = result->getNumber<uint64_t>("lastlogin");
	player->lastLogout = result->getNumber<uint64_t>("lastlogout");
	player->lastIP = result->getNumber<int32_t>("lastip");

	player->offlineTrainingTime = result->getNumber<int32_t>("offlinetraining_time") * 1000;
	player->offlineTrainingSkill = result->getNumber<int32_t>("offlinetraining_skill");

	player->loginPosition = Position(result->getNumber<int32_t>("posx"), result->getNumber<int32_t>("posy"), result->getNumber<int32_t>("posz"));
	if (!player->loginPosition.x || !player->loginPosition.y) {
		player->loginPosition = player->getMasterPosition();
	}

	const uint32_t rankId = result->getNumber<int32_t>("rank_id");
	const std::string nick = result->getString("guildnick");

	if (rankId > 0) {
		query.str("");
		query << "SELECT `guild_ranks`.`name` AS `rank`, `guild_ranks`.`guild_id` AS `guildid`, `guild_ranks`.`level` AS `level`, `guilds`.`name` AS `guildname` FROM `guild_ranks`, `guilds` WHERE `guild_ranks`.`id` = " << rankId << " AND `guild_ranks`.`guild_id` = `guilds`.`id` LIMIT 1";
		if ((result = g_database.storeQuery(query.str()))) {
			player->guildId = result->getNumber<int32_t>("guildid");
			player->guildName = result->getString("guildname");
			player->guildLevel = (GuildLevel_t)result->getNumber<int32_t>("level");

			player->rankId = rankId;
			player->rankName = result->getString("rank");
			player->guildNick = nick;

			std::string tmpStatus = "AND `status` IN (1,4)";
			if (g_config.getBool(ConfigManager::EXTERNAL_GUILD_WARS_MANAGEMENT)) {
				tmpStatus = "AND `status` IN (1,4,9)";
			}

			query.str("");
			query << "SELECT `id`, `guild_id`, `enemy_id` FROM `guild_wars` WHERE (`guild_id` = "
				  << player->guildId << " OR `enemy_id` = " << player->guildId << ")" << tmpStatus;
			if ((result = g_database.storeQuery(query.str()))) {
				War_t war;
				do {
					uint32_t guild = result->getNumber<int32_t>("guild_id");
					if (player->guildId == guild) {
						war.type = WAR_ENEMY;
						war.war = result->getNumber<int32_t>("id");
						player->addEnemy(result->getNumber<int32_t>("enemy_id"), war);
					} else {
						war.type = WAR_GUILD;
						war.war = result->getNumber<int32_t>("id");
						player->addEnemy(guild, war);
					}
				} while (result->next());
			}
		}
	} else if (g_config.getBool(ConfigManager::INGAME_GUILD_MANAGEMENT)) {
		query.str("");
		query << "SELECT `guild_id` FROM `guild_invites` WHERE `player_id` = " << player->getGUID();
		if ((result = g_database.storeQuery(query.str()))) {
			do {
				player->invitationsList.push_back((uint32_t)result->getNumber<int32_t>("guild_id"));
			} while (result->next());
		}
	}

	query.str("");
	query << "SELECT `password` FROM `accounts` WHERE `id` = " << accountId << " LIMIT 1";
	if (!(result = g_database.storeQuery(query.str()))) {
		return false;
	}

	player->password = result->getString("password");

	// we need to find out our skills
	// so we query the skill table
	query.str("");
	query << "SELECT `skillid`, `value`, `count` FROM `player_skills` WHERE `player_id` = " << player->getGUID();
	if ((result = g_database.storeQuery(query.str()))) {
		// now iterate over the skills
		do {
			int16_t skillId = result->getNumber<int32_t>("skillid");
			if (skillId < SKILL_FIRST || skillId > SKILL_LAST) {
				continue;
			}

			uint32_t skillLevel = result->getNumber<int32_t>("value");
			uint64_t nextSkillCount = player->vocation->getReqSkillTries(
						 skillId, skillLevel + 1),
					 skillCount = result->getNumber<uint64_t>("count");
			if (skillCount > nextSkillCount) {
				skillCount = 0;
			}

			player->skills[skillId][SKILL_LEVEL] = skillLevel;
			player->skills[skillId][SKILL_TRIES] = skillCount;
			player->skills[skillId][SKILL_PERCENT] = Player::getPercentLevel(skillCount, nextSkillCount);
		} while (result->next());
	}

	query.str("");
	query << "SELECT `player_id`, `name` FROM `player_spells` WHERE `player_id` = " << player->getGUID();
	if ((result = g_database.storeQuery(query.str()))) {
		do {
			player->learnedInstantSpellList.push_back(result->getString("name"));
		} while (result->next());
	}

	ItemMap itemMap;
	ItemMap::iterator it;

	// load inventory items
	query.str("");
	query << "SELECT `pid`, `sid`, `itemtype`, `count`, `attributes`, `serial` FROM `player_items` WHERE `player_id` = " << player->getGUID() << " ORDER BY `sid` DESC";
	if ((result = g_database.storeQuery(query.str()))) {
		loadItems(itemMap, result);
		for (ItemMap::reverse_iterator rit = itemMap.rbegin(); rit != itemMap.rend(); ++rit) {
			Item* item = rit->second.first;
			int32_t pid = rit->second.second;
			if (pid > 0 && pid < 11) {
				player->__internalAddThing(pid, item);
			} else {
				it = itemMap.find(pid);
				if (it != itemMap.end()) {
					if (Container* container = it->second.first->getContainer()) {
						container->__internalAddThing(item);
					}
				}
			}
		}

		itemMap.clear();
	}

	// load depot items
	query.str("");
	query << "SELECT `pid`, `sid`, `itemtype`, `count`, `attributes`, `serial` FROM `player_depotitems` WHERE `player_id` = " << player->getGUID() << " ORDER BY `sid` DESC";
	if ((result = g_database.storeQuery(query.str()))) {
		loadItems(itemMap, result);
		for (ItemMap::reverse_iterator rit = itemMap.rbegin(); rit != itemMap.rend(); ++rit) {
			Item* item = rit->second.first;
			int32_t pid = rit->second.second;
			if (pid >= 0 && pid < 100) {
				if (Container* c = item->getContainer()) {
					if (Depot* depot = c->getDepot()) {
						player->addDepot(depot, pid);
					} else {
						std::clog << "[Error - IOLoginData::loadPlayer] Cannot load depot " << pid << " for player " << name << std::endl;
					}
				} else {
					std::clog << "[Error - IOLoginData::loadPlayer] Cannot load depot " << pid << " for player " << name << std::endl;
				}
			} else if ((it = itemMap.find(pid)) != itemMap.end()) {
				if (Container* container = it->second.first->getContainer()) {
					container->__internalAddThing(item);
				}
			}
		}

		itemMap.clear();
	}

	// load storage map
	query.str("");
	query << "SELECT `key`, `value` FROM `player_storage` WHERE `player_id` = " << player->getGUID();
	if ((result = g_database.storeQuery(query.str()))) {
		do {
			player->setStorage(result->getString("key"), result->getString("value"));
		} while (result->next());
	}

	// load autoloot
	if (g_config.getBool(ConfigManager::AUTOLOOT_ENABLE_SYSTEM)) {
		query.str("");
		query << "SELECT `autoloot_list` FROM `player_autoloot` WHERE `player_id` = " << player->getGUID();
		if ((result = g_database.storeQuery(query.str()))) {
			unsigned long lootListSize;
			const char* autoLootList = result->getStream("autoloot_list", lootListSize);
			PropStream propStream;
			propStream.init(autoLootList, lootListSize);

			uint16_t value;
			uint16_t item = propStream.getType<uint16_t>(value);
			while (item) {
				player->addAutoLoot(value);
				item = propStream.getType<uint16_t>(value);
			}
		}

		player->updateStatusAutoLoot(true);

		std::string msg = g_config.getString(ConfigManager::AUTOLOOT_MONEYIDS);
		StringVec strVector = explodeString(msg, ";");
		for (const std::string& str : strVector) {
			uint16_t id = atoi(str.c_str());
			player->addAutoLoot(id);
		}
	}

	// load vip
	query.str("");
	if (!g_config.getBool(ConfigManager::VIPLIST_PER_PLAYER)) {
		query << "SELECT `player_id` AS `vip` FROM `account_viplist` WHERE `account_id` = " << account.number << " AND `world_id` = " << g_config.getNumber(ConfigManager::WORLD_ID);
	} else {
		query << "SELECT `vip_id` AS `vip` FROM `player_viplist` WHERE `player_id` = " << player->getGUID();
	}

	if ((result = g_database.storeQuery(query.str()))) {
		do {
			uint32_t vid = result->getNumber<int32_t>("vip");
			if (storeNameByGuid(vid)) {
				player->addVIP(vid, "", false, true);
			}
		} while (result->next());
	}

	query.str("");
	query << "SELECT `pk`.`player_id`, `pd`.`date` FROM `player_killers` pk LEFT JOIN `killers` k"
		  << " ON `pk`.`kill_id` = `k`.`id` LEFT JOIN `player_deaths` pd ON `k`.`death_id` = `pd`.`id`"
		  << " WHERE `pd`.`player_id` = " << player->getGUID() << " AND `k`.`unjustified` = 1 AND "
		  << "`pd`.`date` >= " << (time(NULL) - (7 * 86400)) << " AND `k`.`war` = 0"; // TODO: configurable

	std::map<uint32_t, time_t> deaths;
	if ((result = g_database.storeQuery(query.str()))) {
		do {
			if (!deaths[result->getNumber<int32_t>("player_id")] || deaths[result->getNumber<int32_t>("player_id")] < (time_t)result->getNumber<int32_t>("date")) { // pick up the latest date
				deaths[result->getNumber<int32_t>("player_id")] = (time_t)result->getNumber<int32_t>("date");
			}
		} while (result->next());
	}

	if (!deaths.empty()) {
		query.str("");
		query << "SELECT `pd`.`player_id`, `pd`.`date` FROM `player_killers` pk LEFT JOIN `killers` k"
			  << " ON `pk`.`kill_id` = `k`.`id` LEFT JOIN `player_deaths` pd ON `k`.`death_id` = `pd`.`id`"
			  << " WHERE `pk`.`player_id` = " << player->getGUID() << " AND `k`.`unjustified` = 0 AND "
			  << "`pd`.`date` >= " << (time(NULL) - (7 * 86400)) << " AND `k`.`war` = 0";
	}

	player->updateInventoryWeight();
	player->updateItemsLight(true);
	player->updateBaseSpeed();
	return true;
}

void IOLoginData::loadItems(ItemMap& itemMap, DBResultPtr result)
{
	do {
		unsigned long attrSize = 0;
		const char* attr = result->getStream("attributes", attrSize);

		PropStream propStream;
		propStream.init(attr, attrSize);
		if (Item* item = Item::CreateItem(result->getNumber<int32_t>("itemtype"), result->getNumber<int32_t>("count"))) {
			if (!item->unserializeAttr(propStream)) {
				std::clog << "[Warning - IOLoginData::loadItems] Unserialize error for item with id " << item->getID() << std::endl;
			}

			std::string serial = result->getString("serial");
			if (serial != "") {
				std::string key = "serial";
				item->setAttribute(key.c_str(), serial);
			}

			itemMap[result->getNumber<int32_t>("sid")] = std::make_pair(item, result->getNumber<int32_t>("pid"));
		}
	} while (result->next());
}

bool IOLoginData::setName(Player* player, std::string newName)
{
	std::ostringstream query;

	DBTransaction trans;
	if (!trans.begin()) {
		return false;
	}

	query.str("");
	query << "UPDATE `players` SET `name` = " << g_database.escapeString(newName) << " WHERE `name` = " << g_database.escapeString(player->getName());

	if (!g_database.executeQuery(query.str())) {
		return false;
	}

	return trans.commit();
}

bool IOLoginData::deletePlayer(Player* player)
{
	std::ostringstream query;

	DBTransaction trans;
	if (!trans.begin()) {
		return false;
	}

	query.str("");
	query << "DELETE FROM `players` WHERE `name` = " << g_database.escapeString(player->getName());

	if (!g_database.executeQuery(query.str())) {
		return false;
	}

	return trans.commit();
}

bool IOLoginData::savePlayer(Player* player, bool preSave /* = true*/, bool shallow /* = false*/)
{
	if (preSave && player->health <= 0) {
		if (g_config.getBool(ConfigManager::USE_BLACK_SKULL)) {
			if (player->getSkull() == SKULL_BLACK) {
				player->health = g_config.getNumber(ConfigManager::BLACK_SKULL_DEATH_HEALTH);
				player->mana = g_config.getNumber(ConfigManager::BLACK_SKULL_DEATH_MANA);
			} else {
				player->health = player->healthMax;
				player->mana = player->manaMax;
			}
		} else {
			player->health = player->healthMax;
			player->mana = player->manaMax;
		}
	}
	std::ostringstream query;

	DBTransaction trans;
	if (!trans.begin()) {
		return false;
	}

	query.str("");
	query << "UPDATE `players` SET `lastlogin` = " << player->lastLogin << ", `lastip` = " << player->lastIP;
	if (!player->isSaving() || !g_config.getBool(ConfigManager::SAVE_PLAYER_DATA)) {
		query << " WHERE `id` = " << player->getGUID() << " LIMIT 1;";
		if (!g_database.executeQuery(query.str())) {
			return false;
		}

		return trans.commit();
	}

	query << ", ";
	query << "`level` = " << std::max((uint32_t)1, player->getLevel()) << ", ";
	query << "`group_id` = " << player->groupId << ", ";
	query << "`health` = " << player->health << ", ";
	query << "`healthmax` = " << player->healthMax << ", ";
	query << "`experience` = " << player->getExperience() << ", ";
	query << "`lookbody` = " << (uint32_t)player->defaultOutfit.lookBody << ", ";
	query << "`lookfeet` = " << (uint32_t)player->defaultOutfit.lookFeet << ", ";
	query << "`lookhead` = " << (uint32_t)player->defaultOutfit.lookHead << ", ";
	query << "`looklegs` = " << (uint32_t)player->defaultOutfit.lookLegs << ", ";
	query << "`looktype` = " << (uint32_t)player->defaultOutfit.lookType << ", ";
	query << "`lookaddons` = " << (uint32_t)player->defaultOutfit.lookAddons << ", ";
	query << "`maglevel` = " << player->magLevel << ", ";
	query << "`mana` = " << player->mana << ", ";
	query << "`manamax` = " << player->manaMax << ", ";
	query << "`manaspent` = " << player->manaSpent << ", ";
	query << "`soul` = " << player->soul << ", ";
	query << "`town_id` = " << player->town << ", ";
	query << "`posx` = " << player->getLoginPosition().x << ", ";
	query << "`posy` = " << player->getLoginPosition().y << ", ";
	query << "`posz` = " << player->getLoginPosition().z << ", ";
	query << "`cap` = " << player->getCapacity() << ", ";
	query << "`sex` = " << player->sex << ", ";
	query << "`balance` = " << player->balance << ", ";
	query << "`stamina` = " << player->getStamina() << ", ";
	if (g_config.getBool(ConfigManager::RESET_SYSTEM_ENABLE)) {
		query << "`reset` = " << player->reset << ", "; // reset system
	}

	Skulls_t skull = SKULL_RED;
	if (g_config.getBool(ConfigManager::USE_BLACK_SKULL)) {
		skull = player->getSkull();
	}

	query << "`skull` = " << skull << ", ";
	query << "`skulltime` = " << player->getSkullEnd() << ", ";
	query << "`promotion` = " << player->promotionLevel << ", ";
	if (g_config.getBool(ConfigManager::STORE_DIRECTION)) {
		query << "`direction` = " << (uint32_t)player->getDirection() << ", ";
	}

	if (!player->isVirtual()) {
		std::string name = player->getName(), nameDescription = player->getNameDescription();
		if (!player->isAccountManager() && nameDescription.length() > name.length()) {
			query << "`description` = " << g_database.escapeString(nameDescription.substr(name.length())) << ", ";
		}
	}

	// serialize conditions
	PropWriteStream propWriteStream;
	for (ConditionList::const_iterator it = player->conditions.begin(); it != player->conditions.end(); ++it) {
		if ((*it)->isPersistent() || (*it)->getType() == CONDITION_GAMEMASTER || (*it)->getType() == CONDITION_MUTED) {
			if (!(*it)->serialize(propWriteStream)) {
				return false;
			}

			propWriteStream.addByte(CONDITIONATTR_END);
		}
	}

	uint32_t conditionsSize = 0;
	const char* conditions = propWriteStream.getStream(conditionsSize);
	query << "`conditions` = " << g_database.escapeBlob(conditions, conditionsSize) << ", ";

	query << "`loss_experience` = " << (uint32_t)player->getLossPercent(LOSS_EXPERIENCE) << ", ";
	query << "`loss_mana` = " << (uint32_t)player->getLossPercent(LOSS_MANA) << ", ";
	query << "`loss_skills` = " << (uint32_t)player->getLossPercent(LOSS_SKILLS) << ", ";
	query << "`loss_containers` = " << (uint32_t)player->getLossPercent(LOSS_CONTAINERS) << ", ";
	query << "`loss_items` = " << (uint32_t)player->getLossPercent(LOSS_ITEMS) << ", ";

	query << "`lastlogout` = " << player->getLastLogout() << ", ";
	if (g_config.getBool(ConfigManager::BLESSINGS) && (player->isPremium() || !g_config.getBool(ConfigManager::BLESSING_ONLY_PREMIUM))) {
		query << "`blessings` = " << player->blessings << ", ";
		query << "`pvp_blessing` = " << (player->hasPVPBlessing() ? "1" : "0") << ", ";
	}

	query << "`offlinetraining_time` = " << player->getOfflineTrainingTime() / 1000 << ", ";
	query << "`offlinetraining_skill` = " << player->getOfflineTrainingSkill() << ", ";

	query << "`marriage` = " << player->marriage << ", ";
	if (g_config.getBool(ConfigManager::INGAME_GUILD_MANAGEMENT)) {
		query << "`guildnick` = " << g_database.escapeString(player->guildNick) << ", ";
		query << "`rank_id` = " << IOGuild::getInstance()->getRankIdByLevel(player->getGuildId(), player->getGuildLevel()) << ", ";
	}

	Vocation* tmpVoc = player->vocation;
	for (uint32_t i = 0; i <= player->promotionLevel; ++i) {
		tmpVoc = Vocations::getInstance()->getVocation(tmpVoc->getFromVocation());
	}

	query << "`vocation` = " << tmpVoc->getId() << " WHERE `id` = " << player->getGUID() << " LIMIT 1;";
	if (!g_database.executeQuery(query.str())) {
		return false;
	}

	// skills
	for (int32_t i = SKILL_FIRST; i <= SKILL_LAST; ++i) {
		query.str("");
		query << "UPDATE `player_skills` SET `value` = " << player->skills[i][SKILL_LEVEL] << ", `count` = " << player->skills[i][SKILL_TRIES] << " WHERE `player_id` = " << player->getGUID() << " AND `skillid` = " << i << " LIMIT 1;";
		if (!g_database.executeQuery(query.str())) {
			return false;
		}
	}

	if (shallow) {
		return trans.commit();
	}

	// learned spells
	query.str("");
	query << "DELETE FROM `player_spells` WHERE `player_id` = " << player->getGUID();
	if (!g_database.executeQuery(query.str())) {
		return false;
	}

	DBInsert stmt;
	if (player->learnedInstantSpellList.size()) {
		query.str("");
		stmt.setQuery("INSERT INTO `player_spells` (`player_id`, `name`) VALUES ");
		for (LearnedInstantSpellList::const_iterator it = player->learnedInstantSpellList.begin(); it != player->learnedInstantSpellList.end(); ++it) {
			query << player->getGUID() << "," << g_database.escapeString(*it);
			if (!stmt.addRow(query.str())) {
				return false;
			}
		}

		if (!stmt.execute()) {
			return false;
		}
	}

	// item saving
	query.str("");
	query << "DELETE FROM `player_items` WHERE `player_id` = " << player->getGUID();
	if (!g_database.executeQuery(query.str())) {
		return false;
	}

	ItemBlockList itemList;
	for (int32_t slotId = 1; slotId < 11; ++slotId) {
		if (Item* item = player->inventory[slotId]) {
			itemList.push_back(itemBlock(slotId, item));
		}
	}

	stmt.setQuery("INSERT INTO `player_items` (`player_id`, `pid`, `sid`, `itemtype`, `count`, `attributes`, `serial`) VALUES ");
	if (!saveItems(player, itemList, stmt)) {
		return false;
	}

	itemList.clear();
	// save depot items
	// std::ostringstream ss;
	for (DepotMap::iterator it = player->depots.begin(); it != player->depots.end(); ++it) {
		/*if(it->second.second)
		{
			it->second.second = false;
			ss << it->first << ",";*/
		itemList.push_back(itemBlock(it->first, it->second.first));
		//}
	}

	/*std::string s = ss.str();
	size_t size = s.length();
	if(size > 0)
	{*/

	query.str("");
	query << "DELETE FROM `player_depotitems` WHERE `player_id` = " << player->getGUID(); // << " AND `pid` IN (" << s.substr(0, --size) << ")";
	if (!g_database.executeQuery(query.str())) {
		return false;
	}

	if (itemList.size()) {
		stmt.setQuery("INSERT INTO `player_depotitems` (`player_id`, `pid`, `sid`, `itemtype`, `count`, `attributes`, `serial`) VALUES ");
		if (!saveItems(player, itemList, stmt)) {
			return false;
		}

		itemList.clear();
	}
	//}

	query.str("");
	query << "DELETE FROM `player_storage` WHERE `player_id` = " << player->getGUID();
	if (!g_database.executeQuery(query.str())) {
		return false;
	}

	player->generateReservedStorage();
	query.str("");

	stmt.setQuery("INSERT INTO `player_storage` (`player_id`, `key`, `value`) VALUES ");
	for (StorageMap::const_iterator cit = player->getStorageBegin(); cit != player->getStorageEnd(); ++cit) {
		query << player->getGUID() << "," << g_database.escapeString(cit->first) << "," << g_database.escapeString(cit->second);
		if (!stmt.addRow(query.str())) {
			return false;
		}
	}

	if (!stmt.execute()) {
		return false;
	}

	if (g_config.getBool(ConfigManager::INGAME_GUILD_MANAGEMENT)) {
		query.str("");
		// save guild invites
		query << "DELETE FROM `guild_invites` WHERE player_id = " << player->getGUID();
		if (!g_database.executeQuery(query.str())) {
			return false;
		}

		query.str("");
		stmt.setQuery("INSERT INTO `guild_invites` (`player_id`, `guild_id`) VALUES ");
		for (InvitationsList::iterator it = player->invitationsList.begin(); it != player->invitationsList.end(); ++it) {
			if (!IOGuild::getInstance()->guildExists(*it)) {
				it = player->invitationsList.erase(it);
				continue;
			}

			query << player->getGUID() << "," << (*it);
			if (!stmt.addRow(query.str())) {
				return false;
			}
		}

		if (!stmt.execute()) {
			return false;
		}
	}

	query.str("");
	// save vip list
	if (!g_config.getBool(ConfigManager::VIPLIST_PER_PLAYER)) {
		query << "DELETE FROM `account_viplist` WHERE `account_id` = " << player->getAccount() << " AND `world_id` = " << g_config.getNumber(ConfigManager::WORLD_ID);
	} else {
		query << "DELETE FROM `player_viplist` WHERE `player_id` = " << player->getGUID();
	}

	if (!g_database.executeQuery(query.str())) {
		return false;
	}

	// save autoloot
	if (g_config.getBool(ConfigManager::AUTOLOOT_ENABLE_SYSTEM)) {
		query.str("");
		query << "DELETE FROM `player_autoloot` WHERE `player_id` = " << player->getGUID();
		if (!g_database.executeQuery(query.str())) {
			return false;
		}

		std::list<uint16_t> autoLootList = player->getAutoLoot();
		PropWriteStream PWS_AutoLoot;
		for (const uint16_t& loot : autoLootList) {
			PWS_AutoLoot.addShort(loot);
		}

		uint32_t PWS_Size = 0;
		const char* autoLoot = PWS_AutoLoot.getStream(PWS_Size);

		query.str("");
		stmt.setQuery("INSERT INTO `player_autoloot` (`player_id`, `autoloot_list`) VALUES ");
		query << player->getGUID() << ',' << g_database.escapeBlob(autoLoot, PWS_Size);
		if (!stmt.addRow(query.str())) {
			return false;
		}

		if (!stmt.execute()) {
			return false;
		}
	}

	if (!g_config.getBool(ConfigManager::VIPLIST_PER_PLAYER)) {
		stmt.setQuery("INSERT INTO `account_viplist` (`account_id`, `world_id`, `player_id`) VALUES ");
	} else {
		stmt.setQuery("INSERT INTO `player_viplist` (`player_id`, `vip_id`) VALUES ");
	}

	query.str("");
	for (VIPSet::iterator it = player->VIPList.begin(); it != player->VIPList.end(); ++it) {
		if (!playerExists(*it, false, false)) {
			continue;
		}

		if (!g_config.getBool(ConfigManager::VIPLIST_PER_PLAYER)) {
			query << player->getAccount() << "," << g_config.getNumber(ConfigManager::WORLD_ID) << "," << (*it);
		} else {
			query << player->getGUID() << "," << (*it);
		}

		if (!stmt.addRow(query.str())) {
			return false;
		}
	}

	if (!stmt.execute()) {
		return false;
	}

	// End the transaction
	return trans.commit();
}

bool IOLoginData::savePlayerItems(Player* player)
{
	std::ostringstream query;
	DBInsert stmt;

	if (!player) {
		return false;
	}

	DBTransaction trans;
	if (!trans.begin()) {
		return false;
	}

	// item saving
	query.str("");
	query << "DELETE FROM `player_items` WHERE `player_id` = " << player->getGUID();
	if (!g_database.executeQuery(query.str())) {
		return false;
	}

	ItemBlockList itemList;
	for (int32_t slotId = 1; slotId < 11; ++slotId) {
		if (Item* item = player->inventory[slotId]) {
			itemList.push_back(itemBlock(slotId, item));
		}
	}

	stmt.setQuery("INSERT INTO `player_items` (`player_id`, `pid`, `sid`, `itemtype`, `count`, `attributes`, `serial`) VALUES ");
	if (!saveItems(player, itemList, stmt)) {
		return false;
	}

	itemList.clear();
	// save depot items
	// std::ostringstream ss;
	for (DepotMap::iterator it = player->depots.begin(); it != player->depots.end(); ++it) {
		/*if(it->second.second)
		{
		it->second.second = false;
		ss << it->first << ",";*/
		itemList.push_back(itemBlock(it->first, it->second.first));
		//}
	}

	/*std::string s = ss.str();
	size_t size = s.length();
	if(size > 0)
	{*/

	query.str("");
	query << "DELETE FROM `player_depotitems` WHERE `player_id` = " << player->getGUID(); // << " AND `pid` IN (" << s.substr(0, --size) << ")";
	if (!g_database.executeQuery(query.str())) {
		return false;
	}

	if (itemList.size()) {
		stmt.setQuery("INSERT INTO `player_depotitems` (`player_id`, `pid`, `sid`, `itemtype`, `count`, `attributes`, `serial`) VALUES ");
		if (!saveItems(player, itemList, stmt)) {
			return false;
		}

		itemList.clear();
	}
	//}

	query.str("");
	query << "DELETE FROM `player_storage` WHERE `player_id` = " << player->getGUID();
	if (!g_database.executeQuery(query.str())) {
		return false;
	}

	player->generateReservedStorage();
	query.str("");

	stmt.setQuery("INSERT INTO `player_storage` (`player_id`, `key`, `value`) VALUES ");
	for (StorageMap::const_iterator cit = player->getStorageBegin(); cit != player->getStorageEnd(); ++cit) {
		query << player->getGUID() << "," << g_database.escapeString(cit->first) << "," << g_database.escapeString(cit->second);
		if (!stmt.addRow(query.str())) {
			return false;
		}
	}

	if (!stmt.execute()) {
		return false;
	}

	// End the transaction
	return trans.commit();
}

bool IOLoginData::saveItems(const Player* player, const ItemBlockList& itemList, DBInsert& stmt)
{
	typedef std::pair<Container*, uint32_t> Stack;
	std::list<Stack> stackList;

	Item* item = NULL;
	int32_t runningId = 101;
	for (ItemBlockList::const_iterator it = itemList.begin(); it != itemList.end(); ++it, ++runningId) {
		item = it->second;
		PropWriteStream propWriteStream;
		item->serializeAttr(propWriteStream);

		std::string key = "serial";
		boost::any value = item->getAttribute(key.c_str());
		if (value.empty()) {
			item->generateSerial();
			value = item->getAttribute(key.c_str());
		}

		item->eraseAttribute(key.c_str());

		uint32_t attributesSize = 0;
		const char* attributes = propWriteStream.getStream(attributesSize);

		std::ostringstream buffer;
		buffer << player->getGUID() << "," << it->first << "," << runningId << "," << item->getID() << ","
			   << (int32_t)item->getSubType() << "," << g_database.escapeBlob(attributes, attributesSize) << "," << g_database.escapeString(boost::any_cast<std::string>(value).c_str());
		if (!stmt.addRow(buffer.str())) {
			return false;
		}

		if (Container* container = item->getContainer()) {
			stackList.push_back(Stack(container, runningId));
		}
	}

	while (stackList.size()) {
		Stack stack = stackList.front();
		stackList.pop_front();

		Container* container = stack.first;
		for (uint32_t i = 0; i < container->size(); ++i, ++runningId) {
			item = container->getItem(i);
			if (Container* subContainer = item->getContainer()) {
				stackList.push_back(Stack(subContainer, runningId));
			}

			PropWriteStream propWriteStream;
			item->serializeAttr(propWriteStream);

			std::string key = "serial";
			boost::any value = item->getAttribute(key.c_str());
			if (value.empty()) {
				item->generateSerial();
				value = item->getAttribute(key.c_str());
			}

			item->eraseAttribute(key.c_str());

			uint32_t attributesSize = 0;
			const char* attributes = propWriteStream.getStream(attributesSize);

			std::ostringstream buffer;
			buffer << player->getGUID() << "," << stack.second << "," << runningId << "," << item->getID() << ","
				   << (int32_t)item->getSubType() << "," << g_database.escapeBlob(attributes, attributesSize) << "," << g_database.escapeString(boost::any_cast<std::string>(value).c_str());
			if (!stmt.addRow(buffer.str())) {
				return false;
			}
		}
	}

	return stmt.execute();
}

bool IOLoginData::playerStatement(Player* _player, uint16_t channelId, const std::string& text, uint32_t& statementId)
{
	std::ostringstream query;

	query << "INSERT INTO `player_statements` (`player_id`, `channel_id`, `text`, `date`) VALUES (" << _player->getGUID()
		  << ", " << channelId << ", " << g_database.escapeString(text) << ", " << time(NULL) << ")";
	if (!g_database.executeQuery(query.str())) {
		return false;
	}

	statementId = g_database.getLastInsertId();
	return true;
}

bool IOLoginData::playerDeath(Player* _player, const DeathList& dl)
{
	std::ostringstream query;

	DBTransaction trans;
	if (!trans.begin()) {
		return false;
	}

	query << "INSERT INTO `player_deaths` (`player_id`, `date`, `level`) VALUES (" << _player->getGUID()
		  << ", " << time(NULL) << ", " << _player->getLevel() << ")";
	if (!g_database.executeQuery(query.str())) {
		return false;
	}

	uint32_t i = 0, size = dl.size(), tmp = g_config.getNumber(ConfigManager::DEATH_ASSISTS) + 1;
	if (size > tmp) {
		size = tmp;
	}

	DeathList wl;
	bool war = false;

	uint64_t deathId = g_database.getLastInsertId();
	for (DeathList::const_iterator it = dl.begin(); i < size && it != dl.end(); ++it, ++i) {
		query.str("");
		query << "INSERT INTO `killers` (`death_id`, `final_hit`, `unjustified`, `war`) VALUES ("
			  << deathId << ", " << it->isLast() << ", " << it->isUnjustified() << ", " << it->getWar().war << ")";
		if (!g_database.executeQuery(query.str())) {
			return false;
		}

		std::string name;
		uint64_t killId = g_database.getLastInsertId();
		if (it->isCreatureKill()) {
			Creature* creature = it->getKillerCreature();
			Player* player = creature->getPlayer();
			if (creature->getMaster()) {
				player = creature->getPlayerMaster();
				name = creature->getNameDescription();
			}

			if (player) {
				if (_player->isEnemy(player, false)) {
					wl.push_back(*it);
				}

				query.str("");
				query << "INSERT INTO `player_killers` (`kill_id`, `player_id`) VALUES ("
					  << killId << ", " << player->getGUID() << ")";
				if (!g_database.executeQuery(query.str())) {
					return false;
				}
			} else {
				name = creature->getNameDescription();
			}
		} else {
			name = it->getKillerName();
		}

		if (!name.empty()) {
			query.str("");
			query << "INSERT INTO `environment_killers` (`kill_id`, `name`) VALUES ("
				  << killId << ", " << g_database.escapeString(name) << ")";
			if (!g_database.executeQuery(query.str())) {
				return false;
			}
		}
	}

	if (!wl.empty()) {
		IOGuild::getInstance()->frag(_player, deathId, wl, war);
	}

	return trans.commit();
}

bool IOLoginData::playerMail(Creature* actor, std::string name, uint32_t townId, Item* item)
{
	Player* player = g_game.getPlayerByNameEx(name);
	if (!player) {
		return false;
	}

	if (!townId) {
		townId = player->getTown();
	}

	Depot* depot = player->getDepot(townId, true);
	if (g_game.internalMoveItem(actor, item->getParent(), depot, INDEX_WHEREEVER,
			item, item->getItemCount(), NULL, FLAG_NOLIMIT)
		!= RET_NOERROR) {
		if (player->isVirtual()) {
			delete player;
		}

		return false;
	}

	g_game.transformItem(item, item->getID() == ITEM_PARCEL ? ITEM_PARCEL_STAMPED : ITEM_LETTER_STAMPED);
	bool result = true, opened = player->getContainerID(depot) != -1;

	Player* tmp = NULL;
	if (actor) {
		tmp = actor->getPlayer();
	}

	CreatureEventList mailEvents = player->getCreatureEvents(CREATURE_EVENT_MAIL_RECEIVE);
	for (CreatureEventList::iterator it = mailEvents.begin(); it != mailEvents.end(); ++it) {
		if (!(*it)->executeMail(player, tmp, item, opened) && result) {
			result = false;
		}
	}

	if (tmp) {
		mailEvents = tmp->getCreatureEvents(CREATURE_EVENT_MAIL_SEND);
		for (CreatureEventList::iterator it = mailEvents.begin(); it != mailEvents.end(); ++it) {
			if (!(*it)->executeMail(tmp, player, item, opened) && result) {
				result = false;
			}
		}
	}

	if (player->isVirtual()) {
		savePlayer(player);
		delete player;
	}

	return result;
}

bool IOLoginData::hasFlag(const std::string& name, PlayerFlags value)
{
	std::ostringstream query;
	query << "SELECT `group_id` FROM `players` WHERE `name` = " << g_database.escapeString(name) << " AND `deleted` = 0 LIMIT 1";

	DBResultPtr result;
	if (!(result = g_database.storeQuery(query.str()))) {
		return false;
	}

	Group* group = Groups::getInstance()->getGroup(result->getNumber<int32_t>("group_id"));
	return group && group->hasFlag(value);
}

bool IOLoginData::hasCustomFlag(const std::string& name, PlayerCustomFlags value)
{
	std::ostringstream query;
	query << "SELECT `group_id` FROM `players` WHERE `name` = " << g_database.escapeString(name) << " AND `deleted` = 0 LIMIT 1";

	DBResultPtr result;
	if (!(result = g_database.storeQuery(query.str()))) {
		return false;
	}

	Group* group = Groups::getInstance()->getGroup(result->getNumber<int32_t>("group_id"));
	return group && group->hasCustomFlag(value);
}

bool IOLoginData::hasFlag(PlayerFlags value, uint32_t guid)
{
	std::ostringstream query;
	query << "SELECT `group_id` FROM `players` WHERE `id` = " << guid << " AND `deleted` = 0 LIMIT 1";

	DBResultPtr result;
	if (!(result = g_database.storeQuery(query.str()))) {
		return false;
	}

	Group* group = Groups::getInstance()->getGroup(result->getNumber<int32_t>("group_id"));
	return group && group->hasFlag(value);
}

bool IOLoginData::hasCustomFlag(PlayerCustomFlags value, uint32_t guid)
{
	std::ostringstream query;
	query << "SELECT `group_id` FROM `players` WHERE `id` = " << guid << " AND `deleted` = 0 LIMIT 1";

	DBResultPtr result;
	if (!(result = g_database.storeQuery(query.str()))) {
		return false;
	}

	Group* group = Groups::getInstance()->getGroup(result->getNumber<int32_t>("group_id"));
	return group && group->hasCustomFlag(value);
}

bool IOLoginData::isPremium(uint32_t guid)
{
	if (g_config.getBool(ConfigManager::FREE_PREMIUM)) {
		return true;
	}

	std::ostringstream query;
	query << "SELECT `account_id`, `group_id` FROM `players` WHERE `id` = " << guid << " AND `deleted` = 0 LIMIT 1";

	DBResultPtr result;
	if (!(result = g_database.storeQuery(query.str()))) {
		return false;
	}

	Group* group = Groups::getInstance()->getGroup(result->getNumber<int32_t>("group_id"));
	const uint32_t account = result->getNumber<int32_t>("account_id");

	if (group && group->hasFlag(PlayerFlag_IsAlwaysPremium)) {
		return true;
	}

	query.str("");
	query << "SELECT `premdays` FROM `accounts` WHERE `id` = " << account << " LIMIT 1";
	if (!(result = g_database.storeQuery(query.str()))) {
		return false;
	}

	const uint32_t premium = result->getNumber<int32_t>("premdays");
	return premium > 0;
}

bool IOLoginData::playerExists(uint32_t guid, bool multiworld /*= false*/, bool checkCache /*= true*/)
{
	if (checkCache) {
		NameCacheMap::iterator it = nameCacheMap.find(guid);
		if (it != nameCacheMap.end()) {
			return true;
		}
	}

	std::ostringstream query;

	query << "SELECT `name` FROM `players` WHERE `id` = " << guid << " AND `deleted` = 0";
	if (!multiworld) {
		query << " AND `world_id` = " << g_config.getNumber(ConfigManager::WORLD_ID);
	}

	query << " LIMIT 1";
	DBResultPtr result;
	if (!(result = g_database.storeQuery(query.str()))) {
		return false;
	}

	const std::string name = result->getString("name");

	nameCacheMap[guid] = name;
	return true;
}

bool IOLoginData::playerExists(std::string& name, bool multiworld /*= false*/, bool checkCache /*= true*/)
{
	if (checkCache) {
		GuidCacheMap::iterator it = guidCacheMap.find(name);
		if (it != guidCacheMap.end()) {
			name = it->first;
			return true;
		}
	}

	std::ostringstream query;

	query << "SELECT `id`, `name` FROM `players` WHERE `name` = " << g_database.escapeString(name) << " AND `deleted` = 0";
	if (!multiworld) {
		query << " AND `world_id` = " << g_config.getNumber(ConfigManager::WORLD_ID);
	}

	query << " LIMIT 1";
	DBResultPtr result;
	if (!(result = g_database.storeQuery(query.str()))) {
		return false;
	}

	name = result->getString("name");
	guidCacheMap[name] = result->getNumber<int32_t>("id");

	return true;
}

bool IOLoginData::getNameByGuid(uint32_t guid, std::string& name, bool multiworld /*= false*/)
{
	NameCacheMap::iterator it = nameCacheMap.find(guid);
	if (it != nameCacheMap.end()) {
		name = it->second;
		return true;
	}

	std::ostringstream query;

	query << "SELECT `name` FROM `players` WHERE `id` = " << guid << " AND `deleted` = 0";
	if (!multiworld) {
		query << " AND `world_id` = " << g_config.getNumber(ConfigManager::WORLD_ID);
	}

	query << " LIMIT 1";
	DBResultPtr result;
	if (!(result = g_database.storeQuery(query.str()))) {
		return false;
	}

	name = result->getString("name");

	nameCacheMap[guid] = name;
	return true;
}

bool IOLoginData::storeNameByGuid(uint32_t guid)
{
	NameCacheMap::iterator it = nameCacheMap.find(guid);
	if (it != nameCacheMap.end()) {
		return true;
	}

	std::ostringstream query;
	query << "SELECT `name` FROM `players` WHERE `id` = " << guid << " AND `deleted` = 0 LIMIT 1";

	DBResultPtr result;
	if (!(result = g_database.storeQuery(query.str()))) {
		return false;
	}

	nameCacheMap[guid] = result->getString("name");
	return true;
}

bool IOLoginData::getGuidByName(uint32_t& guid, std::string& name, bool multiworld /*= false*/)
{
	GuidCacheMap::iterator it = guidCacheMap.find(name);
	if (it != guidCacheMap.end()) {
		name = it->first;
		guid = it->second;
		return true;
	}

	std::ostringstream query;

	query << "SELECT `name`, `id` FROM `players` WHERE `name` = " << g_database.escapeString(name) << " AND `deleted` = 0";
	if (!multiworld) {
		query << " AND `world_id` = " << g_config.getNumber(ConfigManager::WORLD_ID);
	}

	query << " LIMIT 1";
	DBResultPtr result;
	if (!(result = g_database.storeQuery(query.str()))) {
		return false;
	}

	name = result->getString("name");
	guid = result->getNumber<int32_t>("id");

	guidCacheMap[name] = guid;
	return true;
}

bool IOLoginData::getGuidByNameEx(uint32_t& guid, bool& specialVip, std::string& name)
{
	std::ostringstream query;
	query << "SELECT `id`, `name`, `group_id` FROM `players` WHERE `name` = " << g_database.escapeString(name) << " AND `deleted` = 0 AND `world_id` = " << g_config.getNumber(ConfigManager::WORLD_ID) << " LIMIT 1";

	DBResultPtr result;
	if (!(result = g_database.storeQuery(query.str()))) {
		return false;
	}

	guid = result->getNumber<int32_t>("id");
	name = result->getString("name");
	if (Group* group = Groups::getInstance()->getGroup(result->getNumber<int32_t>("group_id"))) {
		specialVip = group->hasFlag(PlayerFlag_SpecialVIP);
	}

	return true;
}

bool IOLoginData::changeName(uint32_t guid, std::string newName, std::string oldName)
{
	std::ostringstream query;

	query << "INSERT INTO `player_namelocks` (`player_id`, `name`, `new_name`, `date`) VALUES (" << guid << ", "
		  << g_database.escapeString(oldName) << ", " << g_database.escapeString(newName) << ", " << time(NULL) << ")";
	if (!g_database.executeQuery(query.str())) {
		return false;
	}

	query.str("");
	query << "UPDATE `players` SET `name` = " << g_database.escapeString(newName) << " WHERE `id` = " << guid << " LIMIT 1;";
	if (!g_database.executeQuery(query.str())) {
		return false;
	}

	GuidCacheMap::iterator it = guidCacheMap.find(oldName);
	if (it != guidCacheMap.end()) {
		guidCacheMap.erase(it);
		guidCacheMap[newName] = guid;
	}

	nameCacheMap[guid] = newName;
	return true;
}

bool IOLoginData::createCharacter(uint32_t accountId, std::string characterName, int32_t vocationId, uint16_t sex)
{
	std::ostringstream query;
	query << "SELECT `id` FROM `players` WHERE `name` = " << g_database.escapeString(characterName) << ";";
	DBResultPtr result = g_database.storeQuery(query.str());
	if (result) {
		return false;
	}

	Vocation* vocation = Vocations::getInstance()->getVocation(vocationId);
	Vocation* rookVoc = Vocations::getInstance()->getVocation(0);

	uint16_t healthMax = 150, manaMax = 0, capMax = 400, lookType = 136;
	if (sex % 2) {
		lookType = 128;
	}

	uint32_t level = g_config.getNumber(ConfigManager::START_LEVEL), tmpLevel = std::min((uint32_t)7, (level - 1));
	uint64_t exp = 0;
	if (level > 1) {
		exp = Player::getExpForLevel(level);
	}

	if (tmpLevel > 0) {
		healthMax += rookVoc->getGain(GAIN_HEALTH) * tmpLevel;
		manaMax += rookVoc->getGain(GAIN_MANA) * tmpLevel;
		capMax += rookVoc->getGainCap() * tmpLevel;
		if (level > 8) {
			tmpLevel = level - 8;
			healthMax += vocation->getGain(GAIN_HEALTH) * tmpLevel;
			manaMax += vocation->getGain(GAIN_MANA) * tmpLevel;
			capMax += vocation->getGainCap() * tmpLevel;
		}
	}

	query.str("");
	query << "INSERT INTO `players` (`id`, `name`, `world_id`, `group_id`, `account_id`, `level`, `vocation`, `health`, `healthmax`, `experience`, `lookbody`, `lookfeet`, `lookhead`, `looklegs`, `looktype`, `lookaddons`, `maglevel`, `mana`, `manamax`, `manaspent`, `soul`, `town_id`, `posx`, `posy`, `posz`, `conditions`, `cap`, `sex`, `lastlogin`, `lastip`, `skull`, `skulltime`, `save`, `rank_id`, `guildnick`, `lastlogout`, `blessings`, `online`) VALUES (NULL, " << g_database.escapeString(characterName) << ", " << g_config.getNumber(ConfigManager::WORLD_ID) << ", 1, " << accountId << ", " << level << ", " << vocationId << ", " << healthMax << ", " << healthMax << ", " << exp << ", 68, 76, 78, 39, " << lookType << ", 0, " << g_config.getNumber(ConfigManager::START_MAGICLEVEL) << ", " << manaMax << ", " << manaMax << ", 0, 100, " << g_config.getNumber(ConfigManager::SPAWNTOWN_ID) << ", " << g_config.getNumber(ConfigManager::SPAWNPOS_X) << ", " << g_config.getNumber(ConfigManager::SPAWNPOS_Y) << ", " << g_config.getNumber(ConfigManager::SPAWNPOS_Z) << ", 0, " << capMax << ", " << sex << ", 0, 0, 0, 0, 1, 0, '', 0, 0, 0)";
	return g_database.executeQuery(query.str());
}

DeleteCharacter_t IOLoginData::deleteCharacter(uint32_t accountId, const std::string& characterName)
{
	if (g_game.getPlayerByName(characterName)) {
		return DELETE_ONLINE;
	}

	std::ostringstream query;
	query << "SELECT `id` FROM `players` WHERE `name` = " << g_database.escapeString(characterName) << " AND `account_id` = " << accountId << " AND `deleted` = 0 LIMIT 1";

	DBResultPtr result;
	if (!(result = g_database.storeQuery(query.str()))) {
		return DELETE_INTERNAL;
	}

	uint32_t id = result->getNumber<int32_t>("id");

	House* house = Houses::getInstance()->getHouseByPlayerId(id);
	if (house) {
		return DELETE_HOUSE;
	}

	if (IOGuild::getInstance()->getGuildLevel(id) == 3) {
		return DELETE_LEADER;
	}

	query.str("");
	query << "UPDATE `players` SET `deleted` = " << time(NULL) << " WHERE `id` = " << id << " LIMIT 1;";
	if (!g_database.executeQuery(query.str())) {
		return DELETE_INTERNAL;
	}

	query.str("");
	query << "DELETE FROM `guild_invites` WHERE `player_id` = " << id;
	g_database.executeQuery(query.str());

	query.str("");
	query << "DELETE FROM `player_viplist` WHERE `vip_id` = " << id;
	g_database.executeQuery(query.str());

	for (AutoList<Player>::iterator it = Player::autoList.begin(); it != Player::autoList.end(); ++it) {
		VIPSet::iterator it_ = it->second->VIPList.find(id);
		if (it_ != it->second->VIPList.end()) {
			it->second->VIPList.erase(it_);
		}
	}

	GuidCacheMap::iterator it = guidCacheMap.find(characterName);
	if (it != guidCacheMap.end()) {
		guidCacheMap.erase(it);
	}

	NameCacheMap::iterator it2 = nameCacheMap.find(id);
	if (it2 != nameCacheMap.end()) {
		nameCacheMap.erase(it2);
	}

	return DELETE_SUCCESS;
}

uint32_t IOLoginData::getLevel(uint32_t guid) const
{
	std::ostringstream query;
	query << "SELECT `level` FROM `players` WHERE `id` = " << guid << " AND `deleted` = 0 LIMIT 1";

	DBResultPtr result;
	if (!(result = g_database.storeQuery(query.str()))) {
		return 0;
	}

	uint32_t level = result->getNumber<int32_t>("level");
	return level;
}

uint32_t IOLoginData::getLastIP(uint32_t guid) const
{
	std::ostringstream query;
	query << "SELECT `lastip` FROM `players` WHERE `id` = " << guid << " AND `deleted` = 0 LIMIT 1";

	DBResultPtr result;
	if (!(result = g_database.storeQuery(query.str()))) {
		return 0;
	}

	const uint32_t ip = result->getNumber<int32_t>("lastip");
	return ip;
}

uint32_t IOLoginData::getLastIPByName(const std::string& name) const
{
	std::ostringstream query;
	query << "SELECT `lastip` FROM `players` WHERE `name` = " << g_database.escapeString(name) << " AND `deleted` = 0 LIMIT 1";

	DBResultPtr result;
	if (!(result = g_database.storeQuery(query.str()))) {
		return 0;
	}

	const uint32_t ip = result->getNumber<int32_t>("lastip");
	return ip;
}

uint32_t IOLoginData::getAccountIdByName(const std::string& name) const
{
	std::ostringstream query;
	query << "SELECT `account_id` FROM `players` WHERE `name` = " << g_database.escapeString(name) << " AND `deleted` = 0 LIMIT 1";

	DBResultPtr result;
	if (!(result = g_database.storeQuery(query.str()))) {
		return 0;
	}

	const uint32_t accountId = result->getNumber<int32_t>("account_id");
	return accountId;
}

bool IOLoginData::getUnjustifiedDates(uint32_t guid, std::vector<time_t>& dateList, time_t _time)
{
	std::ostringstream query;

	uint32_t maxLength = std::max(g_config.getNumber(ConfigManager::FRAG_THIRD_LIMIT),
		std::max(g_config.getNumber(ConfigManager::FRAG_SECOND_LIMIT),
			g_config.getNumber(ConfigManager::FRAG_LIMIT)));
	query << "SELECT `pd`.`date` FROM `player_killers` pk LEFT JOIN `killers` k ON `pk`.`kill_id` = `k`.`id`"
		  << "LEFT JOIN `player_deaths` pd ON `k`.`death_id` = `pd`.`id` WHERE `pk`.`player_id` = " << guid
		  << " AND `k`.`unjustified` = 1 AND `pd`.`date` >= " << (_time - maxLength) << " AND `k`.`war` = 0";

	DBResultPtr result;
	if (!(result = g_database.storeQuery(query.str()))) {
		return false;
	}

	do {
		dateList.push_back((time_t)result->getNumber<int32_t>("date"));
	} while (result->next());
	return true;
}

bool IOLoginData::getDefaultTownByName(const std::string& name, uint32_t& townId)
{
	std::ostringstream query;
	query << "SELECT `town_id` FROM `players` WHERE `name` = " << g_database.escapeString(name) << " AND `deleted` = 0 LIMIT 1";

	DBResultPtr result;
	if (!(result = g_database.storeQuery(query.str()))) {
		return false;
	}

	townId = result->getNumber<int32_t>("town_id");
	return true;
}

bool IOLoginData::updatePremiumDays()
{
	std::ostringstream query;

	DBTransaction trans;
	if (!trans.begin()) {
		return false;
	}

	DBResultPtr result;
	query << "SELECT `id` FROM `accounts` WHERE `lastday` <= " << time(NULL) - 86400;
	if (!(result = g_database.storeQuery(query.str()))) {
		return false;
	}

	Account account;
	do {
		removePremium((account = loadAccount(result->getNumber<int32_t>("id"), true)));
	} while (result->next());

	query.str("");
	return trans.commit();
}

bool IOLoginData::updateOnlineStatus(uint32_t guid, bool login)
{
	std::ostringstream query;

	uint16_t value = login;
	if (g_config.getNumber(ConfigManager::ALLOW_CLONES)) {
		query << "SELECT `online` FROM `players` WHERE `id` = " << guid << " AND `deleted` = 0 LIMIT 1";
		DBResultPtr result;
		if (!(result = g_database.storeQuery(query.str()))) {
			return false;
		}

		value = result->getNumber<int32_t>("online");

		if (login) {
			value++;
		} else if (value > 0) {
			value--;
		}

		query.str("");
	}

	query << "UPDATE `players` SET `online` = " << value << " WHERE `id` = " << guid << " LIMIT 1;";
	return g_database.executeQuery(query.str());
}

bool IOLoginData::resetGuildInformation(uint32_t guid)
{
	std::ostringstream query;
	query << "UPDATE `players` SET `rank_id` = 0, `guildnick` = '' WHERE `id` = " << guid << " AND `deleted` = 0 LIMIT 1;";
	return g_database.executeQuery(query.str());
}

void IOLoginData::increaseBankBalance(uint32_t guid, uint64_t bankBalance)
{
	std::ostringstream query;
	query << "UPDATE `players` SET `balance` = `balance` + " << bankBalance << " WHERE `id` = " << guid << ";";
	// return g_database.executeQuery(query.str());
}
