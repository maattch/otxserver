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

#include "ioban.h"

#include "database.h"
#include "game.h"
#include "iologindata.h"

bool IOBan::isIpBanished(uint32_t ip, uint32_t mask /* = 0xFFFFFFFF*/) const
{
	if (!ip) {
		return false;
	}

	DBResultPtr result;

	std::ostringstream query;
	query << "SELECT `id`, `value`, `param`, `expires` FROM `bans` WHERE `type` = " << static_cast<int>(BAN_IP) << " AND `active` = 1";
	if (!(result = g_database.storeQuery(query.str()))) {
		return false;
	}

	bool ret = false;
	do {
		uint32_t value = result->getNumber<int32_t>("value"), param = result->getNumber<int32_t>("param");
		if ((ip & mask & param) == (value & param & mask)) {
			int32_t expires = result->getNumber<uint64_t>("expires");
			if (expires > 0 && expires <= time(nullptr)) {
				removeIpBanishment(value, param);
			} else if (!ret) {
				ret = true;
			}
		}
	} while (result->next());
	return ret;
}

bool IOBan::isPlayerBanished(uint32_t playerId, PlayerBan_t type) const
{
	DBResultPtr result;

	std::ostringstream query;
	query << "SELECT `id` FROM `bans` WHERE `type` = " << static_cast<int>(BAN_PLAYER) << " AND `value` = " << playerId << " AND `param` = " << static_cast<int>(type) << " AND `active` = 1 LIMIT 1";
	if (!(result = g_database.storeQuery(query.str()))) {
		return false;
	}

	return true;
}

bool IOBan::isPlayerBanished(std::string name, PlayerBan_t type) const
{
	uint32_t _guid;
	return IOLoginData::getInstance()->getGuidByName(_guid, name)
		&& isPlayerBanished(_guid, type);
}

bool IOBan::isAccountBanished(uint32_t account, uint32_t playerId /* = 0*/) const
{
	DBResultPtr result;

	std::ostringstream query;
	query << "SELECT `expires` FROM `bans` WHERE `type` = " << BAN_ACCOUNT << " AND `value` = " << account;
	if (playerId > 0) {
		query << " AND `param` = " << playerId;
	}

	query << " AND `active` = 1 LIMIT 1";
	if (!(result = g_database.storeQuery(query.str()))) {
		return false;
	}

	const auto expires = result->getNumber<int32_t>("expires");
	if (expires <= 0 || expires > time(nullptr)) {
		return true;
	}

	removeAccountBanishment(account);
	return false;
}

bool IOBan::addIpBanishment(uint32_t ip, int64_t banTime, uint32_t reasonId,
	std::string comment, uint32_t gamemaster, uint32_t mask /* = 0xFFFFFFFF*/, std::string statement /* = ""*/) const
{
	if (isIpBanished(ip)) {
		return false;
	}

	for (Player* player : g_game.getPlayersByIP(ip, mask)) {
		player->kick(true, true);
	}

	std::ostringstream query;

	query << "INSERT INTO `bans` (`id`, `type`, `value`, `param`, `expires`, `added`, `admin_id`, `comment`, `reason`, `statement`) ";
	query << "VALUES (NULL, " << static_cast<int>(BAN_IP) << ", " << ip << ", " << mask << ", " << banTime << ", " << time(nullptr) << ", " << gamemaster;
	query << ", " << g_database.escapeString(comment) << ", " << reasonId << ", " << g_database.escapeString(statement) << ")";
	return g_database.executeQuery(query.str());
}

bool IOBan::addPlayerBanishment(uint32_t playerId, int64_t banTime, uint32_t reasonId, ViolationAction_t actionId,
	std::string comment, uint32_t gamemaster, PlayerBan_t type, std::string statement /* = ""*/) const
{
	if (isPlayerBanished(playerId, type)) {
		return false;
	}

	std::ostringstream query;

	query << "INSERT INTO `bans` (`id`, `type`, `value`, `param`, `expires`, `added`, `admin_id`, `comment`, `reason`, `action`, `statement`) ";
	query << "VALUES (NULL, " << static_cast<int>(BAN_PLAYER) << ", " << playerId << ", " << static_cast<int>(type) << ", " << banTime << ", " << time(nullptr) << ", " << gamemaster;
	query << ", " << g_database.escapeString(comment) << ", " << reasonId << ", " << actionId << ", " << g_database.escapeString(statement) << ")";
	return g_database.executeQuery(query.str());
}

bool IOBan::addPlayerBanishment(std::string name, int64_t banTime, uint32_t reasonId, ViolationAction_t actionId, std::string comment, uint32_t gamemaster, PlayerBan_t type, std::string statement /* = ""*/) const
{
	uint32_t _guid;
	return IOLoginData::getInstance()->getGuidByName(_guid, name) && addPlayerBanishment(_guid, banTime, reasonId, actionId, comment, gamemaster, type, statement);
}

bool IOBan::addAccountBanishment(uint32_t account, int64_t banTime, uint32_t reasonId, ViolationAction_t actionId, std::string comment, uint32_t gamemaster, uint32_t playerId, std::string statement /* = ""*/) const
{
	if (isAccountBanished(account)) {
		return false;
	}

	std::ostringstream query;

	query << "INSERT INTO `bans` (`id`, `type`, `value`, `param`, `expires`, `added`, `admin_id`, `comment`, `reason`, `action`, `statement`) ";
	query << "VALUES (NULL, " << static_cast<int>(BAN_ACCOUNT) << ", " << account << ", " << playerId << ", " << banTime << ", " << time(nullptr) << ", " << gamemaster;
	query << ", " << g_database.escapeString(comment) << ", " << reasonId << ", " << actionId << ", " << g_database.escapeString(statement) << ")";
	return g_database.executeQuery(query.str());
}

bool IOBan::addNotation(uint32_t account, uint32_t reasonId, std::string comment, uint32_t gamemaster, uint32_t playerId, std::string statement /* = ""*/) const
{
	std::ostringstream query;

	query << "INSERT INTO `bans` (`id`, `type`, `value`, `param`, `expires`, `added`, `admin_id`, `comment`, `reason`, `statement`) ";
	query << "VALUES (NULL, " << static_cast<int>(BAN_NOTATION) << ", " << account << ", " << playerId << ", '-1', " << time(nullptr) << ", " << gamemaster;
	query << ", " << g_database.escapeString(comment) << ", " << reasonId << ", " << g_database.escapeString(statement) << ")";
	return g_database.executeQuery(query.str());
}

bool IOBan::addStatement(uint32_t playerId, uint32_t reasonId, std::string comment, uint32_t gamemaster, int16_t channelId /* = -1*/, std::string statement /* = ""*/) const
{
	std::ostringstream query;

	query << "INSERT INTO `bans` (`id`, `type`, `value`, ";
	if (channelId >= 0) {
		query << "`param`, ";
	}

	query << "`expires`, `added`, `admin_id`, `comment`, `reason`, `statement`) VALUES (NULL, " << BAN_STATEMENT << ", " << playerId;
	if (channelId >= 0) {
		query << ", " << channelId;
	}

	query << ", '-1', " << time(nullptr) << ", " << gamemaster << ", " << g_database.escapeString(comment);
	query << ", " << reasonId << ", " << g_database.escapeString(statement) << ")";
	return g_database.executeQuery(query.str());
}

bool IOBan::addStatement(std::string name, uint32_t reasonId, std::string comment, uint32_t gamemaster, int16_t channelId /* = -1*/, std::string statement /* = ""*/) const
{
	uint32_t _guid;
	return IOLoginData::getInstance()->getGuidByName(_guid, name) && addStatement(_guid, reasonId, comment, gamemaster, channelId, statement);
}

bool IOBan::removeIpBanishment(uint32_t ip, uint32_t mask /* = 0xFFFFFFFF*/) const
{
	std::ostringstream query;

	query << "UPDATE `bans` SET `active` = 0 WHERE `value` = " << ip << " AND `param` = " << mask
		  << " AND `type` = " << static_cast<int>(BAN_IP) << " AND `active` = 1 LIMIT 1";
	return g_database.executeQuery(query.str());
}

bool IOBan::removePlayerBanishment(uint32_t guid, PlayerBan_t type) const
{
	std::ostringstream query;

	query << "UPDATE `bans` SET `active` = 0 WHERE `value` = " << guid << " AND `param` = " << type
		  << " AND `type` = " << static_cast<int>(BAN_PLAYER) << " AND `active` = 1 LIMIT 1";
	return g_database.executeQuery(query.str());
}

bool IOBan::removePlayerBanishment(std::string name, PlayerBan_t type) const
{
	uint32_t _guid;
	return IOLoginData::getInstance()->getGuidByName(_guid, name)
		&& removePlayerBanishment(_guid, type);
}

bool IOBan::removeAccountBanishment(uint32_t account, uint32_t playerId /* = 0*/) const
{
	std::ostringstream query;

	query << "UPDATE `bans` SET `active` = 0 WHERE `value` = " << account;
	if (playerId > 0) {
		query << " AND `param` = " << playerId;
	}

	query << " AND `type` = " << static_cast<int>(BAN_ACCOUNT) << " AND `active` = 1 LIMIT 1";
	return g_database.executeQuery(query.str());
}

bool IOBan::removeNotations(uint32_t account, uint32_t playerId /* = 0*/) const
{
	std::ostringstream query;

	query << "UPDATE `bans` SET `active` = 0 WHERE `value` = " << account;
	if (playerId > 0) {
		query << " AND `param` = " << playerId;
	}

	query << " AND `type` = " << static_cast<int>(BAN_NOTATION) << " AND `active` = 1";
	return g_database.executeQuery(query.str());
}

bool IOBan::removeStatements(uint32_t playerId, int16_t channelId /* = -1*/) const
{
	std::ostringstream query;

	query << "UPDATE `bans` SET `active` = 0 WHERE `value` = " << playerId;
	if (channelId >= 0) {
		query << " AND `param` = " << channelId;
	}

	query << " AND `type` = " << static_cast<int>(BAN_STATEMENT) << " AND `active` = 1";
	return g_database.executeQuery(query.str());
}

bool IOBan::removeStatements(std::string name, int16_t channelId /* = -1*/) const
{
	uint32_t _guid;
	return IOLoginData::getInstance()->getGuidByName(_guid, name) && removeStatements(_guid, channelId);
}

uint32_t IOBan::getNotationsCount(uint32_t account, uint32_t playerId /* = 0*/) const
{
	DBResultPtr result;

	std::ostringstream query;
	query << "SELECT COUNT(`id`) AS `count` FROM `bans` WHERE `value` = " << account;
	if (playerId > 0) {
		query << " AND `param` = " << playerId;
	}

	query << " AND `type` = " << static_cast<int>(BAN_NOTATION) << " AND `active` = 1";
	if (!(result = g_database.storeQuery(query.str()))) {
		return 0;
	}

	const uint32_t count = result->getNumber<int32_t>("count");
	return count;
}

uint32_t IOBan::getStatementsCount(uint32_t playerId, int16_t channelId /* = -1*/) const
{
	DBResultPtr result;

	std::ostringstream query;
	query << "SELECT COUNT(`id`) AS `count` FROM `bans` WHERE `value` = " << playerId;
	if (channelId >= 0) {
		query << " AND `param` = " << channelId;
	}

	query << " AND `type` = " << static_cast<int>(BAN_STATEMENT) << " AND `active` = 1";
	if (!(result = g_database.storeQuery(query.str()))) {
		return 0;
	}

	const uint32_t count = result->getNumber<int32_t>("count");
	return count;
}

uint32_t IOBan::getStatementsCount(std::string name, int16_t channelId /* = -1*/) const
{
	uint32_t _guid;
	if (!IOLoginData::getInstance()->getGuidByName(_guid, name)) {
		return 0;
	}

	return getStatementsCount(_guid, channelId);
}

bool IOBan::getData(Ban& ban) const
{
	std::ostringstream query;

	query << "SELECT * FROM `bans` WHERE `value` = " << ban.value;
	if (ban.param) {
		query << " AND `param` = " << ban.param;
	}

	if (ban.type != BAN_NONE) {
		query << " AND `type` = " << ban.type;
	}

	query << " AND `active` = 1 AND (`expires` > " << time(nullptr) << " OR `expires` <= 0) LIMIT 1";
	DBResultPtr result;
	if (!(result = g_database.storeQuery(query.str()))) {
		return false;
	}

	ban.id = result->getNumber<int32_t>("id");
	ban.type = static_cast<Ban_t>(result->getNumber<int32_t>("type"));
	ban.value = result->getNumber<int32_t>("value");
	ban.param = result->getNumber<int32_t>("param");
	ban.expires = result->getNumber<uint64_t>("expires");
	ban.added = result->getNumber<uint64_t>("added");
	ban.adminId = result->getNumber<int32_t>("admin_id");
	ban.comment = result->getString("comment");
	ban.reason = result->getNumber<int32_t>("reason");
	ban.action = static_cast<ViolationAction_t>(result->getNumber<uint8_t>("action"));
	ban.statement = result->getString("statement");
	return true;
}

BansVec IOBan::getList(Ban_t type, uint32_t value /* = 0*/, uint32_t param /* = 0*/)
{
	DBResultPtr result;

	std::ostringstream query;
	query << "SELECT * FROM `bans` WHERE ";
	if (value) {
		query << "`value` = " << value << " AND ";
	}

	if (param) {
		query << "`param` = " << param << " AND ";
	}

	query << "`type` = " << type << " AND `active` = 1 AND (`expires` > " << time(nullptr) << " OR `expires` <= 0)";
	BansVec data;
	if ((result = g_database.storeQuery(query.str()))) {
		Ban tmp;
		do {
			tmp.id = result->getNumber<int32_t>("id");
			tmp.type = static_cast<Ban_t>(result->getNumber<int32_t>("type"));
			tmp.value = result->getNumber<int32_t>("value");
			tmp.param = result->getNumber<int32_t>("param");
			tmp.expires = result->getNumber<uint64_t>("expires");
			tmp.added = result->getNumber<uint64_t>("added");
			tmp.adminId = result->getNumber<int32_t>("admin_id");
			tmp.comment = result->getString("comment");
			tmp.reason = result->getNumber<int32_t>("reason");
			tmp.action = static_cast<ViolationAction_t>(result->getNumber<uint8_t>("action"));
			tmp.statement = result->getString("statement");
			data.push_back(tmp);
		} while (result->next());
	}
	return data;
}

bool IOBan::clearTemporials() const
{
	std::ostringstream query;
	query << "UPDATE `bans` SET `active` = 0 WHERE `expires` <= " << time(nullptr) << " AND `expires` >= 0 AND `active` = 1 LIMIT 1";
	return g_database.executeQuery(query.str());
}
