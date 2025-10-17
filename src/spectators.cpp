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

#include "spectators.h"

#include "chat.h"
#include "configmanager.h"
#include "database.h"
#include "game.h"
#include "player.h"
#include "tools.h"

#include "otx/util.hpp"

Spectators::Spectators(ProtocolGame_ptr client) :
	m_owner(client),
	m_broadcast_time(otx::util::mstime())
{
	//
}

void Spectators::clear(bool full)
{
	for (const auto& it : m_spectators) {
		if (!it.first->twatchername.empty()) {
			it.first->parseTelescopeBack(true);
		} else {
			it.first->disconnect();
		}
	}

	m_spectators.clear();
	m_mutes.clear();

	m_id = 0;
	if (!full) {
		return;
	}

	m_bans.clear();
	m_password.clear();
	m_broadcast = m_auth = false;
	m_broadcast_time = 0;
}

bool Spectators::check(const std::string& _password)
{
	if (m_password.empty()) {
		return true;
	}

	std::string t = _password;
	otx::util::trim_string(t);
	return t == m_password;
}

void Spectators::sendLook(ProtocolGame* client, const Position& pos, uint16_t spriteId, int16_t stackpos)
{
	if (!m_owner) {
		return;
	}

	auto sit = m_spectators.find(client);
	if (sit == m_spectators.end()) {
		return;
	}

	Thing* thing = g_game.internalGetThing(client->player, pos, stackpos, spriteId, STACKPOS_LOOK);
	if (!thing) {
		return;
	}

	Position thingPos = pos;
	if (pos.x == 0xFFFF) {
		thingPos = thing->getPosition();
	}

	Position playerPos = client->player->getPosition();
	int32_t lookDistance = -1;
	if (thing != client->player) {
		lookDistance = std::max(std::abs(playerPos.x - thingPos.x), std::abs(playerPos.y - thingPos.y));
		if (playerPos.z != thingPos.z) {
			lookDistance += 15;
		}
	}

	std::ostringstream ss;
	ss << "You see " << thing->getDescription(lookDistance);

	if (Item* item = thing->getItem()) {
		if (client->player->hasCustomFlag(PlayerCustomFlag_CanSeeItemDetails)) {
			ss << ".";
			const ItemType& it = Item::items[item->getID()];
			if (it.transformEquipTo) {
				ss << std::endl
				   << "TransformTo: [" << it.transformEquipTo << "] (onEquip).";
			} else if (it.transformDeEquipTo) {
				ss << std::endl
				   << "TransformTo: [" << it.transformDeEquipTo << "] (onDeEquip).";
			}

			if (it.transformUseTo) {
				ss << std::endl
				   << "TransformTo: [" << it.transformUseTo << "] (onUse).";
			}

			if (it.decayTo != -1) {
				ss << std::endl
				   << "DecayTo: [" << it.decayTo << "].";
			}
		}
	}

	client->sendTextMessage(MSG_INFO_DESCR, ss.str());
	return;
}

void Spectators::handle(ProtocolGame* client, const std::string& text, uint16_t channelId)
{
	if (!m_owner) {
		return;
	}

	auto sit = m_spectators.find(client);
	if (sit == m_spectators.end()) {
		return;
	}

	PrivateChatChannel* channel = g_chat.getPrivateChannel(m_owner->getPlayer());
	if (text[0] == '/') {
		StringVec t = explodeString(text.substr(1, text.length()), " ", true, 1);
		otx::util::to_lower_string(t[0]);
		if (t[0] == "show") {
			std::ostringstream s;
			s << m_spectators.size() << " spectators. ";

			bool begin = true;
			for (const auto& it : m_spectators) {
				if (!it.first->spy) {
					if (!begin) {
						s << ", ";
					} else {
						begin = false;
					}

					s << it.second.first;
				}
			}

			s << ".";
			client->sendCreatureSay(m_owner->getPlayer(), MSG_PRIVATE, s.str(), nullptr, 0);
		} else if (t[0] == "name") {
			if (t.size() > 1) {
				if (t[1].length() > 2) {
					if (t[1].length() < 26) {
						t[1] += " [G]";
						bool found = false;
						for (const auto& iit : m_spectators) {
							if (caseInsensitiveEqual(iit.second.first, t[1])) {
								found = true;
								break;
							}
						}

						if (!found) {
							client->sendCreatureSay(m_owner->getPlayer(), MSG_PRIVATE, "Your name has been set to " + t[1] + ".", nullptr, 0);
							if (!m_auth && channel) {
								sendChannelMessage("", sit->second.first + " is now known as " + t[1] + ".", MSG_GAMEMASTER_CHANNEL, channel->getId());
							}

							auto mit = std::find(m_mutes.begin(), m_mutes.end(), otx::util::as_lower_string(sit->second.first));
							if (mit != m_mutes.end()) {
								(*mit) = otx::util::as_lower_string(t[1]);
							}

							sit->second.first = t[1];
							sit->second.second = false;
						} else {
							client->sendCreatureSay(m_owner->getPlayer(), MSG_PRIVATE, "Specified name is already taken.", nullptr, 0);
						}
					} else {
						client->sendCreatureSay(m_owner->getPlayer(), MSG_PRIVATE, "Specified name is too long.", nullptr, 0);
					}
				} else {
					client->sendCreatureSay(m_owner->getPlayer(), MSG_PRIVATE, "Specified name is too short.", nullptr, 0);
				}
			} else {
				client->sendCreatureSay(m_owner->getPlayer(), MSG_PRIVATE, "Not enough param(s) given.", nullptr, 0);
			}
		} else if (t[0] == "auth") {
			if ((time(nullptr) - client->lastCastMsg) < 5) {
				client->sendCreatureSay(m_owner->getPlayer(), MSG_PRIVATE, "You need to wait a bit before trying to authenticate again.", nullptr, 0);
				return;
			}
			client->lastCastMsg = time(nullptr);
			if (t.size() > 1) {
				StringVec _t = explodeString(t[1], " ", true, 1);
				if (_t.size() > 1) {
					std::ostringstream query;

					query << "SELECT `id`, `salt`, `password` FROM `accounts` WHERE `name` = " << g_database.escapeString(_t[0]) << " LIMIT 1";
					if (DBResultPtr result = g_database.storeQuery(query.str())) {
						std::string password = result->getString("salt") + _t[1],
									hash = result->getString("password");
						uint32_t id = result->getNumber<int32_t>("id");

						if (transformToSHA1(password) == hash) {
							query.str("");
							query << "SELECT `name` FROM `players` WHERE `account_id` = " << id << " ORDER BY `level` DESC LIMIT 1";
							if ((result = g_database.storeQuery(query.str()))) {
								std::string nickname = result->getString("name");

								client->sendCreatureSay(m_owner->getPlayer(), MSG_PRIVATE, "You have authenticated as " + nickname + ".", nullptr, 0);
								if (channel) {
									sendChannelMessage("", sit->second.first + " authenticated as " + nickname + ".", MSG_GAMEMASTER_CHANNEL, channel->getId());
								}

								auto mit = std::find(m_mutes.begin(), m_mutes.end(), otx::util::as_lower_string(sit->second.first));
								if (mit != m_mutes.end()) {
									(*mit) = otx::util::as_lower_string(nickname);
								}

								sit->second.first = nickname;
								sit->second.second = true;
							} else {
								client->sendCreatureSay(m_owner->getPlayer(), MSG_PRIVATE, "Your account has no characters yet.", nullptr, 0);
							}
						} else {
							client->sendCreatureSay(m_owner->getPlayer(), MSG_PRIVATE, "Invalid password.", nullptr, 0);
						}
					} else {
						client->sendCreatureSay(m_owner->getPlayer(), MSG_PRIVATE, "Invalid account name.", nullptr, 0);
					}
				} else {
					client->sendCreatureSay(m_owner->getPlayer(), MSG_PRIVATE, "Not enough param(s) given.", nullptr, 0);
				}
			} else {
				client->sendCreatureSay(m_owner->getPlayer(), MSG_PRIVATE, "Not enough param(s) given.", nullptr, 0);
			}
		} else {
			client->sendCreatureSay(m_owner->getPlayer(), MSG_PRIVATE, "Command not found.", nullptr, 0);
		}

		return;
	}

	if (!m_auth || sit->second.second) {
		StringVec::const_iterator mit = std::find(m_mutes.begin(), m_mutes.end(), otx::util::as_lower_string(sit->second.first));
		if (mit == m_mutes.end()) {
			if (channel && channel->getId() == channelId) {
				uint16_t exhaust = otx::config::getInteger(otx::config::EXHAUST_SPECTATOR_SAY);
				if (exhaust <= 2) { // prevents exhaust < 2s
					exhaust = 2;
				}
				if ((time(nullptr) - client->lastCastMsg) < exhaust) {
					uint16_t timeToSpeak = (client->lastCastMsg - time(nullptr) + exhaust);
					client->sendCreatureSay(m_owner->getPlayer(), MSG_PRIVATE, ("You are exhausted, wait " + std::to_string(timeToSpeak) + " second" + (timeToSpeak > 1 ? "s" : "") + " to talk again!"), nullptr, 0);
					return;
				}
				client->lastCastMsg = time(nullptr);
				std::string _text = otx::util::as_lower_string(text);
				StringVec prohibitedWords;
				prohibitedWords = explodeString(otx::config::getString(otx::config::ADVERTISING_BLOCK), ";");
				bool fakeChat = false;

				std::string concatenatedText = g_game.removeNonAlphabetic(_text);
				if (!prohibitedWords.empty() && !_text.empty()) { // if advertising is empty, don't need check nothing
					for (const auto& prohibitedWord : prohibitedWords) {
						if (!prohibitedWord.empty() && concatenatedText.find(prohibitedWord) != std::string::npos) {
							fakeChat = true;
							break;
						}
					}
				}
				channel->talk(sit->second.first, MSG_CHANNEL_HIGHLIGHT, text, fakeChat, client->getIP());
			}
		} else {
			client->sendCreatureSay(m_owner->getPlayer(), MSG_PRIVATE, "You are muted.", nullptr, 0);
		}
	} else {
		client->sendCreatureSay(m_owner->getPlayer(), MSG_PRIVATE, "This chat is protected, you have to authenticate first.", nullptr, 0);
	}
}

void Spectators::chat(uint16_t channelId)
{
	if (!m_owner) {
		return;
	}

	for (auto& spectator : m_spectators) {
		spectator.first->sendClosePrivate(channelId);
		if (channelId == 100) { // idk why cast channelId is 100
			spectator.first->sendCreatureSay(m_owner->getPlayer(), MSG_PRIVATE, "Chat has been disabled.", nullptr, 0);
		}
	}
}

std::vector<std::string> Spectators::list()
{
	std::vector<std::string> t;
	for (const auto& it : m_spectators) {
		t.push_back(it.second.first);
	}
	return t;
}

void Spectators::kick(std::vector<std::string>&& list)
{
	for (const auto& name : list) {
		for (auto it = m_spectators.begin(); it != m_spectators.end(); ++it) {
			if (!it->first->spy && otx::util::as_lower_string(it->second.first) == name) {
				it->first->disconnect();
			}
		}
	}
}

void Spectators::ban(std::vector<std::string>&& bans)
{
	for (auto bit = m_bans.begin(); bit != m_bans.end();) {
		auto it = std::find(bans.begin(), bans.end(), bit->first);
		if (it == bans.end()) {
			bit = m_bans.erase(bit);
		} else {
			++bit;
		}
	}

	for (const std::string& ban : bans) {
		for (const auto& spectator : m_spectators) {
			if (caseInsensitiveEqual(spectator.second.first, ban)) {
				m_bans[ban] = spectator.first->getIP();
				spectator.first->disconnect();
			}
		}
	}
}

bool Spectators::banned(uint32_t ip) const
{
	for (const auto& it : m_bans) {
		if (it.second == ip) {
			return true;
		}
	}
	return false;
}

void Spectators::addSpectator(ProtocolGame* client, std::string name, bool spy)
{
	if (++m_id == 65536) {
		m_id = 1;
	}

	std::ostringstream s;
	if (name.empty()) {
		s << "Spectator " << m_id << "";
	} else {
		s << name << " (Telescope)";
		for (const auto& it : m_spectators) {
			if (it.second.first.compare(name) == 0) {
				s << " " << m_id;
			}
		}
	}

	m_spectators[client] = std::make_pair(s.str(), false);
	if (!spy) {
		sendTextMessage(MSG_EVENT_ORANGE, s.str() + " has entered the cast.");

		std::ostringstream query;

		query << "SELECT `castDescription` FROM `players` WHERE `id` = " << m_owner->getPlayer()->getGUID();
		if (DBResultPtr result = g_database.storeQuery(query.str())) {
			std::string comment = result->getString("castDescription");

			if (comment != "") {
				client->sendCreatureSay(m_owner->getPlayer(), MSG_STATUS_WARNING, comment, nullptr, 0);
			}
		}
	}
}

void Spectators::removeSpectator(ProtocolGame* client, bool spy)
{
	auto it = m_spectators.find(client);
	if (it == m_spectators.end()) {
		return;
	}

	m_mutes.erase(std::remove(m_mutes.begin(), m_mutes.end(), it->second.first), m_mutes.end());

	if (!spy) {
		sendTextMessage(MSG_STATUS_CONSOLE_RED, it->second.first + " has left the cast.");
	}
	m_spectators.erase(it);
}

int64_t Spectators::getBroadcastTime() const
{
	return otx::util::mstime() - m_broadcast_time;
}

bool Spectators::canSee(const Position& pos) const
{
	if (!m_owner) {
		return false;
	}
	return m_owner->canSee(pos);
}

void Spectators::sendChannelMessage(std::string author, std::string text, MessageType_t type, uint16_t channel, bool fakeChat, uint32_t ip)
{
	if (!m_owner) {
		return;
	}

	if (!fakeChat) {
		m_owner->sendChannelMessage(author, text, type, channel);
	}
	PrivateChatChannel* tmp = g_chat.getPrivateChannel(m_owner->getPlayer());
	if (!tmp || tmp->getId() != channel) {
		return;
	}

	for (auto& spectator : m_spectators) {
		if (fakeChat && spectator.first->getIP() != ip) {
			continue;
		}
		spectator.first->sendChannelMessage(author, text, type, channel);
	}
}

void Spectators::sendCreatureSay(const Creature* creature, MessageType_t type, const std::string& text, Position* pos, uint32_t statementId, bool fakeChat)
{
	if (!m_owner) {
		return;
	}

	m_owner->sendCreatureSay(creature, type, text, pos, statementId);
	if (type == MSG_PRIVATE || type == MSG_GAMEMASTER_PRIVATE || type == MSG_NPC_TO) { // care for privacy!
		return;
	}

	if (m_owner->getPlayer()) {
		for (auto& spectator : m_spectators) {
			if (fakeChat && spectator.first->getIP() != m_owner->getIP()) {
				continue;
			}
			spectator.first->sendCreatureSay(creature, type, text, pos, statementId);
		}
	}
}

void Spectators::sendCreatureChannelSay(const Creature* creature, MessageType_t type, const std::string& text, uint16_t channelId, uint32_t statementId, bool fakeChat)
{
	if (!m_owner) {
		return;
	}

	m_owner->sendCreatureChannelSay(creature, type, text, channelId, statementId);
	PrivateChatChannel* tmp = g_chat.getPrivateChannel(m_owner->getPlayer());
	if (!tmp || tmp->getId() != channelId) {
		return;
	}

	if (m_owner->getPlayer()) {
		for (const auto& spectator : m_spectators) {
			if (fakeChat && spectator.first->getIP() != m_owner->getIP()) {
				continue;
			}
			spectator.first->sendCreatureChannelSay(creature, type, text, channelId, statementId);
		}
	}
}

void Spectators::sendClosePrivate(uint16_t channelId)
{
	if (!m_owner) {
		return;
	}

	m_owner->sendClosePrivate(channelId);
	PrivateChatChannel* tmp = g_chat.getPrivateChannel(m_owner->getPlayer());
	if (!tmp || tmp->getId() != channelId) {
		return;
	}

	for (const auto& spectator : m_spectators) {
		spectator.first->sendClosePrivate(channelId);
		spectator.first->sendCreatureSay(m_owner->getPlayer(), MSG_PRIVATE, "Chat has been disabled.", nullptr, 0);
	}
}

void Spectators::sendCreatePrivateChannel(uint16_t channelId, const std::string& channelName)
{
	if (!m_owner) {
		return;
	}

	m_owner->sendCreatePrivateChannel(channelId, channelName);
	PrivateChatChannel* tmp = g_chat.getPrivateChannel(m_owner->getPlayer());
	if (!tmp || tmp->getId() != channelId) {
		return;
	}

	for (const auto& spectator : m_spectators) {
		spectator.first->sendCreatePrivateChannel(channelId, channelName);
		spectator.first->sendCreatureSay(m_owner->getPlayer(), MSG_PRIVATE, "Chat has been enabled.", nullptr, 0);
	}
}

void Spectators::sendChannelsDialog(const ChannelsList& channels)
{
	if (m_owner) {
		m_owner->sendChannelsDialog(channels);
	}
}

void Spectators::sendChannel(uint16_t channelId, const std::string& channelName)
{
	if (!m_owner) {
		return;
	}

	m_owner->sendChannel(channelId, channelName);
	for (const auto& it : m_spectators) {
		it.first->sendChannel(channelId, channelName);
	}
}

void Spectators::sendOpenPrivateChannel(const std::string& receiver)
{
	if (!m_owner) {
		return;
	}

	m_owner->sendOpenPrivateChannel(receiver);
	for (const auto& it : m_spectators) {
		it.first->sendOpenPrivateChannel(receiver);
	}
}

void Spectators::sendIcons(int32_t icons)
{
	if (!m_owner) {
		return;
	}

	m_owner->sendIcons(icons);
	for (const auto& it : m_spectators) {
		it.first->sendIcons(icons);
	}
}

void Spectators::sendFYIBox(const std::string& message)
{
	if (m_owner) {
		m_owner->sendFYIBox(message);
	}
}

void Spectators::sendDistanceShoot(const Position& from, const Position& to, uint16_t type)
{
	if (!m_owner) {
		return;
	}

	m_owner->sendDistanceShoot(from, to, type);
	for (const auto& it : m_spectators) {
		it.first->sendDistanceShoot(from, to, type);
	}
}

void Spectators::sendMagicEffect(const Position& pos, uint16_t type)
{
	if (!m_owner) {
		return;
	}

	m_owner->sendMagicEffect(pos, type);
	for (const auto& it : m_spectators) {
		it.first->sendMagicEffect(pos, type);
	}
}

void Spectators::sendAnimatedText(const Position& pos, uint8_t color, const std::string& text)
{
	if (!m_owner) {
		return;
	}

	m_owner->sendAnimatedText(pos, color, text);
	for (const auto& it : m_spectators) {
		it.first->sendAnimatedText(pos, color, text);
	}
}

void Spectators::sendCreatureHealth(const Creature* creature)
{
	if (!m_owner) {
		return;
	}

	m_owner->sendCreatureHealth(creature);
	for (const auto& it : m_spectators) {
		it.first->sendCreatureHealth(creature);
	}
}

void Spectators::sendSkills()
{
	if (!m_owner) {
		return;
	}

	m_owner->sendSkills();
	for (const auto& it : m_spectators) {
		it.first->sendSkills();
	}
}

void Spectators::sendPing()
{
	if (!m_owner) {
		return;
	}

	m_owner->sendPing();
	for (const auto& it : m_spectators) {
		it.first->sendPing();
	}
}

void Spectators::sendCreatureTurn(const Creature* creature, int16_t stackpos)
{
	if (!m_owner) {
		return;
	}

	m_owner->sendCreatureTurn(creature, stackpos);
	for (const auto& it : m_spectators) {
		it.first->sendCreatureTurn(creature, stackpos);
	}
}

void Spectators::sendCancelWalk()
{
	if (!m_owner) {
		return;
	}

	m_owner->sendCancelWalk();
	for (const auto& it : m_spectators) {
		it.first->sendCancelWalk();
	}
}

void Spectators::sendChangeSpeed(const Creature* creature, uint32_t speed)
{
	if (!m_owner) {
		return;
	}

	m_owner->sendChangeSpeed(creature, speed);
	for (const auto& it : m_spectators) {
		it.first->sendChangeSpeed(creature, speed);
	}
}

void Spectators::sendCancelTarget()
{
	if (!m_owner) {
		return;
	}

	m_owner->sendCancelTarget();
	for (const auto& it : m_spectators) {
		it.first->sendCancelTarget();
	}
}

void Spectators::sendCreatureOutfit(const Creature* creature, const Outfit_t& outfit)
{
	if (!m_owner) {
		return;
	}

	m_owner->sendCreatureOutfit(creature, outfit);
	for (const auto& it : m_spectators) {
		it.first->sendCreatureOutfit(creature, outfit);
	}
}

void Spectators::sendStats()
{
	if (!m_owner) {
		return;
	}

	m_owner->sendStats();
	for (const auto& it : m_spectators) {
		it.first->sendStats();
	}
}

void Spectators::sendTextMessage(MessageType_t type, const std::string& message)
{
	if (!m_owner) {
		return;
	}

	m_owner->sendTextMessage(type, message);
	for (const auto& it : m_spectators) {
		it.first->sendTextMessage(type, message);
	}
}

void Spectators::sendReLoginWindow()
{
	if (m_owner) {
		m_owner->sendReLoginWindow();
		clear(true); // just smoothly disconnect
	}
}

void Spectators::sendTutorial(uint8_t tutorialId)
{
	if (m_owner) {
		m_owner->sendTutorial(tutorialId);
	}
}

void Spectators::sendAddMarker(const Position& pos, MapMarks_t markType, const std::string& desc)
{
	if (m_owner) {
		m_owner->sendAddMarker(pos, markType, desc);
	}
}

void Spectators::sendExtendedOpcode(uint8_t opcode, const std::string& buffer)
{
	if (m_owner) {
		m_owner->sendExtendedOpcode(opcode, buffer);
	}
}

void Spectators::sendRuleViolationsChannel(uint16_t channelId)
{
	if (m_owner) {
		m_owner->sendRuleViolationsChannel(channelId);
	}
}

void Spectators::sendRemoveReport(const std::string& name)
{
	if (m_owner) {
		m_owner->sendRemoveReport(name);
	}
}

void Spectators::sendLockRuleViolation()
{
	if (m_owner) {
		m_owner->sendLockRuleViolation();
	}
}

void Spectators::sendRuleViolationCancel(const std::string& name)
{
	if (m_owner) {
		m_owner->sendRuleViolationCancel(name);
	}
}

void Spectators::sendCreatureSkull(const Creature* creature)
{
	if (!m_owner) {
		return;
	}

	m_owner->sendCreatureSkull(creature);
	for (const auto& it : m_spectators) {
		it.first->sendCreatureSkull(creature);
	}
}

void Spectators::sendCreatureShield(const Creature* creature)
{
	if (!m_owner) {
		return;
	}

	m_owner->sendCreatureShield(creature);
	for (const auto& it : m_spectators) {
		it.first->sendCreatureShield(creature);
	}
}

void Spectators::sendCreatureEmblem(const Creature* creature)
{
	if (!m_owner) {
		return;
	}

	m_owner->sendCreatureEmblem(creature);
	for (const auto& it : m_spectators) {
		it.first->sendCreatureEmblem(creature);
	}
}

void Spectators::sendCreatureWalkthrough(const Creature* creature, bool walkthrough)
{
	if (!m_owner) {
		return;
	}

	m_owner->sendCreatureWalkthrough(creature, walkthrough);
	for (const auto& it : m_spectators) {
		it.first->sendCreatureWalkthrough(creature, walkthrough);
	}
}

void Spectators::sendShop(const ShopInfoList& shop)
{
	if (m_owner) {
		m_owner->sendShop(shop);
	}
}

void Spectators::sendCloseShop()
{
	if (m_owner) {
		m_owner->sendCloseShop();
	}
}

void Spectators::sendGoods(const ShopInfoList& shop)
{
	if (m_owner) {
		m_owner->sendGoods(shop);
	}
}

void Spectators::sendTradeItemRequest(const Player* player, const Item* item, bool ack)
{
	if (m_owner) {
		m_owner->sendTradeItemRequest(player, item, ack);
	}
}

void Spectators::sendCloseTrade()
{
	if (m_owner) {
		m_owner->sendCloseTrade();
	}
}

void Spectators::sendTextWindow(uint32_t windowTextId, Item* item, uint16_t maxLen, bool canWrite)
{
	if (m_owner) {
		m_owner->sendTextWindow(windowTextId, item, maxLen, canWrite);
	}
}

void Spectators::sendHouseWindow(uint32_t windowTextId, const std::string& text)
{
	if (m_owner) {
		m_owner->sendHouseWindow(windowTextId, text);
	}
}

void Spectators::sendOutfitWindow()
{
	if (m_owner) {
		m_owner->sendOutfitWindow();
	}
}

void Spectators::sendQuests()
{
	if (m_owner) {
		m_owner->sendQuests();
	}
}

void Spectators::sendQuestInfo(const Quest* quest)
{
	if (m_owner) {
		m_owner->sendQuestInfo(quest);
	}
}

void Spectators::sendVIPLogIn(uint32_t guid)
{
	if (m_owner) {
		m_owner->sendVIPLogIn(guid);
	}
}

void Spectators::sendVIPLogOut(uint32_t guid)
{
	if (m_owner) {
		m_owner->sendVIPLogOut(guid);
	}
}

void Spectators::sendVIP(uint32_t guid, const std::string& name, bool isOnline)
{
	if (m_owner) {
		m_owner->sendVIP(guid, name, isOnline);
	}
}

void Spectators::sendCreatureLight(const Creature* creature)
{
	if (!m_owner) {
		return;
	}

	m_owner->sendCreatureLight(creature);
	for (const auto& it : m_spectators) {
		it.first->sendCreatureLight(creature);
	}
}

void Spectators::sendWorldLight(const LightInfo& lightInfo)
{
	if (!m_owner) {
		return;
	}

	m_owner->sendWorldLight(lightInfo);
	for (const auto& it : m_spectators) {
		it.first->sendWorldLight(lightInfo);
	}
}

void Spectators::sendCreatureSquare(const Creature* creature, uint8_t color)
{
	if (!m_owner) {
		return;
	}

	m_owner->sendCreatureSquare(creature, color);
	for (const auto& it : m_spectators) {
		it.first->sendCreatureSquare(creature, color);
	}
}

void Spectators::sendAddTileItem(const Position& pos, uint32_t stackpos, const Item* item)
{
	if (!m_owner) {
		return;
	}

	m_owner->sendAddTileItem(pos, stackpos, item);
	for (const auto& it : m_spectators) {
		it.first->sendAddTileItem(pos, stackpos, item);
	}
}

void Spectators::sendUpdateTileItem(const Position& pos, uint32_t stackpos, const Item* item)
{
	if (!m_owner) {
		return;
	}

	m_owner->sendUpdateTileItem(pos, stackpos, item);
	for (const auto& it : m_spectators) {
		it.first->sendUpdateTileItem(pos, stackpos, item);
	}
}

void Spectators::sendRemoveTileItem(const Position& pos, uint32_t stackpos)
{
	if (!m_owner) {
		return;
	}

	m_owner->sendRemoveTileItem(pos, stackpos);
	for (const auto& it : m_spectators) {
		it.first->sendRemoveTileItem(pos, stackpos);
	}
}

void Spectators::sendUpdateTile(const Tile* tile, const Position& pos)
{
	if (!m_owner) {
		return;
	}

	m_owner->sendUpdateTile(tile, pos);
	for (const auto& it : m_spectators) {
		it.first->sendUpdateTile(tile, pos);
	}
}

void Spectators::sendAddCreature(const Creature* creature, const Position& pos, uint32_t stackpos)
{
	if (!m_owner) {
		return;
	}

	m_owner->sendAddCreature(creature, pos, stackpos);
	for (const auto& it : m_spectators) {
		it.first->sendAddCreature(creature, pos, stackpos);
	}
}

void Spectators::sendRemoveCreature(const Position& pos, uint32_t stackpos)
{
	if (!m_owner) {
		return;
	}

	m_owner->sendRemoveCreature(pos, stackpos);
	for (const auto& it : m_spectators) {
		it.first->sendRemoveCreature(pos, stackpos);
	}
}

void Spectators::sendMoveCreature(const Creature* creature, const Position& newPos, uint32_t newStackPos,
	const Position& oldPos, uint32_t oldStackpos, bool teleport)
{
	if (!m_owner) {
		return;
	}

	m_owner->sendMoveCreature(creature, newPos, newStackPos, oldPos, oldStackpos, teleport);
	for (const auto& it : m_spectators) {
		it.first->sendMoveCreature(creature, newPos, newStackPos, oldPos, oldStackpos, teleport);
	}
}

void Spectators::sendAddContainerItem(uint8_t cid, const Item* item)
{
	if (!m_owner) {
		return;
	}

	m_owner->sendAddContainerItem(cid, item);
	for (const auto& it : m_spectators) {
		it.first->sendAddContainerItem(cid, item);
	}
}

void Spectators::sendUpdateContainerItem(uint8_t cid, uint8_t slot, const Item* item)
{
	if (!m_owner) {
		return;
	}

	m_owner->sendUpdateContainerItem(cid, slot, item);
	for (const auto& it : m_spectators) {
		it.first->sendUpdateContainerItem(cid, slot, item);
	}
}

void Spectators::sendRemoveContainerItem(uint8_t cid, uint8_t slot)
{
	if (!m_owner) {
		return;
	}

	m_owner->sendRemoveContainerItem(cid, slot);
	for (const auto& it : m_spectators) {
		it.first->sendRemoveContainerItem(cid, slot);
	}
}

void Spectators::sendContainer(uint32_t cid, const Container* container, bool hasParent)
{
	if (!m_owner) {
		return;
	}

	m_owner->sendContainer(cid, container, hasParent);
	for (const auto& it : m_spectators) {
		it.first->sendContainer(cid, container, hasParent);
	}
}

void Spectators::sendCloseContainer(uint32_t cid)
{
	if (!m_owner) {
		return;
	}

	m_owner->sendCloseContainer(cid);
	for (const auto& it : m_spectators) {
		it.first->sendCloseContainer(cid);
	}
}

void Spectators::sendAddInventoryItem(uint8_t slot, const Item* item)
{
	if (!m_owner) {
		return;
	}

	m_owner->sendAddInventoryItem(slot, item);
	for (const auto& it : m_spectators) {
		it.first->sendAddInventoryItem(slot, item);
	}
}

void Spectators::sendUpdateInventoryItem(uint8_t slot, const Item* item)
{
	if (!m_owner) {
		return;
	}

	m_owner->sendUpdateInventoryItem(slot, item);
	for (const auto& it : m_spectators) {
		it.first->sendUpdateInventoryItem(slot, item);
	}
}

void Spectators::sendRemoveInventoryItem(uint8_t slot)
{
	if (!m_owner) {
		return;
	}

	m_owner->sendRemoveInventoryItem(slot);
	for (const auto& it : m_spectators) {
		it.first->sendRemoveInventoryItem(slot);
	}
}

void Spectators::sendCastList()
{
	if (m_owner) {
		m_owner->sendCastList();
	}
}
