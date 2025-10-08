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

class Protocol;

class ServiceBase
{
public:
	virtual bool is_single_socket() const = 0;
	virtual bool is_checksummed() const = 0;
	virtual uint8_t get_protocol_identifier() const = 0;
	virtual const char* get_protocol_name() const = 0;

	virtual Protocol_ptr make_protocol(const Connection_ptr& c) const = 0;
};

template<typename ProtocolType>
class Service final : public ServiceBase
{
public:
	bool is_single_socket() const override { return ProtocolType::server_sends_first; }
	bool is_checksummed() const override { return ProtocolType::use_checksum; }
	uint8_t get_protocol_identifier() const override { return ProtocolType::protocol_identifier; }
	const char* get_protocol_name() const override { return ProtocolType::protocol_name(); }

	Protocol_ptr make_protocol(const Connection_ptr& c) const override {
		return std::make_shared<ProtocolType>(c);
	}
};

class ServicePort : public std::enable_shared_from_this<ServicePort>
{
public:
	explicit ServicePort(boost::asio::io_context& io_context) : io_context(io_context) {}
	~ServicePort();

	// non-copyable
	ServicePort(const ServicePort&) = delete;
	ServicePort& operator=(const ServicePort&) = delete;

	void open(uint16_t port);
	void close();
	bool is_single_socket() const;

	std::string get_protocol_names() const;

	bool add_service(const Service_ptr& new_svc);

	Protocol_ptr make_protocol(NetworkMessage& msg, const Connection_ptr& connection, bool checksummed) const;

	void onStopServer();
	void onAccept(Connection_ptr connection, const boost::system::error_code& error);

private:
	void accept();

	boost::asio::io_context& io_context;
	std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor;
	std::vector<Service_ptr> services;

	uint16_t serverPort = 0;
	bool pendingStart = false;
};

class ServiceManager final
{
public:
	ServiceManager() = default;
	~ServiceManager() { stop(); }

	// non-copyable
	ServiceManager(const ServiceManager&) = delete;
	ServiceManager& operator=(const ServiceManager&) = delete;

	void run();
	void stop();

	template <typename ProtocolType>
	bool add(uint16_t port);

	bool is_running() const { return acceptors.empty() == false; }

private:
	void die();

	std::unordered_map<uint16_t, ServicePort_ptr> acceptors;

	boost::asio::io_context io_context;
	boost::asio::steady_timer death_timer{ io_context };
	bool running = false;
};

template<typename ProtocolType>
bool ServiceManager::add(uint16_t port)
{
	if (port == 0) {
		std::clog << "ERROR: No port provided for service " << ProtocolType::protocol_name() << ". Service disabled." << std::endl;
		return false;
	}

	ServicePort_ptr service_port;

	auto foundServicePort = acceptors.find(port);

	if (foundServicePort == acceptors.end()) {
		service_port = std::make_shared<ServicePort>(io_context);
		service_port->open(port);
		acceptors[port] = service_port;
	} else {
		service_port = foundServicePort->second;

		if (service_port->is_single_socket() || ProtocolType::server_sends_first) {
			std::clog << "ERROR: " << ProtocolType::protocol_name() << " and " << service_port->get_protocol_names() << " cannot use the same port " << port << '.' << std::endl;
			return false;
		}
	}
	return service_port->add_service(std::make_shared<Service<ProtocolType>>());
}
