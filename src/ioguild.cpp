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

#include "ioguild.h"

#include "chat.h"
#include "configmanager.h"
#include "database.h"
#include "game.h"
#include "player.h"

bool IOGuild::getGuildId(uint32_t& id, const std::string& name)
{
	DBResultPtr result;

	std::ostringstream query;
	query << "SELECT `id` FROM `guilds` WHERE `name` = " << g_database.escapeString(name) << " LIMIT 1";
	if (!(result = g_database.storeQuery(query.str()))) {
		return false;
	}

	id = result->getNumber<int32_t>("id");
	return true;
}

bool IOGuild::getGuildById(std::string& name, uint32_t id)
{
	DBResultPtr result;

	std::ostringstream query;
	query << "SELECT `name` FROM `guilds` WHERE `id` = " << id << " LIMIT 1";
	if (!(result = g_database.storeQuery(query.str()))) {
		return false;
	}

	name = result->getString("name");
	return true;
}

bool IOGuild::swapGuildIdToOwner(uint32_t& value)
{
	DBResultPtr result;

	std::ostringstream query;
	query << "SELECT `ownerid` FROM `guilds` WHERE `id` = " << value << " LIMIT 1";
	if (!(result = g_database.storeQuery(query.str()))) {
		return false;
	}

	value = result->getNumber<int32_t>("ownerid");
	return true;
}

bool IOGuild::guildExists(uint32_t guild)
{
	DBResultPtr result;

	std::ostringstream query;
	query << "SELECT `id` FROM `guilds` WHERE `id` = " << guild << " LIMIT 1";
	if (!(result = g_database.storeQuery(query.str()))) {
		return false;
	}

	return true;
}

uint32_t IOGuild::getRankIdByName(uint32_t guild, const std::string& name)
{
	DBResultPtr result;

	std::ostringstream query;
	query << "SELECT `id` FROM `guild_ranks` WHERE `guild_id` = " << guild << " AND `name` = " << g_database.escapeString(name) << " LIMIT 1";
	if (!(result = g_database.storeQuery(query.str()))) {
		return 0;
	}

	const uint32_t id = result->getNumber<int32_t>("id");
	return id;
}

uint32_t IOGuild::getRankIdByLevel(uint32_t guild, GuildLevel_t level)
{
	DBResultPtr result;

	std::ostringstream query;
	query << "SELECT `id` FROM `guild_ranks` WHERE `guild_id` = " << guild << " AND `level` = " << level << " LIMIT 1";
	if (!(result = g_database.storeQuery(query.str()))) {
		return 0;
	}

	const uint32_t id = result->getNumber<int32_t>("id");
	return id;
}

bool IOGuild::getRankEx(uint32_t& id, std::string& name, uint32_t guild, GuildLevel_t level)
{
	DBResultPtr result;

	std::ostringstream query;
	query << "SELECT `id`, `name` FROM `guild_ranks` WHERE `guild_id` = " << guild << " AND `level` = " << level;
	if (id) {
		query << " AND `id` = " << id;
	}

	query << " LIMIT 1";
	if (!(result = g_database.storeQuery(query.str()))) {
		return false;
	}

	name = result->getString("name");
	if (!id) {
		id = result->getNumber<int32_t>("id");
	}

	return true;
}

std::string IOGuild::getRank(uint32_t guid)
{
	DBResultPtr result;

	std::ostringstream query;
	query << "SELECT `guild_ranks`.`name` FROM `players`, `guild_ranks` WHERE `players`.`id` = " << guid << " AND `guild_ranks`.`id` = `players`.`rank_id` LIMIT 1";
	if (!(result = g_database.storeQuery(query.str()))) {
		return "";
	}

	const std::string name = result->getString("name");
	return name;
}

bool IOGuild::changeRank(uint32_t guild, const std::string& oldName, const std::string& newName)
{
	DBResultPtr result;

	std::ostringstream query;
	query << "SELECT `id` FROM `guild_ranks` WHERE `guild_id` = " << guild << " AND `name` = " << g_database.escapeString(oldName) << " LIMIT 1";
	if (!(result = g_database.storeQuery(query.str()))) {
		return false;
	}

	const uint32_t id = result->getNumber<int32_t>("id");

	query.str("");
	query << "UPDATE `guild_ranks` SET `name` = " << g_database.escapeString(newName) << " WHERE `id` = " << id << " LIMIT 1;";
	if (!g_database.executeQuery(query.str())) {
		return false;
	}

	for (const auto& it : g_game.getPlayers()) {
		if (it.second->getRankId() == id) {
			it.second->setRankName(newName);
		}
	}
	return true;
}

bool IOGuild::createGuild(Player* player)
{
	DBResultPtr result;

	std::ostringstream query;
	query << "INSERT INTO `guilds` (`id`, `name`, `ownerid`, `creationdata`, `motd`, `checkdata`) VALUES (NULL, " << g_database.escapeString(player->getGuildName()) << ", " << player->getGUID() << ", " << time(nullptr) << ", 'Your guild has been successfully created, to view all available commands type: !commands. If you would like to remove this message use !cleanmotd and to set new motd use !setmotd text.', 0)";
	if (!g_database.executeQuery(query.str())) {
		return false;
	}

	query.str("");
	query << "SELECT `id` FROM `guilds` WHERE `ownerid` = " << player->getGUID() << " LIMIT 1";
	if (!(result = g_database.storeQuery(query.str()))) {
		return false;
	}

	const uint32_t guildId = result->getNumber<int32_t>("id");
	return joinGuild(player, guildId, true);
}

bool IOGuild::joinGuild(Player* player, uint32_t guildId, bool creation /* = false*/)
{
	DBResultPtr result;

	std::ostringstream query;
	query << "SELECT `id` FROM `guild_ranks` WHERE `guild_id` = " << guildId << " AND `level` = " << (creation ? "3" : "1") << " LIMIT 1";
	if (!(result = g_database.storeQuery(query.str()))) {
		return false;
	}

	const uint32_t rankId = result->getNumber<int32_t>("id");

	std::string guildName;
	if (!creation) {
		query.str("");
		query << "SELECT `name` FROM `guilds` WHERE `id` = " << guildId << " LIMIT 1";
		if (!(result = g_database.storeQuery(query.str()))) {
			return false;
		}

		guildName = result->getString("name");
	}

	query.str("");
	query << "UPDATE `players` SET `rank_id` = " << rankId << " WHERE `id` = " << player->getGUID() << " LIMIT 1;";
	if (!g_database.executeQuery(query.str())) {
		return false;
	}

	player->setGuildId(guildId);
	GuildLevel_t level = GUILDLEVEL_MEMBER;
	if (!creation) {
		player->setGuildName(guildName);
	} else {
		level = GUILDLEVEL_LEADER;
	}

	player->setGuildLevel(level, rankId);
	player->invitationsList.clear();
	return true;
}

bool IOGuild::disbandGuild(uint32_t guildId)
{
	std::ostringstream query;
	query << "UPDATE `players` SET `rank_id` = '' AND `guildnick` = '' WHERE `rank_id` = " << getRankIdByLevel(guildId, GUILDLEVEL_LEADER) << " OR rank_id = " << getRankIdByLevel(guildId, GUILDLEVEL_VICE) << " OR rank_id = " << getRankIdByLevel(guildId, GUILDLEVEL_MEMBER);
	if (!g_database.executeQuery(query.str())) {
		return false;
	}

	InvitationsList::iterator iit;
	for (const auto& it : g_game.getPlayers()) {
		if (it.second->getGuildId() == guildId) {
			it.second->leaveGuild();
		} else if ((iit = std::find(it.second->invitationsList.begin(), it.second->invitationsList.end(), guildId)) != it.second->invitationsList.end()) {
			it.second->invitationsList.erase(iit);
		}
	}

	query.str("");
	query << "DELETE FROM `guilds` WHERE `id` = " << guildId << " LIMIT 1;";
	if (!g_database.executeQuery(query.str())) {
		return false;
	}

	query.str("");
	query << "DELETE FROM `guild_invites` WHERE `guild_id` = " << guildId;
	if (!g_database.executeQuery(query.str())) {
		return false;
	}

	query.str("");
	query << "DELETE FROM `guild_ranks` WHERE `guild_id` = " << guildId;
	return g_database.executeQuery(query.str());
}

bool IOGuild::hasGuild(uint32_t guid)
{
	DBResultPtr result;

	std::ostringstream query;
	query << "SELECT `rank_id` FROM `players` WHERE `id` = " << guid << " LIMIT 1";
	if (!(result = g_database.storeQuery(query.str()))) {
		return false;
	}

	const bool ret = result->getNumber<int32_t>("rank_id") != 0;
	return ret;
}

bool IOGuild::isInvited(uint32_t guild, uint32_t guid)
{
	DBResultPtr result;

	std::ostringstream query;
	query << "SELECT `guild_id` FROM `guild_invites` WHERE `player_id` = " << guid << " AND `guild_id`= " << guild << " LIMIT 1";
	if (!(result = g_database.storeQuery(query.str()))) {
		return false;
	}

	return true;
}

bool IOGuild::invitePlayer(uint32_t guild, uint32_t guid)
{
	std::ostringstream query;
	query << "INSERT INTO `guild_invites` (`player_id`, `guild_id`) VALUES ('" << guid << "', '" << guild << "')";
	return g_database.executeQuery(query.str());
}

bool IOGuild::revokeInvite(uint32_t guild, uint32_t guid)
{
	std::ostringstream query;
	query << "DELETE FROM `guild_invites` WHERE `player_id` = " << guid << " AND `guild_id` = " << guild;
	return g_database.executeQuery(query.str());
}

uint32_t IOGuild::getGuildId(uint32_t guid)
{
	DBResultPtr result;

	std::ostringstream query;
	query << "SELECT `guild_ranks`.`guild_id` FROM `players`, `guild_ranks` WHERE `players`.`id` = " << guid << " AND `guild_ranks`.`id` = `players`.`rank_id` LIMIT 1";
	if (!(result = g_database.storeQuery(query.str()))) {
		return 0;
	}

	const uint32_t guildId = result->getNumber<int32_t>("guild_id");
	return guildId;
}

GuildLevel_t IOGuild::getGuildLevel(uint32_t guid)
{
	DBResultPtr result;

	std::ostringstream query;
	query << "SELECT `guild_ranks`.`level` FROM `players`, `guild_ranks` WHERE `players`.`id` = " << guid << " AND `guild_ranks`.`id` = `players`.`rank_id` LIMIT 1";
	if (!(result = g_database.storeQuery(query.str()))) {
		return GUILDLEVEL_NONE;
	}

	const GuildLevel_t level = (GuildLevel_t)result->getNumber<int32_t>("level");
	return level;
}

bool IOGuild::setGuildLevel(uint32_t guid, GuildLevel_t level)
{
	DBResultPtr result;

	std::ostringstream query;
	query << "SELECT `id` FROM `guild_ranks` WHERE `guild_id` = " << getGuildId(guid) << " AND `level` = " << level << " LIMIT 1";
	if (!(result = g_database.storeQuery(query.str()))) {
		return false;
	}

	query.str("");
	query << "UPDATE `players` SET `rank_id` = " << result->getNumber<int32_t>("id") << " WHERE `id` = " << guid << " LIMIT 1;";
	return g_database.executeQuery(query.str());
}

bool IOGuild::updateOwnerId(uint32_t guild, uint32_t guid)
{
	std::ostringstream query;
	query << "UPDATE `guilds` SET `ownerid` = " << guid << " WHERE `id` = " << guild << " LIMIT 1;";
	return g_database.executeQuery(query.str());
}

bool IOGuild::setGuildNick(uint32_t guid, const std::string& nick)
{
	std::ostringstream query;
	query << "UPDATE `players` SET `guildnick` = " << g_database.escapeString(nick) << " WHERE `id` = " << guid << " LIMIT 1;";
	return g_database.executeQuery(query.str());
}

bool IOGuild::setMotd(uint32_t guild, const std::string& newMessage)
{
	std::ostringstream query;
	query << "UPDATE `guilds` SET `motd` = " << g_database.escapeString(newMessage) << " WHERE `id` = " << guild << " LIMIT 1;";
	return g_database.executeQuery(query.str());
}

std::string IOGuild::getMotd(uint32_t guild)
{
	DBResultPtr result;

	std::ostringstream query;
	query << "SELECT `motd` FROM `guilds` WHERE `id` = " << guild << " LIMIT 1";
	if (!(result = g_database.storeQuery(query.str()))) {
		return "";
	}

	const std::string motd = result->getString("motd");
	return motd;
}

void IOGuild::checkWars()
{
	if (!g_config.getBool(ConfigManager::EXTERNAL_GUILD_WARS_MANAGEMENT)) {
		return;
	}

	DBResultPtr result;

	std::ostringstream query;

	// I don't know if there any easier way to make it right.
	// If exists other solution just let me know.
	// NOTE:
	// status states 6,7,8,9 are additional only for external management, do not anything in talkaction, those states make possible to manage wars for example from webpage
	// status 6 means accepted invite, it's before proper start of war
	// status 7 means 'mend fences', related to signed an armistice declaration by enemy
	// status 8 means ended up, when guild ended up war without signed an armistice declaration by enemy
	// status 9 means signed an armistice declaration by enemy
	std::ostringstream s;
	uint32_t tmpInterval = (uint32_t)(EVENT_WARSINTERVAL / 1000) + 10; //+10 for sure

	query << "SELECT `g`.`name` as `guild_name`, `e`.`name` as `enemy_name`, `guild_wars`.`frags` as `frags`  FROM `guild_wars` LEFT JOIN `guilds` as `g` ON `guild_wars`.`guild_id` = `g`.`id` LEFT JOIN `guilds` as `e` ON `guild_wars`.`enemy_id` = `e`.`id` WHERE (`begin` > 0 AND (`begin` + " << tmpInterval << ") > UNIX_TIMESTAMP()) AND `status` IN (0, 6)";
	if ((result = g_database.storeQuery(query.str()))) {
		do {
			s << result->getString("guild_name") << " has invited " << result->getString("enemy_name") << " to war till " << result->getNumber<int32_t>("frags") << " frags.";
			g_game.broadcastMessage(s.str().c_str(), MSG_EVENT_ADVANCE);
			s.str("");
		} while (result->next());
	}

	query.str("");
	query << "UPDATE `guild_wars` SET `begin` = UNIX_TIMESTAMP(), `end` = ((`end` - `begin`) + UNIX_TIMESTAMP()), `status` = 1 WHERE `status` = 6";
	g_database.executeQuery(query.str());

	query.str("");
	query << "SELECT `g`.`name` as `guild_name`, `e`.`name` as `enemy_name`, `g`.`id` as `guild_id`, `e`.`id` as `enemy_id`, `guild_wars`.*  FROM `guild_wars` LEFT JOIN `guilds` as `g` ON `guild_wars`.`guild_id` = `g`.`id` LEFT JOIN `guilds` as `e` ON `guild_wars`.`enemy_id` = `e`.`id` WHERE (`begin` > 0 AND (`begin` + " << tmpInterval << ") > UNIX_TIMESTAMP()) AND `status` = 1";
	if ((result = g_database.storeQuery(query.str()))) {
		do {
			s << result->getString("enemy_name") << " accepted " << result->getString("guild_name") << " invitation to war.";
			g_game.broadcastMessage(s.str().c_str(), MSG_EVENT_ADVANCE);
			s.str("");

			War_t tmp;
			tmp.war = result->getNumber<int32_t>("id");
			tmp.ids[WAR_GUILD] = result->getNumber<int32_t>("guild_id");
			tmp.ids[WAR_ENEMY] = result->getNumber<int32_t>("enemy_id");
			for (const auto& it : g_game.getPlayers()) {
				bool update = false;
				if (it.second->getGuildId() == tmp.ids[WAR_GUILD]) {
					tmp.type = WAR_ENEMY;
					it.second->addEnemy(tmp.ids[WAR_ENEMY], tmp);
					update = true;
				} else if (it.second->getGuildId() == tmp.ids[WAR_ENEMY]) {
					tmp.type = WAR_ENEMY;
					it.second->addEnemy(tmp.ids[WAR_GUILD], tmp);
					update = true;
				}

				if (update) {
					g_game.updateCreatureEmblem(it.second);
				}
			}
		} while (result->next());
	}

	query.str("");

	query << "SELECT `g`.`name` as `guild_name`, `e`.`name` as `enemy_name`  FROM `guild_wars` LEFT JOIN `guilds` as `g` ON `guild_wars`.`guild_id` = `g`.`id` LEFT JOIN `guilds` as `e` ON `guild_wars`.`enemy_id` = `e`.`id` WHERE (`end` > 0 AND (`end` + " << tmpInterval << ") > UNIX_TIMESTAMP()) AND `status` = 2";
	if ((result = g_database.storeQuery(query.str()))) {
		do {
			s << result->getString("enemy_name") << " rejected " << result->getString("guild_name") << " invitation to war.";
			g_game.broadcastMessage(s.str().c_str(), MSG_EVENT_ADVANCE);
			s.str("");
		} while (result->next());
	}

	query.str("");
	query << "SELECT `g`.`name` as `guild_name`, `e`.`name` as `enemy_name`  FROM `guild_wars` LEFT JOIN `guilds` as `g` ON `guild_wars`.`guild_id` = `g`.`id` LEFT JOIN `guilds` as `e` ON `guild_wars`.`enemy_id` = `e`.`id` WHERE (`end` > 0 AND (`end` + " << tmpInterval << ") > UNIX_TIMESTAMP()) AND `status` = 3";
	if ((result = g_database.storeQuery(query.str()))) {
		do {
			s << result->getString("guild_name") << " canceled invitation to a war with " << result->getString("enemy_name") << ".";
			g_game.broadcastMessage(s.str().c_str(), MSG_EVENT_ADVANCE);
			s.str("");
		} while (result->next());
	}

	query.str("");
	query << "SELECT `g`.`name` as `guild_name`, `e`.`name` as `enemy_name`, `guild_wars`.`status` as `status`, `g`.`id` as `guild_id`, `e`.`id` as `enemy_id`, `guild_wars`.*   FROM `guild_wars` LEFT JOIN `guilds` as `g` ON `guild_wars`.`guild_id` = `g`.`id` LEFT JOIN `guilds` as `e` ON `guild_wars`.`enemy_id` = `e`.`id` WHERE (`end` > 0 AND (`end` + " << tmpInterval << ") > UNIX_TIMESTAMP()) AND `status` IN (7,8)";
	if ((result = g_database.storeQuery(query.str()))) {
		do {
			if (result->getNumber<int32_t>("status") == 7) {
				s << result->getString("guild_name") << " has mend fences with " << result->getString("enemy_name") << ".";
			} else {
				s << result->getString("guild_name") << " has ended up a war with " << result->getString("enemy_name") << ".";
			}

			War_t tmp;
			tmp.war = result->getNumber<int32_t>("id");
			tmp.ids[WAR_GUILD] = result->getNumber<int32_t>("guild_id");
			tmp.ids[WAR_ENEMY] = result->getNumber<int32_t>("enemy_id");
			for (const auto& it : g_game.getPlayers()) {
				bool update = false;
				if (it.second->getGuildId() == tmp.ids[WAR_GUILD]) {
					it.second->removeEnemy(tmp.ids[WAR_ENEMY]);
					update = true;
				} else if (it.second->getGuildId() == tmp.ids[WAR_ENEMY]) {
					it.second->removeEnemy(tmp.ids[WAR_GUILD]);
					update = true;
				}

				if (update) {
					g_game.updateCreatureEmblem(it.second);
				}
			}

			g_game.broadcastMessage(s.str().c_str(), MSG_EVENT_ADVANCE);
			s.str("");
		} while (result->next());
	}

	query.str("");

	query << "UPDATE `guild_wars` SET `end` = UNIX_TIMESTAMP(), `status` = 5 WHERE `status` IN (7,8)";
	g_database.executeQuery(query.str());

	query.str("");
}

void IOGuild::checkEndingWars()
{
	DBResultPtr result;

	std::ostringstream query;

	query << "SELECT `id`, `guild_id`, `enemy_id` FROM `guild_wars` WHERE `status` IN (1,4) AND `end` > 0 AND `end` < " << time(nullptr);
	if (!(result = g_database.storeQuery(query.str()))) {
		return;
	}

	War_t tmp;
	do {
		tmp.war = result->getNumber<int32_t>("id");
		tmp.ids[WAR_GUILD] = result->getNumber<int32_t>("guild_id");
		tmp.ids[WAR_ENEMY] = result->getNumber<int32_t>("enemy_id");
		finishWar(tmp, false);
	}

	while (result->next());
}

bool IOGuild::updateWar(War_t& war)
{
	DBResultPtr result;

	std::ostringstream query;
	query << "SELECT `g`.`name` AS `guild_name`, `e`.`name` AS `enemy_name`, `w`.* FROM `guild_wars` w LEFT JOIN `guilds` g ON `w`.`guild_id` = `g`.`id` LEFT JOIN `guilds` e ON `w`.`enemy_id` = `e`.`id` WHERE `w`.`id` = " << war.war;
	if (!(result = g_database.storeQuery(query.str()))) {
		return false;
	}

	war.ids[WAR_GUILD] = result->getNumber<int32_t>("guild_id");
	war.ids[WAR_ENEMY] = result->getNumber<int32_t>("enemy_id");
	war.names[WAR_GUILD] = result->getString("guild_name");
	war.names[WAR_ENEMY] = result->getString("enemy_name");

	war.frags[WAR_GUILD] = result->getNumber<int32_t>("guild_kills");
	war.frags[WAR_ENEMY] = result->getNumber<int32_t>("enemy_kills");
	war.frags[war.type]++;

	war.limit = result->getNumber<int32_t>("frags");
	war.payment = result->getNumber<int32_t>("payment");

	if (war.frags[WAR_GUILD] >= war.limit || war.frags[WAR_ENEMY] >= war.limit) {
		addSchedulerTask(1000, ([this, war]() { finishWar(war, true); }));
		return true;
	}

	query.str("");
	query << "UPDATE `guild_wars` SET `guild_kills` = " << war.frags[WAR_GUILD] << ", `enemy_kills` = " << war.frags[WAR_ENEMY] << " WHERE `id` = " << war.war;
	return g_database.executeQuery(query.str());
}

void IOGuild::finishWar(War_t war, bool finished)
{
	std::ostringstream query;
	if (finished) {
		query << "UPDATE `guilds` SET `balance` = `balance` + " << (war.payment << 1) << " WHERE `id` = " << war.ids[war.type];
		if (!g_database.executeQuery(query.str())) {
			return;
		}

		query.str("");
	}

	query << "UPDATE `guild_wars` SET ";
	if (finished) {
		query << "`guild_kills` = " << war.frags[WAR_GUILD] << ", `enemy_kills` = " << war.frags[WAR_ENEMY] << ",";
	}

	query << "`end` = " << time(nullptr) << ", `status` = 5 WHERE `id` = " << war.war;
	if (!g_database.executeQuery(query.str())) {
		return;
	}

	for (const auto& it : g_game.getPlayers()) {
		bool update = false;
		if (it.second->getGuildId() == war.ids[WAR_GUILD]) {
			it.second->removeEnemy(war.ids[WAR_ENEMY]);
			update = true;
		} else if (it.second->getGuildId() == war.ids[WAR_ENEMY]) {
			it.second->removeEnemy(war.ids[WAR_GUILD]);
			update = true;
		}

		if (update) {
			g_game.updateCreatureEmblem(it.second);
		}
	}

	if (finished) {
		std::ostringstream s;
		s << war.names[war.type] << " has just won the war against " << war.names[war.type == WAR_GUILD] << ".";
		g_game.broadcastMessage(s.str().c_str(), MSG_EVENT_ADVANCE);
	}
}

void IOGuild::frag(Player* player, uint64_t deathId, const DeathList& list, bool score)
{
	War_t war;
	std::ostringstream s;
	for (DeathList::const_iterator it = list.begin(); it != list.end();) {
		if (score) {
			if (it->isLast()) {
				war = it->getWar();
			}
		} else if (!war.war) {
			war = it->getWar();
		}

		Creature* creature = it->getKillerCreature();
		if (it != list.begin()) {
			++it;
			if (it == list.end()) {
				s << " and ";
			} else {
				s << ", ";
			}
		} else {
			++it;
		}

		s << creature->getName();
	}

	if (!war.ids[war.type]) {
#ifdef __DEBUG__
		std::clog << "[Notice - IOGuild::frag] Unable to attach war frag to player " << player->getName() << "." << std::endl;
#endif
		return;
	}

	std::string killers = s.str();
	s.str("");

	ChatChannel* channel = nullptr;
	if ((channel = g_chat.getChannel(player, CHANNEL_GUILD))) {
		s << "Guild member " << player->getName() << " was killed by " << killers << ".";
		if (score) {
			s << " The new score is " << war.frags[war.type == WAR_GUILD] << ":"
			  << war.frags[war.type] << " frags (limit " << war.limit << ").";
		}

		channel->talk("", MSG_GAMEMASTER_CHANNEL, s.str());
	}

	s.str("");
	if ((channel = g_chat.getChannel(list[0].getKillerCreature()->getPlayer(), CHANNEL_GUILD))) {
		s << "Opponent " << player->getName() << " was killed by " << killers << ".";
		if (score) {
			s << " The new score is " << war.frags[war.type] << ":"
			  << war.frags[war.type == WAR_GUILD] << " frags (limit " << war.limit << ").";
		}

		channel->talk("", MSG_CHANNEL_HIGHLIGHT, s.str());
	}

	std::ostringstream query;

	query << "INSERT INTO `guild_kills` (`guild_id`, `war_id`, `death_id`) VALUES ("
		  << war.ids[war.type] << ", " << war.war << ", " << deathId << ");";
	g_database.executeQuery(query.str());
}
