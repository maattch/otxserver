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

#include "server.h"

#include "configmanager.h"
#include "scheduler.h"
#include "otx/util.hpp"

namespace
{

	struct ConnectBlock
	{
		ConnectBlock(int64_t lastAttempt) : lastAttempt(lastAttempt) {}

		int64_t lastAttempt;
		int64_t blockTime = 0;
		uint32_t count = 1;
	};

	bool acceptConnection(uint32_t clientIP)
	{
		static std::recursive_mutex mu;
		static std::map<uint32_t, ConnectBlock> ipConnectMap;

		std::lock_guard lock{ mu };

		const int64_t currentTime = otx::util::mstime();

		auto it = ipConnectMap.find(clientIP);
		if (it == ipConnectMap.end()) {
			ipConnectMap.emplace(clientIP, currentTime);
			return true;
		}

		ConnectBlock& connectBlock = it->second;
		if (connectBlock.blockTime > currentTime) {
			connectBlock.blockTime += 250;
			return false;
		}

		const int64_t timeDiff = currentTime - connectBlock.lastAttempt;
		connectBlock.lastAttempt = currentTime;
		if (timeDiff <= 5000) {
			if (++connectBlock.count > 5) {
				connectBlock.count = 0;
				if (timeDiff <= 500) {
					connectBlock.blockTime = currentTime + 3000;
					return false;
				}
			}
		} else {
			connectBlock.count = 1;
		}
		return true;
	}

	void openAcceptor(std::weak_ptr<ServicePort> weak_service, uint16_t port)
	{
		if (auto service = weak_service.lock()) {
			service->open(port);
		}
	}

} // namespace


void ServiceManager::die()
{
	io_context.stop();
}

void ServiceManager::run()
{
	assert(!running);
	try {
		running = true;
		io_context.run();
	} catch (...) {
		running = false;
		std::cout << "[ServiceManager::run] Exception throw!" << std::endl;
	}
}

void ServiceManager::stop()
{
	if (!running) {
		return;
	}

	running = false;

	for (auto& servicePortIt : acceptors) {
		try {
			boost::asio::post(io_context, [servicePort = servicePortIt.second]() { servicePort->onStopServer(); });
		} catch (boost::system::system_error& e) {
			std::cout << "[ServiceManager::stop] Network Error: " << e.what() << std::endl;
		}
	}

	acceptors.clear();

	death_timer.expires_after(std::chrono::seconds(3));
	death_timer.async_wait([this](const boost::system::error_code&) { die(); });
}

ServicePort::~ServicePort()
{
	close();
}

bool ServicePort::is_single_socket() const
{
	return !services.empty() && services.front()->is_single_socket();
}

std::string ServicePort::get_protocol_names() const
{
	if (services.empty()) {
		return std::string();
	}

	std::string str = services.front()->get_protocol_name();
	for (size_t i = 1; i < services.size(); ++i) {
		str.push_back(',');
		str.push_back(' ');
		str.append(services[i]->get_protocol_name());
	}
	return str;
}

void ServicePort::accept()
{
	if (!acceptor) {
		return;
	}

	auto connection = ConnectionManager::getInstance().createConnection(io_context, shared_from_this());
	acceptor->async_accept(connection->getSocket(),
		[connection, thisPtr = shared_from_this()](const boost::system::error_code& error) { thisPtr->onAccept(connection, error); });
}

void ServicePort::onAccept(Connection_ptr connection, const boost::system::error_code& error)
{
	if (!error) {
		if (services.empty()) {
			return;
		}

		uint32_t remote_ip = connection->getIP();
		if (remote_ip != 0 && acceptConnection(remote_ip)) {
			Service_ptr service = services.front();
			if (service->is_single_socket()) {
				connection->accept(service->make_protocol(connection));
			} else {
				connection->accept();
			}
		} else {
			connection->close(Connection::FORCE_CLOSE);
		}

		accept();
	} else if (error != boost::asio::error::operation_aborted) {
		if (!pendingStart) {
			close();
			pendingStart = true;
			g_scheduler.addEvent(createSchedulerTask(15000,
				([port = serverPort, service = std::weak_ptr<ServicePort>(shared_from_this())]() { ::openAcceptor(service, port); })));
		}
	}
}

Protocol_ptr ServicePort::make_protocol(NetworkMessage& msg, const Connection_ptr& connection, bool checksummed) const
{
	const uint8_t protocolID = msg.getByte();
	for (auto& service : services) {
		if (protocolID != service->get_protocol_identifier()) {
			continue;
		}

		if ((checksummed && service->is_checksummed()) || !service->is_checksummed()) {
			return service->make_protocol(connection);
		}
	}
	return nullptr;
}

void ServicePort::onStopServer()
{
	close();
}

void ServicePort::open(uint16_t port)
{
	namespace ip = boost::asio::ip;

	close();

	serverPort = port;
	pendingStart = false;

	try {
		if (g_config.getBool(ConfigManager::BIND_ONLY_GLOBAL_ADDRESS)) {
			acceptor.reset(new ip::tcp::acceptor(io_context, ip::tcp::endpoint(ip::make_address_v4(g_config.getString(ConfigManager::IP)), serverPort)));
		} else {
			acceptor.reset(new ip::tcp::acceptor(io_context, ip::tcp::endpoint(ip::make_address_v4(INADDR_ANY), serverPort)));
		}

		acceptor->set_option(ip::tcp::no_delay(true));

		accept();
	} catch (boost::system::system_error& e) {
		std::cout << "[ServicePort::open] Error: " << e.what() << std::endl;

		pendingStart = true;
		g_scheduler.addEvent(createSchedulerTask(15000,
			([port, service = std::weak_ptr<ServicePort>(shared_from_this())]() { ::openAcceptor(service, port); })));
	}
}

void ServicePort::close()
{
	if (acceptor && acceptor->is_open()) {
		boost::system::error_code error;
		acceptor->close(error);
	}
}

bool ServicePort::add_service(const Service_ptr& new_svc)
{
	if (std::any_of(services.begin(), services.end(), [](const Service_ptr& svc) { return svc->is_single_socket(); })) {
		return false;
	}

	services.push_back(new_svc);
	return true;
}
