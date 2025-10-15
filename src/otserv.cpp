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

#include "chat.h"
#include "configmanager.h"
#include "databasemanager.h"
#include "game.h"
#include "group.h"
#include "iologindata.h"
#include "monsters.h"
#include "npc.h"
#include "outfit.h"
#include "protocolgame.h"
#include "protocollogin.h"
#include "protocolold.h"
#include "protocolstatus.h"
#include "quests.h"
#include "raids.h"
#include "rsa.h"
#include "scriptmanager.h"
#include "server.h"
#include "textlogger.h"
#include "vocation.h"

#include "otx/util.hpp"

#ifndef _WIN32
#include <fstream>
#endif

RSA g_RSA;

std::mutex g_loaderLock;
std::condition_variable g_loaderSignal;
std::unique_lock<std::mutex> g_loaderUniqueLock(g_loaderLock);

#ifndef _WIN32
__attribute__((used)) void saveServer()
{
	g_game.setGameState(GAMESTATE_SHUTDOWN);
}
#endif

bool argumentsHandler(StringVec args)
{
	StringVec tmp;
	for (StringVec::iterator it = args.begin(); it != args.end(); ++it) {
		if ((*it) == "--help") {
			std::clog << "Usage:\n"
						 "\n"
						 "\t--config=$1\t\tAlternate configuration file path.\n"
						 "\t--data-directory=$1\tAlternate data directory path.\n"
						 "\t--ip=$1\t\t\tIP address of the server.\n"
						 "\t\t\t\tShould be equal to the global IP.\n"
						 "\t--login-port=$1\tPort for login server to listen on.\n"
						 "\t--game-port=$1\tPort for game server to listen on.\n"
						 "\t--admin-port=$1\tPort for admin server to listen on.\n"
						 "\t--manager-port=$1\tPort for manager server to listen on.\n"
						 "\t--status-port=$1\tPort for status server to listen on.\n";
#ifndef _WIN32
			std::clog << "\t--runfile=$1\t\tSpecifies run file. Will contain the pid\n"
						 "\t\t\t\tof the server process as long as run status.\n";
#endif
			std::clog << "\t--log=$1\t\tWhole standard output will be logged to\n"
						 "\t\t\t\tthis file.\n"
						 "\t--closed\t\t\tStarts the server as closed.\n"
						 "\t--no-script\t\t\tStarts the server without script system.\n";
			return false;
		}

		tmp = explodeString((*it), "=");
		if (tmp[0] == "--config") {
			otx::config::setString(otx::config::CONFIG_FILE, tmp[1]);
		} else if (tmp[0] == "--data-directory") {
			otx::config::setString(otx::config::DATA_DIRECTORY, tmp[1]);
		} else if (tmp[0] == "--logs-directory") {
			otx::config::setString(otx::config::LOGS_DIRECTORY, tmp[1]);
		} else if (tmp[0] == "--ip") {
			otx::config::setString(otx::config::IP, tmp[1]);
		} else if (tmp[0] == "--login-port") {
			otx::config::setInteger(otx::config::LOGIN_PORT, atoi(tmp[1].c_str()));
		} else if (tmp[0] == "--game-port") {
			otx::config::setInteger(otx::config::GAME_PORT, atoi(tmp[1].c_str()));
		} else if (tmp[0] == "--status-port") {
			otx::config::setInteger(otx::config::STATUS_PORT, atoi(tmp[1].c_str()));
		}
#ifndef _WIN32
		else if (tmp[0] == "--runfile" || tmp[0] == "--run-file" || tmp[0] == "--pidfile" || tmp[0] == "--pid-file") {
			otx::config::setString(otx::config::RUNFILE, tmp[1]);
		}
#endif
		else if (tmp[0] == "--log") {
			otx::config::setString(otx::config::OUTPUT_LOG, tmp[1]);
		}
#ifndef _WIN32
		else if (tmp[0] == "--daemon" || tmp[0] == "-d") {
			otx::config::setBoolean(otx::config::DAEMONIZE, true);
		}
#endif
		else if (tmp[0] == "--closed") {
			otx::config::setBoolean(otx::config::START_CLOSED, true);
		}
	}

	return true;
}

#ifndef _WIN32
void signalHandler(int32_t sig)
{
	switch (sig) {
		case SIGHUP:
			addDispatcherTask([]() { g_game.saveGameState(SAVE_PLAYERS | SAVE_MAP | SAVE_STATE); });
			break;

		case SIGTRAP:
			g_game.cleanMap();
			break;

		case SIGCHLD:
			g_game.proceduralRefresh();
			break;

		case SIGUSR1:
			addDispatcherTask([]() { g_game.setGameState(GAMESTATE_CLOSED); });
			break;

		case SIGUSR2:
			g_game.setGameState(GAMESTATE_NORMAL);
			break;

		case SIGCONT:
			addDispatcherTask([]() { g_game.reloadInfo(RELOAD_ALL); });
			break;

		case SIGQUIT:
			addDispatcherTask([]() { g_game.setGameState(GAMESTATE_SHUTDOWN); });
			break;

		case SIGTERM: {
			addDispatcherTask([]() { g_game.shutdown(); });
			g_scheduler.join();
			g_dispatcher.join();
			break;
		}

		default:
			break;
	}
}

void runfileHandler(void)
{
	std::ofstream runfile(otx::config::getString(otx::config::RUNFILE).c_str(), std::ios::trunc | std::ios::out);
	runfile.close();
}
#endif

void allocationHandler()
{
	puts("Allocation failed, server out of memory!\nDecrease size of your map or compile in a 64-bit mode.");
	getchar();
	std::exit(-1);
}

void startupErrorMessage(std::string error = "")
{
	// we will get a crash here as the threads aren't going down smoothly
	if (error.length() > 0) {
		std::clog << std::endl << "> ERROR: " << error << std::endl;
	}

	getchar();
	std::exit(-1);
}

void otserv(ServiceManager* services);

int main(int argc, char* argv[])
{
	StringVec args = StringVec(argv, argv + argc);
	if (argc > 1 && !argumentsHandler(args)) {
		return 0;
	}

	std::srand(otx::util::mstime());
	OutputHandler::getInstance();

	std::set_new_handler(allocationHandler);

	g_dispatcher.start();
	g_scheduler.start();

	g_lua.init();
	otx::scriptmanager::init();

#ifndef _WIN32
	// ignore sigpipe...
	struct sigaction sigh;
	sigh.sa_handler = SIG_IGN;
	sigh.sa_flags = 0;

	sigemptyset(&sigh.sa_mask);
	sigaction(SIGPIPE, &sigh, nullptr);

	// register signals
	signal(SIGHUP, signalHandler); // save
	signal(SIGTRAP, signalHandler); // clean
	signal(SIGCHLD, signalHandler); // refresh
	signal(SIGUSR1, signalHandler); // close server
	signal(SIGUSR2, signalHandler); // open server
	signal(SIGCONT, signalHandler); // reload all
	signal(SIGQUIT, signalHandler); // save & shutdown
	signal(SIGTERM, signalHandler); // shutdown
#endif

	ServiceManager servicer;
	addDispatcherTask([services = &servicer]() { otserv(services); });

	g_loaderSignal.wait(g_loaderUniqueLock);

	if (servicer.is_running()) {
		std::clog << ">> " << otx::config::getString(otx::config::SERVER_NAME) << " server Online!" << std::endl << std::endl;
		servicer.run();
	} else {
		std::clog << ">> " << otx::config::getString(otx::config::SERVER_NAME) << " server Offline! No services available..." << std::endl << std::endl;
		g_scheduler.shutdown();
		g_dispatcher.shutdown();
	}

	otx::scriptmanager::terminate();
	g_lua.terminate();

	g_scheduler.join();
	g_dispatcher.join();
	return 0;
}

void otserv(ServiceManager* services)
{
#ifdef _WIN32
	SetConsoleTitleA(SOFTWARE_NAME);
#endif

	g_game.setGameState(GAMESTATE_STARTUP);

	std::clog << SOFTWARE_NAME << " Version: (" << SOFTWARE_VERSION << "." << MINOR_VERSION << ")\n"
		"Compiled with " << BOOST_COMPILER << " for arch "
#if defined(__amd64__) || defined(_M_X64)
		"64 Bits"
#elif defined(__i386__) || defined(_M_IX86) || defined(_X86_)
		"32 Bits"
#else
		"unk"
#endif
		" at " << __DATE__ << " " << __TIME__ << ".\n"

		"\n"
		"\t\tFEATURES ON THIS BUILD:\n"

#ifdef LUAJIT_VERSION
		<< "\t\tLua Version: " << LUAJIT_VERSION << '\n'
#else
		<< "\t\tLua Version: " << LUA_RELEASE << '\n'
#endif
		<< "\t\tXML Version: " << LIBXML_DOTTED_VERSION << '\n'
		<< "\t\tBOOST Version: " << (BOOST_VERSION / 100000) << '.' << (BOOST_VERSION / 100 % 1000) << '.' << (BOOST_VERSION % 100) << '\n'
#ifdef __GROUND_CACHE__
		<< "\t\tGroundCache: yes\n"
#else
		<< "\t\tGroundCache: no\n"
#endif
#if ENABLE_SERVER_DIAGNOSTIC > 0
		<< "\t\tServerDiag: yes\n"
#else
		<< "\t\tServerDiag: no\n"
#endif
		<< '\n'
		<< "A server developed by: " SOFTWARE_DEVELOPERS ".\n";

	std::clog << ">> Loading config" << std::endl;
	if (!otx::config::load()) {
		startupErrorMessage("Unable to load " + otx::config::getString(otx::config::CONFIG_FILE) + "!");
	}

#ifndef _WIN32
	if (otx::config::getBoolean(otx::config::DAEMONIZE)) {
		std::clog << "> Daemonization... ";
		if (fork()) {
			std::clog << "succeed, bye!" << std::endl;
			exit(0);
		} else {
			std::clog << "failed, continuing." << std::endl;
		}
	}
#endif

	// silently append trailing slash
	std::string path = otx::config::getString(otx::config::DATA_DIRECTORY);
	otx::config::setString(otx::config::DATA_DIRECTORY, path.erase(path.find_last_not_of("/") + 1) + "/");

	path = otx::config::getString(otx::config::LOGS_DIRECTORY);
	otx::config::setString(otx::config::LOGS_DIRECTORY, path.erase(path.find_last_not_of("/") + 1) + "/");

	std::clog << ">> Opening logs" << std::endl;
	Logger::getInstance()->open();

	IntegerVec cores = vectorAtoi(explodeString(otx::config::getString(otx::config::CORES_USED), ","));
	if (cores[0] != -1) {
#ifdef _WIN32
		int32_t mask = 0;
		for (IntegerVec::iterator it = cores.begin(); it != cores.end(); ++it) {
			mask += 1 << (*it);
		}

		SetProcessAffinityMask(GetCurrentProcess(), mask);
	}

	CreateMutexA(nullptr, FALSE, "otxserver");
	if (GetLastError() == ERROR_ALREADY_EXISTS) {
		startupErrorMessage("Another instance is already running.");
	}

	std::string defaultPriority = otx::util::as_lower_string(otx::config::getString(otx::config::DEFAULT_PRIORITY));
	if (defaultPriority == "realtime" || defaultPriority == "real") {
		SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);
	} else if (defaultPriority == "high" || defaultPriority == "regular") {
		SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
	} else if (defaultPriority == "higher" || defaultPriority == "above" || defaultPriority == "normal") {
		SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);
	}

#else
	#ifndef __APPLE__
		cpu_set_t mask;
		CPU_ZERO(&mask);
		for (IntegerVec::iterator it = cores.begin(); it != cores.end(); ++it) {
			CPU_SET((*it), &mask);
		}

		sched_setaffinity(getpid(), sizeof(mask), &mask);
	}
	#endif

	std::string runPath = otx::config::getString(otx::config::RUNFILE);
	if (!runPath.empty() && runPath.length() > 2) {
		std::ofstream runFile(runPath.c_str(), std::ios::trunc | std::ios::out);
		runFile << getpid();
		runFile.close();
		atexit(runfileHandler);
	}

	if (!nice(otx::config::getInteger(otx::config::NICE_LEVEL))) {}
#endif

	std::clog << ">> Loading RSA key" << std::endl;
	const char* p("14299623962416399520070177382898895550795403345466153217470516082934737582776038882967213386204600674145392845853859217990626450972452084065728686565928113");
	const char* q("7630979195970404721891201847792002125535401292779123937207447574596692788513647179235335529307251350570728407373705564708871762033017096809910315212884101");
	const char* d("46730330223584118622160180015036832148732986808519344675210555262940258739805766860224610646919605860206328024326703361630109888417839241959507572247284807035235569619173792292786907845791904955103601652822519121908367187885509270025388641700821735345222087940578381210879116823013776808975766851829020659073");

	g_RSA.initialize(p, q, d);

	std::clog << ">> Starting SQL connection" << std::endl;
	if (g_database.connect()) {
		std::clog << ">> Running Database Manager" << std::endl;
		if (otx::config::getBoolean(otx::config::OPTIMIZE_DATABASE) && !DatabaseManager::getInstance()->optimizeTables()) {
			std::clog << "[Done] No tables to optimize." << std::endl;
		}
	} else {
		startupErrorMessage("Couldn't estabilish connection to SQL database!");
	}

	std::clog << ">> Checking for duplicated items" << std::endl;
	std::ostringstream query;
	query << "SELECT unitedItems.serial, COUNT(1) AS duplicatesCount FROM (SELECT serial FROM `player_items` UNION ALL SELECT serial FROM `player_depotitems` UNION ALL SELECT serial FROM `tile_items`) unitedItems GROUP BY unitedItems.serial HAVING COUNT(1) > 1;";

	DBResultPtr result;
	bool duplicated = false;

	if ((result = g_database.storeQuery(query.str()))) {
		do {
			std::string serial = result->getString("serial");
			if (serial != "" && serial.length() > 1) {
				DBResultPtr result_;
				std::ostringstream query_playeritems;
				query_playeritems << "SELECT `player_id`, `pid`, `sid`, `itemtype`, `count`, `attributes`, `serial` FROM `player_items` WHERE `serial` = " << g_database.escapeString(serial);
				if ((result_ = g_database.storeQuery(query_playeritems.str()))) {
					duplicated = true;
					do {
						std::string name;
						IOLoginData::getInstance()->getNameByGuid(result_->getNumber<uint32_t>("player_id"), name);
						std::clog << ">> Deleted item from 'player_items' with SERIAL: [" << serial.c_str() << "] PLAYER: [" << result_->getNumber<int32_t>("player_id") << "] PLAYER NAME: [" << name.c_str() << "] ITEM: [" << result_->getNumber<int32_t>("itemtype") << "] COUNT: [" << result_->getNumber<int32_t>("count") << "]" << std::endl;
						std::ostringstream logText;
						logText << "Deleted item from 'player_items' with SERIAL: [" << serial << "] PLAYER: [" << result_->getNumber<int32_t>("player_id") << "] PLAYER NAME: [" << name << "] ITEM: [" << result_->getNumber<int32_t>("itemtype") << "] COUNT: [" << result_->getNumber<int32_t>("count") << "]";
						Logger::getInstance()->eFile("anti_dupe.log", logText.str(), true);
					} while (result_->next());
				}

				query_playeritems.clear();
				std::ostringstream query_playerdepotitems;
				query_playerdepotitems << "SELECT `player_id`, `sid`, `pid`, `itemtype`, `count`, `attributes`, `serial` FROM `player_depotitems` WHERE `serial` = " << g_database.escapeString(serial);
				if ((result_ = g_database.storeQuery(query_playerdepotitems.str()))) {
					duplicated = true;
					do {
						std::string name;
						IOLoginData::getInstance()->getNameByGuid(result_->getNumber<uint32_t>("player_id"), name);
						std::clog << ">> Deleted item from 'player_depotitems' with SERIAL: [" << serial.c_str() << "] PLAYER: [" << result_->getNumber<int32_t>("player_id") << "] PLAYER NAME: [" << name.c_str() << "] ITEM: [" << result_->getNumber<int32_t>("itemtype") << "] COUNT: [" << result_->getNumber<int32_t>("count") << "]" << std::endl;
						std::ostringstream logText;
						logText << "Deleted item from 'player_depotitems' with SERIAL: [" << serial << "] PLAYER: [" << result_->getNumber<int32_t>("player_id") << "] PLAYER NAME: [" << name << "] ITEM: [" << result_->getNumber<int32_t>("itemtype") << "] COUNT: [" << result_->getNumber<int32_t>("count") << "]";
						Logger::getInstance()->eFile("anti_dupe.log", logText.str(), true);
					} while (result_->next());
				}

				query_playerdepotitems.clear();
				std::ostringstream query_tileitems;
				query_tileitems << "SELECT `tile_id`, `sid`, `pid`, `itemtype`, `count`, `attributes`, `serial` FROM `tile_items` WHERE `serial` = " << g_database.escapeString(serial);
				if ((result_ = g_database.storeQuery(query_tileitems.str()))) {
					duplicated = true;
					do {
						std::clog << ">> Deleted item from 'tile_items' with SERIAL: [" << serial.c_str() << "] TILE ID: [" << result_->getNumber<int32_t>("tile_id") << "] ITEM: [" << result_->getNumber<int32_t>("itemtype") << "] COUNT: [" << result_->getNumber<int32_t>("count") << "]" << std::endl;
						std::ostringstream logText;
						logText << "Deleted item from 'tile_items' with SERIAL: [" << serial << "] TILE ID: [" << result_->getNumber<int32_t>("tile_id") << "] ITEM: [" << result_->getNumber<int32_t>("itemtype") << "] COUNT: [" << result_->getNumber<int32_t>("count") << "]";
						Logger::getInstance()->eFile("anti_dupe.log", logText.str(), true);
					} while (result_->next());
				}

				query_tileitems.clear();
				std::ostringstream query_deletepi;
				query_deletepi << "DELETE FROM `player_items` WHERE `serial` = " << g_database.escapeString(serial);
				if (!g_database.executeQuery(query_deletepi.str())) {
					std::clog << ">> Cannot delete duplicated items from 'player_items'!" << std::endl;
				}

				query_deletepi.clear();
				std::ostringstream query_deletedi;
				query_deletedi << "DELETE FROM `player_depotitems` WHERE `serial` = " << g_database.escapeString(serial);
				if (!g_database.executeQuery(query_deletedi.str())) {
					std::clog << ">> Cannot delete duplicated items from 'player_depotitems'!" << std::endl;
				}

				query_deletedi.clear();
				std::ostringstream query_deleteti;
				query_deleteti << "DELETE FROM `tile_items` WHERE `serial` = " << g_database.escapeString(serial);
				if (!g_database.executeQuery(query_deleteti.str())) {
					std::clog << ">> Cannot delete duplicated items from 'tile_items'!" << std::endl;
				}

				query_deleteti.clear();
			}
		} while (result->next());

		if (duplicated) {
			std::clog << ">> Duplicated items successfully removed." << std::endl;
		}
	} else {
		std::clog << ">> There wasn't duplicated items in the server." << std::endl;
	}

	std::clog << ">> Loading items (OTB)" << std::endl;
	if (!Item::items.loadFromOtb(getFilePath(FILE_TYPE_OTHER, "items/items.otb"))) {
		startupErrorMessage("Unable to load items (OTB)!");
	}

	std::clog << ">> Loading items (XML)" << std::endl;
	if (!Item::items.loadFromXml()) {
		std::clog << "Unable to load items (XML)! Continue? (y/N)" << std::endl;
		char buffer = getchar();
		if (buffer != 121 && buffer != 89) {
			startupErrorMessage("Unable to load items (XML)!");
		}
	}

	std::clog << ">> Loading groups" << std::endl;
	if (!Groups::getInstance()->loadFromXml()) {
		startupErrorMessage("Unable to load groups!");
	}

	std::clog << ">> Loading outfits" << std::endl;
	if (!Outfits::getInstance()->loadFromXml()) {
		startupErrorMessage("Unable to load outfits!");
	}

	std::clog << ">> Loading quests" << std::endl;
	if (!Quests::getInstance()->loadFromXml()) {
		startupErrorMessage("Unable to load quests!");
	}

	std::clog << ">> Loading raids" << std::endl;
	if (!Raids::getInstance()->loadFromXml()) {
		startupErrorMessage("Unable to load raids!");
	}

	std::clog << ">> Loading vocations" << std::endl;
	if (!g_vocations.loadFromXml()) {
		startupErrorMessage("Unable to load vocations!");
	}

	std::clog << ">> Loading chat channels" << std::endl;
	if (!g_chat.loadFromXml()) {
		startupErrorMessage("Unable to load chat channels!");
	}

	std::clog << ">> Loading experience stages" << std::endl;
	if (!g_game.loadExperienceStages()) {
		startupErrorMessage("Unable to load experience stages!");
	}

	std::clog << ">> Loading script systems:" << std::endl;
	if (!otx::scriptmanager::load()) {
		startupErrorMessage();
	}

	std::clog << ">> Loading monsters" << std::endl;
	if (!g_monsters.loadFromXml()) {
		std::clog << "Unable to load monsters! Continue? (y/N)" << std::endl;
		char buffer = getchar();
		if (buffer != 121 && buffer != 89) {
			startupErrorMessage("Unable to load monsters!");
		}
	}

	if (fileExists(getFilePath(FILE_TYPE_OTHER, "npc/npcs.xml"))) {
		std::clog << ">> Loading npcs" << std::endl;
		if (!g_npcs.loadFromXml()) {
			std::clog << "Unable to load npcs! Continue? (y/N)" << std::endl;
			char buffer = getchar();
			if (buffer != 121 && buffer != 89) {
				startupErrorMessage("Unable to load npcs!");
			}
		}
	}

	std::clog << ">> Loading raids" << std::endl;
	Raids::getInstance()->loadFromXml();

	std::clog << ">> Loading map and spawns..." << std::endl;
	if (!g_game.loadMap(otx::config::getString(otx::config::MAP_NAME))) {
		startupErrorMessage();
	}

	std::clog << ">> Checking world type... ";
	std::string worldType = otx::util::as_lower_string(otx::config::getString(otx::config::WORLD_TYPE));
	if (worldType == "open" || worldType == "2" || worldType == "openpvp") {
		g_game.setWorldType(WORLDTYPE_OPEN);
		std::clog << "Open PvP" << std::endl;
	} else if (worldType == "optional" || worldType == "1" || worldType == "optionalpvp") {
		g_game.setWorldType(WORLDTYPE_OPTIONAL);
		std::clog << "Optional PvP" << std::endl;
	} else if (worldType == "hardcore" || worldType == "3" || worldType == "hardcorepvp") {
		g_game.setWorldType(WORLDTYPE_HARDCORE);
		std::clog << "Hardcore PvP" << std::endl;
	} else {
		std::clog << std::endl;
		startupErrorMessage("Unknown world type: " + otx::config::getString(otx::config::WORLD_TYPE));
	}

	std::clog << ">> Starting to dominate the world... done." << std::endl;

	std::clog << ">> Initializing game state and binding services:" << std::endl;
	g_game.setGameState(GAMESTATE_INIT);

	services->add<ProtocolStatus>(otx::config::getInteger(otx::config::STATUS_PORT));
	if (!otx::config::getBoolean(otx::config::LOGIN_ONLY_LOGINSERVER)) {
		services->add<ProtocolLogin>(otx::config::getInteger(otx::config::LOGIN_PORT));
	}

	services->add<ProtocolOld>(otx::config::getInteger(otx::config::LOGIN_PORT));
	services->add<ProtocolGame>(otx::config::getInteger(otx::config::GAME_PORT));

	std::clog << std::endl << ">> Everything smells good, server is starting up..." << std::endl;
	g_game.start(services);
	g_game.setGameState(otx::config::getBoolean(otx::config::START_CLOSED) ? GAMESTATE_CLOSED : GAMESTATE_NORMAL);
	g_loaderSignal.notify_all();
}
