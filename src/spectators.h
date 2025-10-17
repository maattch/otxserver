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

#include "protocolgame.h"

class Creature;
class Player;
class House;
class Container;
class Tile;
class Quest;

class Spectators final
{
public:
	explicit Spectators(ProtocolGame_ptr client);

	void clear(bool full);

	bool check(const std::string& _password);
	void handle(ProtocolGame* client, const std::string& text, uint16_t channelId);
	void sendLook(ProtocolGame* client, const Position& pos, uint16_t spriteId, int16_t stackpos);
	void chat(uint16_t channelId);

	std::vector<std::string> list();

	void kick(std::vector<std::string>&& list);
	void mute(std::vector<std::string>&& mutes) { m_mutes = std::move(mutes); }
	void ban(std::vector<std::string>&& bans);

	const auto& muteList() { return m_mutes; }
	const auto& banList() { return m_bans; }

	bool banned(uint32_t ip) const;

	ProtocolGame_ptr getOwner() const { return m_owner; }
	void setOwner(ProtocolGame_ptr client) { m_owner = std::move(client); }
	void resetOwner() { m_owner.reset(); }

	const std::string& getPassword() const { return m_password; }
	void setPassword(const std::string& value) { m_password = value; }

	bool isBroadcasting() const { return m_broadcast; }
	void setBroadcast(bool value) { m_broadcast = value; }

	bool isAuth() const { return m_auth; }
	void setAuth(bool value) { m_auth = value; }

	void addSpectator(ProtocolGame* client, std::string name = "", bool spy = false);
	void removeSpectator(ProtocolGame* client, bool spy = false);

	int64_t getBroadcastTime() const;

	// inherited
	uint32_t getIP() const {
		if (!m_owner) {
			return 0;
		}
		return m_owner->getIP();
	}

	bool logout(bool displayEffect, bool forceLogout) {
		if (!m_owner) {
			return true;
		}
		return m_owner->logout(displayEffect, forceLogout);
	}
	void disconnect() {
		if (m_owner) {
			m_owner->disconnect();
		}
	}

private:
	// inherited
	bool canSee(const Position& pos) const;

	void sendChannelMessage(std::string author, std::string text, MessageType_t type, uint16_t channel, bool fakeChat = false, uint32_t ip = 0);
	void sendClosePrivate(uint16_t channelId);
	void sendCreatePrivateChannel(uint16_t channelId, const std::string& channelName);
	void sendChannelsDialog(const ChannelsList& channels);
	void sendChannel(uint16_t channelId, const std::string& channelName);
	void sendOpenPrivateChannel(const std::string& receiver);
	void sendIcons(int32_t icons);
	void sendFYIBox(const std::string& message);

	void sendDistanceShoot(const Position& from, const Position& to, uint16_t type);

	void sendMagicEffect(const Position& pos, uint16_t type);

	void sendAnimatedText(const Position& pos, uint8_t color, const std::string& text);
	void sendCreatureHealth(const Creature* creature);
	void sendSkills();
	void sendPing();
	void sendCreatureTurn(const Creature* creature, int16_t stackpos);
	void sendCreatureSay(const Creature* creature, MessageType_t type, const std::string& text, Position* pos, uint32_t statementId, bool fakeChat = false); // extended
	void sendCreatureChannelSay(const Creature* creature, MessageType_t type, const std::string& text, uint16_t channelId, uint32_t statementId, bool fakeChat = false); // extended

	void sendCancelWalk();
	void sendChangeSpeed(const Creature* creature, uint32_t speed);
	void sendCancelTarget();
	void sendCreatureOutfit(const Creature* creature, const Outfit_t& outfit);
	void sendStats();
	void sendTextMessage(MessageType_t type, const std::string& message);
	void sendReLoginWindow();

	void sendTutorial(uint8_t tutorialId);
	void sendAddMarker(const Position& pos, MapMarks_t markType, const std::string& desc);
	void sendExtendedOpcode(uint8_t opcode, const std::string& buffer);

	void sendRuleViolationsChannel(uint16_t channelId);
	void sendRemoveReport(const std::string& name);
	void sendLockRuleViolation();
	void sendRuleViolationCancel(const std::string& name);

	void sendCreatureSkull(const Creature* creature);
	void sendCreatureShield(const Creature* creature);
	void sendCreatureEmblem(const Creature* creature);
	void sendCreatureWalkthrough(const Creature* creature, bool walkthrough);

	void sendShop(const ShopInfoList& shop);
	void sendCloseShop();
	void sendGoods(const ShopInfoList& shop);
	void sendTradeItemRequest(const Player* player, const Item* item, bool ack);
	void sendCloseTrade();

	void sendTextWindow(uint32_t windowTextId, Item* item, uint16_t maxLen, bool canWrite);
	void sendHouseWindow(uint32_t windowTextId, const std::string& text);

	void sendOutfitWindow();
	void sendQuests();
	void sendQuestInfo(const Quest* quest);

	void sendVIPLogIn(uint32_t guid);
	void sendVIPLogOut(uint32_t guid);
	void sendVIP(uint32_t guid, const std::string& name, bool isOnline);

	void sendCreatureLight(const Creature* creature);
	void sendWorldLight(const LightInfo& lightInfo);

	void sendCreatureSquare(const Creature* creature, uint8_t color);

	void sendAddTileItem(const Position& pos, uint32_t stackpos, const Item* item);
	void sendUpdateTileItem(const Position& pos, uint32_t stackpos, const Item* item);
	void sendRemoveTileItem(const Position& pos, uint32_t stackpos);
	void sendUpdateTile(const Tile* tile, const Position& pos);

	void sendAddCreature(const Creature* creature, const Position& pos, uint32_t stackpos);
	void sendRemoveCreature(const Position& pos, uint32_t stackpos);
	void sendMoveCreature(const Creature* creature, const Position& newPos, uint32_t newStackPos, const Position& oldPos, uint32_t oldStackpos, bool teleport);

	void sendAddContainerItem(uint8_t cid, const Item* item);
	void sendUpdateContainerItem(uint8_t cid, uint8_t slot, const Item* item);
	void sendRemoveContainerItem(uint8_t cid, uint8_t slot);

	void sendContainer(uint32_t cid, const Container* container, bool hasParent);
	void sendCloseContainer(uint32_t cid);

	void sendAddInventoryItem(uint8_t slot, const Item* item);
	void sendUpdateInventoryItem(uint8_t slot, const Item* item);
	void sendRemoveInventoryItem(uint8_t slot);
	void sendCastList();


	std::string m_password;
	std::vector<std::string> m_mutes;
	std::map<std::string, uint32_t> m_bans;
	std::map<ProtocolGame*, std::pair<std::string, bool>> m_spectators;
	ProtocolGame_ptr m_owner;
	int64_t m_broadcast_time = 0;
	uint32_t m_id = 0;
	bool m_broadcast = false;
	bool m_auth = false;

	friend class Player;
};
