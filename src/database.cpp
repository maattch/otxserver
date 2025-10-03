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

#include "database.h"

#include <mysql/errmsg.h>

Database g_database;

extern ConfigManager g_config;

namespace
{
	MysqlPtr connectToDatabase(const bool retryIfError)
	{
		bool isFirstAttemptToConnect = true;
#ifdef _WIN32
		bool verifySSL = false;
#endif
	retry:
		if (!isFirstAttemptToConnect) {
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
		isFirstAttemptToConnect = false;

		// close the connection handle
		MysqlPtr handle(mysql_init(nullptr));
		if (!handle) {
			std::cout << "\n[Error - mysql_init]\nFailed to initialize MySQL connection handle." << std::endl;
			goto error;
		}

#ifdef _WIN32
		mysql_options(handle.get(), MYSQL_OPT_SSL_VERIFY_SERVER_CERT, &verifySSL);
#endif
		// connects to database
		if (!mysql_real_connect(
				handle.get(),
				g_config.getString(ConfigManager::SQL_HOST).data(),
				g_config.getString(ConfigManager::SQL_USER).data(),
				g_config.getString(ConfigManager::SQL_PASS).data(),
				g_config.getString(ConfigManager::SQL_DB).data(),
				static_cast<unsigned int>(g_config.getNumber(ConfigManager::SQL_PORT)),
				nullptr,
				0)) {
			std::cout << "[Error - connectToDatabase]\nMessage: " << mysql_error(handle.get()) << " (" << mysql_errno(handle.get()) << ')' << std::endl;
			goto error;
		}
		return handle;

	error:
		if (retryIfError) {
			goto retry;
		}
		return nullptr;
	}

	bool isLostConnectionError(const unsigned errn)
	{
		return errn == CR_SERVER_LOST || errn == CR_SERVER_GONE_ERROR || errn == CR_CONN_HOST_ERROR || errn == 1053 /*ER_SERVER_SHUTDOWN*/ || errn == CR_CONNECTION_ERROR;
	}

	bool executeDatabaseQuery(MysqlPtr& handle, std::string_view query, const bool retryIfLostConnection)
	{
		while (mysql_real_query(handle.get(), query.data(), query.length()) != 0) {
			unsigned int errn = mysql_errno(handle.get());
			std::cout << "[Error - executeDatabaseQuery]\nQuery: " << query << "\nMessage: " << mysql_error(handle.get()) << " (" << errn << ')' << std::endl;
			if (!isLostConnectionError(errn) || !retryIfLostConnection) {
				return false;
			}

			handle = connectToDatabase(true);
		}
		return true;
	}
}

bool Database::connect()
{
	MysqlPtr newHandle = connectToDatabase(false);
	if (!newHandle) {
		return false;
	}

	handle = std::move(newHandle);

	DBResultPtr result = storeQuery("SHOW VARIABLES LIKE 'max_allowed_packet'");
	if (result) {
		maxPacketSize = result->getNumber<uint64_t>("Value");
	}

	if (maxPacketSize < 8 * 1024 * 1024) {
		std::cout << "[Database::connect] 'max_allowed_packet' less than 8MB, consider increasing it." << std::endl;
	}
	return true;
}

bool Database::beginTransaction()
{
	databaseLock.lock();
	const bool result = executeQuery("START TRANSACTION");
	retryQueries = !result;
	if (!result) {
		databaseLock.unlock();
	}
	return result;
}

bool Database::rollback()
{
	const bool result = executeQuery("ROLLBACK");
	retryQueries = true;
	databaseLock.unlock();
	return result;
}

bool Database::commit()
{
	const bool result = executeQuery("COMMIT");
	retryQueries = true;
	databaseLock.unlock();
	return result;
}

bool Database::executeQuery(std::string_view query)
{
	std::lock_guard<std::recursive_mutex> lockGuard(databaseLock);
	return executeDatabaseQuery(handle, query, retryQueries);
}

bool Database::safeExecuteQuery(std::string_view query)
{
	std::lock_guard<std::recursive_mutex> lockGuard(databaseLock);
	const bool success = executeDatabaseQuery(handle, query, retryQueries);
	// we should call that every time as someone would call executeQuery('SELECT...')
	// as it is described in MySQL manual: "it doesn't hurt" :P
	MysqlResultPtr res{ mysql_store_result(handle.get()) };
	return success;
}

DBResultPtr Database::storeQuery(std::string_view query)
{
	std::lock_guard<std::recursive_mutex> lockGuard(databaseLock);

#if ENABLE_OT_STATISTICS > 0
	std::chrono::high_resolution_clock::time_point time_point = std::chrono::high_resolution_clock::now();
#endif

retry:
	if (!executeDatabaseQuery(handle, query, retryQueries) && !retryQueries) {
		return nullptr;
	}

	// we should call that every time as someone would call executeQuery('SELECT...')
	// as it is described in MySQL manual: "it doesn't hurt" :P
	MysqlResultPtr res{ mysql_store_result(handle.get()) };
	if (!res) {
		const unsigned errorn = mysql_errno(handle.get());
		std::cout << "[Error - Database::storeQuery]\nQuery: " << query << "\nMessage: " << mysql_error(handle.get()) << " (" << errorn << ')' << std::endl;
		if (!isLostConnectionError(errorn) || !retryQueries) {
			return nullptr;
		}
		goto retry;
	}

#if ENABLE_OT_STATISTICS > 0
	int64_t ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - time_point).count();
	g_stats.addSqlStats(Stat(ns, std::string(query), ""));
#endif

	// retrieving results of query
	DBResultPtr result = std::make_shared<DBResult>(std::move(res));
	if (!result->hasNext()) {
		return nullptr;
	}
	return result;
}

std::string Database::escapeString(const std::string& s) const
{
	return escapeBlob(s.data(), s.length());
}

std::string Database::escapeBlob(const char* s, uint32_t length) const
{
	// the worst case is 2n + 1
	size_t maxLength = (length * 2) + 1;

	std::string escaped;
	escaped.reserve(maxLength + 2);
	escaped.push_back('\'');

	if (length != 0) {
		char* output = new char[maxLength];
		size_t realLength = mysql_real_escape_string(handle.get(), output, s, length);
		escaped.append(output, realLength);
		delete[] output;
	}

	escaped.push_back('\'');
	return escaped;
}

DBResult::DBResult(MysqlResultPtr&& res) :
	handle(std::move(res))
{
	size_t i = 0;
	MYSQL_FIELD* field = mysql_fetch_field(handle.get());
	while (field) {
		listNames[field->name] = i++;
		field = mysql_fetch_field(handle.get());
	}

	row = mysql_fetch_row(handle.get());
}

bool DBResult::getBoolean(const std::string& s) const
{
	auto it = listNames.find(s);
	if (it == listNames.end()) {
		std::cout << "[Error - DBResult::getBoolean] Column '" << s << "' doesn't exist in the result set" << std::endl;
		return false;
	}

	if (!row[it->second]) {
		return false;
	}

	const char ch = *row[it->second];
	return (ch == '1' || ch == 'y' || ch == 'Y' || ch == 't' || ch == 'T');
}

std::string DBResult::getString(const std::string& s) const
{
	auto it = listNames.find(s);
	if (it == listNames.end()) {
		std::cout << "[Error - DBResult::getString] Column '" << s << "' does not exist in result set." << std::endl;
		return {};
	}

	if (!row[it->second]) {
		return {};
	}

	unsigned long size = mysql_fetch_lengths(handle.get())[it->second];
	return { row[it->second], size };
}

const char* DBResult::getStream(const std::string& s, unsigned long& size) const
{
	auto it = listNames.find(s);
	if (it == listNames.end()) {
		std::cout << "[Error - DBResult::getStream] Column '" << s << "' does not exist in result set." << std::endl;
		size = 0;
		return nullptr;
	}

	if (!row[it->second]) {
		size = 0;
		return nullptr;
	}

	size = mysql_fetch_lengths(handle.get())[it->second];
	return row[it->second];
}

bool DBResult::hasNext() const
{
	return row != nullptr;
}

bool DBResult::next()
{
	row = mysql_fetch_row(handle.get());
	return row != nullptr;
}

DBInsert::DBInsert(std::string query) :
	query(std::move(query))
{
	this->length = this->query.length();
}

bool DBInsert::addRow(const std::string& row)
{
	// adds new row to buffer
	const size_t rowLength = row.length();
	length += rowLength;
	if (length > g_database.getMaxPacketSize() && !execute()) {
		return false;
	}

	if (values.empty()) {
		values.reserve(rowLength + 2);
		values.push_back('(');
		values.append(row);
		values.push_back(')');
	} else {
		values.reserve(values.length() + rowLength + 3);
		values.push_back(',');
		values.push_back('(');
		values.append(row);
		values.push_back(')');
	}
	return true;
}

bool DBInsert::execute()
{
	if (values.empty()) {
		return true;
	}

	// executes buffer
	bool res = g_database.executeQuery(query + values);
	values.clear();
	length = query.length();
	return res;
}

DBTransaction::~DBTransaction()
{
	if (state == STATE_START) {
		g_database.rollback();
	}
}

bool DBTransaction::begin()
{
	state = STATE_START;
	return g_database.beginTransaction();
}

bool DBTransaction::commit()
{
	if (state != STATE_START) {
		return false;
	}

	state = STATE_COMMIT;
	return g_database.commit();
}
