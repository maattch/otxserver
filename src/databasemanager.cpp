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

#include "databasemanager.h"

#include "configmanager.h"

extern ConfigManager g_config;

bool DatabaseManager::optimizeTables()
{
	std::ostringstream query;
	query << "SELECT `TABLE_NAME` FROM `information_schema`.`tables` WHERE `TABLE_SCHEMA` = " << g_database.escapeString(g_config.getString(ConfigManager::SQL_DB)) << " AND `DATA_FREE` > 0;";

	DBResultPtr result = g_database.storeQuery(query.str());
	if (!result) {
		return false;
	}

	query.str("");
	do {
		std::clog << "\033[33m>>> Optimizing table: \033[0m" << result->getString("TABLE_NAME") << "... ";
		query << "OPTIMIZE TABLE `" << result->getString("TABLE_NAME") << "`;";

		if (g_database.executeQuery(query.str())) {
			std::clog << "\033[32m[success]\033[0m" << std::endl;
		} else {
			std::clog << "\033[31m[failure]\033[0m" << std::endl;
		}

		query.str("");
	} while (result->next());

	return true;
}

bool DatabaseManager::triggerExists(std::string trigger)
{
	std::ostringstream query;
	query << "SELECT `TRIGGER_NAME` FROM `information_schema`.`triggers` WHERE `TRIGGER_SCHEMA` = " << g_database.escapeString(g_config.getString(ConfigManager::SQL_DB)) << " AND `TRIGGER_NAME` = " << g_database.escapeString(trigger) << ";";

	DBResultPtr result;
	if (!(result = g_database.storeQuery(query.str()))) {
		return false;
	}

	return true;
}

bool DatabaseManager::tableExists(std::string table)
{
	std::ostringstream query;
	query << "SELECT `TABLE_NAME` FROM `information_schema`.`tables` WHERE `TABLE_SCHEMA` = " << g_database.escapeString(g_config.getString(ConfigManager::SQL_DB)) << " AND `TABLE_NAME` = " << g_database.escapeString(table) << ";";

	DBResultPtr result;
	if (!(result = g_database.storeQuery(query.str()))) {
		return false;
	}

	return true;
}

bool DatabaseManager::isDatabaseSetup()
{
	std::ostringstream query;
	query << "SELECT `TABLE_NAME` FROM `information_schema`.`tables` WHERE `TABLE_SCHEMA` = " << g_database.escapeString(g_config.getString(ConfigManager::SQL_DB)) << ";";

	DBResultPtr result;
	if (!(result = g_database.storeQuery(query.str()))) {
		return false;
	}

	return true;
}

int32_t DatabaseManager::getDatabaseVersion()
{
	if (!tableExists("server_config")) {
		return 0;
	}

	int32_t value = 0;
	if (getDatabaseConfig("db_version", value)) {
		return value;
	}

	return -1;
}

bool DatabaseManager::getDatabaseConfig(std::string config, int32_t& value)
{
	value = 0;

	DBResultPtr result;

	std::ostringstream query;
	query << "SELECT `value` FROM `server_config` WHERE `config` = " << g_database.escapeString(config) << ";";
	if (!(result = g_database.storeQuery(query.str()))) {
		return false;
	}

	value = atoi(result->getString("value").c_str());
	return true;
}

bool DatabaseManager::getDatabaseConfig(std::string config, std::string& value)
{
	value = "";

	DBResultPtr result;

	std::ostringstream query;
	query << "SELECT `value` FROM `server_config` WHERE `config` = " << g_database.escapeString(config) << ";";
	if (!(result = g_database.storeQuery(query.str()))) {
		return false;
	}

	value = result->getString("value");
	return true;
}

void DatabaseManager::registerDatabaseConfig(std::string config, int32_t value)
{
	std::ostringstream query;

	int32_t tmp = 0;
	if (!getDatabaseConfig(config, tmp)) {
		query << "INSERT INTO `server_config` VALUES (" << g_database.escapeString(config) << ", '" << value << "');";
	} else {
		query << "UPDATE `server_config` SET `value` = '" << value << "' WHERE `config` = " << g_database.escapeString(config) << ";";
	}

	g_database.executeQuery(query.str());
}

void DatabaseManager::registerDatabaseConfig(std::string config, std::string value)
{
	std::ostringstream query;

	std::string tmp;
	if (!getDatabaseConfig(config, tmp)) {
		query << "INSERT INTO `server_config` VALUES (" << g_database.escapeString(config) << ", " << g_database.escapeString(value) << ");";
	} else {
		query << "UPDATE `server_config` SET `value` = " << g_database.escapeString(value) << " WHERE `config` = " << g_database.escapeString(config) << ";";
	}

	g_database.executeQuery(query.str());
}
