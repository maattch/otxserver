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

#include "networkmessage.h"
#include "protocol.h"

class ProtocolStatus;
using ProtocolStatusPtr = std::shared_ptr<ProtocolStatus>;

class ProtocolStatus final : public Protocol
{
public:
	// static protocol information
	enum
	{
		server_sends_first = false
	};
	enum
	{
		protocol_identifier = 0xFF
	};
	enum
	{
		use_checksum = false
	};
	static const char* protocol_name()
	{
		return "status protocol";
	}

	explicit ProtocolStatus(Connection_ptr connection) :
		Protocol(connection) {}

	void onRecvFirstMessage(NetworkMessage& msg) final;

	void sendStatusString();
	void sendInfo(uint16_t requestedInfo, const std::string& characterName);

	ProtocolStatusPtr getThis()
	{
		return std::static_pointer_cast<ProtocolStatus>(shared_from_this());
	}

	static const int64_t start;

protected:
	static std::map<uint32_t, int64_t> ipConnectMap;
};
