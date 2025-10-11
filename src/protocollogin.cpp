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

#include "protocollogin.h"

#include "configmanager.h"
#include "connection.h"
#include "const.h"
#include "game.h"
#include "ioban.h"
#include "iologindata.h"
#include "outputmessage.h"

#if ENABLE_SERVER_DIAGNOSTIC > 0
uint32_t ProtocolLogin::protocolLoginCount = 0;
#endif

void ProtocolLogin::disconnectClient(uint8_t error, const char* message)
{
	OutputMessage_ptr output = OutputMessagePool::getOutputMessage();
	output->addByte(error);
	output->addString(message);
	send(output);
	disconnect();
}

void ProtocolLogin::onRecvFirstMessage(NetworkMessage& msg)
{
	if (g_game.getGameState() == GAMESTATE_SHUTDOWN) {
		disconnect();
		return;
	}

	auto connection = getConnection();
	if (!connection) {
		return;
	}

	uint32_t clientIp = connection->getIP();
	msg.skipBytes(2); // client platform
	uint16_t version = msg.get<uint16_t>();

	// skip
	// dat signature (4 bytes)
	// spr signature (4 bytes)
	// pic signature (4 bytes)
	msg.skipBytes(12);

	if (!RSA_decrypt(msg)) {
		disconnect();
		return;
	}

	otx::xtea::key key;
	key[0] = msg.get<uint32_t>();
	key[1] = msg.get<uint32_t>();
	key[2] = msg.get<uint32_t>();
	key[3] = msg.get<uint32_t>();

	enableXTEAEncryption();
	setXTEAKey(std::move(key));

	std::string name = msg.getString(), password = msg.getString();
	if (name.empty()) {
		name = "10";
	}

	if (!otx::config::getBoolean(otx::config::MANUAL_ADVANCED_CONFIG)) {
		if (version < otx::config::getInteger(otx::config::VERSION_MIN) || version > otx::config::getInteger(otx::config::VERSION_MAX)) {
			disconnectClient(0x14, otx::config::getString(otx::config::VERSION_MSG).c_str());
			return;
		}
	} else {
		if (version < CLIENT_VERSION_MIN || version > CLIENT_VERSION_MAX) {
			disconnectClient(0x14, "Only clients with protocol " CLIENT_VERSION_STRING " allowed!");
			return;
		}
	}

	if (g_game.getGameState() < GAMESTATE_NORMAL) {
		disconnectClient(0x0A, "Server is just starting up, please wait.");
		return;
	}

	if (g_game.getGameState() == GAMESTATE_MAINTAIN) {
		disconnectClient(0x0A, "Server is under maintenance, please re-connect in a while.");
		return;
	}

	if (IOBan::getInstance()->isIpBanished(clientIp)) {
		disconnectClient(0x0A, "Your IP is banished!");
		return;
	}

	Account account;
	if (!IOLoginData::getInstance()->loadAccount(account, name) || (account.name != "10" && transformToSHA1(account.salt + password) != account.password)) {
		disconnectClient(0x0A, "Invalid account name or password.");
		return;
	}

	Ban ban;
	ban.value = account.number;

	ban.type = BAN_ACCOUNT;
	if (IOBan::getInstance()->getData(ban) && !IOLoginData::getInstance()->hasFlag(account.number, PlayerFlag_CannotBeBanned)) {
		bool deletion = ban.expires < 0;
		std::string name_ = "Automatic ";
		if (!ban.adminId) {
			name_ += (deletion ? "deletion" : "banishment");
		} else {
			IOLoginData::getInstance()->getNameByGuid(ban.adminId, name_);
		}

		std::ostringstream ss;
		ss << "Your account has been " << (deletion ? "deleted" : "banished") << " at:\n"
		   << formatDateEx(ban.added, "%d %b %Y").c_str()
		   << " by: " << name_.c_str() << ".\nThe comment given was:\n"
		   << ban.comment.c_str() << ".\nYour " << (deletion ? "account won't be undeleted" : "banishment will be lifted at:\n") << (deletion ? "" : formatDateEx(ban.expires).c_str()) << ".";

		disconnectClient(0x0A, ss.str().c_str());
		return;
	}

	// remove premium days
	IOLoginData::getInstance()->removePremium(account);
	if (account.name != "10" && !otx::config::getBoolean(otx::config::ACCOUNT_MANAGER) && !account.charList.size()) {
		disconnectClient(0x0A, std::string("This account does not contain any character yet.\nCreate a new character on the " + otx::config::getString(otx::config::SERVER_NAME) + " website at " + otx::config::getString(otx::config::URL) + ".").c_str());
		return;
	}

	OutputMessage_ptr output = OutputMessagePool::getOutputMessage();
	output->addByte(0x14);

	char motd[1300];
	if (account.name == "10" && account.name != "0") {
		srand(time(nullptr));
		int random_number = std::rand();
		sprintf(motd, "%d\nWelcome to cast system!\n\n Do you know you can use CTRL + ARROWS\n to switch casts?\n\nVoc� sabia que pode usar CTRL + SETAS\n para alternar casts?", random_number);
		output->addString(motd);
	} else {
		sprintf(motd, "%d\n%s", g_game.getMotdId(), otx::config::getString(otx::config::MOTD).c_str());
		output->addString(motd);
	}

	// Add char list
	output->addByte(0x64);
	if (account.name == "10" && account.name != "0") {
		std::vector<Player*> players;
		for (const auto& it : g_game.getPlayers()) {
			if (it.second->m_client->isBroadcasting()) {
				players.push_back(it.second);
			}
		}

		if (!players.size()) {
			disconnectClient(0x0A, "There are no livestreams online right now.");
		} else {
			// sort alphabetically
			std::sort(players.begin(), players.end(), [](const Player* lhs, const Player* rhs) {
				return lhs->getName() < rhs->getName();
			});

			output->addByte(players.size());
			for (Player* player : players) {
				std::ostringstream s;
				s << "L." << player->getLevel() << " | " << player->m_client->list().size() << "/50";
				if (!player->m_client->check(password)) {
					s << " *";
				}

				output->addString(player->getName());
				output->addString(s.str());
				output->add<uint32_t>(otx::config::getIPNumber());
				output->add<uint16_t>(otx::config::getInteger(otx::config::GAME_PORT));
			}
		}
	} else {
		if (otx::config::getBoolean(otx::config::ACCOUNT_MANAGER) && account.number != 1) {
			output->addByte(account.charList.size() + 1);
			output->addString("Account Manager");

			output->addString(otx::config::getString(otx::config::SERVER_NAME));
			output->add<uint32_t>(otx::config::getIPNumber());
			output->add<uint16_t>(otx::config::getInteger(otx::config::GAME_PORT));
		} else {
			output->addByte((uint8_t)account.charList.size());
		}

		for (const std::string& charName : account.charList) {
			output->addString(charName);
			if (otx::config::getBoolean(otx::config::ON_OR_OFF_CHARLIST)) {
				if (g_game.getPlayerByName(charName)) {
					output->addString("Online");
				} else {
					output->addString("Offline");
				}
			} else {
				output->addString(otx::config::getString(otx::config::SERVER_NAME));
			}

			output->add<uint32_t>(otx::config::getIPNumber());
			output->add<uint16_t>(otx::config::getInteger(otx::config::GAME_PORT));
		}
	}

	// Add premium days
	if (otx::config::getBoolean(otx::config::FREE_PREMIUM)) {
		output->add<uint16_t>(GRATIS_PREMIUM);
	} else {
		output->add<uint16_t>(account.premiumDays);
	}

	send(output);
	disconnect();
}
