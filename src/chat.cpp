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

#include "chat.h"

#include "configmanager.h"
#include "game.h"
#include "ioguild.h"
#include "iologindata.h"
#include "player.h"

#include "otx/util.hpp"

#include <fstream>

Chat g_chat;

namespace
{
	std::string PARTY_NAME = "Party";

} // namespace

uint16_t ChatChannel::staticFlags = CHANNELFLAG_ENABLED | CHANNELFLAG_ACTIVE;

bool PrivateChatChannel::isInvited(const Player* player) const
{
	if (player->getGUID() == m_owner) {
		return true;
	}
	return std::find(m_invites.begin(), m_invites.end(), player->getGUID()) != m_invites.end();
}

bool PrivateChatChannel::addInvited(Player* player)
{
	if (std::find(m_invites.begin(), m_invites.end(), player->getGUID()) != m_invites.end()) {
		return false;
	}

	m_invites.push_back(player->getGUID());
	return true;
}

bool PrivateChatChannel::removeInvited(Player* player)
{
	auto it = std::find(m_invites.begin(), m_invites.end(), player->getGUID());
	if (it == m_invites.end()) {
		return false;
	}

	m_invites.erase(it);
	return true;
}

void PrivateChatChannel::invitePlayer(Player* player, Player* invitePlayer)
{
	if (player == invitePlayer || !addInvited(invitePlayer)) {
		return;
	}

	std::ostringstream msg;
	msg << player->getName() << " invites you to " << (player->getSex(false) ? "his" : "her") << " private chat channel.";
	invitePlayer->sendTextMessage(MSG_INFO_DESCR, msg.str());

	msg.str("");
	msg << invitePlayer->getName() << " has been invited.";
	player->sendTextMessage(MSG_INFO_DESCR, msg.str());
}

void PrivateChatChannel::excludePlayer(Player* player, Player* excludePlayer)
{
	if (player == excludePlayer || !removeInvited(excludePlayer)) {
		return;
	}

	std::string msg = excludePlayer->getName();
	msg += " has been excluded.";
	player->sendTextMessage(MSG_INFO_DESCR, msg);

	removeUser(excludePlayer, true);
	excludePlayer->sendClosePrivate(getId());
}

void PrivateChatChannel::closeChannel()
{
	for (const auto& it : m_users) {
		it.second->sendClosePrivate(m_id);
	}
}

ChatChannel::ChatChannel(uint16_t id, const std::string& name, uint16_t flags) :
	m_id(id),
	m_name(name),
	m_flags(flags)
{
	if (hasFlag(CHANNELFLAG_LOGGED)) {
		m_file.reset(new std::ofstream(getFilePath(FILE_TYPE_LOG, "chat/" + otx::config::getString(otx::config::PREFIX_CHANNEL_LOGS) + m_name + ".log").c_str(), std::ios::app | std::ios::out));
		if (!m_file->is_open()) {
			m_flags &= ~CHANNELFLAG_LOGGED;
		}
	}
}

bool ChatChannel::addUser(Player* player)
{
	if (!player) {
		return false;
	}

	if (m_users.find(player->getID()) != m_users.end()) {
		return true;
	}

	ChatChannel* channel = g_chat.getChannel(player, m_id);
	if (!channel) {
		return false;
	}

	m_users[player->getID()] = player;
	if (player->hasEventRegistered(CREATURE_EVENT_CHANNEL_JOIN)) {
		for (CreatureEvent* it : player->getCreatureEvents(CREATURE_EVENT_CHANNEL_JOIN)) {
			it->executeChannel(player, m_id, m_users);
		}
	}
	return true;
}

bool ChatChannel::removeUser(Player* player, bool /* exclude = false*/)
{
	if (!player) {
		return false;
	}

	auto it = m_users.find(player->getID());
	if (it == m_users.end()) {
		return true;
	}

	m_users.erase(it);
	if (player->hasEventRegistered(CREATURE_EVENT_CHANNEL_LEAVE)) {
		for (CreatureEvent* creatureEvent : player->getCreatureEvents(CREATURE_EVENT_CHANNEL_LEAVE)) {
			creatureEvent->executeChannel(player, m_id, m_users);
		}
	}
	return true;
}

bool ChatChannel::hasUser(Player* player) const
{
	return player && m_users.find(player->getID()) != m_users.end();
}

bool ChatChannel::talk(Player* player, MessageType_t type, const std::string& text, uint32_t statementId, bool fakeChat /*= false*/)
{
	auto it = m_users.find(player->getID());
	if (it == m_users.end()) {
		return false;
	}

	MessageType_t ntype = type;
	uint16_t channelId = getId();
	if (channelId == 2 || channelId == 6 || channelId == 9) {
		if (player->getGroupId() >= 5) {
			ntype = MSG_GAMEMASTER_CHANNEL;
		} else if (player->getGroupId() >= 2) {
			ntype = MSG_CHANNEL_HIGHLIGHT;
		}
	}

	if (m_condition && !player->hasFlag(PlayerFlag_CannotBeMuted)) {
		if (Condition* condition = m_condition->clone()) {
			player->addCondition(condition);
		}
	}

	for (it = m_users.begin(); it != m_users.end(); ++it) {
		if (fakeChat && it->second->getIP() != player->getIP()) { // if fake chat, send only to player, not in others screen
			continue;
		}
		it->second->sendCreatureChannelSay(player, ntype, text, m_id, statementId, fakeChat);
	}

	if (hasFlag(CHANNELFLAG_LOGGED) && m_file->is_open()) {
		*m_file << "[" << formatDate() << "] " << player->getName() << ": " << text << std::endl;
	}
	return true;
}

bool ChatChannel::talk(std::string nick, MessageType_t type, const std::string& text, bool fakeChat, uint32_t ip)
{
	for (const auto& it : m_users) {
		it.second->sendChannelMessage(nick, text, type, m_id, fakeChat, ip);
	}

	if (hasFlag(CHANNELFLAG_LOGGED) && m_file->is_open()) {
		*m_file << "[" << formatDate() << "] " << nick << ": " << text << std::endl;
	}
	return true;
}

void Chat::clear()
{
	m_normalChannels.clear();
	m_partyChannels.clear();
	m_privateChannels.clear();

	m_dummyPrivate.reset();
}

bool Chat::reload()
{
	clear();
	return loadFromXml();
}

bool Chat::loadFromXml()
{
	xmlDocPtr doc = xmlParseFile(getFilePath(FILE_TYPE_XML, "channels.xml").c_str());
	if (!doc) {
		std::clog << "[Warning - Chat::loadFromXml] Cannot load channels file." << std::endl;
		std::clog << getLastXMLError() << std::endl;
		return false;
	}

	xmlNodePtr root = xmlDocGetRootElement(doc);
	if (xmlStrcmp(root->name, reinterpret_cast<const xmlChar*>("channels"))) {
		std::clog << "[Error - Chat::loadFromXml] Malformed channels file" << std::endl;
		xmlFreeDoc(doc);
		return false;
	}

	for (xmlNodePtr p = root->children; p; p = p->next) {
		parseChannelNode(p);
	}

	xmlFreeDoc(doc);
	return true;
}

bool Chat::parseChannelNode(xmlNodePtr p)
{
	if (xmlStrcmp(p->name, reinterpret_cast<const xmlChar*>("channel"))) {
		return false;
	}

	int32_t intValue;
	if (!readXMLInteger(p, "id", intValue) || intValue <= CHANNEL_GUILD) {
		std::clog << "[Warning - Chat::loadFromXml] Invalid or not specified channel id." << std::endl;
		return false;
	}

	const uint16_t id = intValue;
	if (m_normalChannels.find(id) != m_normalChannels.end()) {
		std::clog << "[Warning - Chat::loadFromXml] Duplicated channel with id: " << id << "." << std::endl;
		return false;
	}

	std::string strValue;
	if (!readXMLString(p, "name", strValue)) {
		std::clog << "[Warning - Chat::loadFromXml] Missing name for channel with id: " << id << "." << std::endl;
		return false;
	}

	std::string name = strValue;
	uint16_t flags = ChatChannel::staticFlags;
	if (readXMLString(p, "enabled", strValue) && !booleanString(strValue)) {
		flags &= ~CHANNELFLAG_ENABLED;
	}

	if (readXMLString(p, "active", strValue) && !booleanString(strValue)) {
		flags &= ~CHANNELFLAG_ACTIVE;
	}

	if ((readXMLString(p, "logged", strValue) || readXMLString(p, "log", strValue)) && booleanString(strValue)) {
		flags |= CHANNELFLAG_LOGGED;
	}

	uint32_t access = 0;
	if (readXMLInteger(p, "access", intValue)) {
		access = intValue;
	}

	uint32_t level = 1;
	if (readXMLInteger(p, "level", intValue)) {
		level = intValue;
	}

	int32_t conditionId = -1;
	std::string conditionMessage = "You are muted.";

	std::unique_ptr<Condition> channelCondition;
	if (readXMLInteger(p, "muted", intValue)) {
		conditionId = 3;
		const int32_t ticks = intValue * 1000;
		if (readXMLInteger(p, "conditionId", intValue)) {
			conditionId = intValue;
			if (conditionId < 3) {
				std::clog << "[Warning - Chat::parseChannelNode] Using reserved muted condition sub id (" << conditionId << ")" << std::endl;
			}
		}

		if (readXMLString(p, "conditionMessage", strValue)) {
			conditionMessage = strValue;
		}

		channelCondition.reset(Condition::createCondition(CONDITIONID_DEFAULT, CONDITION_MUTED, ticks, 0, false, conditionId));
		if (!channelCondition) {
			conditionId = -1;
		}
	}

	std::vector<std::string> vocStringVec;
	VocationMap vocMap;

	std::string error;
	for (xmlNodePtr tmpNode = p->children; tmpNode; tmpNode = tmpNode->next) {
		if (!parseVocationNode(tmpNode, vocMap, vocStringVec, error)) {
			std::clog << "[Warning - Chat::loadFromXml] " << error << std::endl;
		}
	}

	switch (id) {
		case CHANNEL_PARTY: {
			PARTY_NAME = std::move(name);
			break;
		}
		case CHANNEL_PRIVATE: {
			m_dummyPrivate.reset(new PrivateChatChannel(CHANNEL_PRIVATE, std::move(name), flags));
			break;
		}

		default: {
			auto& addedChannel = m_normalChannels.emplace(id, ChatChannel(id, std::move(name), flags)).first->second;
			addedChannel.setLevel(level);
			addedChannel.setAccess(access);
			addedChannel.setCondition(channelCondition.release());
			addedChannel.setConditionId(conditionId);
			addedChannel.setConditionMessage(conditionMessage);
			if (!vocMap.empty()) {
				addedChannel.setVocationMap(new VocationMap(vocMap));
			}
			break;
		}
	}
	return true;
}

ChatChannel* Chat::createChannel(Player* player, uint16_t channelId)
{
	if (!player || player->isRemoved() || getChannel(player, channelId)) {
		return nullptr;
	}

	switch (channelId) {
		case CHANNEL_GUILD: {
			auto ret = m_guildChannels.emplace(player->getGuildId(), ChatChannel(channelId, player->getGuildName(), ChatChannel::staticFlags));
			return &ret.first->second;
		}
		case CHANNEL_PARTY: {
			if (Party* party = player->getParty()) {
				auto ret = m_partyChannels.emplace(party, ChatChannel(channelId, PARTY_NAME, ChatChannel::staticFlags));
				return &ret.first->second;
			}
			break;
		}
		case CHANNEL_PRIVATE: {
			// only 1 private channel for each premium player
			if (getPrivateChannel(player)) {
				break;
			}

			// find a free private channel slot
			for (uint16_t i = 100; i < 10000; ++i) {
				auto it = m_privateChannels.find(i);
				if (it == m_privateChannels.end()) {
					const uint16_t flags = (m_dummyPrivate ? m_dummyPrivate->getFlags() : 0);
					auto ret = m_privateChannels.emplace(i, PrivateChatChannel(i, player->getName() + "'s Channel", flags));
					auto& newChannel = ret.first->second;
					newChannel.setOwner(player->getGUID());
					return &newChannel;
				}
			}
			break;
		}

		default:
			break;
	}
	return nullptr;
}

bool Chat::deleteChannel(Player* player, uint16_t channelId)
{
	switch (channelId) {
		case CHANNEL_GUILD: {
			auto it = m_guildChannels.find(player->getGuildId());
			if (it == m_guildChannels.end()) {
				return false;
			}

			m_guildChannels.erase(it);
			return true;
		}
		case CHANNEL_PARTY: {
			auto it = m_partyChannels.find(player->getParty());
			if (it == m_partyChannels.end()) {
				return false;
			}

			m_partyChannels.erase(it);
			return true;
		}

		default: {
			auto it = m_privateChannels.find(channelId);
			if (it == m_privateChannels.end()) {
				return false;
			}

			it->second.closeChannel();
			m_privateChannels.erase(it);
			return true;
		}
	}
}

ChatChannel* Chat::addUserToChannel(Player* player, uint16_t channelId)
{
	ChatChannel* channel = getChannel(player, channelId);
	if (channel && channel->addUser(player)) {
		return channel;
	}
	return nullptr;
}

void Chat::reOpenChannels(Player* player)
{
	if (!player || player->isRemoved()) {
		return;
	}

	for (const auto& it : m_normalChannels) {
		if (it.second.hasUser(player)) {
			player->sendChannel(it.second.getId(), it.second.getName());
		}
	}

	for (const auto& it : m_partyChannels) {
		if (it.second.hasUser(player)) {
			player->sendChannel(it.second.getId(), it.second.getName());
		}
	}

	for (const auto& it : m_guildChannels) {
		if (it.second.hasUser(player)) {
			player->sendChannel(it.second.getId(), it.second.getName());
		}
	}

	for (const auto& it : m_privateChannels) {
		if (it.second.hasUser(player)) {
			player->sendChannel(it.second.getId(), it.second.getName());
		}
	}
}

bool Chat::removeUserFromChannel(Player* player, uint16_t channelId)
{
	ChatChannel* channel = getChannel(player, channelId);
	if (!channel || !channel->removeUser(player)) {
		return false;
	}

	if (channel->getOwner() == player->getGUID()) {
		deleteChannel(player, channelId);
	}
	return true;
}

void Chat::removeUserFromChannels(Player* player)
{
	if (!player || player->isRemoved()) {
		return;
	}

	for (auto& it : m_normalChannels) {
		it.second.removeUser(player);
	}

	for (auto& it : m_partyChannels) {
		it.second.removeUser(player);
	}

	for (auto& it : m_guildChannels) {
		it.second.removeUser(player);
	}

	const uint32_t playerGUID = player->getGUID();
	uint16_t channelToDelete = 0;
	for (auto& it : m_privateChannels) {
		it.second.removeUser(player);
		if (channelToDelete == 0 && it.second.getOwner() == playerGUID) {
			channelToDelete = it.second.getId();
		}
	}

	if (channelToDelete != 0) {
		deleteChannel(player, channelToDelete);
	}
}

bool Chat::talk(Player* player, MessageType_t type, const std::string& text, uint16_t channelId,
	uint32_t statementId, bool anonymous /* = false*/, bool fakeChat /*= false*/)
{
	if (text.empty()) {
		return false;
	}

	ChatChannel* channel = getChannel(player, channelId);
	if (!channel) {
		return false;
	}

	if (!player->hasFlag(PlayerFlag_CannotBeMuted)) {
		if (!channel->hasFlag(CHANNELFLAG_ACTIVE)) {
			player->sendTextMessage(MSG_STATUS_SMALL, "You may not speak into this channel.");
			return true;
		}

		if (player->getLevel() < channel->getLevel()) {
			char buffer[100];
			sprintf(buffer, "You may not speak into this channel as long as you are on level %d.", channel->getLevel());
			player->sendCancel(buffer);
			return true;
		}

		if (channel->getConditionId() >= 0 && player->hasCondition(CONDITION_MUTED, channel->getConditionId())) {
			player->sendCancel(channel->getConditionMessage());
			return true;
		}
	}

	if (channelId != CHANNEL_GUILD || !otx::config::getBoolean(otx::config::INGAME_GUILD_MANAGEMENT)
		|| (text[0] != '!' && text[0] != '/')) {
		if (channelId == CHANNEL_GUILD) {
			switch (player->getGuildLevel()) {
				case GUILDLEVEL_VICE:
					return channel->talk(player, MSG_CHANNEL_HIGHLIGHT, text, statementId, fakeChat);
				case GUILDLEVEL_LEADER:
					return channel->talk(player, MSG_GAMEMASTER_CHANNEL, text, statementId, fakeChat);
				default:
					break;
			}
		}

		if (anonymous) {
			return channel->talk("", type, text);
		}

		return channel->talk(player, type, text, statementId, fakeChat);
	}

	if (!player->getGuildId()) {
		player->sendCancel("You are not in a guild.");
		return true;
	}

	if (!IOGuild::getInstance()->guildExists(player->getGuildId())) {
		player->sendCancel("It seems like your guild does not exist anymore.");
		return true;
	}

	char buffer[350];
	if (text.substr(1) == "disband") {
		if (player->getGuildLevel() == GUILDLEVEL_LEADER) {
			IOGuild::getInstance()->disbandGuild(player->getGuildId());
			channel->talk("", MSG_GAMEMASTER_CHANNEL, "The guild has been disbanded.");
		} else {
			player->sendCancel("You are not the leader of your guild.");
		}
	} else if (text.substr(1, 6) == "invite") {
		if (player->getGuildLevel() > GUILDLEVEL_MEMBER) {
			if (text.length() > 7) {
				std::string param = text.substr(8);
				otx::util::trim_string(param);

				Player* paramPlayer = nullptr;
				if (g_game.getPlayerByNameWildcard(param, paramPlayer) == RET_NOERROR) {
					if (paramPlayer->getGuildId() == 0) {
						if (!paramPlayer->isGuildInvited(player->getGuildId())) {
							sprintf(buffer, "%s has invited you to join the guild, %s. You may join this guild by writing: !joinguild %s", player->getName().c_str(), player->getGuildName().c_str(), player->getGuildName().c_str());
							paramPlayer->sendTextMessage(MSG_EVENT_GUILD, buffer);

							sprintf(buffer, "%s has invited %s to the guild.", player->getName().c_str(), paramPlayer->getName().c_str());
							channel->talk("", MSG_CHANNEL_HIGHLIGHT, buffer);
							paramPlayer->m_invitationsList.push_back(player->getGuildId());
						} else {
							player->sendCancel("A player with that name has already been invited to your guild.");
						}
					} else {
						player->sendCancel("A player with that name is already in a guild.");
					}
				} else if (IOLoginData::getInstance()->playerExists(param)) {
					uint32_t guid;
					IOLoginData::getInstance()->getGuidByName(guid, param);
					if (!IOGuild::getInstance()->hasGuild(guid)) {
						if (!IOGuild::getInstance()->isInvited(player->getGuildId(), guid)) {
							if (IOGuild::getInstance()->guildExists(player->getGuildId())) {
								IOGuild::getInstance()->invitePlayer(player->getGuildId(), guid);
								sprintf(buffer, "%s has invited %s to the guild.", player->getName().c_str(), param.c_str());
								channel->talk("", MSG_CHANNEL_HIGHLIGHT, buffer);
							} else {
								player->sendCancel("Your guild does not exist anymore.");
							}
						} else {
							player->sendCancel("A player with that name has already been invited to your guild.");
						}
					} else {
						player->sendCancel("A player with that name is already in a guild.");
					}
				} else {
					player->sendCancel("A player with that name does not exist.");
				}
			} else {
				player->sendCancel("Invalid guildcommand parameters.");
			}
		} else {
			player->sendCancel("You don't have rights to invite players to your guild.");
		}
	} else if (text.substr(1, 5) == "leave") {
		if (player->getGuildLevel() < GUILDLEVEL_LEADER) {
			if (!player->hasEnemy()) {
				sprintf(buffer, "%s has left the guild.", player->getName().c_str());
				channel->talk("", MSG_CHANNEL_HIGHLIGHT, buffer);
				player->leaveGuild();
			} else {
				player->sendCancel("Your guild is currently at war, you cannot leave it right now.");
			}
		} else {
			player->sendCancel("You cannot leave your guild because you are the leader of it, you have to pass the leadership to another member of your guild or disband the guild.");
		}
	} else if (text.substr(1, 6) == "revoke") {
		if (player->getGuildLevel() > GUILDLEVEL_MEMBER) {
			if (text.length() > 7) {
				std::string param = text.substr(8);
				otx::util::trim_string(param);

				Player* paramPlayer = nullptr;
				if (g_game.getPlayerByNameWildcard(param, paramPlayer) == RET_NOERROR) {
					if (paramPlayer->getGuildId() == 0) {
						auto it = std::find(paramPlayer->m_invitationsList.begin(), paramPlayer->m_invitationsList.end(), player->getGuildId());
						if (it != paramPlayer->m_invitationsList.end()) {
							sprintf(buffer, "%s has revoked your invite to %s guild.", player->getName().c_str(), (player->getSex(false) ? "his" : "her"));
							paramPlayer->sendTextMessage(MSG_EVENT_GUILD, buffer);

							sprintf(buffer, "%s has revoked the guildinvite of %s.", player->getName().c_str(), paramPlayer->getName().c_str());
							channel->talk("", MSG_CHANNEL_HIGHLIGHT, buffer);

							paramPlayer->m_invitationsList.erase(it);
							return true;
						} else {
							player->sendCancel("A player with that name is not invited to your guild.");
						}
					} else {
						player->sendCancel("A player with that name is already in a guild.");
					}
				} else if (IOLoginData::getInstance()->playerExists(param)) {
					uint32_t guid;
					IOLoginData::getInstance()->getGuidByName(guid, param);
					if (IOGuild::getInstance()->isInvited(player->getGuildId(), guid)) {
						if (IOGuild::getInstance()->guildExists(player->getGuildId())) {
							sprintf(buffer, "%s has revoked the guildinvite of %s.", player->getName().c_str(), param.c_str());
							channel->talk("", MSG_CHANNEL_HIGHLIGHT, buffer);
							IOGuild::getInstance()->revokeInvite(player->getGuildId(), guid);
						} else {
							player->sendCancel("It seems like your guild does not exist anymore.");
						}
					} else {
						player->sendCancel("A player with that name is not invited to your guild.");
					}
				} else {
					player->sendCancel("A player with that name does not exist.");
				}
			} else {
				player->sendCancel("Invalid guildcommand parameters.");
			}
		} else {
			player->sendCancel("You don't have rights to revoke an invite of someone in your guild.");
		}
	} else if (text.substr(1, 7) == "promote" || text.substr(1, 6) == "demote" || text.substr(1, 14) == "passleadership" || text.substr(1, 4) == "kick") {
		if (player->getGuildLevel() == GUILDLEVEL_LEADER) {
			std::string param;
			uint32_t length = 0;
			if (text[2] == 'r') {
				length = 9;
			} else if (text[2] == 'e') {
				length = 7;
			} else if (text[2] == 'a') {
				length = 16;
			} else {
				length = 6;
			}

			if (text.length() < length) {
				player->sendCancel("Invalid guildcommand parameters.");
				return true;
			}

			param = text.substr(length);
			otx::util::trim_string(param);

			Player* paramPlayer = nullptr;
			if (g_game.getPlayerByNameWildcard(param, paramPlayer) == RET_NOERROR) {
				if (paramPlayer->getGuildId()) {
					if (IOGuild::getInstance()->guildExists(paramPlayer->getGuildId())) {
						if (player->getGuildId() == paramPlayer->getGuildId()) {
							if (text[2] == 'r') {
								if (paramPlayer->getGuildLevel() == GUILDLEVEL_MEMBER) {
									paramPlayer->setGuildLevel(GUILDLEVEL_VICE);
									sprintf(buffer, "%s has promoted %s to %s.", player->getName().c_str(), paramPlayer->getName().c_str(), paramPlayer->getRankName().c_str());
									channel->talk("", MSG_CHANNEL_HIGHLIGHT, buffer);
								} else {
									player->sendCancel("You can only promote Members to Vice-Leaders.");
								}
							} else if (text[2] == 'e') {
								if (paramPlayer->getGuildLevel() == GUILDLEVEL_VICE) {
									paramPlayer->setGuildLevel(GUILDLEVEL_MEMBER);
									sprintf(buffer, "%s has demoted %s to %s.", player->getName().c_str(), paramPlayer->getName().c_str(), paramPlayer->getRankName().c_str());
									channel->talk("", MSG_CHANNEL_HIGHLIGHT, buffer);
								} else {
									player->sendCancel("You can only demote Vice-Leaders to Members.");
								}
							} else if (text[2] == 'a') {
								if (paramPlayer->getGuildLevel() == GUILDLEVEL_VICE) {
									const uint32_t levelToFormGuild = otx::config::getInteger(otx::config::LEVEL_TO_FORM_GUILD);
									if (paramPlayer->getLevel() >= levelToFormGuild) {
										paramPlayer->setGuildLevel(GUILDLEVEL_LEADER);
										player->setGuildLevel(GUILDLEVEL_VICE);

										IOGuild::getInstance()->updateOwnerId(paramPlayer->getGuildId(), paramPlayer->getGUID());
										sprintf(buffer, "%s has passed the guild leadership to %s.", player->getName().c_str(), paramPlayer->getName().c_str());
										channel->talk("", MSG_GAMEMASTER_CHANNEL, buffer);
									} else {
										sprintf(buffer, "The new guild leader has to be at least Level %d.", levelToFormGuild);
										player->sendCancel(buffer);
									}
								} else {
									player->sendCancel("A player with that name is not a Vice-Leader.");
								}
							} else {
								if (player->getGuildLevel() > paramPlayer->getGuildLevel()) {
									if (!player->hasEnemy()) {
										sprintf(buffer, "%s has been kicked from the guild by %s.", paramPlayer->getName().c_str(), player->getName().c_str());
										channel->talk("", MSG_CHANNEL_HIGHLIGHT, buffer);
										paramPlayer->leaveGuild();
									} else {
										player->sendCancel("Your guild is currently at war, you cannot kick right now.");
									}
								} else {
									player->sendCancel("You may only kick players with a guild rank below your.");
								}
							}
						} else {
							player->sendCancel("You are not in the same guild as a player with that name.");
						}
					} else {
						player->sendCancel("Could not find the guild of a player with that name.");
					}
				} else {
					player->sendCancel("A player with that name is not in a guild.");
				}
			} else if (IOLoginData::getInstance()->playerExists(param)) {
				uint32_t guid;
				IOLoginData::getInstance()->getGuidByName(guid, param);
				if (IOGuild::getInstance()->hasGuild(guid)) {
					if (player->getGuildId() == IOGuild::getInstance()->getGuildId(guid)) {
						if (text[2] == 'r') {
							if (IOGuild::getInstance()->getGuildLevel(guid) == GUILDLEVEL_MEMBER) {
								IOGuild::getInstance()->setGuildLevel(guid, GUILDLEVEL_VICE);
								sprintf(buffer, "%s has promoted %s to %s.", player->getName().c_str(), param.c_str(), IOGuild::getInstance()->getRank(guid).c_str());
								channel->talk("", MSG_CHANNEL_HIGHLIGHT, buffer);
							} else {
								player->sendCancel("You can only promote Members to Vice-Leaders.");
							}
						} else if (text[2] == 'e') {
							if (IOGuild::getInstance()->getGuildLevel(guid) == GUILDLEVEL_VICE) {
								IOGuild::getInstance()->setGuildLevel(guid, GUILDLEVEL_MEMBER);
								sprintf(buffer, "%s has demoted %s to %s.", player->getName().c_str(), param.c_str(), IOGuild::getInstance()->getRank(guid).c_str());
								channel->talk("", MSG_CHANNEL_HIGHLIGHT, buffer);
							} else {
								player->sendCancel("You can only demote Vice-Leaders to Members.");
							}
						} else if (text[2] == 'a') {
							if (IOGuild::getInstance()->getGuildLevel(guid) == GUILDLEVEL_VICE) {
								const uint32_t levelToFormGuild = otx::config::getInteger(otx::config::LEVEL_TO_FORM_GUILD);
								if (IOLoginData::getInstance()->getLevel(guid) >= levelToFormGuild) {
									IOGuild::getInstance()->setGuildLevel(guid, GUILDLEVEL_LEADER);
									player->setGuildLevel(GUILDLEVEL_VICE);

									sprintf(buffer, "%s has passed the guild leadership to %s.", player->getName().c_str(), param.c_str());
									channel->talk("", MSG_GAMEMASTER_CHANNEL, buffer);
								} else {
									sprintf(buffer, "The new guild leader has to be at least Level %d.", levelToFormGuild);
									player->sendCancel(buffer);
								}
							} else {
								player->sendCancel("A player with that name is not a Vice-Leader.");
							}
						} else {
							sprintf(buffer, "%s has been kicked from the guild by %s.", param.c_str(), player->getName().c_str());
							channel->talk("", MSG_CHANNEL_HIGHLIGHT, buffer);
							IOLoginData::getInstance()->resetGuildInformation(guid);
						}
					}
				} else {
					player->sendCancel("A player with that name is not in a guild.");
				}
			} else {
				player->sendCancel("A player with that name does not exist.");
			}
		} else {
			player->sendCancel("You are not the leader of your guild.");
		}
	} else if (text.substr(1, 4) == "nick" && text.length() > 5) {
		auto params = explodeString(text.substr(6), ",");
		if (params.size() >= 2) {
			std::string param1 = params[0], param2 = params[1];
			otx::util::trim_string(param1);
			otx::util::trim_string(param2);

			Player* paramPlayer = nullptr;
			if (g_game.getPlayerByNameWildcard(param1, paramPlayer) == RET_NOERROR) {
				if (paramPlayer->getGuildId()) {
					if (param2.length() > 2) {
						if (param2.length() < 21) {
							if (isValidName(param2, false)) {
								if (IOGuild::getInstance()->guildExists(paramPlayer->getGuildId())) {
									if (player->getGuildId() == paramPlayer->getGuildId()) {
										if (paramPlayer->getGuildLevel() < player->getGuildLevel() || (player == paramPlayer && player->getGuildLevel() > GUILDLEVEL_MEMBER)) {
											paramPlayer->setGuildNick(param2);
											if (player != paramPlayer) {
												sprintf(buffer, "%s has set the guildnick of %s to \"%s\".", player->getName().c_str(), paramPlayer->getName().c_str(), param2.c_str());
											} else {
												sprintf(buffer, "%s has set %s guildnick to \"%s\".", player->getName().c_str(), (player->getSex(false) ? "his" : "her"), param2.c_str());
											}

											channel->talk("", MSG_CHANNEL_HIGHLIGHT, buffer);
										} else {
											player->sendCancel("You may only change the guild nick of players that have a lower rank than you.");
										}
									} else {
										player->sendCancel("A player with that name is not in your guild.");
									}
								} else {
									player->sendCancel("A player with that name's guild could not be found.");
								}
							} else {
								player->sendCancel("That guildnick is not valid.");
							}
						} else {
							player->sendCancel("That guildnick is too long, please select a shorter one.");
						}
					} else {
						player->sendCancel("That guildnick is too short, please select a longer one.");
					}
				} else {
					player->sendCancel("A player with that name is not in a guild.");
				}
			} else if (IOLoginData::getInstance()->playerExists(param1)) {
				uint32_t guid;
				IOLoginData::getInstance()->getGuidByName(guid, param1);
				if (IOGuild::getInstance()->hasGuild(guid)) {
					if (param2.length() > 2) {
						if (param2.length() < 21) {
							if (isValidName(param2, false)) {
								if (IOGuild::getInstance()->guildExists(guid)) {
									if (player->getGuildId() == IOGuild::getInstance()->getGuildId(guid)) {
										if (IOGuild::getInstance()->getGuildLevel(guid) < player->getGuildLevel()) {
											IOGuild::getInstance()->setGuildNick(guid, param2);
											sprintf(buffer, "%s has set the guildnick of %s to \"%s\".", player->getName().c_str(), param1.c_str(), param2.c_str());
											channel->talk("", MSG_CHANNEL_HIGHLIGHT, buffer);
										} else {
											player->sendCancel("You may only change the guild nick of players that have a lower rank than you.");
										}
									} else {
										player->sendCancel("A player with that name is not in your guild.");
									}
								} else {
									player->sendCancel("A player with that name's guild could not be found.");
								}
							} else {
								player->sendCancel("That guildnick is not valid.");
							}
						} else {
							player->sendCancel("That guildnick is too long, please select a shorter one.");
						}
					} else {
						player->sendCancel("That guildnick is too short, please select a longer one.");
					}
				} else {
					player->sendCancel("A player with that name is not in any guild.");
				}
			} else {
				player->sendCancel("A player with that name does not exist.");
			}
		} else {
			player->sendCancel("Invalid guildcommand parameters.");
		}
	} else if (text.substr(1, 11) == "setrankname" && text.length() > 12) {
		auto params = explodeString(text.substr(13), ",");
		if (params.size() >= 2) {
			std::string param1 = params[0], param2 = params[1];
			otx::util::trim_string(param1);
			otx::util::trim_string(param2);

			if (player->getGuildLevel() == GUILDLEVEL_LEADER) {
				if (param2.length() > 2) {
					if (param2.length() < 21) {
						if (isValidName(param2, false)) {
							if (IOGuild::getInstance()->getRankIdByName(player->getGuildId(), param1)) {
								if (!IOGuild::getInstance()->getRankIdByName(player->getGuildId(), param2)) {
									IOGuild::getInstance()->changeRank(player->getGuildId(), param1, param2);
									sprintf(buffer, "%s has renamed the guildrank: \"%s\", to: \"%s\".", player->getName().c_str(), param1.c_str(), param2.c_str());
									channel->talk("", MSG_CHANNEL_HIGHLIGHT, buffer);
								} else {
									player->sendCancel("There is already a rank in your guild with that name.");
								}
							} else {
								player->sendCancel("There is no such rankname in your guild.");
							}
						} else {
							player->sendCancel("The new guildrank contains invalid characters.");
						}
					} else {
						player->sendCancel("The new rankname is too long.");
					}
				} else {
					player->sendCancel("The new rankname is too short.");
				}
			} else {
				player->sendCancel("You are not the leader of your guild.");
			}
		} else {
			player->sendCancel("Invalid guildcommand parameters");
		}
	} else if (text.substr(1, 7) == "setmotd") {
		if (player->getGuildLevel() == GUILDLEVEL_LEADER) {
			if (text.length() > 8) {
				std::string param = text.substr(9);
				otx::util::trim_string(param);
				if (param.length() > 2) {
					if (param.length() < 225) {
						IOGuild::getInstance()->setMotd(player->getGuildId(), param);
						sprintf(buffer, "%s has set the Message of the Day to: %s", player->getName().c_str(), param.c_str());
						channel->talk("", MSG_GAMEMASTER_CHANNEL, buffer);
					} else {
						player->sendCancel("That motd is too long.");
					}
				} else {
					player->sendCancel("That motd is too short.");
				}
			} else {
				player->sendCancel("Invalid guildcommand parameters.");
			}
		} else {
			player->sendCancel("Only the leader of your guild can set the guild motd.");
		}
	} else if (text.substr(1, 9) == "cleanmotd") {
		if (player->getGuildLevel() == GUILDLEVEL_LEADER) {
			IOGuild::getInstance()->setMotd(player->getGuildId(), "");
			sprintf(buffer, "%s has cleaned the Message of the Day.", player->getName().c_str());
			channel->talk("", MSG_CHANNEL_HIGHLIGHT, buffer);
		} else {
			player->sendCancel("Only the leader of your guild can clean the guild motd.");
		}
	} else if (text.substr(1, 8) == "commands") {
		sprintf(buffer, "Guild commands with parameters: disband, invite[name], leave, kick[name], revoke[name], demote[name], promote[name], passleadership[name], nick[name, nick], setrankname[oldName, newName], setmotd[text] and cleanmotd.");
		channel->talk("", MSG_CHANNEL_HIGHLIGHT, buffer);
	} else {
		return false;
	}

	return true;
}

std::string Chat::getChannelName(Player* player, uint16_t channelId)
{
	if (ChatChannel* channel = getChannel(player, channelId)) {
		return channel->getName();
	}

	return "";
}

ChannelsList Chat::getChannelList(Player* player)
{
	ChannelsList list;
	if (!player || player->isRemoved()) {
		return list;
	}

	ChatChannel* channel = nullptr;
	if (player->getParty() && ((channel = getChannel(player, CHANNEL_PARTY)) || (channel = createChannel(player, CHANNEL_PARTY)))) {
		list.emplace_back(channel->getId(), channel->getName());
	}

	if (player->getGuildId() && player->getGuildName().length() && ((channel = getChannel(player, CHANNEL_GUILD)) || (channel = createChannel(player, CHANNEL_GUILD)))) {
		list.emplace_back(channel->getId(), channel->getName());
	}

	for (const auto& it : m_normalChannels) {
		if ((channel = getChannel(player, it.first))) {
			list.emplace_back(channel->getId(), channel->getName());
		}
	}

	bool hasPrivate = false;
	for (const auto& it : m_privateChannels) {
		if (it.second.isInvited(player)) {
			list.emplace_back(it.second.getId(), it.second.getName());
		}

		if (it.second.getOwner() == player->getGUID()) {
			hasPrivate = true;
		}
	}

	if (!hasPrivate) {
		list.emplace_front(m_dummyPrivate->getId(), m_dummyPrivate->getName());
	}
	return list;
}

ChatChannel* Chat::getChannel(Player* player, uint16_t channelId)
{
	if (!player || player->isRemoved()) {
		return nullptr;
	}

	if (channelId == CHANNEL_GUILD) {
		auto git = m_guildChannels.find(player->getGuildId());
		if (git != m_guildChannels.end()) {
			return &git->second;
		}
		return nullptr;
	}

	if (channelId == CHANNEL_PARTY) {
		if (player->getParty()) {
			auto it = m_partyChannels.find(player->getParty());
			if (it != m_partyChannels.end()) {
				return &it->second;
			}
		}
		return nullptr;
	}

	auto nit = m_normalChannels.find(channelId);
	if (nit != m_normalChannels.end()) {
		ChatChannel& channel = nit->second;
		if (!channel.hasFlag(CHANNELFLAG_ENABLED) || player->getAccess() < channel.getAccess()
			|| (!player->hasCustomFlag(PlayerCustomFlag_GamemasterPrivileges) && !channel.checkVocation(player->getVocationId()))) {
			return nullptr;
		}

		if (channelId == CHANNEL_RVR && !player->hasFlag(PlayerFlag_CanAnswerRuleViolations)) {
			return nullptr;
		}
		return &channel;
	}

	auto pit = m_privateChannels.find(channelId);
	if (pit != m_privateChannels.end() && pit->second.isInvited(player)) {
		return &pit->second;
	}
	return nullptr;
}

ChatChannel* Chat::getChannelById(uint16_t channelId)
{
	auto it = m_normalChannels.find(channelId);
	if (it != m_normalChannels.end()) {
		return &it->second;
	}
	return nullptr;
}

PrivateChatChannel* Chat::getPrivateChannel(Player* player)
{
	if (!player || player->isRemoved()) {
		return nullptr;
	}

	for (auto& it : m_privateChannels) {
		if (it.second.getOwner() == player->getGUID()) {
			return &it.second;
		}
	}
	return nullptr;
}

std::vector<const ChatChannel*> Chat::getPublicChannels() const
{
	std::vector<const ChatChannel*> channels;
	for (const auto& [channelId, channel] : m_normalChannels) {
		if (isPublicChannel(channelId)) {
			channels.push_back(&channel);
		}
	}
	return channels;
}
