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

#include "otx/cast.hpp"

#include <mysql/mysql.h>

class DBResult;
using DBResultPtr = std::shared_ptr<DBResult>;

struct MysqlDeleter
{
	void operator()(MYSQL* handle) const { mysql_close(handle); }
	void operator()(MYSQL_RES* handle) const { mysql_free_result(handle); }
};

using MysqlPtr = std::unique_ptr<MYSQL, MysqlDeleter>;
using MysqlResultPtr = std::unique_ptr<MYSQL_RES, MysqlDeleter>;

class Database final
{
public:
	Database() = default;

	// non-copyable
	Database(const Database&) = delete;
	Database& operator=(const Database&) = delete;

	/**
	 * Connects to the database
	 *
	 * @return true on successful connection, false on error
	 */
	bool connect();

	/**
	 * Executes command.
	 *
	 * Executes query which doesn't generates results (eg. INSERT, UPDATE, DELETE...).
	 *
	 * @param query command
	 * @return true on success, false on error
	 */
	bool executeQuery(std::string_view query);
	bool safeExecuteQuery(std::string_view query);

	/**
	 * Queries database.
	 *
	 * Executes query which generates results (mostly SELECT).
	 *
	 * @return results object (nullptr on error)
	 */
	DBResultPtr storeQuery(std::string_view query);

	/**
	 * Escapes string for query.
	 *
	 * Prepares string to fit SQL queries including quoting it.
	 *
	 * @param s string to be escaped
	 * @return quoted string
	 */
	std::string escapeString(const std::string& s) const;

	/**
	 * Escapes binary stream for query.
	 *
	 * Prepares binary stream to fit SQL queries.
	 *
	 * @param s binary stream
	 * @param length stream length
	 * @return quoted string
	 */
	std::string escapeBlob(const char* s, uint32_t length) const;

	/**
	 * Retrieve id of last inserted row
	 *
	 * @return id on success, 0 if last query did not result on any rows with auto_increment keys
	 */
	uint64_t getLastInsertId() const { return static_cast<uint64_t>(mysql_insert_id(handle.get())); }

	/**
	 * Get database engine version
	 *
	 * @return the database engine version
	 */
	static const char* getClientVersion() { return mysql_get_client_info(); }

	uint64_t getMaxPacketSize() const { return maxPacketSize; }

	/**
	 * Transaction related methods.
	 *
	 * Methods for starting, commiting and rolling back transaction. Each of the returns boolean value.
	 *
	 * @return true on success, false on error
	 */

	bool beginTransaction();
	bool rollback();
	bool commit();

	const char* getInfo() { return mysql_info(handle.get()); }

	uint32_t getResultCount() const { return mysql_field_count(handle.get()); }
	uint64_t getAffectedRows() const { return mysql_affected_rows(handle.get()); }
	bool setCharset(const char* charset) const { return mysql_set_character_set(handle.get(), charset) == 0; }

private:
	std::recursive_mutex databaseLock;
	MysqlPtr handle;
	uint64_t maxPacketSize = 1048576;
	// Do not retry queries if we are in the middle of a transaction
	bool retryQueries = true;

	friend class DBTransaction;
};

extern Database g_database;

class DBResult
{
public:
	explicit DBResult(MysqlResultPtr&& res);

	// non-copyable
	DBResult(const DBResult&) = delete;
	DBResult& operator=(const DBResult&) = delete;

	template<typename T>
	std::enable_if_t<std::is_integral_v<T> || std::is_floating_point_v<T>, T>
	getNumber(const std::string& s) const
	{
		auto it = listNames.find(s);
		if (it == listNames.end()) {
			std::clog << "[Error - DBResult::getNumber] Column '" << s << "' doesn't exist in the result set" << std::endl;
			return {};
		}

		if (!row[it->second]) {
			return {};
		}
		return otx::util::cast<T>(row[it->second]);
	}

	// returns true if first character is in '1tTyY'
	bool getBoolean(const std::string& s) const;
	std::string getString(const std::string& s) const;
	const char* getStream(const std::string& s, unsigned long& size) const;

	uint64_t getRowsCount() const { return mysql_num_rows(handle.get()); }

	bool hasNext() const;
	bool next();

private:
	std::map<std::string, size_t> listNames;
	MysqlResultPtr handle;
	MYSQL_ROW row;

	friend class Database;
};

/**
 * INSERT statement.
 */
class DBInsert final
{
public:
	DBInsert() = default;
	explicit DBInsert(std::string query);

	bool addRow(const std::string& row);
	bool execute();

	void setQuery(const std::string& s)
	{
		query = s;
		values = std::string();
		length = query.length();
	}

private:
	std::string query;
	std::string values;
	size_t length = 0;
};

class DBTransaction final
{
public:
	DBTransaction() = default;
	~DBTransaction();

	// non-copyable
	DBTransaction(const DBTransaction&) = delete;
	DBTransaction& operator=(const DBTransaction&) = delete;

	bool begin();
	bool commit();

private:
	enum : uint8_t
	{
		STATE_NO_START,
		STATE_START,
		STATE_COMMIT,
	} state
		= STATE_NO_START;
};
