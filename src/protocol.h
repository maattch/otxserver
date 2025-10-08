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

#include "connection.h"
#include "xtea.h"

class Protocol : public std::enable_shared_from_this<Protocol>
{
public:
	explicit Protocol(Connection_ptr connection) : m_connection(connection) {}
	virtual ~Protocol() = default;

	// non-copyable
	Protocol(const Protocol&) = delete;
	Protocol& operator=(const Protocol&) = delete;

	virtual void parsePacket(NetworkMessage&) {}

	void onSendMessage(const OutputMessage_ptr& msg);
	void onRecvMessage(NetworkMessage& msg);
	virtual void onRecvFirstMessage(NetworkMessage& msg) = 0;
	virtual void onConnect() {}

	bool isConnectionExpired() const { return m_connection.expired(); }

	Connection_ptr getConnection() const { return m_connection.lock(); }

	uint32_t getIP() const;

	// Use this function for autosend messages only
	OutputMessage_ptr getOutputBuffer();
	OutputMessage_ptr getOutputBuffer(int32_t size);

	OutputMessage_ptr& getCurrentBuffer() { return m_outputBuffer; }

	void send(OutputMessage_ptr msg) const {
		if (auto connection = getConnection()) {
			connection->send(msg);
		}
	}

	void disconnect() const {
		if (auto connection = getConnection()) {
			connection->close();
		}
	}

protected:
	void enableXTEAEncryption() { m_encryptionEnabled = true; }
	void setXTEAKey(otx::xtea::key&& key) { m_key = otx::xtea::expand_key(std::move(key)); }
	void disableChecksum() { m_checksumEnabled = false; }

	static bool RSA_decrypt(NetworkMessage& msg);

	void setRawMessages(bool value) { m_rawMessages = value; }

	virtual void release() {}

	OutputMessage_ptr m_outputBuffer;

private:
	const ConnectionWeak_ptr m_connection;

	otx::xtea::round_keys m_key;
	bool m_encryptionEnabled{ false };
	bool m_checksumEnabled{ true };
	bool m_rawMessages{ false };

	friend class Connection;
};
