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

#include "connection.h"

#include "configmanager.h"
#include "dispatcher.h"
#include "outputmessage.h"
#include "protocol.h"
#include "server.h"
#include "tools.h"

#if ENABLE_SERVER_DIAGNOSTIC > 0
uint32_t Connection::connectionCount = 0;
#endif

constexpr uint32_t CONNECTION_READ_TIMEOUT = 30;
constexpr uint32_t CONNECTION_WRITE_TIMEOUT = 30;

Connection_ptr ConnectionManager::createConnection(boost::asio::io_context& io_context, ConstServicePort_ptr servicePort)
{
	std::lock_guard<std::mutex> lockClass(connectionManagerLock);

	auto connection = std::make_shared<Connection>(io_context, servicePort);
	connections.insert(connection);
	return connection;
}

void ConnectionManager::releaseConnection(const Connection_ptr& connection)
{
	std::lock_guard<std::mutex> lockClass(connectionManagerLock);

	connections.erase(connection);
}

void ConnectionManager::closeAll()
{
	std::lock_guard<std::mutex> lockClass(connectionManagerLock);

	for (const auto& connection : connections) {
		try {
			boost::system::error_code error;
			connection->m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, error);
			connection->m_socket.close(error);
		} catch (boost::system::system_error&) {
			//
		}
	}
	connections.clear();
}

// Connection
Connection::Connection(boost::asio::io_context& io_context, ConstServicePort_ptr service_port) :
	m_readTimer(io_context),
	m_writeTimer(io_context),
	m_socket(io_context),
	m_service_port(std::move(service_port)),
	m_timeConnected(time(nullptr))
{
#if ENABLE_SERVER_DIAGNOSTIC > 0
	++Connection::connectionCount;
#endif
}

Connection::~Connection()
{
	closeSocket();
#if ENABLE_SERVER_DIAGNOSTIC > 0
	--Connection::connectionCount;
#endif
}

void Connection::close(bool force)
{
	// any thread
	ConnectionManager::getInstance().releaseConnection(shared_from_this());

	std::lock_guard<std::recursive_mutex> lockClass(m_connectionLock);
	if (m_closed) {
		return;
	}

	m_closed = true;

	if (m_protocol) {
		addDispatcherTask([protocol = m_protocol]() { protocol->release(); });
	}

	if (m_messageQueue.empty() || force) {
		closeSocket();
	} else {
		// will be closed by the destructor or onWriteOperation
	}
}

void Connection::closeSocket()
{
	if (m_socket.is_open()) {
		try {
			m_readTimer.cancel();
			m_writeTimer.cancel();
			boost::system::error_code error;
			m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, error);
			m_socket.close(error);
		} catch (boost::system::system_error& e) {
			std::cout << "[Network error - Connection::closeSocket] " << e.what() << std::endl;
		}
	}
}

void Connection::accept(Protocol_ptr protocol)
{
	m_protocol = protocol;
	addDispatcherTask([=]() { protocol->onConnect(); });
	accept();
}

void Connection::accept()
{
	std::lock_guard<std::recursive_mutex> lockClass(m_connectionLock);

	boost::system::error_code error;
	if (auto endpoint = m_socket.remote_endpoint(error); !error) {
		m_ipAddress = htonl(endpoint.address().to_v4().to_uint());
	}

	try {
		m_readTimer.expires_after(std::chrono::seconds(CONNECTION_READ_TIMEOUT));
		m_readTimer.async_wait(
			[thisPtr = std::weak_ptr<Connection>(shared_from_this())](const boost::system::error_code& error) {
			Connection::handleTimeout(thisPtr, error);
		});

		// Read size of the first packet
		boost::asio::async_read(m_socket,
			boost::asio::buffer(m_msg.getBuffer(), NetworkMessage::HEADER_LENGTH),
			[thisPtr = shared_from_this()](const boost::system::error_code& error, size_t /*bytes_transferred*/) {
			thisPtr->parseHeader(error);
		});
	} catch (boost::system::system_error& e) {
		std::cout << "[Network error - Connection::accept] " << e.what() << std::endl;
		close(FORCE_CLOSE);
	}
}

void Connection::parseHeader(const boost::system::error_code& error)
{
	std::lock_guard<std::recursive_mutex> lockClass(m_connectionLock);
	m_readTimer.cancel();

	if (error) {
		close(FORCE_CLOSE);
		return;
	} else if (m_closed) {
		return;
	}

	const time_t timeNow = time(nullptr);
	const uint32_t timePassed = std::max<uint32_t>(1, (timeNow - m_timeConnected) + 1);
	if ((++m_packetsSent / timePassed) > static_cast<uint32_t>(otx::config::getInteger(otx::config::MAX_PACKETS_PER_SECOND))) {
		std::cout << convertIPAddress(getIP()) << " disconnected for exceeding packet per second limit." << std::endl;
		close();
		return;
	}

	if (timePassed > 2) {
		m_timeConnected = timeNow;
		m_packetsSent = 0;
	}

	const uint16_t size = m_msg.getLengthHeader();
	if (size == 0 || size >= NETWORKMESSAGE_MAXSIZE - 16) {
		close(FORCE_CLOSE);
		return;
	}

	try {
		m_readTimer.expires_after(std::chrono::seconds(CONNECTION_READ_TIMEOUT));
		m_readTimer.async_wait(
			[thisPtr = std::weak_ptr<Connection>(shared_from_this())](const boost::system::error_code& error) {
			Connection::handleTimeout(thisPtr, error);
		});

		// Read packet content
		m_msg.setLength(size + NetworkMessage::HEADER_LENGTH);
		boost::asio::async_read(
			m_socket,
			boost::asio::buffer(m_msg.getBodyBuffer(), size),
			[thisPtr = shared_from_this()](const boost::system::error_code& error, size_t /*bytes_transferred*/) {
			thisPtr->parsePacket(error);
		});
	} catch (boost::system::system_error& e) {
		std::cout << "[Network error - Connection::parseHeader] " << e.what() << std::endl;
		close(FORCE_CLOSE);
	}
}

void Connection::parsePacket(const boost::system::error_code& error)
{
	std::lock_guard<std::recursive_mutex> lockClass(m_connectionLock);
	m_readTimer.cancel();

	if (error) {
		close(FORCE_CLOSE);
		return;
	} else if (m_closed) {
		return;
	}

	// Check packet checksum
	uint32_t checksum;
	const int32_t len = m_msg.getLength() - m_msg.getBufferPosition() - NetworkMessage::CHECKSUM_LENGTH;
	if (len > 0) {
		checksum = adlerChecksum(m_msg.getBuffer() + m_msg.getBufferPosition() + NetworkMessage::CHECKSUM_LENGTH, len);
	} else {
		checksum = 0;
	}

	const auto recvChecksum = m_msg.get<uint32_t>();
	if (recvChecksum != checksum) {
		// it might not have been the checksum, step back
		m_msg.skipBytes(-NetworkMessage::CHECKSUM_LENGTH);
	}

	if (!m_receivedFirst) {
		m_receivedFirst = true;

		if (!m_protocol) {
			// Game protocol has already been created at this point
			m_protocol = m_service_port->make_protocol(m_msg, shared_from_this(), recvChecksum == checksum);
			if (!m_protocol) {
				close(FORCE_CLOSE);
				return;
			}
		} else {
			m_msg.skipBytes(1); // Skip protocol ID
		}

		m_protocol->onRecvFirstMessage(m_msg);
	} else {
		m_protocol->onRecvMessage(m_msg); // Send the packet to the current protocol
	}

	try {
		m_readTimer.expires_after(std::chrono::seconds(CONNECTION_READ_TIMEOUT));
		m_readTimer.async_wait(
			[thisPtr = std::weak_ptr<Connection>(shared_from_this())](const boost::system::error_code& error) {
			Connection::handleTimeout(thisPtr, error);
		});

		// Wait to the next packet
		boost::asio::async_read(m_socket,
			boost::asio::buffer(m_msg.getBuffer(), NetworkMessage::HEADER_LENGTH),
			[thisPtr = shared_from_this()](const boost::system::error_code& error, size_t /*bytes_transferred*/) {
			thisPtr->parseHeader(error);
		});
	} catch (boost::system::system_error& e) {
		std::cout << "[Network error - Connection::parsePacket] " << e.what() << std::endl;
		close(FORCE_CLOSE);
	}
}

void Connection::send(const OutputMessage_ptr& msg)
{
	std::lock_guard<std::recursive_mutex> lockClass(m_connectionLock);
	if (m_closed) {
		return;
	}

	bool noPendingWrite = m_messageQueue.empty();
	m_messageQueue.emplace_back(msg);
	if (noPendingWrite) {
		internalSend(msg);
	}
}

uint32_t Connection::getIP()
{
	if (m_ipAddress != 0) {
		return m_ipAddress;
	}

	std::lock_guard<std::recursive_mutex> lockClass(m_connectionLock);

	// IP-address is expressed in network byte order
	boost::system::error_code ec;
	const auto endpoint = m_socket.remote_endpoint(ec);
	if (ec) {
		return 0;
	}

	m_ipAddress = htonl(endpoint.address().to_v4().to_uint());
	return m_ipAddress;
}

void Connection::internalSend(const OutputMessage_ptr& msg)
{
	m_protocol->onSendMessage(msg);
	try {
		m_writeTimer.expires_after(std::chrono::seconds(CONNECTION_WRITE_TIMEOUT));
		m_writeTimer.async_wait(
			[thisPtr = std::weak_ptr<Connection>(shared_from_this())](const boost::system::error_code& error) {
			Connection::handleTimeout(thisPtr, error);
		});

		boost::asio::async_write(m_socket,
			boost::asio::buffer(msg->getOutputBuffer(), msg->getLength()),
			[thisPtr = shared_from_this()](const boost::system::error_code& error, size_t /*bytes_transferred*/) {
			thisPtr->onWriteOperation(error);
		});
	} catch (boost::system::system_error& e) {
		std::cout << "[Network error - Connection::internalSend] " << e.what() << std::endl;
		close(FORCE_CLOSE);
	}
}

void Connection::onWriteOperation(const boost::system::error_code& error)
{
	std::lock_guard<std::recursive_mutex> lockClass(m_connectionLock);
	m_writeTimer.cancel();
	m_messageQueue.pop_front();

	if (error) {
		m_messageQueue.clear();
		close(FORCE_CLOSE);
		return;
	}

	if (!m_messageQueue.empty()) {
		internalSend(m_messageQueue.front());
	} else if (m_closed) {
		closeSocket();
	}
}

void Connection::handleTimeout(ConnectionWeak_ptr connectionWeak, const boost::system::error_code& error)
{
	if (error == boost::asio::error::operation_aborted) {
		// The timer has been manually cancelled
		return;
	}

	if (auto connection = connectionWeak.lock()) {
		connection->close(FORCE_CLOSE);
	}
}
