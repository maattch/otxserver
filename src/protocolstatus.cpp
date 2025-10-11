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

#include "protocolstatus.h"

#include "configmanager.h"
#include "game.h"
#include "networkmessage.h"
#include "outputmessage.h"

#include "otx/util.hpp"

#if ENABLE_SERVER_DIAGNOSTIC > 0
uint32_t ProtocolStatus::protocolStatusCount = 0;
#endif

namespace
{
	std::map<uint32_t, int64_t> connectedIpsMap;

	constexpr uint8_t REQUEST_BASIC_SERVER_INFO    = 1 << 0;
	constexpr uint8_t REQUEST_OWNER_SERVER_INFO    = 1 << 1;
	constexpr uint8_t REQUEST_MISC_SERVER_INFO     = 1 << 2;
	constexpr uint8_t REQUEST_PLAYERS_INFO         = 1 << 3;
	constexpr uint8_t REQUEST_MAP_INFO             = 1 << 4;
	constexpr uint8_t REQUEST_EXT_PLAYERS_INFO     = 1 << 5;
	constexpr uint8_t REQUEST_PLAYER_STATUS_INFO   = 1 << 6;
	constexpr uint8_t REQUEST_SERVER_SOFTWARE_INFO = 1 << 7;

} // namespace

void ProtocolStatus::onRecvFirstMessage(NetworkMessage& msg)
{
	const int64_t timeNow = otx::util::mstime();
	const uint32_t ip = getIP();
	if (ip != 0x0100007F && ip != otx::config::getIPNumber()) {
		auto it = connectedIpsMap.find(ip);
		if (it != connectedIpsMap.end() && (timeNow < (it->second + otx::config::getInteger(otx::config::STATUSQUERY_TIMEOUT)))) {
			disconnect();
			return;
		}
	}

	connectedIpsMap[ip] = timeNow;

	uint8_t type = msg.get<char>();
	switch (type) {
		case 0xFF: {
			if (msg.getString(4) == "info") {
				addDispatcherTask([self = getThis()]() { self->sendStatusString(); });
				return;
			}

			break;
		}

		case 0x01: {
			uint32_t requestedInfo = msg.get<uint16_t>(); // only a byte is necessary, though we could add new infos here
			std::string characterName;
			if (requestedInfo & REQUEST_PLAYER_STATUS_INFO) {
				characterName = msg.getString();
			}

			addDispatcherTask(([self = getThis(), requestedInfo, characterName = std::move(characterName)]() {
				self->sendInfo(requestedInfo, characterName);
			}));
			return;
		}

		default:
			break;
	}

	disconnect();
}

void ProtocolStatus::sendStatusString()
{
	auto output = OutputMessagePool::getOutputMessage();

	setRawMessages(true);

	xmlDocPtr doc = xmlNewDoc((const xmlChar*)"1.0");
	doc->children = xmlNewDocNode(doc, nullptr, (const xmlChar*)"tsqp", nullptr);
	xmlNodePtr root = doc->children;

	char buffer[90];
	xmlSetProp(root, (const xmlChar*)"version", (const xmlChar*)"1.0");

	xmlNodePtr p = xmlNewNode(nullptr, (const xmlChar*)"serverinfo");
	sprintf(buffer, "%u", static_cast<uint32_t>(g_game.getUptime()));
	xmlSetProp(p, (const xmlChar*)"uptime", (const xmlChar*)buffer);
	xmlSetProp(p, (const xmlChar*)"ip", (const xmlChar*)otx::config::getString(otx::config::IP).c_str());
	xmlSetProp(p, (const xmlChar*)"servername", (const xmlChar*)otx::config::getString(otx::config::SERVER_NAME).c_str());
	sprintf(buffer, "%d", (int32_t)otx::config::getInteger(otx::config::LOGIN_PORT));
	xmlSetProp(p, (const xmlChar*)"port", (const xmlChar*)buffer);
	xmlSetProp(p, (const xmlChar*)"location", (const xmlChar*)otx::config::getString(otx::config::LOCATION).c_str());
	xmlSetProp(p, (const xmlChar*)"url", (const xmlChar*)otx::config::getString(otx::config::URL).c_str());
	xmlSetProp(p, (const xmlChar*)"server", (const xmlChar*)SOFTWARE_NAME);
	xmlSetProp(p, (const xmlChar*)"version", (const xmlChar*)SOFTWARE_VERSION);
	xmlSetProp(p, (const xmlChar*)"client", (const xmlChar*)CLIENT_VERSION_STRING);
	xmlAddChild(root, p);

	p = xmlNewNode(nullptr, (const xmlChar*)"owner");
	xmlSetProp(p, (const xmlChar*)"name", (const xmlChar*)otx::config::getString(otx::config::OWNER_NAME).c_str());
	xmlSetProp(p, (const xmlChar*)"email", (const xmlChar*)otx::config::getString(otx::config::OWNER_EMAIL).c_str());
	xmlAddChild(root, p);

	p = xmlNewNode(nullptr, (const xmlChar*)"players");

	uint32_t realOnline = 0;
	uint32_t uniqueOnline = 0;
	std::map<uint32_t, uint32_t> listIP;
	for (const auto& it : g_game.getPlayers()) {
		uint32_t ipAddress = it.second->getIP();
		if (ipAddress != 0 && it.second->getIdleTime() < 960000) {
			auto& count = listIP[ipAddress];
			if (count == 0) {
				++uniqueOnline;
			}

			if (++count <= 4) {
				++realOnline;
			}
		}
	}

	sprintf(buffer, "%d", realOnline);
	xmlSetProp(p, (const xmlChar*)"online", (const xmlChar*)buffer);

	sprintf(buffer, "%d", uniqueOnline);
	xmlSetProp(p, (const xmlChar*)"unique", (const xmlChar*)buffer);

	sprintf(buffer, "%d", (int32_t)otx::config::getInteger(otx::config::MAX_PLAYERS));
	xmlSetProp(p, (const xmlChar*)"max", (const xmlChar*)buffer);

	sprintf(buffer, "%d", g_game.getPlayersRecord());
	xmlSetProp(p, (const xmlChar*)"peak", (const xmlChar*)buffer);

	// this get function in game.cpp with limit = 1 by IP (Unique Players) to otservlist. Xinn can check here https://github.com/FeTads/otxserver/blob/a9bef7ac0fe7584a924a7426aae0f44ec372fe12/sources/game.cpp#L7108
	// sprintf(buffer, "%d", g_game.getUniquePlayersOnline());

	xmlAddChild(root, p);

	p = xmlNewNode(nullptr, (const xmlChar*)"monsters");
	sprintf(buffer, "%d", g_game.getMonstersOnline());
	xmlSetProp(p, (const xmlChar*)"total", (const xmlChar*)buffer);
	xmlAddChild(root, p);

	p = xmlNewNode(nullptr, (const xmlChar*)"npcs");
	sprintf(buffer, "%d", g_game.getNpcsOnline());
	xmlSetProp(p, (const xmlChar*)"total", (const xmlChar*)buffer);
	xmlAddChild(root, p);

	p = xmlNewNode(nullptr, (const xmlChar*)"map");
	xmlSetProp(p, (const xmlChar*)"name", (const xmlChar*)otx::config::getString(otx::config::MAP_NAME).c_str());
	xmlSetProp(p, (const xmlChar*)"author", (const xmlChar*)otx::config::getString(otx::config::MAP_AUTHOR).c_str());

	uint32_t mapWidth, mapHeight;
	g_game.getMapDimensions(mapWidth, mapHeight);
	sprintf(buffer, "%u", mapWidth);
	xmlSetProp(p, (const xmlChar*)"width", (const xmlChar*)buffer);
	sprintf(buffer, "%u", mapHeight);

	xmlSetProp(p, (const xmlChar*)"height", (const xmlChar*)buffer);
	xmlAddChild(root, p);

	xmlNewTextChild(root, nullptr, (const xmlChar*)"motd", (const xmlChar*)otx::config::getString(otx::config::MOTD).c_str());

	xmlChar* s = nullptr;
	int32_t len = 0;
	xmlDocDumpMemory(doc, (xmlChar**)&s, &len);

	std::string xml;
	if (s) {
		xml = std::string((char*)s, len);
	}

	xmlFree(s);
	xmlFreeDoc(doc);

	output->addBytes(xml.c_str(), xml.size());
	send(output);
	disconnect();
}

void ProtocolStatus::sendInfo(uint16_t requestedInfo, const std::string& characterName)
{
	auto output = OutputMessagePool::getOutputMessage();

	if (requestedInfo & REQUEST_BASIC_SERVER_INFO) {
		output->addByte(0x10);
		output->addString(otx::config::getString(otx::config::SERVER_NAME).c_str());
		output->addString(otx::config::getString(otx::config::IP).c_str());

		char buffer[10];
		sprintf(buffer, "%d", (int32_t)otx::config::getInteger(otx::config::LOGIN_PORT));
		output->addString(buffer);
	}

	if (requestedInfo & REQUEST_OWNER_SERVER_INFO) {
		output->addByte(0x11);
		output->addString(otx::config::getString(otx::config::OWNER_NAME).c_str());
		output->addString(otx::config::getString(otx::config::OWNER_EMAIL).c_str());
	}

	if (requestedInfo & REQUEST_MISC_SERVER_INFO) {
		output->addByte(0x12);
		output->addString(otx::config::getString(otx::config::MOTD).c_str());
		output->addString(otx::config::getString(otx::config::LOCATION).c_str());
		output->addString(otx::config::getString(otx::config::URL).c_str());
		output->add<uint64_t>(g_game.getUptime());
	}

	if (requestedInfo & REQUEST_PLAYERS_INFO) {
		output->addByte(0x20);
		output->add<uint32_t>(g_game.getPlayersOnline());
		output->add<uint32_t>((uint32_t)otx::config::getInteger(otx::config::MAX_PLAYERS));
		output->add<uint32_t>(g_game.getPlayersRecord());
	}

	if (requestedInfo & REQUEST_MAP_INFO) {
		output->addByte(0x30);
		output->addString(otx::config::getString(otx::config::MAP_NAME).c_str());
		output->addString(otx::config::getString(otx::config::MAP_AUTHOR).c_str());

		uint32_t mapWidth, mapHeight;
		g_game.getMapDimensions(mapWidth, mapHeight);
		output->add<uint16_t>(mapWidth);
		output->add<uint16_t>(mapHeight);
	}

	if (requestedInfo & REQUEST_EXT_PLAYERS_INFO) {
		output->addByte(0x21);
		std::list<std::pair<std::string, uint32_t>> players;
		for (const auto& it : g_game.getPlayers()) {
			if (!it.second->isGhost()) {
				players.emplace_back(it.second->getName(), it.second->getLevel());
			}
		}

		output->add<uint32_t>(players.size());
		for (std::list<std::pair<std::string, uint32_t>>::iterator it = players.begin(); it != players.end(); ++it) {
			output->addString(it->first);
			output->add<uint32_t>(it->second);
		}
	}

	if (requestedInfo & REQUEST_PLAYER_STATUS_INFO) {
		output->addByte(0x22);

		Player* p = nullptr;
		if (g_game.getPlayerByNameWildcard(characterName, p) == RET_NOERROR && !p->isGhost()) {
			output->addByte(0x01);
		} else {
			output->addByte(0x00);
		}
	}

	if (requestedInfo & REQUEST_SERVER_SOFTWARE_INFO) {
		output->addByte(0x23);
		output->addString(SOFTWARE_NAME);
		output->addString(SOFTWARE_VERSION);
		output->addString(CLIENT_VERSION_STRING);
	}

	send(output);
	disconnect();
}
