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

class Protocol;
using Protocol_ptr = std::shared_ptr<Protocol>;

class OutputMessage;
using OutputMessage_ptr = std::shared_ptr<OutputMessage>;

class Connection;
using Connection_ptr = std::shared_ptr<Connection>;
using ConnectionWeak_ptr = std::weak_ptr<Connection>;

class ServiceBase;
using Service_ptr = std::shared_ptr<ServiceBase>;

class ServicePort;
using ServicePort_ptr = std::shared_ptr<ServicePort>;
using ConstServicePort_ptr = std::shared_ptr<const ServicePort>;

class ConnectionManager final
{
public:
	// non-copyable
	ConnectionManager(const ConnectionManager&) = delete;
	ConnectionManager& operator=(const ConnectionManager&) = delete;

	static ConnectionManager& getInstance() {
		static ConnectionManager instance;
		return instance;
	}

	Connection_ptr createConnection(boost::asio::io_context& io_context, ConstServicePort_ptr servicePort);
	void releaseConnection(const Connection_ptr& connection);
	void closeAll();

private:
	ConnectionManager() = default;

	std::unordered_set<Connection_ptr> connections;
	std::mutex connectionManagerLock;
};

class Connection final : public std::enable_shared_from_this<Connection>
{
public:
	static constexpr bool FORCE_CLOSE = true;
#if ENABLE_SERVER_DIAGNOSTIC > 0
	static uint32_t connectionCount;
#endif

	Connection(boost::asio::io_context& io_context, ConstServicePort_ptr service_port);
	~Connection();

	// non-copyable
	Connection(const Connection&) = delete;
	Connection& operator=(const Connection&) = delete;

	void close(bool force = false);

	// Used by protocols that require server to send first
	void accept(Protocol_ptr protocol);
	void accept();

	void send(const OutputMessage_ptr& msg);

	uint32_t getIP();

private:
	void parseHeader(const boost::system::error_code& error);
	void parsePacket(const boost::system::error_code& error);

	void onWriteOperation(const boost::system::error_code& error);

	static void handleTimeout(ConnectionWeak_ptr connectionWeak, const boost::system::error_code& error);

	void closeSocket();
	void internalSend(const OutputMessage_ptr& msg);

	boost::asio::ip::tcp::socket& getSocket() { return m_socket; }

	NetworkMessage m_msg;

	boost::asio::steady_timer m_readTimer;
	boost::asio::steady_timer m_writeTimer;

	std::recursive_mutex m_connectionLock;

	boost::asio::ip::tcp::socket m_socket;

	std::list<OutputMessage_ptr> m_messageQueue;

	ConstServicePort_ptr m_service_port;
	Protocol_ptr m_protocol;

	time_t m_timeConnected{ 0 };
	uint32_t m_packetsSent{ 0 };
	uint32_t m_ipAddress{ 0 };

	bool m_receivedFirst{ false };
	bool m_closed{ false };

	friend class ServicePort;
	friend class ConnectionManager;
};
