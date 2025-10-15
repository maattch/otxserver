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

#pragma once

#include "const.h"
#include "party.h"
#include "tools.h"

class Player;
class Condition;

enum ChannelFlags_t
{
	CHANNELFLAG_NONE = 0,
	CHANNELFLAG_ENABLED = 1 << 0,
	CHANNELFLAG_ACTIVE = 1 << 1,
	CHANNELFLAG_LOGGED = 1 << 2,
};

typedef std::map<uint32_t, Player*> UsersMap;
typedef std::list<uint32_t> InviteList;

class ChatChannel
{
public:
	ChatChannel(uint16_t id, const std::string& name, uint16_t flags);
	virtual ~ChatChannel() = default;

	// non-copyable
	ChatChannel(const ChatChannel&) = delete;
	ChatChannel& operator=(const ChatChannel&) = delete;

	// moveable
	ChatChannel(ChatChannel&&) = default;
	ChatChannel& operator=(ChatChannel&&) = default;

	static uint16_t staticFlags;

	uint16_t getId() const { return m_id; }
	const std::string& getName() const { return m_name; }
	uint16_t getFlags() const { return m_flags; }

	int32_t getConditionId() const { return m_conditionId; }
	void setConditionId(int32_t id) { m_conditionId = id; }

	const std::string& getConditionMessage() const { return m_conditionMessage; }
	void setConditionMessage(const std::string& message) { m_conditionMessage = message; }

	const UsersMap& getUsers() const { return m_users; }

	uint32_t getLevel() const { return m_level; }
	void setLevel(uint32_t level) { m_level = level; }

	uint32_t getAccess() const { return m_access; }
	void setAccess(uint32_t access) { m_access = access; }

	void setCondition(Condition* condition) { m_condition.reset(condition); }
	void setVocationMap(VocationMap* vocationMap) { m_vocationMap.reset(vocationMap); }

	virtual uint32_t getOwner() const { return 0; }

	bool hasFlag(uint16_t value) const { return (m_flags & value); }
	bool checkVocation(uint32_t vocationId) const {
		return !m_vocationMap || m_vocationMap->empty() || m_vocationMap->find(vocationId) != m_vocationMap->end();
	}

	bool addUser(Player* player);
	bool removeUser(Player* player, bool exclude = false);
	bool hasUser(Player* player) const;

	bool talk(Player* player, MessageType_t type, const std::string& text, uint32_t statementId, bool fakeChat = false);
	bool talk(std::string nick, MessageType_t type, const std::string& text, bool fakeChat = false, uint32_t ip = 0);

protected:
	UsersMap m_users;
	uint16_t m_id = 0;

private:
	std::string m_name;
	std::string m_conditionMessage;
	std::unique_ptr<VocationMap> m_vocationMap;
	std::shared_ptr<std::ofstream> m_file;
	std::unique_ptr<Condition> m_condition;
	int32_t m_conditionId = -1;
	uint32_t m_access = 0;
	uint32_t m_level = 0;
	uint16_t m_flags = 0;
};

class PrivateChatChannel final : public ChatChannel
{
public:
	PrivateChatChannel(uint16_t id, std::string name, uint16_t flags) : ChatChannel(id, name, flags) {}

	uint32_t getOwner() const override { return m_owner; }
	void setOwner(uint32_t id) { m_owner = id; }

	bool isInvited(const Player* player) const;

	void invitePlayer(Player* player, Player* invitePlayer);
	void excludePlayer(Player* player, Player* excludePlayer);

	bool addInvited(Player* player);
	bool removeInvited(Player* player);

	void closeChannel();

	const InviteList& getInvitedUsers() { return m_invites; }

private:
	InviteList m_invites;
	uint32_t m_owner = 0;
};

typedef std::list<std::pair<uint16_t, std::string>> ChannelsList;

class Chat final
{
public:
	Chat() = default;

	bool reload();
	bool loadFromXml();
	bool parseChannelNode(xmlNodePtr p);

	ChatChannel* createChannel(Player* player, uint16_t channelId);
	bool deleteChannel(Player* player, uint16_t channelId);

	ChatChannel* addUserToChannel(Player* player, uint16_t channelId);
	void reOpenChannels(Player* player);
	bool removeUserFromChannel(Player* player, uint16_t channelId);
	void removeUserFromChannels(Player* player);

	bool talk(Player* player, MessageType_t type, const std::string& text,
		uint16_t channelId, uint32_t statementId, bool anonymous = false, bool fakeChat = false);

	ChatChannel* getChannel(Player* player, uint16_t channelId);
	ChatChannel* getChannelById(uint16_t channelId);

	std::string getChannelName(Player* player, uint16_t channelId);
	ChannelsList getChannelList(Player* player);

	PrivateChatChannel* getPrivateChannel(Player* player);
	bool isPrivateChannel(uint16_t cid) const { return m_privateChannels.find(cid) != m_privateChannels.end(); }

	std::vector<const ChatChannel*> getPublicChannels() const;
	bool isPublicChannel(uint16_t cid) const { return cid != CHANNEL_GUILD && cid != CHANNEL_RVR && cid != CHANNEL_PARTY && !isPrivateChannel(cid); }

private:
	void clear();

	std::unique_ptr<ChatChannel> m_dummyPrivate;
	std::map<uint16_t, ChatChannel> m_normalChannels;
	std::map<uint16_t, PrivateChatChannel> m_privateChannels;
	std::map<Party*, ChatChannel> m_partyChannels;
	std::map<uint32_t, ChatChannel> m_guildChannels;
};

extern Chat g_chat;
