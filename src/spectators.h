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

#include "enums.h"
#include "protocolgame.h"

class Creature;
class Player;
class House;
class Container;
class Tile;
class Quest;

typedef std::map<ProtocolGame*, std::pair<std::string, bool>> SpectatorList;
typedef std::map<std::string, uint32_t> DataList;

class Spectators
{
public:
	Spectators(ProtocolGame_ptr client);
	virtual ~Spectators() {}

	void clear(bool full)
	{
		for (SpectatorList::iterator it = m_spectators.begin(); it != m_spectators.end(); ++it) {
			if (!it->first->twatchername.empty()) {
				it->first->parseTelescopeBack(true);
				continue;
			}
			it->first->disconnect();
		}

		m_spectators.clear();
		m_mutes.clear();

		m_id = 0;
		if (!full) {
			return;
		}

		m_bans.clear();
		m_password = "";
		m_broadcast = m_auth = false;
		m_broadcast_time = 0;
	}

	bool check(const std::string& _password);
	void handle(ProtocolGame* client, const std::string& text, uint16_t channelId);
	void sendLook(ProtocolGame* client, const Position& pos, uint16_t spriteId, int16_t stackpos);
	void chat(uint16_t channelId);

	StringVec list()
	{
		StringVec t;
		for (SpectatorList::iterator it = m_spectators.begin(); it != m_spectators.end(); ++it) {
			t.push_back(it->second.first);
		}

		return t;
	}

	void kick(StringVec list);
	StringVec muteList() { return m_mutes; }
	void mute(StringVec _mutes) { m_mutes = _mutes; }
	DataList banList() { return m_bans; }
	void ban(StringVec _bans);

	bool banned(uint32_t ip) const
	{
		for (DataList::const_iterator it = m_bans.begin(); it != m_bans.end(); ++it) {
			if (it->second == ip) {
				return true;
			}
		}

		return false;
	}

	ProtocolGame_ptr getOwner() const { return m_owner; }
	void setOwner(ProtocolGame_ptr client)
	{
		m_owner = client;
	}
	void resetOwner()
	{
		m_owner.reset();
	}

	std::string getPassword() const { return m_password; }
	void setPassword(const std::string& value) { m_password = value; }

	bool isBroadcasting() const { return m_broadcast; }
	void setBroadcast(bool value) { m_broadcast = value; }

	bool isAuth() const { return m_auth; }
	void setAuth(bool value) { m_auth = value; }

	void addSpectator(ProtocolGame* client, std::string name = "", bool spy = false);
	void removeSpectator(ProtocolGame* client, bool spy = false);

	int64_t getBroadcastTime() const;

	// inherited
	uint32_t getIP() const
	{
		if (!m_owner) {
			return 0;
		}

		return m_owner->getIP();
	}

	bool logout(bool displayEffect, bool forceLogout)
	{
		if (!m_owner) {
			return true;
		}

		return m_owner->logout(displayEffect, forceLogout);
	}

	void disconnect()
	{
		if (m_owner) {
			m_owner->disconnect();
		}
	}

private:
	friend class Player;

	SpectatorList m_spectators;
	StringVec m_mutes;
	DataList m_bans;

	ProtocolGame_ptr m_owner;
	uint32_t m_id;
	std::string m_password;
	bool m_broadcast, m_auth;
	int64_t m_broadcast_time;

	// inherited
	bool canSee(const Position& pos) const
	{
		if (!m_owner) {
			return false;
		}

		return m_owner->canSee(pos);
	}

	void sendChannelMessage(std::string author, std::string text, MessageClasses type, uint16_t channel, bool fakeChat = false, uint32_t ip = 0);
	void sendClosePrivate(uint16_t channelId);
	void sendCreatePrivateChannel(uint16_t channelId, const std::string& channelName);
	void sendChannelsDialog(const ChannelsList& channels)
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendChannelsDialog(channels);
	}
	void sendChannel(uint16_t channelId, const std::string& channelName)
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendChannel(channelId, channelName);
		for (SpectatorList::iterator it = m_spectators.begin(); it != m_spectators.end(); ++it) {
			it->first->sendChannel(channelId, channelName);
		}
	}
	void sendOpenPrivateChannel(const std::string& receiver)
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendOpenPrivateChannel(receiver);
		for (SpectatorList::iterator it = m_spectators.begin(); it != m_spectators.end(); ++it) {
			it->first->sendOpenPrivateChannel(receiver);
		}
	}
	void sendIcons(int32_t icons)
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendIcons(icons);
		for (SpectatorList::iterator it = m_spectators.begin(); it != m_spectators.end(); ++it) {
			it->first->sendIcons(icons);
		}
	}
	void sendFYIBox(const std::string& message)
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendFYIBox(message);
	}

	void sendDistanceShoot(const Position& from, const Position& to, uint16_t type)
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendDistanceShoot(from, to, type);
		for (SpectatorList::iterator it = m_spectators.begin(); it != m_spectators.end(); ++it) {
			it->first->sendDistanceShoot(from, to, type);
		}
	}

	void sendMagicEffect(const Position& pos, uint16_t type)
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendMagicEffect(pos, type);
		for (SpectatorList::iterator it = m_spectators.begin(); it != m_spectators.end(); ++it) {
			it->first->sendMagicEffect(pos, type);
		}
	}

	void sendAnimatedText(const Position& pos, uint8_t color, const std::string& text)
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendAnimatedText(pos, color, text);
		for (SpectatorList::iterator it = m_spectators.begin(); it != m_spectators.end(); ++it) {
			it->first->sendAnimatedText(pos, color, text);
		}
	}
	void sendCreatureHealth(const Creature* creature)
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendCreatureHealth(creature);
		for (SpectatorList::iterator it = m_spectators.begin(); it != m_spectators.end(); ++it) {
			it->first->sendCreatureHealth(creature);
		}
	}
	void sendSkills()
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendSkills();
		for (SpectatorList::iterator it = m_spectators.begin(); it != m_spectators.end(); ++it) {
			it->first->sendSkills();
		}
	}
	void sendPing()
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendPing();
		for (SpectatorList::iterator it = m_spectators.begin(); it != m_spectators.end(); ++it) {
			it->first->sendPing();
		}
	}
	void sendCreatureTurn(const Creature* creature, int16_t stackpos)
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendCreatureTurn(creature, stackpos);
		for (SpectatorList::iterator it = m_spectators.begin(); it != m_spectators.end(); ++it) {
			it->first->sendCreatureTurn(creature, stackpos);
		}
	}
	void sendCreatureSay(const Creature* creature, MessageClasses type, const std::string& text, Position* pos, uint32_t statementId, bool fakeChat = false); // extended
	void sendCreatureChannelSay(const Creature* creature, MessageClasses type, const std::string& text, uint16_t channelId, uint32_t statementId, bool fakeChat = false); // extended

	void sendCancel(const std::string& message)
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendCancel(message);
		for (SpectatorList::iterator it = m_spectators.begin(); it != m_spectators.end(); ++it) {
			it->first->sendCancel(message);
		}
	}
	void sendCancelWalk()
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendCancelWalk();
		for (SpectatorList::iterator it = m_spectators.begin(); it != m_spectators.end(); ++it) {
			it->first->sendCancelWalk();
		}
	}
	void sendChangeSpeed(const Creature* creature, uint32_t speed)
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendChangeSpeed(creature, speed);
		for (SpectatorList::iterator it = m_spectators.begin(); it != m_spectators.end(); ++it) {
			it->first->sendChangeSpeed(creature, speed);
		}
	}
	void sendProgressbar(const Creature* creature, uint32_t duration, bool ltr = true)
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendProgressbar(creature, duration, ltr);
		for (SpectatorList::iterator it = m_spectators.begin(); it != m_spectators.end(); ++it) {
			it->first->sendProgressbar(creature, duration, ltr);
		}
	}
	void sendCancelTarget()
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendCancelTarget();
		for (SpectatorList::iterator it = m_spectators.begin(); it != m_spectators.end(); ++it) {
			it->first->sendCancelTarget();
		}
	}
	void sendCreatureOutfit(const Creature* creature, const Outfit_t& outfit)
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendCreatureOutfit(creature, outfit);
		for (SpectatorList::iterator it = m_spectators.begin(); it != m_spectators.end(); ++it) {
			it->first->sendCreatureOutfit(creature, outfit);
		}
	}
	void sendStats()
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendStats();
		for (SpectatorList::iterator it = m_spectators.begin(); it != m_spectators.end(); ++it) {
			it->first->sendStats();
		}
	}
	void sendTextMessage(MessageClasses mclass, const std::string& message)
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendTextMessage(mclass, message);
		for (SpectatorList::iterator it = m_spectators.begin(); it != m_spectators.end(); ++it) {
			it->first->sendTextMessage(mclass, message);
		}
	}
	void sendStatsMessage(MessageClasses mclass, const std::string& message,
		Position pos, MessageDetails* details = nullptr)
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendStatsMessage(mclass, message, pos, details);
		for (SpectatorList::iterator it = m_spectators.begin(); it != m_spectators.end(); ++it) {
			it->first->sendStatsMessage(mclass, message, pos, details);
		}
	}
	void sendReLoginWindow()
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendReLoginWindow();
		clear(true); // just smoothly disconnect
	}

	void sendTutorial(uint8_t tutorialId)
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendTutorial(tutorialId);
	}
	void sendAddMarker(const Position& pos, MapMarks_t markType, const std::string& desc)
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendAddMarker(pos, markType, desc);
	}
	void sendExtendedOpcode(uint8_t opcode, const std::string& buffer)
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendExtendedOpcode(opcode, buffer);
	}

	void sendRuleViolationsChannel(uint16_t channelId)
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendRuleViolationsChannel(channelId);
	}
	void sendRemoveReport(const std::string& name)
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendRemoveReport(name);
	}
	void sendLockRuleViolation()
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendLockRuleViolation();
	}
	void sendRuleViolationCancel(const std::string& name)
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendRuleViolationCancel(name);
	}

	void sendCreatureSkull(const Creature* creature)
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendCreatureSkull(creature);
		for (SpectatorList::iterator it = m_spectators.begin(); it != m_spectators.end(); ++it) {
			it->first->sendCreatureSkull(creature);
		}
	}
	void sendCreatureShield(const Creature* creature)
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendCreatureShield(creature);
		for (SpectatorList::iterator it = m_spectators.begin(); it != m_spectators.end(); ++it) {
			it->first->sendCreatureShield(creature);
		}
	}
	void sendCreatureEmblem(const Creature* creature)
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendCreatureEmblem(creature);
		for (SpectatorList::iterator it = m_spectators.begin(); it != m_spectators.end(); ++it) {
			it->first->sendCreatureEmblem(creature);
		}
	}
	void sendCreatureWalkthrough(const Creature* creature, bool walkthrough)
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendCreatureWalkthrough(creature, walkthrough);
		for (SpectatorList::iterator it = m_spectators.begin(); it != m_spectators.end(); ++it) {
			it->first->sendCreatureWalkthrough(creature, walkthrough);
		}
	}

	void sendShop(Npc* npc, const ShopInfoList& shop)
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendShop(npc, shop);
	}
	void sendCloseShop()
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendCloseShop();
	}
	void sendGoods(const ShopInfoList& shop)
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendGoods(shop);
	}
	void sendTradeItemRequest(const Player* player, const Item* item, bool ack)
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendTradeItemRequest(player, item, ack);
	}
	void sendCloseTrade()
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendCloseTrade();
	}

	void sendTextWindow(uint32_t windowTextId, Item* item, uint16_t maxLen, bool canWrite)
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendTextWindow(windowTextId, item, maxLen, canWrite);
	}
	void sendHouseWindow(uint32_t windowTextId, House* house, uint32_t listId, const std::string& text)
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendHouseWindow(windowTextId, house, listId, text);
	}

	void sendOutfitWindow()
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendOutfitWindow();
	}
	void sendQuests()
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendQuests();
	}
	void sendQuestInfo(Quest* quest)
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendQuestInfo(quest);
	}

	void sendVIPLogIn(uint32_t guid)
	{
		if (m_owner) {
			m_owner->sendVIPLogIn(guid);
		}
	}
	void sendVIPLogOut(uint32_t guid)
	{
		if (m_owner) {
			m_owner->sendVIPLogOut(guid);
		}
	}
	void sendVIP(uint32_t guid, const std::string& name, bool isOnline)
	{
		if (m_owner) {
			m_owner->sendVIP(guid, name, isOnline);
		}
	}

	void sendCreatureLight(const Creature* creature)
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendCreatureLight(creature);
		for (SpectatorList::iterator it = m_spectators.begin(); it != m_spectators.end(); ++it) {
			it->first->sendCreatureLight(creature);
		}
	}
	void sendWorldLight(const LightInfo& lightInfo)
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendWorldLight(lightInfo);
		for (SpectatorList::iterator it = m_spectators.begin(); it != m_spectators.end(); ++it) {
			it->first->sendWorldLight(lightInfo);
		}
	}

	void sendCreatureSquare(const Creature* creature, uint8_t color)
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendCreatureSquare(creature, color);
		for (SpectatorList::iterator it = m_spectators.begin(); it != m_spectators.end(); ++it) {
			it->first->sendCreatureSquare(creature, color);
		}
	}

	void sendAddTileItem(const Tile* tile, const Position& pos, uint32_t stackpos, const Item* item)
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendAddTileItem(tile, pos, stackpos, item);
		for (SpectatorList::iterator it = m_spectators.begin(); it != m_spectators.end(); ++it) {
			it->first->sendAddTileItem(tile, pos, stackpos, item);
		}
	}
	void sendUpdateTileItem(const Tile* tile, const Position& pos, uint32_t stackpos, const Item* item)
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendUpdateTileItem(tile, pos, stackpos, item);
		for (SpectatorList::iterator it = m_spectators.begin(); it != m_spectators.end(); ++it) {
			it->first->sendUpdateTileItem(tile, pos, stackpos, item);
		}
	}
	void sendRemoveTileItem(const Tile* tile, const Position& pos, uint32_t stackpos)
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendRemoveTileItem(tile, pos, stackpos);
		for (SpectatorList::iterator it = m_spectators.begin(); it != m_spectators.end(); ++it) {
			it->first->sendRemoveTileItem(tile, pos, stackpos);
		}
	}
	void sendUpdateTile(const Tile* tile, const Position& pos)
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendUpdateTile(tile, pos);
		for (SpectatorList::iterator it = m_spectators.begin(); it != m_spectators.end(); ++it) {
			it->first->sendUpdateTile(tile, pos);
		}
	}

	void sendAddCreature(const Creature* creature, const Position& pos, uint32_t stackpos)
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendAddCreature(creature, pos, stackpos);
		for (SpectatorList::iterator it = m_spectators.begin(); it != m_spectators.end(); ++it) {
			it->first->sendAddCreature(creature, pos, stackpos);
		}
	}
	void sendRemoveCreature(const Creature* creature, const Position& pos, uint32_t stackpos)
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendRemoveCreature(creature, pos, stackpos);
		for (SpectatorList::iterator it = m_spectators.begin(); it != m_spectators.end(); ++it) {
			it->first->sendRemoveCreature(creature, pos, stackpos);
		}
	}
	void sendMoveCreature(const Creature* creature, const Tile* newTile, const Position& newPos, uint32_t newStackPos,
		const Tile* oldTile, const Position& oldPos, uint32_t oldStackpos, bool teleport)
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendMoveCreature(creature, newTile, newPos, newStackPos, oldTile, oldPos, oldStackpos, teleport);
		for (SpectatorList::iterator it = m_spectators.begin(); it != m_spectators.end(); ++it) {
			it->first->sendMoveCreature(creature, newTile, newPos, newStackPos, oldTile, oldPos, oldStackpos, teleport);
		}
	}

	void sendAddContainerItem(uint8_t cid, const Item* item)
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendAddContainerItem(cid, item);
		for (SpectatorList::iterator it = m_spectators.begin(); it != m_spectators.end(); ++it) {
			it->first->sendAddContainerItem(cid, item);
		}
	}
	void sendUpdateContainerItem(uint8_t cid, uint8_t slot, const Item* item)
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendUpdateContainerItem(cid, slot, item);
		for (SpectatorList::iterator it = m_spectators.begin(); it != m_spectators.end(); ++it) {
			it->first->sendUpdateContainerItem(cid, slot, item);
		}
	}
	void sendRemoveContainerItem(uint8_t cid, uint8_t slot)
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendRemoveContainerItem(cid, slot);
		for (SpectatorList::iterator it = m_spectators.begin(); it != m_spectators.end(); ++it) {
			it->first->sendRemoveContainerItem(cid, slot);
		}
	}

	void sendContainer(uint32_t cid, const Container* container, bool hasParent)
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendContainer(cid, container, hasParent);
		for (SpectatorList::iterator it = m_spectators.begin(); it != m_spectators.end(); ++it) {
			it->first->sendContainer(cid, container, hasParent);
		}
	}
	void sendCloseContainer(uint32_t cid)
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendCloseContainer(cid);
		for (SpectatorList::iterator it = m_spectators.begin(); it != m_spectators.end(); ++it) {
			it->first->sendCloseContainer(cid);
		}
	}

	void sendAddInventoryItem(slots_t slot, const Item* item)
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendAddInventoryItem(slot, item);
		for (SpectatorList::iterator it = m_spectators.begin(); it != m_spectators.end(); ++it) {
			it->first->sendAddInventoryItem(slot, item);
		}
	}
	void sendUpdateInventoryItem(slots_t slot, const Item* item)
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendUpdateInventoryItem(slot, item);
		for (SpectatorList::iterator it = m_spectators.begin(); it != m_spectators.end(); ++it) {
			it->first->sendUpdateInventoryItem(slot, item);
		}
	}
	void sendRemoveInventoryItem(slots_t slot)
	{
		if (!m_owner) {
			return;
		}

		m_owner->sendRemoveInventoryItem(slot);
		for (SpectatorList::iterator it = m_spectators.begin(); it != m_spectators.end(); ++it) {
			it->first->sendRemoveInventoryItem(slot);
		}
	}
	void sendCastList()
	{
		if (m_owner) {
			m_owner->sendCastList();
		}
	}
};
