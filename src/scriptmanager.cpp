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

#include "scriptmanager.h"

#include "actions.h"
#include "creatureevent.h"
#include "globalevent.h"
#include "lua_definitions.h"
#include "movement.h"
#include "spells.h"
#include "talkaction.h"
#include "weapons.h"

void otx::scriptmanager::init()
{
	g_actions.init();
	g_creatureEvents.init();
	g_globalEvents.init();
	g_moveEvents.init();
	g_spells.init();
	g_talkActions.init();
	g_weapons.init();

	Raids::init();
}

bool otx::scriptmanager::load()
{
	std::clog << ">>> Loading global... ";

	LuaInterface* mainInterface = g_lua.getMainInterface();
	if (!mainInterface->loadFile("data/global.lua")) {
		std::clog << "failed!" << std::endl;
		return false;
	}
	std::clog << "(done)." << std::endl;

	std::clog << ">>> Loading weapons... ";
	if (!g_weapons.loadFromXml()) {
		std::clog << "failed!" << std::endl;
		return false;
	}
	std::clog << "(done)." << std::endl;

	std::clog << ">>> Preparing weapons... ";
	g_weapons.loadDefaults();
	std::clog << "(done)." << std::endl;

	std::clog << ">>> Loading spells... ";
	if (!g_spells.loadFromXml()) {
		std::clog << "failed!" << std::endl;
		return false;
	}
	std::clog << "(done)." << std::endl;

	std::clog << ">>> Loading actions... ";
	if (!g_actions.loadFromXml()) {
		std::clog << "failed!" << std::endl;
		return false;
	}
	std::clog << "(done)." << std::endl;

	std::clog << ">>> Loading talkactions... ";
	if (!g_talkActions.loadFromXml()) {
		std::clog << "failed!" << std::endl;
		return false;
	}
	std::clog << "(done)." << std::endl;

	std::clog << ">>> Loading movements... ";
	if (!g_moveEvents.loadFromXml()) {
		std::clog << "failed!" << std::endl;
		return false;
	}
	std::clog << "(done)." << std::endl;

	std::clog << ">>> Loading creaturescripts... ";
	if (!g_creatureEvents.loadFromXml()) {
		std::clog << "failed!" << std::endl;
		return false;
	}
	std::clog << "(done)." << std::endl;

	std::clog << ">>> Loading globalscripts... ";
	if (!g_globalEvents.loadFromXml()) {
		std::clog << "failed!" << std::endl;
		return false;
	}
	std::clog << "(done)." << std::endl;
	return true;
}

void otx::scriptmanager::terminate()
{
	g_actions.terminate();
	g_creatureEvents.terminate();
	g_globalEvents.terminate();
	g_moveEvents.terminate();
	g_spells.terminate();
	g_talkActions.terminate();
	g_weapons.terminate();

	Raids::terminate();
}
