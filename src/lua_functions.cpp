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

#include "lua_functions.h"

#include "baseevents.h"
#include "beds.h"
#include "chat.h"
#include "combat.h"
#include "condition.h"
#include "configmanager.h"
#include "creatureevent.h"
#include "database.h"
#include "databasemanager.h"
#include "game.h"
#include "house.h"
#include "housetile.h"
#include "ioban.h"
#include "ioguild.h"
#include "iologindata.h"
#include "iomap.h"
#include "iomapserialize.h"
#include "item.h"
#include "lua_definitions.h"
#include "monsters.h"
#include "movement.h"
#include "npc.h"
#include "player.h"
#include "raids.h"
#include "spells.h"
#include "talkaction.h"
#include "teleport.h"
#include "town.h"
#include "vocation.h"

#include "otx/util.hpp"

namespace
{
	uint32_t lastResultId = 0;
	std::unordered_map<uint32_t, DBResultPtr> dbResultsMap;

	DBResultPtr getDBResultById(uint32_t id)
	{
		auto it = dbResultsMap.find(id);
		if (it != dbResultsMap.end()) {
			return it->second;
		}
		return nullptr;
	}

	bool removeDBResultById(uint32_t id)
	{
		auto it = dbResultsMap.find(id);
		if (it != dbResultsMap.end()) {
			dbResultsMap.erase(id);
			return true;
		}
		return false;
	}

	uint32_t addDBResult(DBResultPtr result)
	{
		dbResultsMap.emplace(++lastResultId, std::move(result));
		return lastResultId;
	}

	bool parseCombatArea(lua_State* L, int index, std::vector<uint8_t>& vec, uint32_t& rows)
	{
		rows = 0;
		vec.clear();
		if (!otx::lua::isTable(L, index)) {
			return false;
		}

		const int tableSize = lua_objlen(L, index);
		for (int i = 1; i <= tableSize; ++i) {
			lua_rawgeti(L, index, i);
			if (!otx::lua::isTable(L, -1)) {
				lua_pop(L, 1);
				return false;
			}

			const int cols = lua_objlen(L, -1);
			for (int j = 1; j <= cols; ++j) {
				lua_rawgeti(L, -1, j);
				vec.push_back(otx::lua::getNumber<uint8_t>(L, -1));
				lua_pop(L, 1);
			}
			lua_pop(L, 1);
		}

		rows = tableSize;
		return rows != 0;
	}

} // namespace

static int luaGetPlayerNameDescription(lua_State* L)
{
	// getPlayerNameDescription(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		otx::lua::pushString(L, player->getNameDescription());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerSpecialDescription(lua_State* L)
{
	// getPlayerSpecialDescription(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		otx::lua::pushString(L, player->getSpecialDescription());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerFood(lua_State* L)
{
	// getPlayerFood(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		if (Condition* condition = player->getCondition(CONDITION_REGENERATION, CONDITIONID_DEFAULT)) {
			lua_pushnumber(L, condition->getTicks() / 1000);
		} else {
			lua_pushnumber(L, 0);
		}
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerAccess(lua_State* L)
{
	// getPlayerAccess(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		lua_pushnumber(L, player->getAccess());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerGhostAccess(lua_State* L)
{
	// getPlayerGhostAccess(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		lua_pushnumber(L, player->getGhostAccess());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerLevel(lua_State* L)
{
	// getPlayerLevel(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		lua_pushnumber(L, player->getLevel());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerExperience(lua_State* L)
{
	// getPlayerExperience(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		lua_pushnumber(L, player->getExperience());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerSpentMana(lua_State* L)
{
	// getPlayerSpentMana(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		lua_pushnumber(L, player->getSpentMana());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerVocation(lua_State* L)
{
	// getPlayerVocation(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		lua_pushnumber(L, player->getVocationId());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerMoney(lua_State* L)
{
	// getPlayerMoney(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		lua_pushnumber(L, g_game.getMoney(player));
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerFreeCap(lua_State* L)
{
	// getPlayerFreeCap(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		lua_pushnumber(L, static_cast<int64_t>(player->getFreeCapacity()));
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerGuildId(lua_State* L)
{
	// getPlayerGuildId(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		lua_pushnumber(L, player->getGuildId());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerGuildName(lua_State* L)
{
	// getPlayerGuildName(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		otx::lua::pushString(L, player->getGuildName());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerGuildRankId(lua_State* L)
{
	// getPlayerGuildRankId(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		lua_pushnumber(L, player->getRankId());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerGuildRank(lua_State* L)
{
	// getPlayerGuildRank(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		otx::lua::pushString(L, player->getRankName());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerGuildLevel(lua_State* L)
{
	// getPlayerGuildLevel(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		lua_pushnumber(L, player->getGuildLevel());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerGuildNick(lua_State* L)
{
	// getPlayerGuildNick(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		otx::lua::pushString(L, player->getGuildNick());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerTown(lua_State* L)
{
	// getPlayerTown(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		lua_pushnumber(L, player->getTown());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerPromotionLevel(lua_State* L)
{
	// getPlayerPromotionLevel(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		lua_pushnumber(L, player->getPromotionLevel());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerGroupId(lua_State* L)
{
	// getPlayerGroupId(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		lua_pushnumber(L, player->getGroupId());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerGUID(lua_State* L)
{
	// getPlayerGUID(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		lua_pushnumber(L, player->getGUID());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerAccountId(lua_State* L)
{
	// getPlayerAccountId(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		lua_pushnumber(L, player->getAccount());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerAccount(lua_State* L)
{
	// getPlayerAccount(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		otx::lua::pushString(L, player->getAccountName());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerPremiumDays(lua_State* L)
{
	// getPlayerPremiumDays(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		lua_pushnumber(L, player->getPremiumDays());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerBalance(lua_State* L)
{
	// getPlayerBalance(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		if (otx::config::getBoolean(otx::config::BANK_SYSTEM)) {
			lua_pushnumber(L, player->m_balance);
		} else {
			lua_pushnumber(L, 0);
		}
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerStamina(lua_State* L)
{
	// getPlayerStamina(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		lua_pushnumber(L, player->getStaminaMinutes());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerLossSkill(lua_State* L)
{
	// getPlayerLossSkill(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		lua_pushboolean(L, player->getLossSkill());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerPartner(lua_State* L)
{
	// getPlayerPartner(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		lua_pushnumber(L, player->m_marriage);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaIsPlayerPzLocked(lua_State* L)
{
	// isPlayerPzLocked(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		lua_pushboolean(L, player->isPzLocked());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaIsPlayerSaving(lua_State* L)
{
	// isPlayerSaving(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		lua_pushboolean(L, player->isSaving());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaIsPlayerProtected(lua_State* L)
{
	// isPlayerProtected(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		lua_pushboolean(L, player->isProtected());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerIp(lua_State* L)
{
	// getPlayerIp(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		lua_pushnumber(L, player->getIP());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerSkullEnd(lua_State* L)
{
	// getPlayerSkullEnd(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		lua_pushnumber(L, player->getSkullEnd());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoPlayerSendOutfitWindow(lua_State* L)
{
	// doPlayerSendOutfitWindow(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		player->sendOutfitWindow();
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerIdleTime(lua_State* L)
{
	// getPlayerIdleTime(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		lua_pushnumber(L, player->getIdleTime());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaHasPlayerClient(lua_State* L)
{
	// hasPlayerClient(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		lua_pushboolean(L, player->hasClient());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerLastLoad(lua_State* L)
{
	// getPlayerLastLoad(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		lua_pushnumber(L, player->getLastLoad());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerLastLogin(lua_State* L)
{
	// getPlayerLastLogin(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		lua_pushnumber(L, player->getLastLogin());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerAccountManager(lua_State* L)
{
	// getPlayerAccountManager(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		lua_pushnumber(L, player->getAccountManagerType());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerTradeState(lua_State* L)
{
	// getPlayerTradeState(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		lua_pushnumber(L, player->getTradeState());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerOperatingSystem(lua_State* L)
{
	// getPlayerOperatingSystem(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		lua_pushnumber(L, player->getOperatingSystem());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerClientVersion(lua_State* L)
{
	// getPlayerClientVersion(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		lua_pushnumber(L, player->getClientVersion());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerSex(lua_State* L)
{
	// getPlayerSex(cid[, full = false])
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const bool full = otx::lua::getBoolean(L, 2, false);
		lua_pushnumber(L, player->getSex(full));
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoPlayerSetNameDescription(lua_State* L)
{
	// doPlayerSetNameDescription(cid, description)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const std::string description = otx::lua::getString(L, 2);
		player->setNameDescription(player->getNameDescription() + description);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoPlayerSetSpecialDescription(lua_State* L)
{
	// doPlayerSetSpecialDescription(cid, description)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const std::string description = otx::lua::getString(L, 2);
		player->setSpecialDescription(description);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerMagLevel(lua_State* L)
{
	// getPlayerMagLevel(cid[, ignoreModifiers = false])
	if (const Player* player = otx::lua::getPlayer(L, 1)) {
		const bool ignoreModifiers = otx::lua::getBoolean(L, 2, false);
		lua_pushnumber(L, ignoreModifiers ? player->getBaseMagicLevel() : player->getMagicLevel());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerRequiredMana(lua_State* L)
{
	// getPlayerRequiredMana(cid, magicLevel)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const auto magLevel = otx::lua::getNumber<uint32_t>(L, 2);
		lua_pushnumber(L, player->getVocation()->getReqMana(magLevel));
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerRequiredSkillTries(lua_State* L)
{
	// getPlayerRequiredSkillTries(cid, skill, level)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const auto skill = otx::lua::getNumber<uint8_t>(L, 2);
		const auto level = otx::lua::getNumber<int32_t>(L, 3);
		lua_pushnumber(L, player->getVocation()->getReqSkillTries(skill, level));
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerFlagValue(lua_State* L)
{
	// getPlayerFlagValue(cid, flag)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const auto flag = static_cast<PlayerFlags>(otx::lua::getNumber<uint8_t>(L, 2));
		if (flag < PlayerFlag_LastFlag) {
			lua_pushboolean(L, player->hasFlag(flag));
		} else {
			otx::lua::reportErrorEx(L, "Invalid flag value.");
			lua_pushnil(L);
		}
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerCustomFlagValue(lua_State* L)
{
	// getPlayerCustomFlagValue(cid, flag)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const auto flag = static_cast<PlayerCustomFlags>(otx::lua::getNumber<uint8_t>(L, 2));
		if (flag < PlayerCustomFlag_LastFlag) {
			lua_pushboolean(L, player->hasCustomFlag(flag));
		} else {
			otx::lua::reportErrorEx(L, "Invalid flag value.");
			lua_pushnil(L);
		}
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoPlayerLearnInstantSpell(lua_State* L)
{
	// doPlayerLearnInstantSpell(cid, spellName)
	Player* player = otx::lua::getPlayer(L, 1);
	if (!player) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	const std::string spellName = otx::lua::getString(L, 2);
	if (InstantSpell* spell = g_spells.getInstantSpellByName(spellName)) {
		player->learnInstantSpell(spell->getName());
		lua_pushboolean(L, true);
	} else {
		lua_pushboolean(L, false);
	}
	return 1;
}

static int luaDoPlayerUnlearnInstantSpell(lua_State* L)
{
	// doPlayerUnlearnInstantSpell(cid, spellName)
	Player* player = otx::lua::getPlayer(L, 1);
	if (!player) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	const std::string spellName = otx::lua::getString(L, 2);
	if (InstantSpell* spell = g_spells.getInstantSpellByName(spellName)) {
		player->unlearnInstantSpell(spell->getName());
		lua_pushboolean(L, true);
	} else {
		lua_pushboolean(L, false);
	}
	return 1;
}

static int luaGetPlayerLearnedInstantSpell(lua_State* L)
{
	// getPlayerLearnedInstantSpell(cid, spellName)
	Player* player = otx::lua::getPlayer(L, 1);
	if (!player) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	const std::string spellName = otx::lua::getString(L, 2);
	if (InstantSpell* spell = g_spells.getInstantSpellByName(spellName)) {
		lua_pushboolean(L, player->hasLearnedInstantSpell(spellName));
	} else {
		lua_pushboolean(L, false);
	}
	return 1;
}

static int luaGetPlayerInstantSpellCount(lua_State* L)
{
	// getPlayerInstantSpellCount(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		lua_pushnumber(L, g_spells.getInstantSpellCount(player));
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerInstantSpellInfo(lua_State* L)
{
	// getPlayerInstantSpellInfo(cid, index)
	Player* player = otx::lua::getPlayer(L, 1);
	if (!player) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	const auto index = otx::lua::getNumber<uint32_t>(L, 2);
	InstantSpell* spell = g_spells.getInstantSpellByIndex(player, index);
	if (!spell) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_SPELL_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	lua_createtable(L, 0, 6);
	otx::lua::setTableValue(L, "name", spell->getName());
	otx::lua::setTableValue(L, "words", spell->getWords());
	otx::lua::setTableValue(L, "level", spell->getLevel());
	otx::lua::setTableValue(L, "mlevel", spell->getMagicLevel());
	otx::lua::setTableValue(L, "mana", spell->getManaCost(player));
	otx::lua::setTableValue(L, "manapercent", spell->getManaPercent());
	return 1;
}

static int luaGetInstantSpellInfo(lua_State* L)
{
	// getInstantSpellInfo(name)
	InstantSpell* spell = g_spells.getInstantSpellByName(otx::lua::getString(L, 1));
	if (!spell) {
		lua_pushnil(L);
		return 1;
	}

	lua_createtable(L, 0, 6);
	otx::lua::setTableValue(L, "name", spell->getName());
	otx::lua::setTableValue(L, "words", spell->getWords());
	otx::lua::setTableValue(L, "level", spell->getLevel());
	otx::lua::setTableValue(L, "mlevel", spell->getMagicLevel());
	otx::lua::setTableValue(L, "mana", spell->getManaCost(nullptr));
	otx::lua::setTableValue(L, "manapercent", spell->getManaPercent());
	return 1;
}

static int luaDoCreatureCastSpell(lua_State* L)
{
	// doCreatureCastSpell(uid, spellName)
	Creature* creature = otx::lua::getCreature(L, 1);
	if (!creature) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	const std::string spellName = otx::lua::getString(L, 2);
	if (Spell* spell = g_spells.getInstantSpellByName(spellName)) {
		lua_pushboolean(L, spell->castSpell(creature));
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_SPELL_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoRemoveItem(lua_State* L)
{
	// doRemoveItem(uid[, count = -1])
	if (Item* item = otx::lua::getItem(L, 1)) {
		const auto count = otx::lua::getNumber<int32_t>(L, 2, -1);
		lua_pushboolean(L, g_game.internalRemoveItem(nullptr, item, count) != RET_NOERROR);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_ITEM_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoPlayerRemoveItem(lua_State* L)
{
	// doPlayerRemoveItem(cid, itemId, count[, subType = -1, ignoreEquipped = false])
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const auto itemId = otx::lua::getNumber<uint16_t>(L, 2);
		const auto count = otx::lua::getNumber<int32_t>(L, 3);
		const auto subType = otx::lua::getNumber<int32_t>(L, 4, -1);
		const auto ignoreEquipped = otx::lua::getBoolean(L, 5, false);
		lua_pushboolean(L, g_game.removeItemOfType(player, itemId, count, subType, ignoreEquipped));
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoPlayerFeed(lua_State* L)
{
	// doPlayerFeed(cid, food)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const auto food = otx::lua::getNumber<uint32_t>(L, 2);
		player->addDefaultRegeneration(food * 3000);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoPlayerSendCancel(lua_State* L)
{
	// doPlayerSendCancel(cid, text)
	if (const Player* player = otx::lua::getPlayer(L, 1)) {
		const std::string text = otx::lua::getString(L, 2);
		player->sendCancel(text);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoPlayerSendCastList(lua_State* L)
{
	// doPlayerSendCastList(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		player->sendCastList();
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoSendDefaultCancel(lua_State* L)
{
	// doPlayerSendDefaultCancel(cid, ReturnValue)
	if (const Player* player = otx::lua::getPlayer(L, 1)) {
		const auto ret = static_cast<ReturnValue>(otx::lua::getNumber<uint8_t>(L, 2));
		player->sendCancelMessage(ret);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetSearchString(lua_State* L)
{
	// getSearchString(fromPosition, toPosition[, fromIsCreature = false, toIsCreature = false])
	const Position fromPosition = otx::lua::getPosition(L, 1);
	const Position toPosition = otx::lua::getPosition(L, 2);
	const bool fromIsCreature = otx::lua::getBoolean(L, 3, false);
	const bool toIsCreature = otx::lua::getBoolean(L, 4, false);

	if (toPosition.x == 0 || toPosition.y == 0 || fromPosition.x == 0 || fromPosition.y == 0) {
		otx::lua::reportErrorEx(L, "Wrong position(s) specified");
		lua_pushnil(L);
	} else {
		otx::lua::pushString(L, g_game.getSearchString(fromPosition, toPosition, fromIsCreature, toIsCreature));
	}
	return 1;
}

static int luaGetClosestFreeTile(lua_State* L)
{
	// getClosestFreeTile(cid, targetPos[, extended = false, ignoreHouse = true])
	if (Creature* creature = otx::lua::getCreature(L, 1)) {
		const Position targetPos = otx::lua::getPosition(L, 2);
		const bool extended = otx::lua::getBoolean(L, 3, false);
		const bool ignoreHouse = otx::lua::getBoolean(L, 4, true);

		Position newPos = g_game.getClosestFreeTile(creature, targetPos, extended, ignoreHouse);
		if (newPos.x != 0) {
			otx::lua::pushPosition(L, newPos);
		} else {
			lua_pushnil(L);
		}
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoTeleportThing(lua_State* L)
{
	// doTeleportThing(cid, destination[, pushMove = true, fullTeleport = true])
	if (Thing* thing = otx::lua::getThing(L, 1)) {
		const Position destination = otx::lua::getPosition(L, 2);
		const bool pushMove = otx::lua::getBoolean(L, 3, true);
		const bool fullTeleport = otx::lua::getBoolean(L, 4, true);
		lua_pushboolean(L, g_game.internalTeleport(thing, destination, !pushMove, FLAG_NOLIMIT, fullTeleport) == RET_NOERROR);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_THING_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoItemSetDestination(lua_State* L)
{
	// doItemSetDestination(uid, destination)
	Item* item = otx::lua::getItem(L, 1);
	if (!item) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_ITEM_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	const Position destination = otx::lua::getPosition(L, 2);
	if (Teleport* teleport = item->getTeleport()) {
		teleport->setDestination(destination);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, "Target item is not a teleport.");
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoTransformItem(lua_State* L)
{
	// doTransformItem(uid, newId[, count/subType = -1])
	const auto uid = otx::lua::getNumber<uint32_t>(L, 1);
	const auto newId = otx::lua::getNumber<uint16_t>(L, 2);
	auto count = otx::lua::getNumber<int32_t>(L, 3, -1);

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	Item* item = env.getItemByUID(uid);
	if (!item) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_ITEM_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	const ItemType& it = Item::items[newId];
	if (it.stackable && count > 100) {
		count = 100;
	}

	Item* newItem = g_game.transformItem(item, newId, count);
	if (item->isRemoved()) {
		env.removeItem(uid);
	}

	if (newItem && newItem != item) {
		env.insertItem(uid, newItem);
	}

	lua_pushboolean(L, true);
	return 1;
}

static int luaDoCreatureSay(lua_State* L)
{
	// doCreatureSay(uid, text[, messageType = MSG_SPEAK_SAY, ghost = false, cid = 0, pos])
	const std::string text = otx::lua::getString(L, 2);
	const auto messageType = static_cast<MessageClasses>(otx::lua::getNumber<uint8_t>(L, 3, MSG_SPEAK_SAY));
	const bool ghost = otx::lua::getBoolean(L, 4, false);
	const auto creatureId = otx::lua::getNumber<uint32_t>(L, 5, 0);

	Creature* creature = otx::lua::getCreature(L, 1);
	if (!creature) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	SpectatorVec list;
	if (creatureId != 0) {
		Creature* target = g_game.getCreatureByID(creatureId);
		if (!target) {
			otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
			lua_pushnil(L);
			return 1;
		}

		list.insert(target);
	}

	if (lua_gettop(L) >= 6) {
		Position pos = otx::lua::getPosition(L, 6);
		lua_pushboolean(L, g_game.internalCreatureSay(creature, messageType, text, ghost, &list, &pos));
	} else {
		lua_pushboolean(L, g_game.internalCreatureSay(creature, messageType, text, ghost, &list));
	}
	return 1;
}

static int luaDoCreatureChannelSay(lua_State* L)
{
	// doCreatureChannelSay(cid, speaker, message, messageType, channelId)
	Player* player = otx::lua::getPlayer(L, 1);
	if (!player) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	Creature* speaker = otx::lua::getCreature(L, 2);
	if (!speaker) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	const std::string message = otx::lua::getString(L, 3);
	const auto messageType = static_cast<MessageClasses>(otx::lua::getNumber<uint8_t>(L, 4));
	const auto channelId = otx::lua::getNumber<uint16_t>(L, 5);
	player->sendCreatureChannelSay(speaker, messageType, message, channelId);
	lua_pushboolean(L, true);
	return 1;
}

static int luaDoSendMagicEffect(lua_State* L)
{
	// doSendMagicEffect(pos, effectId[, player])
	const Position pos = otx::lua::getPosition(L, 1);
	const auto effectId = otx::lua::getNumber<uint16_t>(L, 2);

	if (Player* player = otx::lua::getPlayer(L, 3)) {
		player->sendMagicEffect(pos, effectId);
	} else {
		g_game.addMagicEffect(pos, effectId);
	}

	lua_pushboolean(L, true);
	return 1;
}

static int luaDoSendDistanceShoot(lua_State* L)
{
	// doSendDistanceShoot(fromPos, toPos, effectId[, player])
	const Position fromPos = otx::lua::getPosition(L, 1);
	const Position toPos = otx::lua::getPosition(L, 2);
	const auto effectId = otx::lua::getNumber<uint16_t>(L, 3);

	if (Player* player = otx::lua::getPlayer(L, 4)) {
		player->sendDistanceShoot(fromPos, toPos, effectId);
	} else {
		g_game.addDistanceEffect(fromPos, toPos, effectId);
	}

	lua_pushboolean(L, true);
	return 1;
}

static int luaDoPlayerAddSkillTry(lua_State* L)
{
	// doPlayerAddSkillTry(cid, skillId, count[, useMultiplier = true])
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const auto skillId = static_cast<skills_t>(otx::lua::getNumber<uint8_t>(L, 2));
		const auto count = otx::lua::getNumber<uint64_t>(L, 3);
		const bool useMultiplier = otx::lua::getBoolean(L, 4, true);

		player->addSkillAdvance(skillId, count, useMultiplier);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetCreatureSpeakType(lua_State* L)
{
	// getCreatureSpeakType(cid)
	if (const Creature* creature = otx::lua::getCreature(L, 1)) {
		lua_pushnumber(L, creature->getSpeakType());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoCreatureSetSpeakType(lua_State* L)
{
	// doCreatureSetSpeakType(cid, messageType)
	if (Creature* creature = otx::lua::getCreature(L, 1)) {
		const auto messageType = static_cast<MessageClasses>(otx::lua::getNumber<uint8_t>(L, 2));
		if (!((messageType >= MSG_SPEAK_FIRST && messageType <= MSG_SPEAK_LAST) || (messageType >= MSG_SPEAK_MONSTER_FIRST && messageType <= MSG_SPEAK_MONSTER_LAST))) {
			otx::lua::reportErrorEx(L, "Invalid speak type");
			lua_pushnil(L);
			return 1;
		}

		creature->setSpeakType(messageType);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetCreatureHideHealth(lua_State* L)
{
	// getCreatureHideHealth(cid)
	if (Creature* creature = otx::lua::getCreature(L, 1)) {
		lua_pushboolean(L, creature->getHideHealth());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoCreatureSetHideHealth(lua_State* L)
{
	// doCreatureSetHideHealth(cid, hide)
	if (Creature* creature = otx::lua::getCreature(L, 1)) {
		const bool hide = otx::lua::getBoolean(L, 2);
		creature->setHideHealth(hide);
		g_game.addCreatureHealth(creature);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoCreatureAddHealth(lua_State* L)
{
	// doCreatureAddHealth(uid, health[, hitEffect = MAGIC_EFFECT_NONE, hitColor = COLOR_NONE, force = false])
	if (Creature* creature = otx::lua::getCreature(L, 1)) {
		CombatDamage damage;
		damage.primary.value = otx::lua::getNumber<int32_t>(L, 2);
		damage.primary.type = (damage.primary.value < 1 ? COMBAT_UNDEFINEDDAMAGE : COMBAT_HEALING);

		const auto hitEffect = static_cast<MagicEffect_t>(otx::lua::getNumber<uint16_t>(L, 3));
		const auto hitColor = static_cast<Color_t>(otx::lua::getNumber<uint16_t>(L, 4));
		const bool force = otx::lua::getBoolean(L, 5, false);
		g_game.combatChangeHealth(nullptr, creature, damage, hitEffect, hitColor, force);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoCreatureAddMana(lua_State* L)
{
	// doCreatureAddMana(uid, mana[, aggressive = true])
	if (Creature* creature = otx::lua::getCreature(L, 1)) {
		const auto mana = otx::lua::getNumber<int32_t>(L, 2);
		const bool aggressive = otx::lua::getBoolean(L, 3, true);
		if (aggressive) {
			CombatDamage damage;
			damage.primary.value = mana;
			damage.primary.type = COMBAT_MANADRAIN;
			g_game.combatChangeMana(nullptr, creature, damage);
		} else {
			creature->changeMana(mana);
		}

		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoPlayerAddSpentMana(lua_State* L)
{
	// doPlayerAddSpentMana(cid, amount[, useMultiplier = true])
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const auto amount = otx::lua::getNumber<uint64_t>(L, 2);
		const bool useMultiplier = otx::lua::getBoolean(L, 3, true);
		player->addManaSpent(amount, useMultiplier);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoPlayerAddItem(lua_State* L)
{
	// doPlayerAddItem(cid, itemId[, count/subType = 1, canDropOnMap = true, slot = 0])
	// doPlayerAddItem(cid, itemId[, count = 1, canDropOnMap = true, subType = 1, slot = 0])
	const auto itemId = otx::lua::getNumber<uint16_t>(L, 2);
	const auto count = otx::lua::getNumber<int32_t>(L, 3);
	const bool canDropOnMap = otx::lua::getBoolean(L, 4, false);

	slots_t slot;
	int32_t subType;
	const int nparam = lua_gettop(L);
	if (nparam >= 6) {
		slot = static_cast<slots_t>(otx::lua::getNumber<uint8_t>(L, 6));
		subType = otx::lua::getNumber<int32_t>(L, 5);
	} else {
		slot = static_cast<slots_t>(otx::lua::getNumber<uint8_t>(L, 5, SLOT_WHEREEVER));
		subType = 1;
	}

	if (slot > SLOT_AMMO) {
		otx::lua::reportErrorEx(L, "Invalid slot");
		lua_pushnil(L);
		return 1;
	}

	Player* player = otx::lua::getPlayer(L, 1);
	if (!player) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	const ItemType& it = Item::items[itemId];
	int32_t itemCount = 1;
	if (nparam >= 6) {
		itemCount = std::max<int32_t>(1, count);
	} else if (it.hasSubType()) {
		if (it.stackable) {
			itemCount = std::ceil(count / 100.0);
		}

		subType = count;
	}

	uint32_t ret = 0;
	while (itemCount-- > 0) {
		int32_t stackCount = std::min(100, subType);
		Item* newItem = Item::CreateItem(itemId, stackCount);
		if (!newItem) {
			otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_ITEM_NOT_FOUND));
			lua_pushnil(L);
			return ++ret;
		}

		if (it.stackable) {
			subType -= stackCount;
		}

		Item* stackItem = nullptr;
		if (g_game.internalPlayerAddItem(nullptr, player, newItem, canDropOnMap, slot, &stackItem) != RET_NOERROR) {
			delete newItem;
			lua_pushboolean(L, false);
			return ++ret;
		}

		++ret;
		if (newItem->getParent()) {
			lua_pushnumber(L, otx::lua::getScriptEnv().addThing(newItem));
		} else if (stackItem) {
			lua_pushnumber(L, otx::lua::getScriptEnv().addThing(stackItem));
		} else {
			lua_pushnil(L);
		}
	}

	if (ret != 0) {
		return ret;
	}

	lua_pushnil(L);
	return 1;
}

static int luaDoPlayerAddItemEx(lua_State* L)
{
	// doPlayerAddItemEx(cid, uid[, canDropOnMap = false, slot = SLOT_WHEREEVER])
	Player* player = otx::lua::getPlayer(L, 1);
	if (!player) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	Item* item = otx::lua::getItem(L, 2);
	if (!item) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_ITEM_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	const bool canDropOnMap = otx::lua::getBoolean(L, 3, false);
	const auto slot = static_cast<slots_t>(otx::lua::getNumber<uint32_t>(L, 4, SLOT_WHEREEVER));

	if (slot > SLOT_AMMO) {
		otx::lua::reportErrorEx(L, "Invalid slot type.");
		lua_pushnil(L);
		return 1;
	}

	if (item->getParent() == VirtualCylinder::virtualCylinder) {
		ReturnValue ret = g_game.internalPlayerAddItem(nullptr, player, item, canDropOnMap, slot);
		if (ret == RET_NOERROR) {
			otx::lua::removeTempItem(item);
		}
		lua_pushnumber(L, ret);
	} else {
		lua_pushboolean(L, false);
	}
	return 1;
}

static int luaDoTileAddItemEx(lua_State* L)
{
	// doTileAddItemEx(pos, uid)
	const Position pos = otx::lua::getPosition(L, 1);
	Item* item = otx::lua::getItem(L, 2);
	if (!item) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_ITEM_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	Tile* tile = g_game.getTile(pos);
	if (!tile) {
		if (item->isGroundTile()) {
			if (item->getParent() == VirtualCylinder::virtualCylinder) {
				tile = IOMap::createTile(item, nullptr, pos.x, pos.y, pos.z);
				g_game.setTile(tile);

				otx::lua::removeTempItem(item);
				lua_pushnumber(L, RET_NOERROR);
			} else {
				lua_pushboolean(L, false);
			}
		} else {
			otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_TILE_NOT_FOUND));
			lua_pushnil(L);
		}
		return 1;
	}

	if (item->getParent() == VirtualCylinder::virtualCylinder) {
		ReturnValue ret = g_game.internalAddItem(nullptr, tile, item);
		if (ret == RET_NOERROR) {
			otx::lua::removeTempItem(item);
		}

		lua_pushnumber(L, ret);
	} else {
		lua_pushboolean(L, false);
	}
	return 1;
}

static int luaDoRelocate(lua_State* L)
{
	// doRelocate(fromPos, toPos[, creatures = true, unmovable = true])
	const Position fromPos = otx::lua::getPosition(L, 1);
	const Position toPos = otx::lua::getPosition(L, 2);
	const bool creatures = otx::lua::getBoolean(L, 3, true);
	const bool unmovable = otx::lua::getBoolean(L, 4, true);

	Tile* fromTile = g_game.getTile(fromPos);
	Tile* toTile = g_game.getTile(toPos);
	if (!fromTile || !toTile) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_TILE_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	if (fromTile == toTile) {
		lua_pushboolean(L, false);
		return 1;
	}

	TileItemVector* toItems = toTile->getItemList();
	TileItemVector* fromItems = fromTile->getItemList();

	if (fromItems && toItems) {
		int32_t itemLimit = otx::config::getInteger(toTile->hasFlag(TILESTATE_PROTECTIONZONE) ? otx::config::PROTECTION_TILE_LIMIT : otx::config::TILE_LIMIT);
		int32_t count = 0;
		for (auto it = fromItems->getBeginDownItem(); it != fromItems->getEndDownItem();) {
			if (itemLimit != 0 && static_cast<int32_t>(toItems->size()) > itemLimit) {
				break;
			}

			const ItemType& iType = Item::items[(*it)->getID()];
			if (!iType.isGroundTile() && !iType.alwaysOnTop && !iType.isMagicField() && (unmovable || iType.movable) && !iType.isDoor()) {
				if (Item* item = (*it)) {
					it = fromItems->erase(it);
					fromItems->removeDownItem();
					fromTile->updateTileFlags(item, true);

					g_moveEvents.onItemMove(nullptr, item, fromTile, false);
					g_moveEvents.onRemoveTileItem(fromTile, item);

					item->setParent(toTile);
					++count;

					toItems->insert(toItems->getBeginDownItem(), item);
					toItems->addDownItem();
					toTile->updateTileFlags(item, false);

					g_moveEvents.onAddTileItem(toTile, item);
					g_moveEvents.onItemMove(nullptr, item, toTile, true);
				} else {
					++it;
				}
			} else {
				++it;
			}
		}

		fromTile->updateThingCount(-count);
		toTile->updateThingCount(count);

		fromTile->onUpdateTile();
		toTile->onUpdateTile();

		if (otx::config::getBoolean(otx::config::STORE_TRASH) && fromTile->hasFlag(TILESTATE_TRASHED)) {
			g_game.addTrash(toPos);
			toTile->setFlag(TILESTATE_TRASHED);
		}
	}

	if (creatures) {
		CreatureVector* creatureVector = fromTile->getCreatures();
		Creature* creature = nullptr;
		while (creatureVector && !creatureVector->empty()) {
			if ((creature = (*creatureVector->begin()))) {
				g_game.internalMoveCreature(nullptr, creature, fromTile, toTile, FLAG_NOLIMIT);
			}
		}
	}

	lua_pushboolean(L, true);
	return 1;
}

static int luaDoCleanTile(lua_State* L)
{
	// doCleanTile(pos[, forceMapLoaded = false])
	// Remove all items from tile, ignore creatures
	const Position pos = otx::lua::getPosition(L, 1);
	const bool forceMapLoaded = otx::lua::getBoolean(L, 2, false);

	Tile* tile = g_game.getTile(pos);
	if (!tile) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_TILE_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	// ignore ground
	for (int32_t i = tile->getThingCount() - 1; i >= 1; --i) {
		Thing* thing = tile->__getThing(i);
		if (!thing) {
			continue;
		}

		Item* item = thing->getItem();
		if (!item) {
			continue;
		}

		if (!item->isLoadedFromMap() || forceMapLoaded) {
			g_game.internalRemoveItem(nullptr, item);
		}
	}

	lua_pushboolean(L, true);
	return 1;
}

static int luaDoPlayerSendTextMessage(lua_State* L)
{
	// doPlayerSendTextMessage(cid, messageType, message[, value = 0, color = COLOR_WHITE, position])
	Player* player = otx::lua::getPlayer(L, 1);
	if (!player) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	const auto messageType = static_cast<MessageClasses>(otx::lua::getNumber<uint8_t>(L, 2));
	const std::string message = otx::lua::getString(L, 3);

	if (lua_gettop(L) > 3) {
		const auto value = otx::lua::getNumber<uint16_t>(L, 4, 0);
		const auto color = static_cast<Color_t>(otx::lua::getNumber<uint16_t>(L, 5, COLOR_WHITE));
		Position pos = otx::lua::getPosition(L, 6);
		if (pos.x == 0 || pos.y == 0) {
			pos = player->getPosition();
		}

		MessageDetails* details = new MessageDetails(value, color);
		player->sendStatsMessage(messageType, message, pos, details);
		delete details;
	} else {
		player->sendTextMessage(messageType, message);
	}

	lua_pushboolean(L, true);
	return 1;
}

static int luaDoPlayerSendChannelMessage(lua_State* L)
{
	// doPlayerSendChannelMessage(cid, author, message, messageType, channelId)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const std::string author = otx::lua::getString(L, 2);
		const std::string message = otx::lua::getString(L, 3);
		const auto messageType = static_cast<MessageClasses>(otx::lua::getNumber<uint8_t>(L, 4));
		const auto channelId = otx::lua::getNumber<uint16_t>(L, 5);
		player->sendChannelMessage(author, message, messageType, channelId);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoPlayerOpenChannel(lua_State* L)
{
	// doPlayerOpenChannel(cid, channelId)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const auto channelId = otx::lua::getNumber<uint16_t>(L, 2);
		lua_pushboolean(L, g_game.playerOpenChannel(player->getID(), channelId));
		return 1;
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoPlayerOpenPrivateChannel(lua_State* L)
{
	// doPlayerOpenPrivateChannel(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		lua_pushboolean(L, g_game.playerCreatePrivateChannel(player->getID()));
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoPlayerSendChannels(lua_State* L)
{
	// doPlayerSendChannels(cid[, list])
	ChannelsList channels;
	bool customChannels = false;
	if (lua_gettop(L) > 1) {
		if (!otx::lua::isTable(L, 2)) {
			otx::lua::reportErrorEx(L, "Invalid channel list table.");
			lua_pushnil(L);
			return 1;
		}

		customChannels = true;

		lua_pushnil(L);
		while (lua_next(L, 2)) {
			channels.emplace_back(otx::lua::getNumber<uint16_t>(L, -2), otx::lua::getString(L, -1));
			lua_pop(L, 1);
		}
	}

	if (Player* player = otx::lua::getPlayer(L, 1)) {
		if (!customChannels) {
			channels = g_chat.getChannelList(player);
		}

		player->sendChannelsDialog(channels);
		player->setSentChat(!customChannels);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoSendCreatureSquare(lua_State* L)
{
	// doSendCreatureSquare(cid, color[, player])
	if (Creature* creature = otx::lua::getCreature(L, 1)) {
		const auto color = otx::lua::getNumber<uint8_t>(L, 2);

		if (Player* player = otx::lua::getPlayer(L, 3)) {
			player->sendCreatureSquare(creature, color);
		} else {
			g_game.addCreatureSquare(creature, color);
		}

		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoSendAnimatedText(lua_State* L)
{
	// doSendAnimatedText(pos, text, color[, player])
	const Position pos = otx::lua::getPosition(L, 1);
	const std::string text = otx::lua::getString(L, 2);
	const auto color = otx::lua::getNumber<uint8_t>(L, 3);

	if (Player* player = otx::lua::getPlayer(L, 4)) {
		player->sendAnimatedText(pos, color, text);
	} else {
		g_game.addAnimatedText(pos, color, text);
	}

	lua_pushboolean(L, true);
	return 1;
}

static int luaGetPlayerSkillLevel(lua_State* L)
{
	// getPlayerSkillLevel(cid, skill[, ignoreModifiers = false])
	if (const Player* player = otx::lua::getPlayer(L, 1)) {
		const auto skillId = static_cast<skills_t>(otx::lua::getNumber<uint8_t>(L, 2));
		const bool ignoreModifiers = otx::lua::getBoolean(L, 3, false);
		if (skillId <= SKILL_LAST) {
			lua_pushnumber(L, ignoreModifiers ? player->getBaseSkillLevel(skillId) : player->getSkillLevel(skillId));
		} else {
			otx::lua::reportErrorEx(L, "Invalid skill type.");
			lua_pushnil(L);
		}
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerSkillTries(lua_State* L)
{
	// getPlayerSkillTries(cid, skillId)
	if (const Player* player = otx::lua::getPlayer(L, 1)) {
		const auto skillId = static_cast<skills_t>(otx::lua::getNumber<uint8_t>(L, 2));
		if (skillId <= SKILL_LAST) {
			lua_pushnumber(L, player->getSkillTries(skillId));
		} else {
			otx::lua::reportErrorEx(L, "Invalid skill type.");
			lua_pushnil(L);
		}
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoPlayerSetOfflineTrainingSkill(lua_State* L)
{
	// doPlayerSetOfflineTrainingSkill(cid, skillId)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const auto skillId = otx::lua::getNumber<uint8_t>(L, 2);
		player->setOfflineTrainingSkill(skillId);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoCreatureSetDropLoot(lua_State* L)
{
	// doCreatureSetDropLoot(cid, doDrop)
	if (Creature* creature = otx::lua::getCreature(L, 1)) {
		const bool doDrop = otx::lua::getBoolean(L, 2);
		creature->setDropLoot(doDrop ? LOOT_DROP_FULL : LOOT_DROP_NONE);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerLossPercent(lua_State* L)
{
	// getPlayerLossPercent(cid, lossType)
	if (const Player* player = otx::lua::getPlayer(L, 1)) {
		const auto lossType = static_cast<lossTypes_t>(otx::lua::getNumber<uint8_t>(L, 2));
		if (lossType <= LOSS_LAST) {
			uint32_t value = player->getLossPercent(lossType);
			lua_pushnumber(L, value);
		} else {
			otx::lua::reportErrorEx(L, "Invalid loss type.");
			lua_pushnil(L);
		}
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoPlayerSetLossPercent(lua_State* L)
{
	// doPlayerSetLossPercent(cid, lossType, percent)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const auto lossType = static_cast<lossTypes_t>(otx::lua::getNumber<uint8_t>(L, 2));
		const auto percent = otx::lua::getNumber<uint32_t>(L, 3);
		if (lossType <= LOSS_LAST) {
			player->setLossPercent(lossType, percent);
			lua_pushboolean(L, true);
		} else {
			otx::lua::reportErrorEx(L, "Invalid loss type.");
			lua_pushnil(L);
		}
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoPlayerSetLossSkill(lua_State* L)
{
	// doPlayerSetLossSkill(cid, doLose)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const bool doLose = otx::lua::getBoolean(L, 2);
		player->setLossSkill(doLose);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoShowTextDialog(lua_State* L)
{
	// doShowTextDialog(cid, itemId[, (text/canWrite)[, (canWrite/length)[, length]]])
	// who did this? the parameters of this function is confuse af
	Player* player = otx::lua::getPlayer(L, 1);
	if (!player) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	const auto itemId = otx::lua::getNumber<uint16_t>(L, 2);

	std::string text;
	int32_t length = -1;
	bool canWrite = false;

	const int nparam = lua_gettop(L);
	if (nparam >= 5) {
		length = otx::lua::getNumber<int32_t>(L, 5);
	}
	if (nparam >= 4) {
		if (otx::lua::isBoolean(L, 4)) {
			canWrite = otx::lua::getBoolean(L, 4);
		} else {
			length = otx::lua::getNumber<int32_t>(L, 4);
		}
	}
	if (nparam >= 3) {
		if (otx::lua::isBoolean(L, 3)) {
			canWrite = otx::lua::getBoolean(L, 3);
		} else {
			text = otx::lua::getString(L, 3);
		}
	}

	Item* item = Item::CreateItem(itemId);
	if (length < 0) {
		length = item->getMaxWriteLength();
	}

	player->m_transferContainer.__addThing(nullptr, item);
	if (!text.empty()) {
		item->setText(text);
		length = std::max<int32_t>(text.size(), length);
	}

	player->setWriteItem(item, length);
	player->m_transferContainer.setParent(player);

	player->sendTextWindow(item, length, canWrite);
	lua_pushboolean(L, true);
	return 1;
}

static int luaDoDecayItem(lua_State* L)
{
	// doDecayItem(uid)
	// Note: to stop decay set decayTo = 0 in items.xml
	if (Item* item = otx::lua::getItem(L, 1)) {
		g_game.startDecay(item);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_ITEM_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetThingFromPosition(lua_State* L)
{
	// getThingFromPosition(pos)
	//  Note:
	//	stackpos = 255- top thing (movable item or creature)
	//	stackpos = 254- magic field
	//	stackpos = 253- top creature
	int32_t stackpos = 0;
	const Position pos = otx::lua::getPosition(L, 1, stackpos);

	if (Tile* tile = g_game.getMap()->getTile(pos)) {
		if (stackpos == -1) {
			lua_pushnumber(L, tile->getThingCount());
			return 1;
		}

		Thing* thing = nullptr;
		if (stackpos == 255) {
			thing = tile->getTopCreature();
			if (!thing) {
				Item* item = tile->getTopDownItem();
				if (item && item->isMovable()) {
					thing = item;
				}
			}
		} else if (stackpos == 254) {
			thing = tile->getFieldItem();
		} else if (stackpos == 253) {
			thing = tile->getTopCreature();
		} else if (stackpos == 0) {
			thing = tile->ground;
		} else {
			thing = tile->__getThing(stackpos);
		}

		otx::lua::pushThing(L, thing);
		return 1;
	}

	if (stackpos != -1) {
		otx::lua::pushThing(L, nullptr);
	} else {
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetTileItemById(lua_State* L)
{
	// getTileItemById(pos, itemId[, subType = -1])
	const Position pos = otx::lua::getPosition(L, 1);
	const auto itemId = otx::lua::getNumber<uint16_t>(L, 2);
	const auto subType = otx::lua::getNumber<int32_t>(L, 3, -1);

	if (Tile* tile = g_game.getTile(pos)) {
		otx::lua::pushThing(L, g_game.findItemOfType(tile, itemId, false, subType));
	} else {
		otx::lua::pushThing(L, nullptr);
	}
	return 1;
}

static int luaGetTileItemByType(lua_State* L)
{
	// getTileItemByType(pos, itemType)
	const Position pos = otx::lua::getPosition(L, 1);
	const auto itemType = static_cast<ItemTypes_t>(otx::lua::getNumber<uint8_t>(L, 2));

	if (itemType >= ITEM_TYPE_LAST) {
		otx::lua::reportErrorEx(L, "Invalid item type.");
		otx::lua::pushThing(L, nullptr);
		return 1;
	}

	Tile* tile = g_game.getTile(pos);
	if (!tile) {
		otx::lua::pushThing(L, nullptr);
		return 1;
	}

	bool found = false;
	switch (itemType) {
		case ITEM_TYPE_TELEPORT: {
			found = tile->hasFlag(TILESTATE_TELEPORT);
			break;
		}
		case ITEM_TYPE_MAGICFIELD: {
			found = tile->hasFlag(TILESTATE_MAGICFIELD);
			break;
		}
		case ITEM_TYPE_MAILBOX: {
			found = tile->hasFlag(TILESTATE_MAILBOX);
			break;
		}
		case ITEM_TYPE_TRASHHOLDER: {
			found = tile->hasFlag(TILESTATE_TRASHHOLDER);
			break;
		}
		case ITEM_TYPE_BED: {
			found = tile->hasFlag(TILESTATE_BED);
			break;
		}
		case ITEM_TYPE_DEPOT: {
			found = tile->hasFlag(TILESTATE_DEPOT);
			break;
		}

		default:
			break;
	}

	if (!found) {
		otx::lua::pushThing(L, nullptr);
		return 1;
	}

	if (tile->ground) {
		const ItemType& it = Item::items[tile->ground->getID()];
		if (it.type == itemType) {
			otx::lua::pushThing(L, tile->ground);
			return 1;
		}
	}

	if (TileItemVector* items = tile->getItemList()) {
		for (Item* item : *items) {
			if (Item::items[item->getID()].type == itemType) {
				otx::lua::pushThing(L, item);
				return 1;
			}
		}
	}

	otx::lua::pushThing(L, nullptr);
	return 1;
}

static int luaGetTileThingByPos(lua_State* L)
{
	// getTileThingByPos(pos)
	int32_t stackpos;
	const Position pos = otx::lua::getPosition(L, 1, stackpos);

	if (Tile* tile = g_game.getTile(pos.x, pos.y, pos.z)) {
		if (stackpos == -1) {
			lua_pushnumber(L, tile->getThingCount());
		} else {
			otx::lua::pushThing(L, tile->__getThing(stackpos));
		}
	} else {
		if (stackpos == -1) {
			lua_pushnumber(L, -1);
		} else {
			otx::lua::pushThing(L, nullptr, 0);
		}
	}
	return 1;
}

static int luaGetTopCreature(lua_State* L)
{
	// getTopCreature(pos)
	const Position pos = otx::lua::getPosition(L, 1);

	if (Tile* tile = g_game.getTile(pos)) {
		otx::lua::pushThing(L, tile->getTopCreature());
	} else {
		otx::lua::pushThing(L, nullptr);
	}
	return 1;
}

static int luaDoCreateItem(lua_State* L)
{
	// doCreateItem(itemId, count, pos)
	// Returns uid of the created item, only works on tiles.
	const auto itemId = otx::lua::getNumber<uint16_t>(L, 1);
	const auto count = otx::lua::getNumber<int32_t>(L, 2);
	const Position pos = otx::lua::getPosition(L, 3);

	const ItemType& it = Item::items[itemId];

	Tile* tile = g_game.getTile(pos);
	if (!tile) {
		if (it.group == ITEM_GROUP_GROUND) {
			Item* item = Item::CreateItem(it.id);
			tile = IOMap::createTile(item, nullptr, pos.x, pos.y, pos.z);
			g_game.setTile(tile);
			lua_pushnumber(L, otx::lua::getScriptEnv().addThing(item));
		} else {
			otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_TILE_NOT_FOUND));
			lua_pushnil(L);
		}
		return 1;
	}

	int32_t itemCount = 1, subType = 1;
	if (it.hasSubType()) {
		if (it.stackable) {
			itemCount = std::ceil(count / 100.0);
		}

		subType = count;
	} else {
		itemCount = std::max<int32_t>(1, count);
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();

	uint32_t ret = 0;
	while (itemCount-- > 0) {
		int32_t stackCount = std::min(100, subType);
		Item* newItem = Item::CreateItem(it.id, stackCount);
		if (!newItem) {
			otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_ITEM_NOT_FOUND));
			lua_pushnil(L);
			return ++ret;
		}

		if (it.stackable) {
			subType -= stackCount;
		}

		uint32_t dummy = 0;
		Item* stackItem = nullptr;
		if (g_game.internalAddItem(nullptr, tile, newItem, INDEX_WHEREEVER, FLAG_NOLIMIT, false, dummy, &stackItem) != RET_NOERROR) {
			delete newItem;
			lua_pushboolean(L, false);
			return ++ret;
		}

		if (newItem->getParent()) {
			lua_pushnumber(L, env.addThing(newItem));
		} else if (stackItem) {
			lua_pushnumber(L, env.addThing(stackItem));
		} else {
			lua_pushnil(L);
		}

		++ret;
	}

	if (ret != 0) {
		return ret;
	}

	lua_pushnil(L);
	return 1;
}

static int luaDoCreateItemEx(lua_State* L)
{
	// doCreateItemEx(itemId[, count/subType = 0])
	const auto itemId = otx::lua::getNumber<uint16_t>(L, 1);
	auto count = otx::lua::getNumber<uint32_t>(L, 2, 0);

	const ItemType& it = Item::items[itemId];
	if (it.stackable && count > 100) {
		count = 100;
	}

	Item* newItem = Item::CreateItem(it.id, count);
	if (!newItem) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_ITEM_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	newItem->setParent(VirtualCylinder::virtualCylinder);
	otx::lua::addTempItem(&env, newItem);

	lua_pushnumber(L, env.addThing(newItem));
	return 1;
}

static int luaDoCreateTeleport(lua_State* L)
{
	// doCreateTeleport(itemId, destination, position)
	const auto itemId = otx::lua::getNumber<uint16_t>(L, 1);
	const Position destination = otx::lua::getPosition(L, 2);
	const Position position = otx::lua::getPosition(L, 3);

	Tile* tile = g_game.getMap()->getTile(position);
	if (!tile) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_TILE_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	Item* newItem = Item::CreateItem(itemId);
	Teleport* newTeleport = newItem->getTeleport();
	if (!newTeleport) {
		delete newItem;
		otx::lua::reportErrorEx(L, "Item " + std::to_string(itemId) + " is not a teleport.");
		lua_pushnil(L);
		return 1;
	}

	uint32_t dummy = 0;
	Item* stackItem = nullptr;
	if (g_game.internalAddItem(nullptr, tile, newItem, INDEX_WHEREEVER, FLAG_NOLIMIT, false, dummy, &stackItem) != RET_NOERROR) {
		delete newItem;
		lua_pushboolean(L, false);
		return 1;
	}

	newTeleport->setDestination(destination);
	if (newItem->getParent()) {
		lua_pushnumber(L, otx::lua::getScriptEnv().addThing(newItem));
	} else if (stackItem) {
		lua_pushnumber(L, otx::lua::getScriptEnv().addThing(stackItem));
	} else {
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetCreatureStorageList(lua_State* L)
{
	// getCreatureStorageList(cid)
	if (Creature* creature = otx::lua::getCreature(L, 1)) {
		const auto& storages = creature->getStorages();
		lua_createtable(L, storages.size(), 0);

		int index = 0;
		for (const auto& it : storages) {
			otx::lua::pushString(L, it.first);
			lua_rawseti(L, -2, ++index);
		}
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetCreatureStorage(lua_State* L)
{
	// getCreatureStorage(cid, key)
	if (Creature* creature = otx::lua::getCreature(L, 1)) {
		const std::string key = otx::lua::getString(L, 2);

		std::string value;
		if (creature->getStorage(key, value)) {
			auto result = otx::util::safe_cast<int64_t>(value.data());
			if (result.second) {
				lua_pushnumber(L, result.first);
			} else {
				otx::lua::pushString(L, value);
			}
		} else {
			lua_pushnumber(L, -1);
		}
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoCreatureSetStorage(lua_State* L)
{
	// doCreatureSetStorage(cid, key[, value])
	if (Creature* creature = otx::lua::getCreature(L, 1)) {
		const std::string key = otx::lua::getString(L, 2);
		if (!otx::lua::isNoneOrNil(L, 3)) {
			const std::string value = otx::lua::getString(L, 3);
			lua_pushboolean(L, creature->setStorage(key, value));
		} else {
			creature->eraseStorage(key);
			lua_pushboolean(L, true);
		}
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerSpectators(lua_State* L)
{
	// getPlayerSpectators(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		Spectators* spectators = player->getSpectators();

		lua_createtable(L, 0, 7);
		otx::lua::setTableValue(L, "broadcast", spectators->isBroadcasting());
		otx::lua::setTableValue(L, "password", spectators->getPassword());
		otx::lua::setTableValue(L, "auth", spectators->isAuth());

		const auto& nameList = player->getSpectators()->list();
		const auto& muteList = player->getSpectators()->muteList();
		const auto& banList = player->getSpectators()->banList();

		int index = 0;
		lua_createtable(L, nameList.size(), 0);
		for (const std::string& name : nameList) {
			otx::lua::pushString(L, name);
			lua_rawseti(L, -2, ++index);
		}
		lua_setfield(L, -2, "names");

		index = 0;
		lua_createtable(L, muteList.size(), 0);
		for (const std::string& name : muteList) {
			otx::lua::pushString(L, name);
			lua_rawseti(L, -2, ++index);
		}
		lua_setfield(L, -2, "mutes");

		index = 0;
		lua_createtable(L, banList.size(), 0);
		for (const auto& it : banList) {
			otx::lua::pushString(L, it.first);
			lua_rawseti(L, -2, ++index);
		}
		lua_setfield(L, -2, "bans");

		lua_createtable(L, 0, 0);
		// TODO: create it
		lua_setfield(L, -2, "kick");
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoPlayerSetSpectators(lua_State* L)
{
	// doPlayerSetSpectators(cid, data)
	Player* player = otx::lua::getPlayer(L, 1);
	if (!player) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	if (!otx::lua::isTable(L, 2)) {
		otx::lua::reportErrorEx(L, "Invalid data table.");
		lua_pushnil(L);
		return 1;
	}

	const bool auth = otx::lua::getTableBool(L, 2, "auth");
	const bool broadcast = otx::lua::getTableBool(L, 2, "broadcast");
	const std::string password = otx::lua::getTableString(L, 2, "password");

	std::vector<std::string> mutes;
	lua_getfield(L, 2, "mutes");
	if (otx::lua::isTable(L, -1)) {
		lua_pushnil(L);
		while (lua_next(L, -2)) {
			mutes.push_back(otx::util::as_lower_string(otx::lua::getString(L, -1)));
			lua_pop(L, 1);
		}
	}
	lua_pop(L, 1);

	std::vector<std::string> bans;
	lua_getfield(L, 2, "bans");
	if (otx::lua::isTable(L, -1)) {
		lua_pushnil(L);
		while (lua_next(L, -2)) {
			bans.push_back(otx::util::as_lower_string(otx::lua::getString(L, -1)));
			lua_pop(L, 1);
		}
	}
	lua_pop(L, 1);

	std::vector<std::string> kicks;
	lua_getfield(L, 2, "kick");
	if (otx::lua::isTable(L, -1)) {
		lua_pushnil(L);
		while (lua_next(L, -2)) {
			kicks.push_back(otx::util::as_lower_string(otx::lua::getString(L, -1)));
			lua_pop(L, 1);
		}
	}
	lua_pop(L, 1);

	Spectators* spectators = player->getSpectators();
	if (spectators->getPassword() != password && !password.empty()) {
		spectators->clear(false);
	}

	spectators->setPassword(password);
	if (!broadcast && spectators->isBroadcasting()) {
		spectators->clear(false);
	}

	spectators->kick(kicks);
	spectators->mute(mutes);
	spectators->ban(bans);

	spectators->setBroadcast(broadcast);
	spectators->setAuth(auth);
	lua_pushboolean(L, true);
	return 1;
}

static int luaGetTileInfo(lua_State* L)
{
	// getTileInfo(pos)
	Position pos = otx::lua::getPosition(L, 1);
	if (Tile* tile = g_game.getMap()->getTile(pos)) {
		otx::lua::pushThing(L, tile->ground, 0, RECURSE_NONE, 0, 19);
		otx::lua::setTableValue(L, "protection", tile->hasFlag(TILESTATE_PROTECTIONZONE));
		otx::lua::setTableValue(L, "optional", tile->hasFlag(TILESTATE_OPTIONALZONE));
		otx::lua::setTableValue(L, "hardcore", tile->hasFlag(TILESTATE_HARDCOREZONE));
		otx::lua::setTableValue(L, "noLogout", tile->hasFlag(TILESTATE_NOLOGOUT));
		otx::lua::setTableValue(L, "refresh", tile->hasFlag(TILESTATE_REFRESH));
		otx::lua::setTableValue(L, "trashed", tile->hasFlag(TILESTATE_TRASHED));
		otx::lua::setTableValue(L, "magicField", tile->hasFlag(TILESTATE_MAGICFIELD));
		otx::lua::setTableValue(L, "trashHolder", tile->hasFlag(TILESTATE_TRASHHOLDER));
		otx::lua::setTableValue(L, "mailbox", tile->hasFlag(TILESTATE_MAILBOX));
		otx::lua::setTableValue(L, "depot", tile->hasFlag(TILESTATE_DEPOT));
		otx::lua::setTableValue(L, "bed", tile->hasFlag(TILESTATE_BED));
		otx::lua::setTableValue(L, "teleport", tile->hasFlag(TILESTATE_TELEPORT));
		otx::lua::setTableValue(L, "things", tile->getThingCount());
		otx::lua::setTableValue(L, "creatures", tile->getCreatureCount());
		otx::lua::setTableValue(L, "items", tile->getItemCount());
		otx::lua::setTableValue(L, "topItems", tile->getTopItemCount());
		otx::lua::setTableValue(L, "downItems", tile->getDownItemCount());
		if (House* house = tile->getHouse()) {
			otx::lua::setTableValue(L, "house", house->getId());
		}

		lua_createtable(L, CHANGE_LAST, 0);
		for (uint8_t i = CHANGE_FIRST; i <= CHANGE_LAST; ++i) {
			lua_pushboolean(L, tile->floorChange(static_cast<FloorChange_t>(i)));
			lua_rawseti(L, -2, i);
		}
		lua_setfield(L, -2, "floorChange");
	} else {
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetHouseFromPosition(lua_State* L)
{
	// getHouseFromPosition(pos)
	const Position pos = otx::lua::getPosition(L, 1);

	Tile* tile = g_game.getMap()->getTile(pos);
	if (!tile) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_TILE_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	HouseTile* houseTile = tile->getHouseTile();
	if (!houseTile) {
		lua_pushnil(L);
		return 1;
	}

	House* house = houseTile->getHouse();
	if (!house) {
		lua_pushnil(L);
		return 1;
	}

	lua_pushnumber(L, house->getId());
	return 1;
}

static int luaDoCreateMonster(lua_State* L)
{
	// doCreateMonster(name, pos[, extend = false, force = false])
	const std::string name = otx::lua::getString(L, 1);
	const Position pos = otx::lua::getPosition(L, 2);
	const bool extend = otx::lua::getBoolean(L, 3, false);
	const bool force = otx::lua::getBoolean(L, 4, false);

	Monster* monster = Monster::createMonster(name);
	if (!monster) {
		otx::lua::reportErrorEx(L, "Monster with name '" + name + "' not found");
		lua_pushnil(L);
		return 1;
	}

	if (!g_game.placeCreature(monster, pos, extend, force)) {
		delete monster;
		otx::lua::reportErrorEx(L, "Cannot create monster: " + name);
		lua_pushnil(L);
		return 1;
	}

	lua_pushnumber(L, otx::lua::getScriptEnv().addThing(monster));
	return 1;
}

static int luaDoCreateNpc(lua_State* L)
{
	// doCreateNpc(name, pos)
	const std::string name = otx::lua::getString(L, 1);
	const Position pos = otx::lua::getPosition(L, 2);

	Npc* npc = Npc::createNpc(name);
	if (!npc) {
		otx::lua::reportErrorEx(L, "Npc with name '" + name + "' not found");
		lua_pushnil(L);
		return 1;
	}

	if (!g_game.placeCreature(npc, pos)) {
		delete npc;
		otx::lua::reportErrorEx(L, "Cannot create npc: " + name);
		lua_pushboolean(L, true); // for scripting compatibility
		return 1;
	}

	lua_pushnumber(L, otx::lua::getScriptEnv().addThing(npc));
	return 1;
}

static int luaDoRemoveCreature(lua_State* L)
{
	// doRemoveCreature(cid[, forceLogout = true])
	if (Creature* creature = otx::lua::getCreature(L, 1)) {
		const bool forceLogout = otx::lua::getBoolean(L, 2, true);
		if (Player* player = creature->getPlayer()) {
			player->kick(true, forceLogout); // Players will get kicked without restrictions
		} else {
			g_game.removeCreature(creature); // Monsters/NPCs will get removed
		}
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoPlayerAddMoney(lua_State* L)
{
	// doPlayerAddMoney(cid, money)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const auto money = otx::lua::getNumber<int64_t>(L, 2);
		g_game.addMoney(player, money);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoPlayerRemoveMoney(lua_State* L)
{
	// doPlayerRemoveMoney(cid, money[, canDrop = true])
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const auto money = otx::lua::getNumber<int64_t>(L, 2);
		const bool canDrop = otx::lua::getBoolean(L, 3, true);
		lua_pushboolean(L, g_game.removeMoney(player, money, 0, canDrop));
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoPlayerTransferMoneyTo(lua_State* L)
{
	// doPlayerTransferMoneyTo(cid, target, money)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const std::string target = otx::lua::getString(L, 2);
		const auto money = otx::lua::getNumber<uint64_t>(L, 3);
		lua_pushboolean(L, player->transferMoneyTo(target, money));
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoPlayerSetPzLocked(lua_State* L)
{
	// doPlayerSetPzLocked(cid, locked)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const bool locked = otx::lua::getBoolean(L, 2);
		if (player->isPzLocked() != locked) {
			player->setPzLocked(locked);
			player->sendIcons();
		}
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoPlayerSetTown(lua_State* L)
{
	// doPlayerSetTown(cid, townId)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const auto townId = otx::lua::getNumber<uint32_t>(L, 2);
		if (Town* town = Towns::getInstance()->getTown(townId)) {
			player->setMasterPosition(town->getPosition());
			player->setTown(townId);
			lua_pushboolean(L, true);
		} else {
			lua_pushboolean(L, false);
		}
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoPlayerSetVocation(lua_State* L)
{
	// doPlayerSetVocation(cid, vocationId)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const auto vocationId = otx::lua::getNumber<uint32_t>(L, 2);
		player->setVocation(vocationId);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoPlayerSetSex(lua_State* L)
{
	// doPlayerSetSex(cid, sex)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const auto sex = otx::lua::getNumber<uint16_t>(L, 2);
		player->setSex(sex);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoPlayerAddSoul(lua_State* L)
{
	// doPlayerAddSoul(cid, amount)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const auto amount = otx::lua::getNumber<int32_t>(L, 2);
		player->changeSoul(amount);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoPlayerSetExtraAttackSpeed(lua_State* L)
{
	// doPlayerSetExtraAttackSpeed(cid, speed)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const auto speed = otx::lua::getNumber<uint32_t>(L, 2);
		player->setPlayerExtraAttackSpeed(speed);
		lua_pushnumber(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnumber(L, false);
	}
	return 1;
}

static int luaGetPlayerItemCount(lua_State* L)
{
	// getPlayerItemCount(cid, itemId[, subType = -1])
	if (const Player* player = otx::lua::getPlayer(L, 1)) {
		const auto itemId = otx::lua::getNumber<uint16_t>(L, 1);
		const auto subType = otx::lua::getNumber<int32_t>(L, 2, -1);
		lua_pushnumber(L, player->__getItemTypeCount(itemId, subType));
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetHouseInfo(lua_State* L)
{
	// getHouseInfo(houseId[, full = true])
	const auto houseId = otx::lua::getNumber<uint32_t>(L, 1);
	const bool full = otx::lua::getBoolean(L, 2, true);

	House* house = Houses::getInstance()->getHouse(houseId);
	if (!house) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_HOUSE_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	lua_createtable(L, 0, full ? 15 : 12);
	otx::lua::setTableValue(L, "id", house->getId());
	otx::lua::setTableValue(L, "name", house->getName());
	otx::lua::setTableValue(L, "owner", house->getOwner());
	otx::lua::setTableValue(L, "rent", house->getRent());
	otx::lua::setTableValue(L, "price", house->getPrice());
	otx::lua::setTableValue(L, "town", house->getTownId());
	otx::lua::setTableValue(L, "paidUntil", house->getPaidUntil());
	otx::lua::setTableValue(L, "warnings", house->getRentWarnings());
	otx::lua::setTableValue(L, "lastWarning", house->getLastWarning());
	otx::lua::setTableValue(L, "guildHall", house->isGuild());
	otx::lua::setTableValue(L, "size", house->getSize());

	otx::lua::pushPosition(L, house->getEntry());
	lua_setfield(L, -2, "entry");

	if (full) {
		const auto& doors = house->getHouseDoors();
		const auto& beds = house->getHouseBeds();
		const auto& tiles = house->getHouseTiles();

		int index = 0;
		lua_createtable(L, doors.size(), 0);
		for (Door* door : doors) {
			otx::lua::pushPosition(L, door->getPosition());
			lua_rawseti(L, -2, ++index);
		}
		lua_setfield(L, -2, "doors");

		index = 0;
		lua_createtable(L, beds.size(), 0);
		for (BedItem* bed : beds) {
			otx::lua::pushPosition(L, bed->getPosition());
			lua_rawseti(L, -2, ++index);
		}
		lua_setfield(L, -2, "beds");

		index = 0;
		lua_createtable(L, tiles.size(), 0);
		for (HouseTile* tile : tiles) {
			otx::lua::pushPosition(L, tile->getPosition());
			lua_rawseti(L, -2, ++index);
		}
		lua_setfield(L, -2, "tiles");
	}
	return 1;
}

static int luaGetHouseAccessList(lua_State* L)
{
	// getHouseAccessList(houseId, listId)
	const auto houseId = otx::lua::getNumber<uint32_t>(L, 1);
	const auto listId = otx::lua::getNumber<uint32_t>(L, 2);

	if (House* house = Houses::getInstance()->getHouse(houseId)) {
		std::string list;
		if (house->getAccessList(listId, list)) {
			otx::lua::pushString(L, list);
		} else {
			lua_pushnil(L);
		}
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_HOUSE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetHouseByPlayerGUID(lua_State* L)
{
	// getHouseByPlayerGUID(guid)
	const auto guid = otx::lua::getNumber<uint32_t>(L, 1);
	if (House* house = Houses::getInstance()->getHouseByPlayerId(guid)) {
		lua_pushnumber(L, house->getId());
	} else {
		lua_pushnil(L);
	}
	return 1;
}

static int luaSetHouseAccessList(lua_State* L)
{
	// setHouseAccessList(houseId, listId, listText)
	const auto houseId = otx::lua::getNumber<uint32_t>(L, 1);
	const auto listId = otx::lua::getNumber<uint32_t>(L, 2);
	const std::string listText = otx::lua::getString(L, 3);

	if (House* house = Houses::getInstance()->getHouse(houseId)) {
		house->setAccessList(listId, listText);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_HOUSE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaSetHouseOwner(lua_State* L)
{
	// setHouseOwner(houseId, owner[, clean = true])
	const auto houseId = otx::lua::getNumber<uint32_t>(L, 1);
	const auto ownerId = otx::lua::getNumber<uint32_t>(L, 2);
	const bool clean = otx::lua::getBoolean(L, 3, true);

	if (House* house = Houses::getInstance()->getHouse(houseId)) {
		lua_pushboolean(L, house->setOwnerEx(ownerId, clean));
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_HOUSE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetWorldType(lua_State* L)
{
	// getWorldType()
	lua_pushnumber(L, g_game.getWorldType());
	return 1;
}

static int luaSetWorldType(lua_State* L)
{
	// setWorldType(type)
	const auto type = static_cast<WorldType_t>(otx::lua::getNumber<uint8_t>(L, 1));
	if (type >= WORLDTYPE_FIRST && type <= WORLDTYPE_LAST) {
		g_game.setWorldType(type);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, "Invalid world type.");
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetWorldTime(lua_State* L)
{
	// getWorldTime()
	lua_pushnumber(L, g_game.getLightHour());
	return 1;
}

static int luaGetWorldLight(lua_State* L)
{
	// getWorldLight()
	LightInfo lightInfo;
	g_game.getWorldLightInfo(lightInfo);

	lua_pushnumber(L, lightInfo.level);
	lua_pushnumber(L, lightInfo.color);
	return 2;
}

static int luaGetWorldCreatures(lua_State* L)
{
	// getWorldCreatures(type)
	// 0 players, 1 monsters, 2 npcs, 3 all
	switch (otx::lua::getNumber<uint8_t>(L, 1)) {
		case 0:
			lua_pushnumber(L, g_game.getPlayersOnline());
			break;
		case 1:
			lua_pushnumber(L, g_game.getMonstersOnline());
			break;
		case 2:
			lua_pushnumber(L, g_game.getNpcsOnline());
			break;
		case 3:
			lua_pushnumber(L, g_game.getCreaturesOnline());
			break;

		default:
			otx::lua::reportErrorEx(L, "Invalid type.");
			lua_pushnil(L);
			break;
	}
	return 1;
}

static int luaGetWorldUpTime(lua_State* L)
{
	// getWorldUpTime()
	lua_pushnumber(L, g_game.getUptime());
	return 1;
}

static int luaGetPlayerLight(lua_State* L)
{
	// getPlayerLight(cid)
	if (const Player* player = otx::lua::getPlayer(L, 1)) {
		LightInfo lightInfo;
		player->getCreatureLight(lightInfo);

		lua_pushnumber(L, lightInfo.level);
		lua_pushnumber(L, lightInfo.color);
		return 2;
	}

	otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
	lua_pushnil(L);
	return 1;
}

static int luaGetPlayerSoul(lua_State* L)
{
	// getPlayerSoul(cid[, ignoreModifiers = false])
	if (const Player* player = otx::lua::getPlayer(L, 1)) {
		const bool ignoreModifiers = otx::lua::getBoolean(L, 2, false);
		lua_pushnumber(L, ignoreModifiers ? player->getBaseSoul() : player->getSoul());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoPlayerAddExperience(lua_State* L)
{
	// doPlayerAddExperience(cid, amount)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const auto amount = otx::lua::getNumber<int64_t>(L, 2);
		if (amount > 0) {
			player->addExperience(amount);
			lua_pushboolean(L, true);
		} else if (amount < 0) {
			player->removeExperience(std::abs(amount));
			lua_pushboolean(L, true);
		} else {
			lua_pushboolean(L, false);
		}
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerSlotItem(lua_State* L)
{
	// getPlayerSlotItem(cid, slot)
	if (const Player* player = otx::lua::getPlayer(L, 1)) {
		const auto slot = otx::lua::getNumber<uint32_t>(L, 2);
		otx::lua::pushThing(L, player->__getThing(slot));
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		otx::lua::pushThing(L, nullptr);
	}
	return 1;
}

static int luaGetPlayerWeapon(lua_State* L)
{
	// getPlayerWeapon(cid[, ignoreAmmo = false])
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const bool ignoreAmmo = otx::lua::getBoolean(L, 2, false);
		otx::lua::pushThing(L, player->getWeapon(ignoreAmmo));
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerItemById(lua_State* L)
{
	// getPlayerItemById(cid, deepSearch, itemId[, subType = -1])
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const bool deepSearch = otx::lua::getBoolean(L, 2);
		const auto itemId = otx::lua::getNumber<uint16_t>(L, 3);
		const auto subType = otx::lua::getNumber<int32_t>(L, 4, -1);
		otx::lua::pushThing(L, g_game.findItemOfType(player, itemId, deepSearch, subType));
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		otx::lua::pushThing(L, nullptr);
	}
	return 1;
}

static int luaGetThing(lua_State* L)
{
	// getThing(uid[, recursive = RECURSE_FIRST])
	const auto uid = otx::lua::getNumber<uint32_t>(L, 1);
	const auto recursive = static_cast<Recursive_t>(otx::lua::getNumber<int8_t>(L, 2, RECURSE_FIRST));
	otx::lua::pushThing(L, otx::lua::getScriptEnv().getThingByUID(uid), uid, recursive);
	return 1;
}

static int luaDoTileQueryAdd(lua_State* L)
{
	// doTileQueryAdd(uid, pos[, flags = 0, displayError = true])
	const Position pos = otx::lua::getPosition(L, 2);
	const auto flags = otx::lua::getNumber<uint32_t>(L, 3, 0);
	const bool displayError = otx::lua::getBoolean(L, 4, true);

	Thing* thing = otx::lua::getThing(L, 1);
	if (!thing) {
		if (displayError) {
			otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_THING_NOT_FOUND));
		}

		lua_pushnumber(L, RET_NOTPOSSIBLE);
		return 1;
	}

	Tile* tile = g_game.getTile(pos);
	if (!tile) {
		if (displayError) {
			otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_TILE_NOT_FOUND));
		}

		lua_pushnumber(L, RET_NOTPOSSIBLE);
		return 1;
	}

	lua_pushnumber(L, tile->__queryAdd(0, thing, 1, flags));
	return 1;
}

static int luaDoItemRaidUnref(lua_State* L)
{
	// doItemRaidUnref(uid)
	if (Item* item = otx::lua::getItem(L, 1)) {
		if (Raid* raid = item->getRaid()) {
			raid->unRef();
			item->setRaid(nullptr);
			lua_pushboolean(L, true);
		} else {
			lua_pushboolean(L, false);
		}
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_ITEM_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetThingPosition(lua_State* L)
{
	// getThingPosition(uid)
	if (Thing* thing = otx::lua::getThing(L, 1)) {
		int32_t stackpos = 0;
		if (Tile* tile = thing->getTile()) {
			stackpos = tile->__getIndexOfThing(thing);
		}

		otx::lua::pushPosition(L, thing->getPosition(), stackpos);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_THING_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaCreateCombatObject(lua_State* L)
{
	// createCombatObject()
	ScriptEnvironment& env = otx::lua::getScriptEnv();
	if (env.getScriptId() != EVENT_ID_LOADING) {
		otx::lua::reportErrorEx(L, "This function can only be used while loading the script.");
		lua_pushnil(L);
		return 1;
	}

	lua_pushnumber(L, env.addCombat(new Combat));
	return 1;
}

static int luaCreateCombatArea(lua_State* L)
{
	// createCombatArea(area[, extArea])
	ScriptEnvironment& env = otx::lua::getScriptEnv();
	if (env.getScriptId() != EVENT_ID_LOADING) {
		otx::lua::reportErrorEx(L, "This function can only be used while loading the script.");
		lua_pushnil(L);
		return 1;
	}

	auto area = std::make_unique<CombatArea>();

	uint32_t areaRows = 0;
	std::vector<uint8_t> areaVec;

	// has extra parameter with diagonal area information
	if (lua_gettop(L) > 1) {
		if (!parseCombatArea(L, 2, areaVec, areaRows)) {
			otx::lua::reportErrorEx(L, "Invalid area table.");
			lua_pushnil(L);
			return 1;
		}

		area->setupExtArea(areaVec, areaRows);
	}

	if (parseCombatArea(L, 1, areaVec, areaRows)) {
		area->setupArea(areaVec, areaRows);
		lua_pushnumber(L, env.addCombatArea(area.release()));
	} else {
		otx::lua::reportErrorEx(L, "Invalid area table.");
		lua_pushnil(L);
	}
	return 1;
}

static int luaCreateConditionObject(lua_State* L)
{
	// createConditionObject(type, [ticks = 0, buff = false, subId = 0, conditionId = CONDITIONID_COMBAT])
	const auto conditionType = static_cast<ConditionType_t>(otx::lua::getNumber<uint32_t>(L, 1));
	const auto ticks = otx::lua::getNumber<int32_t>(L, 2, 0);
	const bool buff = otx::lua::getBoolean(L, 3, false);
	const auto subId = otx::lua::getNumber<int32_t>(L, 4, 0);
	const auto conditionId = static_cast<ConditionId_t>(otx::lua::getNumber<uint8_t>(L, 5, CONDITIONID_COMBAT));

	if (Condition* condition = Condition::createCondition(conditionId, conditionType, ticks, 0, buff, subId)) {
		lua_pushnumber(L, otx::lua::getScriptEnv().addCondition(condition));
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CONDITION_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaSetCombatArea(lua_State* L)
{
	// setCombatArea(combat, area)
	if (otx::lua::getScriptEnv().getScriptId() != EVENT_ID_LOADING) {
		otx::lua::reportErrorEx(L, "This function can only be used while loading the script.");
		lua_pushnil(L);
		return 1;
	}

	const auto combatId = otx::lua::getNumber<uint32_t>(L, 1);
	const auto areaId = otx::lua::getNumber<uint32_t>(L, 2);

	Combat* combat = g_lua.getCombatById(combatId);
	if (!combat) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_COMBAT_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	if (const CombatArea* area = g_lua.getCombatAreaById(areaId)) {
		combat->setArea(std::make_unique<CombatArea>(*area));
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_AREA_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaSetCombatCondition(lua_State* L)
{
	// setCombatCondition(combat, condition)
	if (otx::lua::getScriptEnv().getScriptId() != EVENT_ID_LOADING) {
		otx::lua::reportErrorEx(L, "This function can only be used while loading the script.");
		lua_pushnil(L);
		return 1;
	}

	const auto combatId = otx::lua::getNumber<uint32_t>(L, 1);
	const auto conditionObjId = otx::lua::getNumber<uint32_t>(L, 2);

	Combat* combat = g_lua.getCombatById(combatId);
	if (!combat) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_COMBAT_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	if (const Condition* condition = g_lua.getConditionById(conditionObjId)) {
		combat->setCondition(condition->clone());
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CONDITION_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaSetCombatParam(lua_State* L)
{
	// setCombatParam(combat, key, value)
	if (otx::lua::getScriptEnv().getScriptId() != EVENT_ID_LOADING) {
		otx::lua::reportErrorEx(L, "This function can only be used while loading the script");
		lua_pushnil(L);
		return 1;
	}

	const auto combatId = otx::lua::getNumber<uint32_t>(L, 1);
	const auto key = static_cast<CombatParam_t>(otx::lua::getNumber<uint8_t>(L, 2));
	const auto value = otx::lua::getNumber<uint32_t>(L, 3);

	if (Combat* combat = g_lua.getCombatById(combatId)) {
		combat->setParam(key, value);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_COMBAT_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaSetConditionParam(lua_State* L)
{
	// setConditionParam(condition, key, value)
	const auto conditionObjId = otx::lua::getNumber<uint32_t>(L, 1);
	const auto key = static_cast<ConditionParam_t>(otx::lua::getNumber<uint8_t>(L, 2));
	const auto value = otx::lua::getNumber<int32_t>(L, 3);

	if (Condition* condition = g_lua.getConditionById(conditionObjId)) {
		condition->setParam(key, value);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CONDITION_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaAddDamageCondition(lua_State* L)
{
	// addDamageCondition(condition, rounds, time, value)
	const auto conditionObjId = otx::lua::getNumber<uint32_t>(L, 1);
	const auto rounds = otx::lua::getNumber<int32_t>(L, 2);
	const auto time = otx::lua::getNumber<int32_t>(L, 3);
	const auto value = otx::lua::getNumber<int32_t>(L, 4);

	if (ConditionDamage* condition = dynamic_cast<ConditionDamage*>(g_lua.getConditionById(conditionObjId))) {
		condition->addDamage(rounds, time, value);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CONDITION_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaAddOutfitCondition(lua_State* L)
{
	// addOutfitCondition(condition, outfit)
	const auto conditionObjId = otx::lua::getNumber<uint32_t>(L, 1);
	const Outfit_t outfit = otx::lua::getOutfit(L, 2);

	if (ConditionOutfit* condition = dynamic_cast<ConditionOutfit*>(g_lua.getConditionById(conditionObjId))) {
		condition->addOutfit(outfit);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CONDITION_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaSetCombatCallback(lua_State* L)
{
	// setCombatCallback(combat, key, functionName)
	ScriptEnvironment& env = otx::lua::getScriptEnv();
	if (env.getScriptId() != EVENT_ID_LOADING) {
		otx::lua::reportErrorEx(L, "This function can only be used while loading the script.");
		lua_pushnil(L);
		return 1;
	}

	const auto combatId = otx::lua::getNumber<uint32_t>(L, 1);
	const auto key = static_cast<CallBackParam_t>(otx::lua::getNumber<uint32_t>(L, 2));
	const std::string fname = otx::lua::getString(L, 3);

	Combat* combat = g_lua.getCombatById(combatId);
	if (!combat) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_COMBAT_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	LuaInterface* interface = env.getInterface();
	combat->setCallback(key);

	CallBack* callback = combat->getCallback(key);
	if (!callback) {
		otx::lua::reportErrorEx(L, std::to_string(key) + " is not a valid callback key.");
		lua_pushnil(L);
		return 1;
	}

	if (!callback->loadCallBack(interface, fname)) {
		otx::lua::reportErrorEx(L, "Cannot load callback.");
		lua_pushnil(L);
	} else {
		lua_pushboolean(L, true);
	}
	return 1;
}

static int luaSetCombatFormula(lua_State* L)
{
	// setCombatFormula(combat, formulaType, mina, minb, maxa, maxb[,
	//                  minl = *, maxl = maxl, minm = *, maxm = minm, minc = 0, maxc = 0])
	if (otx::lua::getScriptEnv().getScriptId() != EVENT_ID_LOADING) {
		otx::lua::reportErrorEx(L, "This function can only be used while loading the script.");
		lua_pushnil(L);
		return 1;
	}

	const auto combatId = otx::lua::getNumber<uint32_t>(L, 1);
	const auto formulaType = static_cast<FormulaType_t>(otx::lua::getNumber<uint8_t>(L, 2));
	const auto mina = otx::lua::getNumber<double>(L, 3);
	const auto minb = otx::lua::getNumber<double>(L, 4);
	const auto maxa = otx::lua::getNumber<double>(L, 5);
	const auto maxb = otx::lua::getNumber<double>(L, 6);
	const auto minl = otx::lua::getNumber<double>(L, 7, otx::config::getDouble(otx::config::FORMULA_LEVEL));
	const auto maxl = otx::lua::getNumber<double>(L, 8, minl);
	const auto minm = otx::lua::getNumber<double>(L, 9, otx::config::getDouble(otx::config::FORMULA_MAGIC));
	const auto maxm = otx::lua::getNumber<double>(L, 10, minm);
	const auto minc = otx::lua::getNumber<int32_t>(L, 11, 0);
	const auto maxc = otx::lua::getNumber<int32_t>(L, 12, 0);

	if (Combat* combat = g_lua.getCombatById(combatId)) {
		combat->setPlayerCombatValues(formulaType, mina, minb, maxa, maxb, minl, maxl, minm, maxm, minc, maxc);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_COMBAT_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaSetConditionFormula(lua_State* L)
{
	// setConditionFormula(condition, mina, minb, maxa, maxb)
	const auto conditionObjId = otx::lua::getNumber<uint32_t>(L, 1);
	const auto mina = otx::lua::getNumber<double>(L, 2);
	const auto minb = otx::lua::getNumber<double>(L, 3);
	const auto maxa = otx::lua::getNumber<double>(L, 4);
	const auto maxb = otx::lua::getNumber<double>(L, 5);

	if (ConditionSpeed* condition = dynamic_cast<ConditionSpeed*>(g_lua.getConditionById(conditionObjId))) {
		condition->setFormulaVars(mina, minb, maxa, maxb);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CONDITION_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoCombat(lua_State* L)
{
	// doCombat(cid, combat, var)
	const auto creatureId = otx::lua::getNumber<uint32_t>(L, 1);
	const auto combatId = otx::lua::getNumber<uint32_t>(L, 2);
	const LuaVariant var = otx::lua::getVariant(L, 3);

	Creature* creature = nullptr;
	if (creatureId != 0) {
		creature = g_game.getCreatureByID(creatureId);
		if (!creature) {
			otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
			lua_pushnil(L);
			return 1;
		}
	}

	const Combat* combat = g_lua.getCombatById(combatId);
	if (!combat) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_COMBAT_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	if (var.type == VARIANT_NONE) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_VARIANT_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	switch (var.type) {
		case VARIANT_NUMBER: {
			Creature* target = g_game.getCreatureByID(var.number);
			if (!target || !creature || !creature->canSeeCreature(target)) {
				lua_pushboolean(L, false);
				return 1;
			}

			if (combat->hasArea()) {
				combat->doCombat(creature, target->getPosition());
			} else {
				combat->doCombat(creature, target);
			}
			break;
		}

		case VARIANT_POSITION: {
			combat->doCombat(creature, var.pos);
			break;
		}

		case VARIANT_TARGETPOSITION: {
			if (!combat->hasArea()) {
				combat->postCombatEffects(creature, var.pos);
				g_game.addMagicEffect(var.pos, MAGIC_EFFECT_POFF);
			} else {
				combat->doCombat(creature, var.pos);
			}
			break;
		}

		case VARIANT_STRING: {
			Player* target = g_game.getPlayerByName(var.text);
			if (!target || !creature || !creature->canSeeCreature(target)) {
				lua_pushboolean(L, false);
				return 1;
			}

			combat->doCombat(creature, target);
			break;
		}

		default: {
			otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_VARIANT_UNKNOWN));
			lua_pushnil(L);
			return 1;
		}
	}

	lua_pushboolean(L, true);
	return 1;
}

static int luaDoCombatAreaHealth(lua_State* L)
{
	// doCombatAreaHealth(cid, combatType, pos, area, min, max, effectId[, aggressive = true])
	const auto creatureId = otx::lua::getNumber<uint32_t>(L, 1);
	const auto combatType = static_cast<CombatType_t>(otx::lua::getNumber<uint32_t>(L, 2));
	const Position pos = otx::lua::getPosition(L, 3);
	const auto areaId = otx::lua::getNumber<uint32_t>(L, 4);
	const auto min = otx::lua::getNumber<int32_t>(L, 5);
	const auto max = otx::lua::getNumber<int32_t>(L, 6);
	const auto effectId = static_cast<MagicEffect_t>(otx::lua::getNumber<uint16_t>(L, 7));
	const bool aggressive = otx::lua::getBoolean(L, 8, true);

	Creature* creature = nullptr;
	if (creatureId != 0) {
		creature = g_game.getCreatureByID(creatureId);
		if (!creature) {
			otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
			lua_pushnil(L);
			return 1;
		}
	}

	const CombatArea* area = g_lua.getCombatAreaById(areaId);
	if (area || areaId == 0) {
		CombatDamage damage;
		damage.primary.type = combatType;
		damage.primary.value = random_range(min, max);

		CombatParams params;
		params.combatType = combatType;
		params.impactEffect = effectId;
		params.isAggressive = aggressive;

		Combat::doAreaCombat(creature, pos, area, damage, params);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_AREA_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoTargetCombatHealth(lua_State* L)
{
	// doTargetCombatHealth(cid, target, combatType, min, max, effectId[, aggressive = true])
	const auto creatureId = otx::lua::getNumber<uint32_t>(L, 1);
	const auto combatType = static_cast<CombatType_t>(otx::lua::getNumber<uint32_t>(L, 3));
	const auto min = otx::lua::getNumber<int32_t>(L, 4);
	const auto max = otx::lua::getNumber<int32_t>(L, 5);
	const auto effectId = static_cast<MagicEffect_t>(otx::lua::getNumber<uint16_t>(L, 6));
	const bool aggressive = otx::lua::getBoolean(L, 7, true);

	Creature* creature = nullptr;
	if (creatureId != 0) {
		creature = g_game.getCreatureByID(creatureId);
		if (!creature) {
			otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
			lua_pushnil(L);
			return 1;
		}
	}

	if (Creature* target = otx::lua::getCreature(L, 2)) {
		CombatDamage damage;
		damage.primary.type = combatType;
		damage.primary.value = random_range(min, max);

		CombatParams params;
		params.combatType = combatType;
		params.impactEffect = effectId;
		params.isAggressive = aggressive;

		Combat::doTargetCombat(creature, target, damage, params);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoCombatAreaMana(lua_State* L)
{
	// doCombatAreaMana(cid, pos, area, min, max, effectId[, aggressive = true])
	const auto creatureId = otx::lua::getNumber<uint32_t>(L, 1);
	const Position pos = otx::lua::getPosition(L, 2);
	const auto areaId = otx::lua::getNumber<uint32_t>(L, 3);
	const auto min = otx::lua::getNumber<int32_t>(L, 4);
	const auto max = otx::lua::getNumber<int32_t>(L, 5);
	const auto effectId = static_cast<MagicEffect_t>(otx::lua::getNumber<uint16_t>(L, 6));
	const bool aggressive = otx::lua::getBoolean(L, 7, true);

	Creature* creature = nullptr;
	if (creatureId != 0) {
		creature = g_game.getCreatureByID(creatureId);
		if (!creature) {
			otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
			lua_pushnil(L);
			return 1;
		}
	}

	const CombatArea* area = g_lua.getCombatAreaById(areaId);
	if (area || areaId == 0) {
		CombatDamage damage;
		damage.primary.type = COMBAT_MANADRAIN;
		damage.primary.value = random_range(min, max);

		CombatParams params;
		params.impactEffect = effectId;
		params.isAggressive = aggressive;

		Combat::doAreaCombat(creature, pos, area, damage, params);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_AREA_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoTargetCombatMana(lua_State* L)
{
	// doTargetCombatMana(cid, target, min, max, effectId[, aggressive = true])
	const auto creatureId = otx::lua::getNumber<uint32_t>(L, 1);
	const auto min = otx::lua::getNumber<int32_t>(L, 3);
	const auto max = otx::lua::getNumber<int32_t>(L, 4);
	const auto effectId = static_cast<MagicEffect_t>(otx::lua::getNumber<uint16_t>(L, 5));
	const bool aggressive = otx::lua::getBoolean(L, 6, true);

	Creature* creature = nullptr;
	if (creatureId != 0) {
		creature = g_game.getCreatureByID(creatureId);
		if (!creature) {
			otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
			lua_pushnil(L);
			return 1;
		}
	}

	if (Creature* target = otx::lua::getCreature(L, 2)) {
		CombatDamage damage;
		damage.primary.type = COMBAT_MANADRAIN;
		damage.primary.value = random_range(min, max);
		
		CombatParams params;
		params.impactEffect = effectId;
		params.isAggressive = aggressive;

		Combat::doTargetCombat(creature, target, damage, params);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoCombatAreaCondition(lua_State* L)
{
	// doCombatAreaCondition(cid, pos, areaId, conditionObjId, effectId)
	const auto creatureId = otx::lua::getNumber<uint32_t>(L, 1);
	const Position pos = otx::lua::getPosition(L, 2);
	const auto areaId = otx::lua::getNumber<uint32_t>(L, 3);
	const auto conditionObjId = otx::lua::getNumber<uint32_t>(L, 4);
	const auto effectId = static_cast<MagicEffect_t>(otx::lua::getNumber<uint16_t>(L, 5));

	Creature* creature = nullptr;
	if (creatureId != 0) {
		creature = g_game.getCreatureByID(creatureId);
		if (!creature) {
			otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
			lua_pushnil(L);
			return 1;
		}
	}

	if (const Condition* condition = g_lua.getConditionById(conditionObjId)) {
		const CombatArea* area = g_lua.getCombatAreaById(areaId);
		if (area || areaId == 0) {
			CombatParams params;
			params.impactEffect = effectId;
			params.conditionList.push_back(condition);

			Combat::doAreaCombat(creature, pos, area, params);
			lua_pushboolean(L, true);
		} else {
			otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_AREA_NOT_FOUND));
			lua_pushnil(L);
		}
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CONDITION_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoTargetCombatCondition(lua_State* L)
{
	// doTargetCombatCondition(cid, target, conditionObjId, effectId)
	const auto creatureId = otx::lua::getNumber<uint32_t>(L, 1);
	const auto conditionObjId = otx::lua::getNumber<uint32_t>(L, 3);
	const auto effectId = static_cast<MagicEffect_t>(otx::lua::getNumber<uint16_t>(L, 4));

	Creature* creature = nullptr;
	if (creatureId != 0) {
		creature = g_game.getCreatureByID(creatureId);
		if (!creature) {
			otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
			lua_pushnil(L);
			return 1;
		}
	}

	if (Creature* target = otx::lua::getCreature(L, 2)) {
		if (const Condition* condition = g_lua.getConditionById(conditionObjId)) {
			CombatParams params;
			params.impactEffect = effectId;
			params.conditionList.push_back(condition);

			Combat::doTargetCombat(creature, target, params);
			lua_pushboolean(L, true);
		} else {
			otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CONDITION_NOT_FOUND));
			lua_pushnil(L);
		}
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoCombatAreaDispel(lua_State* L)
{
	// doCombatAreaDispel(cid, pos, area, dispelType, effectId)
	const auto creatureId = otx::lua::getNumber<uint32_t>(L, 1);
	const Position position = otx::lua::getPosition(L, 2);
	const auto areaId = otx::lua::getNumber<uint32_t>(L, 3);
	const auto dispelType = static_cast<ConditionType_t>(otx::lua::getNumber<uint32_t>(L, 4));
	const auto effectId = static_cast<MagicEffect_t>(otx::lua::getNumber<uint16_t>(L, 5));

	Creature* creature = nullptr;
	if (creatureId != 0) {
		creature = g_game.getCreatureByID(creatureId);
		if (!creature) {
			otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
			lua_pushnil(L);
			return 1;
		}
	}

	const CombatArea* area = g_lua.getCombatAreaById(areaId);
	if (area || areaId == 0) {
		CombatParams params;
		params.dispelType = dispelType;
		params.impactEffect = effectId;

		Combat::doAreaCombat(creature, position, area, params);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_AREA_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoTargetCombatDispel(lua_State* L)
{
	// doTargetCombatDispel(cid, target, dispelType, effectId)
	const auto creatureId = otx::lua::getNumber<uint32_t>(L, 1);
	Creature* creature = nullptr;
	if (creatureId != 0) {
		creature = g_game.getCreatureByID(creatureId);
		if (!creature) {
			otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
			lua_pushnil(L);
			return 1;
		}
	}

	if (Creature* target = otx::lua::getCreature(L, 2)) {
		const auto dispelType = static_cast<ConditionType_t>(otx::lua::getNumber<uint32_t>(L, 3));
		const auto effectId = static_cast<MagicEffect_t>(otx::lua::getNumber<uint16_t>(L, 4));

		CombatParams params;
		params.dispelType = dispelType;
		params.impactEffect = effectId;

		Combat::doTargetCombat(creature, target, params);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoChallengeCreature(lua_State* L)
{
	// doChallengeCreature(cid, target)
	Creature* creature = otx::lua::getCreature(L, 1);
	Creature* target = otx::lua::getCreature(L, 2);
	if (creature && target) {
		lua_pushboolean(L, target->challengeCreature(creature));
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoSummonMonster(lua_State* L)
{
	// doSummonMonster(cid, name)
	if (Creature* creature = otx::lua::getCreature(L, 1)) {
		const std::string name = otx::lua::getString(L, 2);
		lua_pushnumber(L, g_game.placeSummon(creature, name));
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoConvinceCreature(lua_State* L)
{
	// doConvinceCreature(cid, target)
	Creature* creature = otx::lua::getCreature(L, 1);
	Creature* target = otx::lua::getCreature(L, 2);
	if (creature && target) {
		lua_pushboolean(L, target->convinceCreature(creature));
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetMonsterTargetList(lua_State* L)
{
	// getMonsterTargetList(cid)
	Monster* monster = otx::lua::getMonster(L, 1);
	if (!monster) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_MONSTER_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	const auto& targetList = monster->getTargetList();
	lua_createtable(L, targetList.size(), 0);

	int index = 0;
	for (Creature* target : targetList) {
		if (monster->isTarget(target)) {
			lua_pushnumber(L, env.addThing(target));
			lua_rawseti(L, -2, ++index);
		}
	}
	return 1;
}

static int luaGetMonsterFriendList(lua_State* L)
{
	// getMonsterFriendList(cid)
	Monster* monster = otx::lua::getMonster(L, 1);
	if (!monster) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_MONSTER_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	const auto& friendList = monster->getFriendList();
	lua_createtable(L, friendList.size(), 0);

	int index = 0;
	for (const auto& it : friendList) {
		if (!it.second->isRemoved() && it.second->getPosition().z == monster->getPosition().z) {
			lua_pushnumber(L, env.addThing(it.second));
			lua_rawseti(L, -2, ++index);
		}
	}
	return 1;
}

static int luaDoMonsterSetTarget(lua_State* L)
{
	// doMonsterSetTarget(cid, target)
	Monster* monster = otx::lua::getMonster(L, 1);
	if (!monster) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_MONSTER_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	Creature* target = otx::lua::getCreature(L, 2);
	if (!target) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	if (!monster->isSummon()) {
		lua_pushboolean(L, monster->selectTarget(target));
	} else {
		lua_pushboolean(L, false);
	}
	return 1;
}

static int luaDoMonsterChangeTarget(lua_State* L)
{
	// doMonsterChangeTarget(cid)
	if (Monster* monster = otx::lua::getMonster(L, 1)) {
		if (!monster->isSummon()) {
			monster->searchTarget(TARGETSEARCH_RANDOM);
		}
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_MONSTER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetMonsterInfo(lua_State* L)
{
	// getMonsterInfo(name)
	const MonsterType* mType = g_monsters.getMonsterType(otx::lua::getString(L, 1));
	if (!mType) {
		lua_pushnil(L);
		return 1;
	}

	lua_createtable(L, 0, 28);
	otx::lua::setTableValue(L, "name", mType->name);
	otx::lua::setTableValue(L, "description", mType->nameDescription);
	otx::lua::setTableValue(L, "file", mType->file);
	otx::lua::setTableValue(L, "experience", mType->experience);
	otx::lua::setTableValue(L, "health", mType->health);
	otx::lua::setTableValue(L, "healthMax", mType->healthMax);
	otx::lua::setTableValue(L, "manaCost", mType->manaCost);
	otx::lua::setTableValue(L, "defense", mType->defense);
	otx::lua::setTableValue(L, "armor", mType->armor);
	otx::lua::setTableValue(L, "baseSpeed", mType->baseSpeed);
	otx::lua::setTableValue(L, "lookCorpse", mType->lookCorpse);
	otx::lua::setTableValue(L, "corpseUnique", mType->corpseUnique);
	otx::lua::setTableValue(L, "corpseAction", mType->corpseAction);
	otx::lua::setTableValue(L, "race", mType->race);
	otx::lua::setTableValue(L, "skull", mType->skull);
	otx::lua::setTableValue(L, "partyShield", mType->partyShield);
	otx::lua::setTableValue(L, "guildEmblem", mType->guildEmblem);
	otx::lua::setTableValue(L, "summonable", mType->isSummonable);
	otx::lua::setTableValue(L, "illusionable", mType->isIllusionable);
	otx::lua::setTableValue(L, "convinceable", mType->isConvinceable);
	otx::lua::setTableValue(L, "attackable", mType->isAttackable);
	otx::lua::setTableValue(L, "hostile", mType->isHostile);
	otx::lua::setTableValue(L, "passive", mType->isPassive);

	otx::lua::pushOutfit(L, mType->outfit);
	lua_setfield(L, -2, "outfit");

	int index = 0;
	lua_createtable(L, mType->spellDefenseList.size(), 0);
	for (const spellBlock_t& sb : mType->spellDefenseList) {
		lua_createtable(L, 0, 6);
		otx::lua::setTableValue(L, "speed", sb.speed);
		otx::lua::setTableValue(L, "chance", sb.chance);
		otx::lua::setTableValue(L, "range", sb.range);
		otx::lua::setTableValue(L, "minCombatValue", sb.minCombatValue);
		otx::lua::setTableValue(L, "maxCombatValue", sb.maxCombatValue);
		otx::lua::setTableValue(L, "isMelee", sb.isMelee);
		lua_rawseti(L, -2, ++index);
	}
	lua_setfield(L, -2, "defenses");

	index = 0;
	lua_createtable(L, mType->spellAttackList.size(), 0);
	for (const spellBlock_t& sb : mType->spellAttackList) {
		lua_createtable(L, 0, 6);
		otx::lua::setTableValue(L, "speed", sb.speed);
		otx::lua::setTableValue(L, "chance", sb.chance);
		otx::lua::setTableValue(L, "range", sb.range);
		otx::lua::setTableValue(L, "minCombatValue", sb.minCombatValue);
		otx::lua::setTableValue(L, "maxCombatValue", sb.maxCombatValue);
		otx::lua::setTableValue(L, "isMelee", sb.isMelee);
		lua_rawseti(L, -2, ++index);
	}
	lua_setfield(L, -2, "attacks");

	index = 0;
	lua_createtable(L, mType->lootItems.size(), 0);
	for (const LootBlock& lb : mType->lootItems) {
		if (lb.ids.empty()) {
			continue;
		}

		lua_createtable(L, 0, 8);

		if (lb.ids.size() > 1) {
			int index2 = 0;
			lua_createtable(L, lb.ids.size(), 0);
			for (uint16_t itemId : lb.ids) {
				lua_pushnumber(L, itemId);
				lua_rawseti(L, -2, ++index2);
			}
			lua_setfield(L, -2, "ids");
		} else {
			otx::lua::setTableValue(L, "id", lb.ids[0]);
		}

		otx::lua::setTableValue(L, "count", lb.count);
		otx::lua::setTableValue(L, "chance", lb.chance);
		otx::lua::setTableValue(L, "subType", lb.subType);
		otx::lua::setTableValue(L, "actionId", lb.actionId);
		otx::lua::setTableValue(L, "uniqueId", lb.uniqueId);
		otx::lua::setTableValue(L, "text", lb.text);

		if (!lb.childLoot.empty()) {
			int index2 = 0;
			lua_createtable(L, lb.childLoot.size(), 0);
			for (const LootBlock& clb : lb.childLoot) {
				if (clb.ids.empty()) {
					continue;
				}

				lua_createtable(L, 0, 7);
				if (clb.ids.size() > 1) {
					lua_createtable(L, clb.ids.size(), 0);
					int index3 = 0;
					for (uint16_t itemId : clb.ids) {
						lua_pushnumber(L, itemId);
						lua_rawseti(L, -2, ++index3);
					}
					lua_setfield(L, -2, "ids");
				} else {
					otx::lua::setTableValue(L, "id", clb.ids[0]);
				}

				otx::lua::setTableValue(L, "count", clb.count);
				otx::lua::setTableValue(L, "chance", clb.chance);
				otx::lua::setTableValue(L, "subType", clb.subType);
				otx::lua::setTableValue(L, "actionId", clb.actionId);
				otx::lua::setTableValue(L, "uniqueId", clb.uniqueId);
				otx::lua::setTableValue(L, "text", clb.text);

				lua_rawseti(L, -2, ++index2);
			}
			lua_setfield(L, -2, "child");
		}

		lua_rawseti(L, -2, ++index);
	}
	lua_setfield(L, -2, "loot");
	
	index = 0;
	lua_createtable(L, mType->summonList.size(), 0);
	for (const summonBlock_t& sb : mType->summonList) {
		lua_createtable(L, 0, 4);
		otx::lua::setTableValue(L, "name", sb.name);
		otx::lua::setTableValue(L, "chance", sb.chance);
		otx::lua::setTableValue(L, "interval", sb.interval);
		otx::lua::setTableValue(L, "amount", sb.amount);
		lua_rawseti(L, -2, ++index);
	}
	lua_setfield(L, -2, "summons");
	return 1;
}

static int luaGetTalkActionList(lua_State* L)
{
	// getTalkactionList()
	const auto& talkactions = g_talkActions.getTalkActions();
	lua_createtable(L, talkactions.size(), 0);

	int index = 0;
	for (const auto& it : talkactions) {
		lua_createtable(L, 0, 9);
		otx::lua::setTableValue(L, "words", it.first);
		otx::lua::setTableValue(L, "access", it.second.getAccess());
		otx::lua::setTableValue(L, "log", it.second.isLogged());
		otx::lua::setTableValue(L, "logged", it.second.isLogged());
		otx::lua::setTableValue(L, "hide", it.second.isHidden());
		otx::lua::setTableValue(L, "hidden", it.second.isHidden());
		otx::lua::setTableValue(L, "functionName", it.second.getFunctionName());
		otx::lua::setTableValue(L, "channel", it.second.getChannel());

		lua_createtable(L, it.second.getGroups().size(), 0);
		int index2 = 0;
		for (int32_t groupId : it.second.getGroups()) {
			lua_pushnumber(L, groupId);
			lua_rawseti(L, -2, ++index2);
		}
		lua_setfield(L, -2, "groups");

		lua_rawseti(L, -2, ++index);
	}
	return 1;
}

static int luaGetExperienceStageList(lua_State* L)
{
	// getExperienceStageList()
	if (!otx::config::getBoolean(otx::config::EXPERIENCE_STAGES)) {
		lua_pushnil(L);
		return 1;
	}

	const auto& stages = g_game.getExperienceStages();
	lua_createtable(L, stages.size(), 0);

	int index = 0;
	for (const auto& [level, multiplier] : stages) {
		lua_createtable(L, 0, 2);
		otx::lua::setTableValue(L, "level", level);
		otx::lua::setTableValue(L, "multiplier", multiplier);
		lua_rawseti(L, -2, ++index);
	}
	return 1;
}

static int luaDoAddCondition(lua_State* L)
{
	// doAddCondition(cid, conditionObjId)
	Creature* creature = otx::lua::getCreature(L, 1);
	if (!creature) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	const auto conditionObjId = otx::lua::getNumber<uint32_t>(L, 2);
	Condition* condition = g_lua.getConditionById(conditionObjId);
	if (!condition) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CONDITION_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	creature->addCondition(condition->clone());
	lua_pushboolean(L, true);
	return 1;
}

static int luaDoRemoveCondition(lua_State* L)
{
	// doRemoveCondition(cid, type[, subId = 0, conditionId = CONDITIONID_COMBAT])
	if (Creature* creature = otx::lua::getCreature(L, 1)) {
		const auto conditionType = static_cast<ConditionType_t>(otx::lua::getNumber<uint32_t>(L, 2));
		const auto subId = otx::lua::getNumber<uint32_t>(L, 3, 0);
		const auto conditionId = static_cast<ConditionId_t>(otx::lua::getNumber<int8_t>(L, 4, CONDITIONID_COMBAT));

		Condition* condition;
		while ((condition = creature->getCondition(conditionType, conditionId, subId))) {
			creature->removeCondition(condition);
		}

		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoRemoveConditions(lua_State* L)
{
	// doRemoveConditions(cid[, onlyPersistent = true])
	if (Creature* creature = otx::lua::getCreature(L, 1)) {
		const bool onlyPersistent = otx::lua::getBoolean(L, 2, true);
		creature->removeConditions(CONDITIONEND_ABORT, onlyPersistent);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaNumberToVariant(lua_State* L)
{
	// numberToVariant(number)
	const auto number = otx::lua::getNumber<uint32_t>(L, 1);

	LuaVariant var;
	var.type = VARIANT_NUMBER;
	var.number = number;
	otx::lua::pushVariant(L, var);
	return 1;
}

static int luaStringToVariant(lua_State* L)
{
	// stringToVariant(string)
	std::string text = otx::lua::getString(L, 1);

	LuaVariant var;
	var.type = VARIANT_STRING;
	var.text = std::move(text);
	otx::lua::pushVariant(L, var);
	return 1;
}

static int luaPositionToVariant(lua_State* L)
{
	// positionToVariant(pos)
	const Position pos = otx::lua::getPosition(L, 1);

	LuaVariant var;
	var.type = VARIANT_POSITION;
	var.pos = pos;
	otx::lua::pushVariant(L, var);
	return 1;
}

static int luaTargetPositionToVariant(lua_State* L)
{
	// targetPositionToVariant(pos)
	const Position pos = otx::lua::getPosition(L, 1);

	LuaVariant var;
	var.type = VARIANT_TARGETPOSITION;
	var.pos = pos;
	otx::lua::pushVariant(L, var);
	return 1;
}

static int luaVariantToNumber(lua_State* L)
{
	// variantToNumber(var)
	const LuaVariant var = otx::lua::getVariant(L, 1);
	if (var.type == VARIANT_NUMBER) {
		lua_pushnumber(L, var.number);
	} else {
		lua_pushnumber(L, 0);
	}
	return 1;
}

static int luaVariantToString(lua_State* L)
{
	// variantToString(var)
	const LuaVariant var = otx::lua::getVariant(L, 1);
	if (var.type == VARIANT_STRING) {
		otx::lua::pushString(L, var.text);
	} else {
		otx::lua::pushString(L, "");
	}
	return 1;
}

static int luaVariantToPosition(lua_State* L)
{
	// variantToPosition(var)
	const LuaVariant var = otx::lua::getVariant(L, 1);
	if (var.type == VARIANT_POSITION || var.type == VARIANT_TARGETPOSITION) {
		otx::lua::pushPosition(L, var.pos);
	} else {
		otx::lua::pushPosition(L, Position());
	}
	return 1;
}

static int luaDoChangeSpeed(lua_State* L)
{
	// doChangeSpeed(cid, delta)
	if (Creature* creature = otx::lua::getCreature(L, 1)) {
		const auto delta = otx::lua::getNumber<int32_t>(L, 2);
		g_game.changeSpeed(creature, delta);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaSetCreatureOutfit(lua_State* L)
{
	// doSetCreatureOutfit(cid, outfit[, time = -1])
	if (Creature* creature = otx::lua::getCreature(L, 1)) {
		const Outfit_t outfit = otx::lua::getOutfit(L, 2);
		const auto time = otx::lua::getNumber<int32_t>(L, 3, -1);
		lua_pushboolean(L, Spell::CreateIllusion(creature, outfit, time) == RET_NOERROR);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetCreatureOutfit(lua_State* L)
{
	// getCreatureOutfit(cid)
	if (const Creature* creature = otx::lua::getCreature(L, 1)) {
		otx::lua::pushOutfit(L, creature->getCurrentOutfit());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaSetMonsterOutfit(lua_State* L)
{
	// doSetMonsterOutfit(cid, monsterName[, time = -1])
	if (Creature* creature = otx::lua::getCreature(L, 1)) {
		const std::string monsterName = otx::lua::getString(L, 2);
		const auto time = otx::lua::getNumber<int32_t>(L, 3, -1);
		lua_pushboolean(L, Spell::CreateIllusion(creature, monsterName, time) == RET_NOERROR);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaSetItemOutfit(lua_State* L)
{
	// doSetItemOutfit(cid, itemId[, time = -1])
	if (Creature* creature = otx::lua::getCreature(L, 1)) {
		const auto itemId = otx::lua::getNumber<uint16_t>(L, 2);
		const auto time = otx::lua::getNumber<int32_t>(L, 3, -1);
		lua_pushboolean(L, Spell::CreateIllusion(creature, itemId, time) == RET_NOERROR);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetStorageList(lua_State* L)
{
	// getStorageList()
	const auto& storages = g_game.getGlobalStorages();
	lua_createtable(L, storages.size(), 0);

	int index = 0;
	for (const auto& it : storages) {
		otx::lua::pushString(L, it.first);
		lua_rawseti(L, -2, ++index);
	}
	return 1;
}

static int luaGetStorage(lua_State* L)
{
	// getStorage(key)
	const std::string key = otx::lua::getString(L, 1);

	const std::string* value = g_game.getGlobalStorage(key);
	if (!value) {
		lua_pushnumber(L, -1);
		return 1;
	}

	auto result = otx::util::safe_cast<int64_t>(value->data());
	if (result.second) {
		lua_pushnumber(L, result.first);
	} else {
		otx::lua::pushString(L, *value);
	}
	return 1;
}

static int luaDoSetStorage(lua_State* L)
{
	// doSetStorage(key[, value = nil])
	const std::string key = otx::lua::getString(L, 1);
	if (!otx::lua::isNoneOrNil(L, 2)) {
		const std::string value = otx::lua::getString(L, 2);
		g_game.setGlobalStorage(key, value);
	} else {
		g_game.removeGlobalStorage(key);
	}

	lua_pushboolean(L, true);
	return 1;
}

static int luaGetPlayerDepotItems(lua_State* L)
{
	// getPlayerDepotItems(cid, depotId)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const auto depotId = otx::lua::getNumber<uint32_t>(L, 2);
		if (const Depot* depot = player->getDepot(depotId, true)) {
			lua_pushnumber(L, depot->getItemHoldingCount());
		} else {
			lua_pushnil(L);
		}
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoPlayerSetGuildId(lua_State* L)
{
	// doPlayerSetGuildId(cid, id)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const auto id = otx::lua::getNumber<uint32_t>(L, 2);
		if (player->getGuildId() != 0) {
			player->leaveGuild();

			if (id == 0) {
				lua_pushboolean(L, true);
			} else if (IOGuild::getInstance()->guildExists(id)) {
				lua_pushboolean(L, IOGuild::getInstance()->joinGuild(player, id));
			} else {
				lua_pushboolean(L, false);
			}
		} else if (id != 0 && IOGuild::getInstance()->guildExists(id)) {
			lua_pushboolean(L, IOGuild::getInstance()->joinGuild(player, id));
		} else {
			lua_pushboolean(L, false);
		}
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoPlayerSetGuildLevel(lua_State* L)
{
	// doPlayerSetGuildLevel(cid, level[, rank = 0])
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const auto level = static_cast<GuildLevel_t>(otx::lua::getNumber<uint8_t>(L, 2));
		const auto rank = otx::lua::getNumber<uint32_t>(L, 3, 0);
		lua_pushboolean(L, player->setGuildLevel(level, rank));
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoPlayerSetGuildNick(lua_State* L)
{
	// doPlayerSetGuildNick(cid, nick)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const std::string nick = otx::lua::getString(L, 2);
		player->setGuildNick(nick);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetGuildId(lua_State* L)
{
	// getGuildId(guildName)
	const std::string guildName = otx::lua::getString(L, 1);
	uint32_t guildId;
	if (IOGuild::getInstance()->getGuildId(guildId, guildName)) {
		lua_pushnumber(L, guildId);
	} else {
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetGuildMotd(lua_State* L)
{
	// getGuildMotd(guildId)
	const auto guildId = otx::lua::getNumber<uint32_t>(L, 1);
	if (IOGuild::getInstance()->guildExists(guildId)) {
		otx::lua::pushString(L, IOGuild::getInstance()->getMotd(guildId));
	} else {
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoMoveCreature(lua_State* L)
{
	// doMoveCreature(cid, direction[, flag = FLAG_NOLIMIT])
	if (Creature* creature = otx::lua::getCreature(L, 1)) {
		const auto direction = static_cast<Direction>(otx::lua::getNumber<uint8_t>(L, 2));
		const auto flags = otx::lua::getNumber<uint8_t>(L, 3, FLAG_NOLIMIT);

		if (direction > NORTHEAST) {
			otx::lua::reportErrorEx(L, "Invalid direction.");
			lua_pushnil(L);
			return 1;
		}
		lua_pushnumber(L, g_game.internalMoveCreature(creature, direction, flags));
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoSteerCreature(lua_State* L)
{
	// doSteerCreature(cid, position[, maxNodes = 100])
	if (Creature* creature = otx::lua::getCreature(L, 1)) {
		const Position position = otx::lua::getPosition(L, 2);
		const auto maxNodes = otx::lua::getNumber<uint16_t>(L, 3, 100);
		lua_pushboolean(L, g_game.steerCreature(creature, position, maxNodes));
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaIsCreature(lua_State* L)
{
	// isCreature(cid)
	lua_pushboolean(L, otx::lua::getCreature(L, 1) != nullptr);
	return 1;
}

static int luaIsMovable(lua_State* L)
{
	// isMovable(uid)
	Thing* thing = otx::lua::getThing(L, 1);
	lua_pushboolean(L, thing && thing->isPushable());
	return 1;
}

static int luaGetCreatureByName(lua_State* L)
{
	// getCreatureByName(name)
	const std::string name = otx::lua::getString(L, 1);
	if (Creature* creature = g_game.getCreatureByName(name, CREATURE_TYPE_UNDEFINED)) {
		lua_pushnumber(L, otx::lua::getScriptEnv().addThing(creature));
	} else {
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerByName(lua_State* L)
{
	// getPlayerByName(name)
	const std::string name = otx::lua::getString(L, 1);
	if (Player* player = g_game.getPlayerByName(name)) {
		lua_pushnumber(L, otx::lua::getScriptEnv().addThing(player));
	} else {
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerByGUID(lua_State* L)
{
	// getPlayerByGUID(guid)
	const auto guid = otx::lua::getNumber<uint32_t>(L, 1);
	if (Player* player = g_game.getPlayerByGUID(guid)) {
		lua_pushnumber(L, otx::lua::getScriptEnv().addThing(player));
	} else {
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerByNameWildcard(lua_State* L)
{
	// getPlayerByNameWildcard(name[, pushValue = false])
	const std::string name = otx::lua::getString(L, 1);
	const bool pushValue = otx::lua::getBoolean(L, 2, false);

	Player* player = nullptr;
	ReturnValue ret = g_game.getPlayerByNameWildcard(name, player);
	if (ret == RET_NOERROR) {
		lua_pushnumber(L, otx::lua::getScriptEnv().addThing(player));
	} else if (pushValue) {
		lua_pushnumber(L, ret);
	} else {
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerGUIDByName(lua_State* L)
{
	// getPlayerGUIDByName(name)
	std::string name = otx::lua::getString(L, 1);

	uint32_t guid;
	if (Player* player = g_game.getPlayerByName(name)) {
		lua_pushnumber(L, player->getGUID());
	} else if (IOLoginData::getInstance()->getGuidByName(guid, name)) {
		lua_pushnumber(L, guid);
	} else {
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerNameByGUID(lua_State* L)
{
	// getPlayerNameByGUID(guid)
	const auto guid = otx::lua::getNumber<uint32_t>(L, 1);

	std::string name;
	if (IOLoginData::getInstance()->getNameByGuid(guid, name)) {
		otx::lua::pushString(L, name);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoPlayerChangeName(lua_State* L)
{
	// doPlayerChangeName(guid, oldName, newName)
	const auto guid = otx::lua::getNumber<uint32_t>(L, 1);
	const std::string oldName = otx::lua::getString(L, 2);
	const std::string newName = otx::lua::getString(L, 3);

	if (IOLoginData::getInstance()->changeName(guid, newName, oldName)) {
		if (House* house = Houses::getInstance()->getHouseByPlayerId(guid)) {
			house->updateDoorDescription(newName);
		}
		lua_pushboolean(L, true);
	} else {
		lua_pushboolean(L, false);
	}
	return 1;
}

static int luaGetPlayersByAccountId(lua_State* L)
{
	// getPlayersByAccountId(accId)
	const auto accountId = otx::lua::getNumber<uint32_t>(L, 1);
	const auto& players = g_game.getPlayersByAccount(accountId);

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	lua_createtable(L, players.size(), 0);

	int index = 0;
	for (Player* player : players) {
		lua_pushnumber(L, env.addThing(player));
		lua_rawseti(L, -2, ++index);
	}
	return 1;
}

static int luaGetIpByName(lua_State* L)
{
	// getIpByName(name)
	std::string name = otx::lua::getString(L, 1);
	if (Player* player = g_game.getPlayerByName(name)) {
		lua_pushnumber(L, player->getIP());
	} else if (IOLoginData::getInstance()->playerExists(name)) {
		lua_pushnumber(L, IOLoginData::getInstance()->getLastIPByName(name));
	} else {
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayersByIp(lua_State* L)
{
	// getPlayersByIp(ip[, mask = 0xFFFFFFFF])
	const auto ip = otx::lua::getNumber<uint32_t>(L, 1);
	const auto mask = otx::lua::getNumber<uint32_t>(L, 2, 0xFFFFFFFF);

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	const auto& players = g_game.getPlayersByIP(ip, mask);
	lua_createtable(L, players.size(), 0);

	int index = 0;
	for (Player* player : players) {
		lua_pushnumber(L, env.addThing(player));
		lua_rawseti(L, -2, ++index);
	}
	return 1;
}

static int luaGetAccountIdByName(lua_State* L)
{
	// getAccountIdByName(name)
	const std::string name = otx::lua::getString(L, 1);
	if (Player* player = g_game.getPlayerByName(name)) {
		lua_pushnumber(L, player->getAccount());
	} else {
		lua_pushnumber(L, IOLoginData::getInstance()->getAccountIdByName(name));
	}
	return 1;
}

static int luaGetAccountByName(lua_State* L)
{
	// getAccountByName(name)
	const std::string name = otx::lua::getString(L, 1);
	if (Player* player = g_game.getPlayerByName(name)) {
		otx::lua::pushString(L, player->getAccountName());
	} else {
		std::string tmp;
		IOLoginData::getInstance()->getAccountName(IOLoginData::getInstance()->getAccountIdByName(name), tmp);
		otx::lua::pushString(L, tmp);
	}
	return 1;
}

static int luaGetAccountIdByAccount(lua_State* L)
{
	// getAccountIdByAccount(accName)
	const std::string accountName = otx::lua::getString(L, 1);
	uint32_t value = 0;
	IOLoginData::getInstance()->getAccountId(accountName, value);
	lua_pushnumber(L, value);
	return 1;
}

static int luaGetAccountByAccountId(lua_State* L)
{
	// getAccountByAccountId(accountId)
	const auto accountId = otx::lua::getNumber<uint32_t>(L, 1);

	std::string value;
	IOLoginData::getInstance()->getAccountName(accountId, value);
	otx::lua::pushString(L, value);
	return 1;
}

static int luaGetAccountFlagValue(lua_State* L)
{
	// getAccountFlagValue(name/id, flag)
	const auto flag = static_cast<PlayerFlags>(otx::lua::getNumber<uint8_t>(L, 2));
	if (otx::lua::isNumber(L, 1)) {
		lua_pushboolean(L, IOLoginData::getInstance()->hasFlag(otx::lua::getNumber<uint32_t>(L, 1), flag));
	} else {
		lua_pushboolean(L, IOLoginData::getInstance()->hasFlag(flag, otx::lua::getString(L, 1)));
	}
	return 1;
}

static int luaGetAccountCustomFlagValue(lua_State* L)
{
	// getAccountCustomFlagValue(name/id, flag)
	const auto flag = static_cast<PlayerCustomFlags>(otx::lua::getNumber<uint8_t>(L, 2));
	if (otx::lua::isNumber(L, 1)) {
		lua_pushboolean(L, IOLoginData::getInstance()->hasCustomFlag(otx::lua::getNumber<uint32_t>(L, 1), flag));
	} else {
		lua_pushboolean(L, IOLoginData::getInstance()->hasCustomFlag(flag, otx::lua::getString(L, 1)));
	}
	return 1;
}

static int luaRegisterCreatureEvent(lua_State* L)
{
	// registerCreatureEvent(cid, name)
	if (Creature* creature = otx::lua::getCreature(L, 1)) {
		const std::string name = otx::lua::getString(L, 2);
		lua_pushboolean(L, creature->registerCreatureEvent(name));
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaUnregisterCreatureEvent(lua_State* L)
{
	// unregisterCreatureEvent(cid, name)
	if (Creature* creature = otx::lua::getCreature(L, 1)) {
		const std::string name = otx::lua::getString(L, 2);
		lua_pushboolean(L, creature->unregisterCreatureEvent(name));
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaUnregisterCreatureEventType(lua_State* L)
{
	// unregisterCreatureEventType(cid, typeName)
	if (Creature* creature = otx::lua::getCreature(L, 1)) {
		const std::string typeName = otx::lua::getString(L, 2);
		CreatureEventType_t _type = g_creatureEvents.getType(typeName);
		if (_type != CREATURE_EVENT_NONE) {
			creature->unregisterCreatureEvent(_type);
			lua_pushboolean(L, true);
		} else {
			otx::lua::reportErrorEx(L, "Invalid event type.");
			lua_pushnil(L);
		}
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetContainerSize(lua_State* L)
{
	// getContainerSize(uid)
	if (Container* container = otx::lua::getContainer(L, 1)) {
		lua_pushnumber(L, container->size());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CONTAINER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetContainerCap(lua_State* L)
{
	// getContainerCap(uid)
	if (Container* container = otx::lua::getContainer(L, 1)) {
		lua_pushnumber(L, container->capacity());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CONTAINER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetContainerItem(lua_State* L)
{
	// getContainerItem(uid, slot)
	if (Container* container = otx::lua::getContainer(L, 1)) {
		const auto slot = otx::lua::getNumber<uint32_t>(L, 2);
		otx::lua::pushThing(L, container->getItem(slot));
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CONTAINER_NOT_FOUND));
		otx::lua::pushThing(L, nullptr);
	}
	return 1;
}

static int luaDoAddContainerItemEx(lua_State* L)
{
	// doAddContainerItemEx(uid, item)
	if (Container* container = otx::lua::getContainer(L, 1)) {
		const auto uid = otx::lua::getNumber<uint32_t>(L, 2);

		ScriptEnvironment& env = otx::lua::getScriptEnv();
		Item* item = env.getItemByUID(uid);
		if (!item) {
			otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_ITEM_NOT_FOUND));
			lua_pushnil(L);
			return 1;
		}

		if (item->getParent() != VirtualCylinder::virtualCylinder) {
			lua_pushboolean(L, false);
			return 1;
		}

		ReturnValue ret = g_game.internalAddItem(nullptr, container, item);
		if (ret == RET_NOERROR) {
			env.removeItem(uid);
		}

		lua_pushnumber(L, ret);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CONTAINER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoAddContainerItem(lua_State* L)
{
	// doAddContainerItem(uid, itemId[, count/subType = 1])
	Container* container = otx::lua::getContainer(L, 1);
	if (!container) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CONTAINER_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	const auto itemId = otx::lua::getNumber<uint16_t>(L, 2);
	const auto count = otx::lua::getNumber<int32_t>(L, 3, 1);

	const ItemType& it = Item::items[itemId];

	int32_t itemCount = 1;
	int32_t subType = 1;
	if (it.hasSubType()) {
		if (it.stackable) {
			itemCount = std::ceil(count / 100.0);
		}

		subType = count;
	} else {
		itemCount = std::max<int32_t>(1, count);
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();

	uint32_t ret = 0;
	while (itemCount > 0) {
		int32_t stackCount = std::min(100, subType);
		Item* newItem = Item::CreateItem(itemId, stackCount);
		if (!newItem) {
			otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_ITEM_NOT_FOUND));
			lua_pushnil(L);
			return 1;
		}

		if (it.stackable) {
			subType -= stackCount;
		}

		uint32_t dummy = 0;
		Item* stackItem = nullptr;
		if (g_game.internalAddItem(nullptr, container, newItem, INDEX_WHEREEVER, FLAG_NOLIMIT, false, dummy, &stackItem) != RET_NOERROR) {
			delete newItem;
			lua_pushboolean(L, false);
			return ++ret;
		}

		++ret;
		if (newItem->getParent()) {
			lua_pushnumber(L, env.addThing(newItem));
		} else if (stackItem) {
			lua_pushnumber(L, env.addThing(stackItem));
		} else { // stackable item stacked with existing object, newItem will be released
			lua_pushnil(L);
		}

		--itemCount;
	}

	if (ret != 0) {
		return ret;
	}

	lua_pushnil(L);
	return 1;
}

static int luaGetOutfitIdByLooktype(lua_State* L)
{
	// getOutfitIdByLooktype(lookType)
	const auto lookType = otx::lua::getNumber<uint32_t>(L, 1);
	lua_pushnumber(L, Outfits::getInstance()->getOutfitId(lookType));
	return 1;
}

static int luaDoPlayerAddOutfit(lua_State* L)
{
	// Consider using doPlayerAddOutfitId instead
	// doPlayerAddOutfit(cid, looktype, addons)
	Player* player = otx::lua::getPlayer(L, 1);
	if (!player) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	const auto lookType = otx::lua::getNumber<uint16_t>(L, 2);
	const auto addons = otx::lua::getNumber<uint8_t>(L, 3);

	Outfit outfit;
	if (Outfits::getInstance()->getOutfit(lookType, outfit)) {
		lua_pushboolean(L, player->addOutfit(outfit.outfitId, addons));
	} else {
		lua_pushboolean(L, false);
	}
	return 1;
}

static int luaDoPlayerRemoveOutfit(lua_State* L)
{
	// Consider using doPlayerRemoveOutfitId instead
	// doPlayerRemoveOutfit(cid, lookType[, addons = 0xFF])
	Player* player = otx::lua::getPlayer(L, 1);
	if (!player) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	const auto lookType = otx::lua::getNumber<uint16_t>(L, 2);
	const auto addons = otx::lua::getNumber<uint32_t>(L, 3, 0xFF);

	Outfit outfit;
	if (Outfits::getInstance()->getOutfit(lookType, outfit)) {
		lua_pushboolean(L, player->removeOutfit(outfit.outfitId, addons));
	} else {
		lua_pushboolean(L, false);
	}
	return 1;
}

static int luaDoPlayerAddOutfitId(lua_State* L)
{
	// doPlayerAddOutfitId(cid, outfitId, addon)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const auto outfitId = otx::lua::getNumber<uint32_t>(L, 2);
		const auto addons = otx::lua::getNumber<uint8_t>(L, 3);
		lua_pushboolean(L, player->addOutfit(outfitId, addons));
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoPlayerRemoveOutfitId(lua_State* L)
{
	// doPlayerRemoveOutfitId(cid, outfitId[, addons = 0])
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const auto outfitId = otx::lua::getNumber<uint32_t>(L, 2);
		const auto addons = otx::lua::getNumber<uint8_t>(L, 3, 0);
		lua_pushboolean(L, player->removeOutfit(outfitId, addons));
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaCanPlayerWearOutfit(lua_State* L)
{
	// canPlayerWearOutfit(cid, lookType[, addons = 0])
	Player* player = otx::lua::getPlayer(L, 1);
	if (!player) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	const auto lookType = otx::lua::getNumber<uint16_t>(L, 2);
	const auto addons = otx::lua::getNumber<uint8_t>(L, 3, 0);

	Outfit outfit;
	if (Outfits::getInstance()->getOutfit(lookType, outfit)) {
		lua_pushboolean(L, player->canWearOutfit(outfit.outfitId, addons));
	} else {
		lua_pushboolean(L, false);
	}
	return 1;
}

static int luaCanPlayerWearOutfitId(lua_State* L)
{
	// canPlayerWearOutfitId(cid, outfitId[, addons = 0])
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const auto outfitId = otx::lua::getNumber<uint32_t>(L, 2);
		const auto addons = otx::lua::getNumber<uint32_t>(L, 3, 0);
		lua_pushboolean(L, player->canWearOutfit(outfitId, addons));
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoCreatureChangeOutfit(lua_State* L)
{
	// doCreatureChangeOutfit(cid, outfit)
	if (Creature* creature = otx::lua::getCreature(L, 1)) {
		const Outfit_t outfit = otx::lua::getOutfit(L, 2);
		if (Player* player = creature->getPlayer()) {
			player->changeOutfit(outfit, false);
		} else {
			creature->setDefaultOutfit(outfit);
		}

		if (!creature->hasCondition(CONDITION_OUTFIT, 1)) {
			g_game.internalCreatureChangeOutfit(creature, outfit);
		}
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoPlayerPopupFYI(lua_State* L)
{
	// doPlayerPopupFYI(cid, message)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const std::string message = otx::lua::getString(L, 2);
		player->sendFYIBox(message);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoPlayerSendTutorial(lua_State* L)
{
	// doPlayerSendTutorial(cid, id)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const auto id = otx::lua::getNumber<uint8_t>(L, 2);
		player->sendTutorial(id);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoPlayerSendMailByName(lua_State* L)
{
	// doPlayerSendMailByName(name, item[, townId = 0, actorId = 0])
	const std::string name = otx::lua::getString(L, 1);
	const auto uid = otx::lua::getNumber<uint32_t>(L, 2);
	const auto townId = otx::lua::getNumber<uint32_t>(L, 3, 0);
	const auto actorId = otx::lua::getNumber<uint32_t>(L, 4, 0);

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	Item* item = env.getItemByUID(uid);
	if (!item) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_ITEM_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	if (item->getParent() != VirtualCylinder::virtualCylinder) {
		lua_pushboolean(L, false);
		return 1;
	}

	Creature* actor = g_game.getCreatureByID(actorId);
	const bool result = IOLoginData::getInstance()->playerMail(actor, name, townId, item);
	if (result) {
		env.removeItem(uid);
	}

	lua_pushboolean(L, result);
	return 1;
}

static int luaDoPlayerAddMapMark(lua_State* L)
{
	// doPlayerAddMapMark(cid, position, markType[, description = ""])
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const auto position = otx::lua::getPosition(L, 2);
		const auto markType = static_cast<MapMarks_t>(otx::lua::getNumber<uint8_t>(L, 3));
		const std::string description = otx::lua::getString(L, 4);
		player->sendAddMarker(position, markType, description);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoPlayerAddPremiumDays(lua_State* L)
{
	// doPlayerAddPremiumDays(cid, days)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const auto days = otx::lua::getNumber<int32_t>(L, 2);
		player->addPremiumDays(days);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetCreatureLastPosition(lua_State* L)
{
	// getCreatureLastPosition(cid)
	if (Creature* creature = otx::lua::getCreature(L, 1)) {
		otx::lua::pushPosition(L, creature->getLastPosition());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetCreatureName(lua_State* L)
{
	// getCreatureName(cid)
	if (Creature* creature = otx::lua::getCreature(L, 1)) {
		otx::lua::pushString(L, creature->getName().c_str());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaUpdatePlayerStats(lua_State* L)
{
	// doUpdatePlayerStats(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		player->sendStats();
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerDamageMultiplier(lua_State* L)
{
	// getPlayerDamageMultiplier(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		lua_pushnumber(L, player->getDamageMultiplier());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaSetPlayerDamageMultiplier(lua_State* L)
{
	// setPlayerDamageMultiplier(cid, multiplier)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const auto multiplier = otx::lua::getNumber<double>(L, 2);
		player->setDamageMultiplier(multiplier);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetCreatureNoMove(lua_State* L)
{
	// getCreatureNoMove(cid)
	if (Creature* creature = otx::lua::getCreature(L, 1)) {
		lua_pushboolean(L, creature->getNoMove());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetCreatureGuildEmblem(lua_State* L)
{
	// getCreatureGuildEmblem(cid[, targetId = 0])
	if (Creature* creature = otx::lua::getCreature(L, 1)) {
		const auto targetId = otx::lua::getNumber<uint32_t>(L, 2);
		if (targetId == 0) {
			lua_pushnumber(L, creature->getEmblem());
		} else if (Creature* target = otx::lua::getCreature(L, targetId)) {
			lua_pushnumber(L, creature->getGuildEmblem(target));
		} else {
			otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
			lua_pushnil(L);
		}
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoCreatureSetGuildEmblem(lua_State* L)
{
	// doCreatureSetGuildEmblem(cid, emblem)
	if (Creature* creature = otx::lua::getCreature(L, 1)) {
		const auto emblem = static_cast<GuildEmblems_t>(otx::lua::getNumber<uint8_t>(L, 2));
		creature->setEmblem(emblem);
		g_game.updateCreatureEmblem(creature);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetCreaturePartyShield(lua_State* L)
{
	// getCreaturePartyShield(cid[, targetId = 0])
	if (Creature* creature = otx::lua::getCreature(L, 1)) {
		const auto targetId = otx::lua::getNumber<uint32_t>(L, 2);
		if (targetId == 0) {
			lua_pushnumber(L, creature->getShield());
		} else if (Creature* target = otx::lua::getCreature(L, targetId)) {
			lua_pushnumber(L, creature->getPartyShield(target));
		} else {
			otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
			lua_pushnil(L);
		}
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoCreatureSetPartyShield(lua_State* L)
{
	// doCreatureSetPartyShield(cid, shield)
	if (Creature* creature = otx::lua::getCreature(L, 1)) {
		const auto shield = static_cast<PartyShields_t>(otx::lua::getNumber<uint8_t>(L, 2));
		creature->setShield(shield);
		g_game.updateCreatureShield(creature);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetCreatureSkullType(lua_State* L)
{
	// getCreatureSkullType(cid[, targetId = 0])
	if (Creature* creature = otx::lua::getCreature(L, 1)) {
		const auto targetId = otx::lua::getNumber<uint32_t>(L, 2);
		if (targetId == 0) {
			lua_pushnumber(L, creature->getSkull());
		} else if (Creature* target = otx::lua::getCreature(L, targetId)) {
			lua_pushnumber(L, creature->getSkullType(target));
		} else {
			otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
			lua_pushnil(L);
		}
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoCreatureSetLookDir(lua_State* L)
{
	// doCreatureSetLookDirection(cid, dir)
	if (Creature* creature = otx::lua::getCreature(L, 1)) {
		const auto direction = static_cast<Direction>(otx::lua::getNumber<uint8_t>(L, 2));
		if (direction <= WEST) {
			g_game.internalCreatureTurn(creature, direction);
			lua_pushboolean(L, true);
		} else {
			otx::lua::reportErrorEx(L, "Invalid direction.");
			lua_pushnil(L);
		}
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoCreatureSetSkullType(lua_State* L)
{
	// doCreatureSetSkullType(cid, skull)
	if (Creature* creature = otx::lua::getCreature(L, 1)) {
		const auto skull = static_cast<Skulls_t>(otx::lua::getNumber<uint8_t>(L, 2));
		creature->setSkull(skull);
		g_game.updateCreatureSkull(creature);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoPlayerSetSkullEnd(lua_State* L)
{
	// doPlayerSetSkullEnd(cid, time, type)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const auto time = std::max<int64_t>(0, otx::lua::getNumber<int64_t>(L, 2));
		const auto skull = static_cast<Skulls_t>(otx::lua::getNumber<uint8_t>(L, 3));
		player->setSkullEnd(time, false, skull);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetCreatureSpeed(lua_State* L)
{
	// getCreatureSpeed(cid)
	if (Creature* creature = otx::lua::getCreature(L, 1)) {
		lua_pushnumber(L, creature->getSpeed());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaSetCreatureSpeed(lua_State* L)
{
	// setCreatureSpeed(cid, speed)
	if (Creature* creature = otx::lua::getCreature(L, 1)) {
		const auto speed = otx::lua::getNumber<int32_t>(L, 2);
		g_game.setCreatureSpeed(creature, speed);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaSendProgressbar(lua_State* L)
{
	// sendPlayerProgressBar(cid, duration, leftToRight)
	if (Creature* creature = otx::lua::getCreature(L, 1)) {
		const auto duration = otx::lua::getNumber<uint32_t>(L, 2);
		const bool leftToRight = otx::lua::getBoolean(L, 3);
		g_game.startProgressbar(creature, duration, leftToRight);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetCreatureBaseSpeed(lua_State* L)
{
	// getCreatureBaseSpeed(cid)
	if (Creature* creature = otx::lua::getCreature(L, 1)) {
		lua_pushnumber(L, creature->getBaseSpeed());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetCreatureTarget(lua_State* L)
{
	// getCreatureTarget(cid)
	if (Creature* creature = otx::lua::getCreature(L, 1)) {
		lua_pushnumber(L, otx::lua::getScriptEnv().addThing(creature->getAttackedCreature()));
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaIsSightClear(lua_State* L)
{
	// isSightClear(fromPos, toPos, floorCheck)
	const Position fromPos = otx::lua::getPosition(L, 1);
	const Position toPos = otx::lua::getPosition(L, 2);
	const bool floorCheck = otx::lua::getBoolean(L, 3);
	lua_pushboolean(L, g_game.isSightClear(fromPos, toPos, floorCheck));
	return 1;
}

static int luaAddEvent(lua_State* L)
{
	// addEvent(callback, delay, ...)
	if (!otx::lua::isFunction(L, 1)) {
		otx::lua::reportErrorEx(L, "Invalid function.");
		lua_pushnil(L);
		return 1;
	}

	if (!otx::lua::isNumber(L, 2)) {
		otx::lua::reportErrorEx(L, "Invalid delay.");
		lua_pushnil(L);
		return 1;
	}

	const int nparams = lua_gettop(L) - 2;

	LuaTimerEvent event;
	event.parameters.reserve(nparams);
	for (int i = 0; i < nparams; ++i) {
		event.parameters.push_back(luaL_ref(L, LUA_REGISTRYINDEX));
	}

	const uint32_t delay = std::max<int32_t>(SCHEDULER_MINTICKS, otx::lua::getNumber<int32_t>(L, 2));
	lua_pop(L, 1);

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	event.funcRef = luaL_ref(L, LUA_REGISTRYINDEX);
	event.script = otx::lua::getSource(L, 1);
	event.scriptId = env.getScriptId();
	event.npc = env.getNpc();
	lua_pushnumber(L, g_lua.addTimerEvent(std::move(event), delay));
	return 1;
}

static int luaStopEvent(lua_State* L)
{
	// stopEvent(eventId)
	const auto eventId = otx::lua::getNumber<uint32_t>(L, 1);
	lua_pushboolean(L, g_lua.stopTimerEvent(L, eventId));
	return 1;
}

static int luaHasCreatureCondition(lua_State* L)
{
	// hasCreatureCondition(cid, conditionType[, subId = 0, conditionId = *])
	if (Creature* creature = otx::lua::getCreature(L, 1)) {
		const auto conditionType = static_cast<ConditionType_t>(otx::lua::getNumber<uint32_t>(L, 2));
		const auto subId = otx::lua::getNumber<uint32_t>(L, 3, 0);
		const auto conditionId = static_cast<ConditionId_t>(otx::lua::getNumber<int8_t>(L, 4, CONDITIONID_COMBAT));
		if (lua_gettop(L) >= 4) {
			lua_pushboolean(L, creature->getCondition(conditionType, conditionId, subId) != nullptr);
		} else if (creature->getCondition(conditionType, CONDITIONID_DEFAULT, subId) != nullptr) {
			lua_pushboolean(L, true);
			return 1;
		} else {
			lua_pushboolean(L, creature->getCondition(conditionType, CONDITIONID_COMBAT, subId) != nullptr);
		}
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetCreatureConditionInfo(lua_State* L)
{
	// getCreatureConditionInfo(cid, conditionType[, subId = 0, conditionId = CONDITIONID_COMBAT])
	if (Creature* creature = otx::lua::getCreature(L, 1)) {
		const auto conditionType = static_cast<ConditionType_t>(otx::lua::getNumber<uint32_t>(L, 2));
		const auto subId = otx::lua::getNumber<uint32_t>(L, 3, 0);
		const auto conditionId = static_cast<ConditionId_t>(otx::lua::getNumber<int8_t>(L, 4, CONDITIONID_COMBAT));
		if (Condition* condition = creature->getCondition(conditionType, conditionId, subId)) {
			lua_createtable(L, 0, 5);
			otx::lua::setTableValue(L, "icons", condition->getIcons());
			otx::lua::setTableValue(L, "endTime", condition->getEndTime());
			otx::lua::setTableValue(L, "ticks", condition->getTicks());
			otx::lua::setTableValue(L, "persistent", condition->isPersistent());
			otx::lua::setTableValue(L, "subId", condition->getSubId());
		} else {
			lua_pushnil(L);
		}
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerBlessing(lua_State* L)
{
	// getPlayerBlessing(cid, blessing)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const auto blessing = otx::lua::getNumber<int16_t>(L, 2) - 1;
		lua_pushboolean(L, player->hasBlessing(blessing));
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoPlayerAddBlessing(lua_State* L)
{
	// doPlayerAddBlessing(cid, blessing)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const auto blessing = otx::lua::getNumber<int16_t>(L, 2) - 1;
		if (!player->hasBlessing(blessing)) {
			player->addBlessing(1 << blessing);
			lua_pushboolean(L, true);
		} else {
			lua_pushboolean(L, false);
		}
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerPVPBlessing(lua_State* L)
{
	// getPlayerPVPBlessing(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		lua_pushboolean(L, player->hasPVPBlessing());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoPlayerSetPVPBlessing(lua_State* L)
{
	// doPlayerSetPVPBlessing(cid[, value = true])
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const bool value = otx::lua::getBoolean(L, 2);
		player->setPVPBlessing(value);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoPlayerSetPromotionLevel(lua_State* L)
{
	// doPlayerSetPromotionLevel(cid, level)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const auto level = otx::lua::getNumber<uint32_t>(L, 2);
		player->setPromotionLevel(level);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoPlayerSetGroupId(lua_State* L)
{
	// doPlayerSetGroupId(cid, groupId)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const auto groupId = otx::lua::getNumber<uint32_t>(L, 2);
		if (Group* group = Groups::getInstance()->getGroup(groupId)) {
			player->setGroup(group);
			lua_pushboolean(L, true);
		} else {
			lua_pushboolean(L, false);
		}
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetCreatureMana(lua_State* L)
{
	// getCreatureMana(cid)
	if (Creature* creature = otx::lua::getCreature(L, 1)) {
		lua_pushnumber(L, creature->getMana());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetCreatureMaxMana(lua_State* L)
{
	// getCreatureMaxMana(cid[, ignoreModifiers = false])
	if (Creature* creature = otx::lua::getCreature(L, 1)) {
		const bool ignoreModifiers = otx::lua::getBoolean(L, 2, false);
		lua_pushnumber(L, ignoreModifiers ? creature->getBaseMaxMana() : creature->getMaxMana());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetCreatureHealth(lua_State* L)
{
	// getCreatureHealth(cid)
	if (Creature* creature = otx::lua::getCreature(L, 1)) {
		lua_pushnumber(L, creature->getHealth());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetCreatureLookDirection(lua_State* L)
{
	// getCreatureLookDirection(cid)
	if (Creature* creature = otx::lua::getCreature(L, 1)) {
		lua_pushnumber(L, creature->getDirection());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaExistMonsterByName(lua_State* L)
{
	// existMonsterByName(name)
	lua_pushboolean(L, g_game.existMonsterByName(otx::lua::getString(L, 1)));
	return 1;
}

static int luaGetCreatureMaxHealth(lua_State* L)
{
	// getCreatureMaxHealth(cid[, ignoreModifiers = false])
	if (Creature* creature = otx::lua::getCreature(L, 1)) {
		const bool ignoreModifiers = otx::lua::getBoolean(L, 2, false);
		lua_pushnumber(L, ignoreModifiers ? creature->getBaseMaxHealth() : creature->getMaxHealth());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoPlayerSetStamina(lua_State* L)
{
	// doPlayerSetStamina(cid, minutes)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const auto minutes = otx::lua::getNumber<uint32_t>(L, 2);
		player->setStaminaMinutes(minutes);
		player->sendStats();
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoPlayerSetBalance(lua_State* L)
{
	// doPlayerSetBalance(cid, balance)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const auto balance = otx::lua::getNumber<uint64_t>(L, 2);
		player->m_balance = balance;
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoPlayerSetPartner(lua_State* L)
{
	// doPlayerSetPartner(cid, guid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const auto guid = otx::lua::getNumber<uint32_t>(L, 2);
		player->m_marriage = guid;
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoPlayerFollowCreature(lua_State* L)
{
	// doPlayerFollowCreature(cid, target)
	Player* player = otx::lua::getPlayer(L, 1);
	if (!player) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	Creature* creature = otx::lua::getCreature(L, 2);
	if (!creature) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	lua_pushboolean(L, g_game.playerFollowCreature(player->getID(), creature->getID()));
	return 1;
}

static int luaGetPlayerParty(lua_State* L)
{
	// getPlayerParty(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		if (Party* party = player->getParty()) {
			lua_pushnumber(L, otx::lua::getScriptEnv().addThing(party->getLeader()));
		} else {
			lua_pushnil(L);
		}
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoPlayerJoinParty(lua_State* L)
{
	// doPlayerJoinParty(cid, lid)
	Player* player = otx::lua::getPlayer(L, 1);
	if (!player) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	Player* leader = otx::lua::getPlayer(L, 2);
	if (!leader) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	g_game.playerJoinParty(player->getID(), leader->getID());
	lua_pushboolean(L, true);
	return 1;
}

static int luaDoPlayerLeaveParty(lua_State* L)
{
	// doPlayerLeaveParty(cid[, forced = false])
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const bool forced = otx::lua::getBoolean(L, 2, false);
		g_game.playerLeaveParty(player->getID(), forced);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaIsPlayerUsingOtclient(lua_State* L)
{
	// isPlayerUsingOtclient(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		lua_pushboolean(L, player->isUsingOtclient());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoSendPlayerExtendedOpcode(lua_State* L)
{
	// doSendPlayerExtendedOpcode(cid, opcode, buffer)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const auto opcode = otx::lua::getNumber<uint8_t>(L, 2);
		const std::string buffer = otx::lua::getString(L, 3);
		player->sendExtendedOpcode(opcode, buffer);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPartyMembers(lua_State* L)
{
	// getPartyMembers(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		if (Party* party = player->getParty()) {
			const auto& partyMembers = party->getMembers();
			lua_createtable(L, partyMembers.size() + 1, 0);

			int index = 0;
			lua_pushnumber(L, party->getLeader()->getID());
			lua_rawseti(L, -2, ++index);

			for (Player* member : partyMembers) {
				lua_pushnumber(L, member->getID());
				lua_rawseti(L, -2, ++index);
			}
			return 1;
		}
	}

	lua_pushnil(L);
	return 1;
}

static int luaGetVocationInfo(lua_State* L)
{
	// getVocationInfo(vocationId)
	const auto vocationId = otx::lua::getNumber<uint32_t>(L, 1);
	Vocation* voc = g_vocations.getVocation(vocationId);
	if (!voc) {
		lua_pushnil(L);
		return 1;
	}

	lua_createtable(L, 0, 20);
	otx::lua::setTableValue(L, "id", voc->id);
	otx::lua::setTableValue(L, "name", voc->name);
	otx::lua::setTableValue(L, "description", voc->description);
	otx::lua::setTableValue(L, "healthGain", voc->gain[GAIN_HEALTH]);
	otx::lua::setTableValue(L, "healthGainTicks", voc->gainTicks[GAIN_HEALTH]);
	otx::lua::setTableValue(L, "healthGainAmount", voc->gainAmount[GAIN_HEALTH]);
	otx::lua::setTableValue(L, "manaGain", voc->gain[GAIN_MANA]);
	otx::lua::setTableValue(L, "manaGainTicks", voc->gainTicks[GAIN_MANA]);
	otx::lua::setTableValue(L, "manaGainAmount", voc->gainAmount[GAIN_MANA]);
	otx::lua::setTableValue(L, "attackSpeed", voc->attackSpeed);
	otx::lua::setTableValue(L, "baseSpeed", voc->baseSpeed);
	otx::lua::setTableValue(L, "fromVocation", voc->fromVocationId);
	otx::lua::setTableValue(L, "soul", voc->gain[GAIN_SOUL]);
	otx::lua::setTableValue(L, "soulAmount", voc->gainAmount[GAIN_SOUL]);
	otx::lua::setTableValue(L, "soulTicks", voc->gainTicks[GAIN_SOUL]);
	otx::lua::setTableValue(L, "capacity", voc->capGain);
	otx::lua::setTableValue(L, "attackable", voc->attackable);
	otx::lua::setTableValue(L, "needPremium", voc->needPremium);
	otx::lua::setTableValue(L, "promotedVocation", g_vocations.getPromotedVocation(vocationId));
	otx::lua::setTableValue(L, "experienceMultiplier", voc->getExperienceMultiplier());
	return 1;
}

static int luaGetGroupInfo(lua_State* L)
{
	// getGroupInfo(groupId[, premium = false])
	const auto groupId = otx::lua::getNumber<uint32_t>(L, 1);
	const bool premium = otx::lua::getBoolean(L, 2, false);
	Group* group = Groups::getInstance()->getGroup(groupId);
	if (!group) {
		lua_pushnil(L);
		return 1;
	}

	lua_createtable(L, 0, 9);
	otx::lua::setTableValue(L, "id", group->getId());
	otx::lua::setTableValue(L, "name", group->getName());
	otx::lua::setTableValue(L, "access", group->getAccess());
	otx::lua::setTableValue(L, "ghostAccess", group->getGhostAccess());
	otx::lua::setTableValue(L, "flags", group->getFlags());
	otx::lua::setTableValue(L, "customFlags", group->getCustomFlags());
	otx::lua::setTableValue(L, "depotLimit", group->getDepotLimit(premium));
	otx::lua::setTableValue(L, "maxVips", group->getMaxVips(premium));
	otx::lua::setTableValue(L, "outfit", group->getOutfit());
	return 1;
}

static int luaGetChannelUsers(lua_State* L)
{
	// getChannelUsers(channelId)
	const auto channelId = otx::lua::getNumber<uint16_t>(L, 2);

	if (ChatChannel* channel = g_chat.getChannelById(channelId)) {
		ScriptEnvironment& env = otx::lua::getScriptEnv();
		const auto& usersMap = channel->getUsers();
		lua_createtable(L, usersMap.size(), 0);

		int index = 0;
		for (const auto& it : usersMap) {
			lua_pushnumber(L, env.addThing(it.second));
			lua_rawseti(L, -2, ++index);
		}
	} else {
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayersOnline(lua_State* L)
{
	// getPlayersOnline()
	ScriptEnvironment& env = otx::lua::getScriptEnv();
	const auto& players = g_game.getPlayers();
	lua_createtable(L, players.size(), 0);

	int index = 0;
	for (const auto& it : players) {
		lua_pushnumber(L, env.addThing(it.second));
		lua_rawseti(L, -2, ++index);
	}
	return 1;
}

static int luaSetCreatureMaxHealth(lua_State* L)
{
	// setCreatureMaxHealth(uid, maxHealth)
	if (Creature* creature = otx::lua::getCreature(L, 1)) {
		const auto maxHealth = otx::lua::getNumber<int32_t>(L, 2);
		creature->changeMaxHealth(maxHealth);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaSetCreatureMaxMana(lua_State* L)
{
	// setCreatureMaxMana(uid, maxMana)
	if (Creature* creature = otx::lua::getCreature(L, 1)) {
		const auto maxMana = otx::lua::getNumber<int32_t>(L, 2);
		creature->changeMaxMana(maxMana);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoPlayerSetMaxCapacity(lua_State* L)
{
	// doPlayerSetMaxCapacity(uid, cap)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const auto cap = otx::lua::getNumber<double>(L, 2);
		player->setCapacity(cap);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetCreatureMaster(lua_State* L)
{
	// getCreatureMaster(cid)
	Creature* creature = otx::lua::getCreature(L, 1);
	if (!creature) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	if (Creature* master = creature->getMaster()) {
		lua_pushnumber(L, otx::lua::getScriptEnv().addThing(master));
	} else {
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetCreatureSummons(lua_State* L)
{
	// getCreatureSummons(cid)
	Creature* creature = otx::lua::getCreature(L, 1);
	if (!creature) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	const auto& summons = creature->getSummons();
	lua_createtable(L, summons.size(), 0);

	int index = 0;
	for (Creature* summon : summons) {
		lua_pushnumber(L, env.addThing(summon));
		lua_rawseti(L, -2, ++index);
	}
	return 1;
}

static int luaDoPlayerSetIdleTime(lua_State* L)
{
	// doPlayerSetIdleTime(cid, amount)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const auto amount = otx::lua::getNumber<uint32_t>(L, 2);
		player->setIdleTime(amount);
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoCreatureSetNoMove(lua_State* L)
{
	// doCreatureSetNoMove(cid, block)
	if (Creature* creature = otx::lua::getCreature(L, 1)) {
		const auto block = otx::lua::getBoolean(L, 2);
		creature->setNoMove(block);
		creature->onWalkAborted();
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetPlayerModes(lua_State* L)
{
	// getPlayerModes(cid)
	Player* player = otx::lua::getPlayer(L, 1);
	if (!player) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	lua_createtable(L, 0, 3);
	otx::lua::setTableValue(L, "chase", player->getChaseMode());
	otx::lua::setTableValue(L, "fight", player->getFightMode());
	otx::lua::setTableValue(L, "secure", player->getSecureMode());
	return 1;
}

static int luaGetPlayerRates(lua_State* L)
{
	// getPlayerRates(cid)
	Player* player = otx::lua::getPlayer(L, 1);
	if (!player) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	lua_createtable(L, SKILL__LAST + 1, 0);
	for (uint8_t i = SKILL_FIRST; i <= SKILL__LAST; ++i) {
		lua_pushnumber(L, player->m_rates[i]);
		lua_rawseti(L, -2, i + 1);
	}
	return 1;
}

static int luaDoPlayerSetRate(lua_State* L)
{
	// doPlayerSetRate(cid, skill, value)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const auto skill = static_cast<skills_t>(otx::lua::getNumber<uint8_t>(L, 2));
		const auto value = otx::lua::getNumber<double>(L, 3);
		if (skill <= SKILL__LAST) {
			player->m_rates[skill] = value;
			lua_pushboolean(L, true);
		} else {
			otx::lua::reportErrorEx(L, "Invalid skill.");
			lua_pushnil(L);
		}
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoPlayerSwitchSaving(lua_State* L)
{
	// doPlayerSwitchSaving(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		player->switchSaving();
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoPlayerSave(lua_State* L)
{
	// doPlayerSave(cid[, shallow = false])
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		const bool shallow = otx::lua::getBoolean(L, 2, false);
		player->setLoginPosition(player->getPosition());
		lua_pushboolean(L, IOLoginData::getInstance()->savePlayer(player, false, shallow));
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoPlayerSaveItems(lua_State* L)
{
	// doPlayerSaveItems(cid)
	if (Player* player = otx::lua::getPlayer(L, 1)) {
		player->setLoginPosition(player->getPosition());
		IOLoginData* p = IOLoginData::getInstance();
		std::thread th(&IOLoginData::savePlayerItems, p, player);
		th.detach();
		lua_pushboolean(L, true);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetTownId(lua_State* L)
{
	// getTownId(townName)
	const std::string townName = otx::lua::getString(L, 1);
	if (Town* town = Towns::getInstance()->getTown(townName)) {
		lua_pushnumber(L, town->getID());
	} else {
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetTownName(lua_State* L)
{
	// getTownName(townId)
	const auto townId = otx::lua::getNumber<uint32_t>(L, 1);
	if (Town* town = Towns::getInstance()->getTown(townId)) {
		otx::lua::pushString(L, town->getName());
	} else {
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetTownTemplePosition(lua_State* L)
{
	// getTownTemplePosition(townId)
	const auto townId = otx::lua::getNumber<uint32_t>(L, 1);
	if (Town* town = Towns::getInstance()->getTown(townId)) {
		otx::lua::pushPosition(L, town->getPosition());
	} else {
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetTownHouses(lua_State* L)
{
	// getTownHouses([townId = 0])
	const auto townId = otx::lua::getNumber<uint32_t>(L, 1, 0);

	const auto& houses = Houses::getInstance()->getHouses();
	lua_createtable(L, 0, 0);

	int index = 0;
	for (const auto& it : houses) {
		if (townId != 0 && it.second->getTownId() != townId) {
			continue;
		}

		lua_pushnumber(L, it.second->getId());
		lua_rawseti(L, -2, ++index);
	}
	return 1;
}

static int luaGetSpectators(lua_State* L)
{
	// getSpectators(centerPos, rangeX, rangeY[, multiFloor = false, onlyPlayers = false])
	const Position centerPos = otx::lua::getPosition(L, 1);
	const auto rangeX = otx::lua::getNumber<int32_t>(L, 2);
	const auto rangeY = otx::lua::getNumber<int32_t>(L, 3);
	const auto multiFloor = otx::lua::getBoolean(L, 4, false);
	const auto onlyPlayers = otx::lua::getBoolean(L, 5, false);

	SpectatorVec spectators;
	g_game.getSpectators(spectators, centerPos, multiFloor, onlyPlayers, rangeX, rangeX, rangeY, rangeY);
	if (spectators.empty()) {
		lua_pushnil(L);
		return 1;
	}

	ScriptEnvironment& env = otx::lua::getScriptEnv();
	lua_createtable(L, spectators.size(), 0);

	int index = 0;
	for (Creature* spectator : spectators) {
		lua_pushnumber(L, env.addThing(spectator));
		lua_rawseti(L, -2, ++index);
	}
	return 1;
}

static int luaGetHighscoreString(lua_State* L)
{
	// getHighscoreString(skillId)
	const auto skillId = otx::lua::getNumber<uint16_t>(L, 1);
	if (skillId <= SKILL__LAST) {
		otx::lua::pushString(L, g_game.getHighscoreString(skillId));
	} else {
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetVocationList(lua_State* L)
{
	// getVocationList()
	const auto& vocations = g_vocations.getVocations();
	lua_createtable(L, vocations.size(), 0);

	int index = 0;
	for (const auto& it : vocations) {
		lua_createtable(L, 0, 2);
		otx::lua::setTableValue(L, "id", it.first);
		otx::lua::setTableValue(L, "name", it.second.name);
		lua_rawseti(L, -2, ++index);
	}
	return 1;
}

static int luaGetGroupList(lua_State* L)
{
	// getGroupList()
	const auto& groups = Groups::getInstance()->getGroups();
	lua_createtable(L, groups.size(), 0);

	int index = 0;
	for (const auto& it : groups) {
		lua_createtable(L, 0, 2);
		otx::lua::setTableValue(L, "id", it.first);
		otx::lua::setTableValue(L, "name", it.second->getName());
		lua_rawseti(L, -2, ++index);
	}
	return 1;
}

static int luaGetChannelList(lua_State* L)
{
	// getChannelList()
	const auto& channels = g_chat.getPublicChannels();
	lua_createtable(L, channels.size(), 0);

	int index = 0;
	for (ChatChannel* channel : channels) {
		lua_createtable(L, 0, 5);
		otx::lua::setTableValue(L, "id", channel->getId());
		otx::lua::setTableValue(L, "name", channel->getName());
		otx::lua::setTableValue(L, "flags", channel->getFlags());
		otx::lua::setTableValue(L, "level", channel->getLevel());
		otx::lua::setTableValue(L, "access", channel->getAccess());
		lua_rawseti(L, -2, ++index);
	}
	return 1;
}

static int luaGetTownList(lua_State* L)
{
	// getTownList()
	const auto& towns = Towns::getInstance()->getTowns();
	lua_createtable(L, towns.size(), 0);

	int index = 0;
	for (const auto& it : towns) {
		lua_createtable(L, 0, 2);
		otx::lua::setTableValue(L, "id", it.first);
		otx::lua::setTableValue(L, "name", it.second->getName());
		lua_rawseti(L, -2, ++index);
	}
	return 1;
}

static int luaGetWaypointList(lua_State* L)
{
	// getWaypointList()
	const auto& waypointsMap = g_game.getMap()->waypoints.getWaypointsMap();
	lua_createtable(L, waypointsMap.size(), 0);

	int index = 0;
	for (const auto& it : waypointsMap) {
		lua_createtable(L, 0, 2);
		otx::lua::setTableValue(L, "name", it.first);
		otx::lua::setTableValue(L, "pos", it.second->pos);
		lua_rawseti(L, -2, ++index);
	}
	return 1;
}

static int luaGetWaypointPosition(lua_State* L)
{
	// getWaypointPosition(name)
	const std::string name = otx::lua::getString(L, 1);
	if (WaypointPtr waypoint = g_game.getMap()->waypoints.getWaypointByName(name)) {
		otx::lua::pushPosition(L, waypoint->pos);
	} else {
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoWaypointAddTemporial(lua_State* L)
{
	// doWaypointAddTemporial(name, pos)
	const std::string name = otx::lua::getString(L, 1);
	const Position pos = otx::lua::getPosition(L, 2);

	g_game.getMap()->waypoints.addWaypoint(std::make_unique<Waypoint>(name, pos));
	lua_pushboolean(L, true);
	return 1;
}

static int luaGetGameState(lua_State* L)
{
	// getGameState()
	lua_pushnumber(L, g_game.getGameState());
	return 1;
}

static int luaDoSetGameState(lua_State* L)
{
	// doSetGameState(id)
	const auto id = static_cast<GameState_t>(otx::lua::getNumber<uint8_t>(L, 1));
	if (id >= GAMESTATE_FIRST && id <= GAMESTATE_LAST) {
		addDispatcherTask([id]() { g_game.setGameState(id); });
		lua_pushboolean(L, true);
	} else {
		lua_pushboolean(L, false);
	}
	return 1;
}

static int luaDoCreatureExecuteTalkAction(lua_State* L)
{
	// doCreatureExecuteTalkAction(cid, text[, ignoreAccess = false, channelId = CHANNEL_DEFAULT])
	if (Creature* creature = otx::lua::getCreature(L, 1)) {
		const std::string text = otx::lua::getString(L, 2);
		const auto ignoreAccess = otx::lua::getBoolean(L, 3, false);
		const auto channelId = otx::lua::getNumber<uint16_t>(L, 4, CHANNEL_DEFAULT);

		lua_pushboolean(L, g_talkActions.onPlayerSay(creature, channelId, text, ignoreAccess));
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoExecuteRaid(lua_State* L)
{
	// doExecuteRaid(name)
	if (Raids::getInstance()->getRunning()) {
		lua_pushboolean(L, false);
		return 1;
	}

	const std::string raidName = otx::lua::getString(L, 1);
	Raid* raid = Raids::getInstance()->getRaidByName(raidName);
	if (!raid || !raid->isLoaded()) {
		otx::lua::reportErrorEx(L, "Raid with name " + raidName + " does not exists");
		lua_pushnil(L);
		return 1;
	}

	lua_pushboolean(L, raid->startRaid());
	return 1;
}

static int luaDoReloadInfo(lua_State* L)
{
	// doReloadInfo(reloadId)
	const auto reloadId = static_cast<ReloadInfo_t>(otx::lua::getNumber<uint8_t>(L, 1));
	if (reloadId == RELOAD_GLOBAL) {
		LuaInterface* mainInterface = g_lua.getMainInterface();
		lua_pushboolean(L, mainInterface->loadFile("data/global.lua"));
		g_lua.collectGarbage();
	} else {
		lua_pushboolean(L, g_game.reloadInfo(reloadId));
	}
	return 1;
}

static int luaDoSaveServer(lua_State* L)
{
	// doSaveServer([flags = 13])
	const auto flags = otx::lua::getNumber<uint32_t>(L, 1, 13);
	g_game.saveGameState(flags);
	return 0;
}

static int luaDoSaveHouse(lua_State* L)
{
	// doSaveHouse(id/list)
	std::vector<uint32_t> houseList;
	if (otx::lua::isTable(L, 1)) {
		const int tableSize = lua_objlen(L, 1);
		for (int i = 1; i <= tableSize; ++i) {
			lua_rawgeti(L, 1, i);
			houseList.push_back(otx::lua::getNumber<uint32_t>(L, -1));
			lua_pop(L, 1);
		}
	} else {
		houseList.push_back(otx::lua::getNumber<uint32_t>(L, 1));
	}

	std::vector<House*> houses;
	for (uint32_t houseId : houseList) {
		House* house = Houses::getInstance()->getHouse(houseId);
		if (!house) {
			std::ostringstream s;
			s << "House not found, ID: " << houseId;
			otx::lua::reportErrorEx(L, s.str());
			lua_pushnil(L);
			return 1;
		}

		houses.push_back(house);
	}

	DBTransaction trans;
	if (!trans.begin()) {
		lua_pushboolean(L, false);
		return 1;
	}

	for (House* house : houses) {
		if (!IOMapSerialize::getInstance()->saveHouse(house)) {
			std::ostringstream s;
			s << "Unable to save house information, ID: " << house->getId();
			otx::lua::reportErrorEx(L, s.str());
		}

		if (!IOMapSerialize::getInstance()->saveHouseItems(house)) {
			std::ostringstream s;
			s << "Unable to save house items, ID: " << house->getId();
			otx::lua::reportErrorEx(L, s.str());
		}
	}

	lua_pushboolean(L, trans.commit());
	return 1;
}

static int luaDoCleanHouse(lua_State* L)
{
	// doCleanHouse(houseId)
	const auto houseId = otx::lua::getNumber<uint32_t>(L, 1);
	if (House* house = Houses::getInstance()->getHouse(houseId)) {
		house->clean();
		lua_pushboolean(L, true);
	} else {
		lua_pushboolean(L, false);
	}
	return 1;
}

static int luaDoCleanMap(lua_State* L)
{
	// doCleanMap()
	uint32_t count = 0;
	g_game.cleanMapEx(count);
	lua_pushnumber(L, count);
	return 1;
}

static int luaDoRefreshMap(lua_State* L)
{
	// doRefreshMap()
	UNUSED(L);
	g_game.proceduralRefresh();
	return 0;
}

static int luaDoUpdateHouseAuctions(lua_State* L)
{
	// doUpdateHouseAuctions()
	lua_pushboolean(L, g_game.getMap()->updateAuctions());
	return 1;
}

static int luaGetItemIdByName(lua_State* L)
{
	// getItemIdByName(name)
	const uint16_t itemId = Item::items.getItemIdByName(otx::lua::getString(L, 1));
	if (itemId != 0) {
		lua_pushnumber(L, itemId);
	} else {
		lua_pushnil(L);
	}
	return 1;
}

// make by feetads
static int luaisItemDoor(lua_State* L)
{
	// isItemDoor(itemId)
	const ItemType& iType = Item::items.getItemType(otx::lua::getNumber<uint16_t>(L, 1));
	if (iType.id != 0) {
		lua_pushboolean(L, iType.isDoor());
	} else {
		lua_pushboolean(L, false);
	}
	return 1;
}

static int luaGetItemInfo(lua_State* L)
{
	// getItemInfo(itemid)
	const ItemType& iType = Item::items.getItemType(otx::lua::getNumber<uint16_t>(L, 1));
	if (iType.id == 0) {
		lua_pushnil(L);
		return 1;
	}

	lua_createtable(L, 0, 88);
	otx::lua::setTableValue(L, "isDoor", iType.isDoor());
	otx::lua::setTableValue(L, "stopTime", iType.stopTime);
	otx::lua::setTableValue(L, "showCount", iType.showCount);
	otx::lua::setTableValue(L, "stackable", iType.stackable);
	otx::lua::setTableValue(L, "showDuration", iType.showDuration);
	otx::lua::setTableValue(L, "showCharges", iType.showCharges);
	otx::lua::setTableValue(L, "showAttributes", iType.showAttributes);
	otx::lua::setTableValue(L, "distRead", iType.allowDistRead);
	otx::lua::setTableValue(L, "readable", iType.canReadText);
	otx::lua::setTableValue(L, "writable", iType.canWriteText);
	otx::lua::setTableValue(L, "forceSerialize", iType.forceSerialize);
	otx::lua::setTableValue(L, "vertical", iType.isVertical);
	otx::lua::setTableValue(L, "horizontal", iType.isHorizontal);
	otx::lua::setTableValue(L, "hangable", iType.isHangable);
	otx::lua::setTableValue(L, "usable", iType.usable);
	otx::lua::setTableValue(L, "movable", iType.movable);
	otx::lua::setTableValue(L, "pickupable", iType.pickupable);
	otx::lua::setTableValue(L, "rotable", iType.rotable);
	otx::lua::setTableValue(L, "replacable", iType.replacable);
	otx::lua::setTableValue(L, "hasHeight", iType.hasHeight);
	otx::lua::setTableValue(L, "blockSolid", iType.blockSolid);
	otx::lua::setTableValue(L, "blockPickupable", iType.blockPickupable);
	otx::lua::setTableValue(L, "blockProjectile", iType.blockProjectile);
	otx::lua::setTableValue(L, "blockPathing", iType.blockPathFind);
	otx::lua::setTableValue(L, "allowPickupable", iType.allowPickupable);
	otx::lua::setTableValue(L, "alwaysOnTop", iType.alwaysOnTop);
	otx::lua::setTableValue(L, "dualWield", iType.dualWield);
	otx::lua::setTableValue(L, "specialDoor", iType.specialDoor);
	otx::lua::setTableValue(L, "closingDoor", iType.closingDoor);
	otx::lua::setTableValue(L, "magicEffect", iType.magicEffect);
	otx::lua::setTableValue(L, "fluidSource", iType.fluidSource);
	otx::lua::setTableValue(L, "weaponType", iType.weaponType);
	otx::lua::setTableValue(L, "bedPartnerDirection", iType.bedPartnerDir);
	otx::lua::setTableValue(L, "ammoAction", iType.ammoAction);
	otx::lua::setTableValue(L, "combatType", iType.combatType);
	otx::lua::setTableValue(L, "corpseType", iType.corpseType);
	otx::lua::setTableValue(L, "shootType", iType.shootType);
	otx::lua::setTableValue(L, "ammoType", iType.ammoType);
	otx::lua::setTableValue(L, "group", iType.group);
	otx::lua::setTableValue(L, "type", iType.type);
	otx::lua::setTableValue(L, "transformUseTo", iType.transformUseTo);
	otx::lua::setTableValue(L, "transformEquipTo", iType.transformEquipTo);
	otx::lua::setTableValue(L, "transformDeEquipTo", iType.transformDeEquipTo);
	otx::lua::setTableValue(L, "clientId", iType.clientId);
	otx::lua::setTableValue(L, "maxItems", iType.maxItems);
	otx::lua::setTableValue(L, "slotPosition", iType.slotPosition);
	otx::lua::setTableValue(L, "wieldPosition", iType.wieldPosition);
	otx::lua::setTableValue(L, "speed", iType.speed);
	otx::lua::setTableValue(L, "maxTextLength", iType.maxTextLength);
	otx::lua::setTableValue(L, "writeOnceItemId", iType.writeOnceItemId);
	otx::lua::setTableValue(L, "date", iType.date);
	otx::lua::setTableValue(L, "writer", iType.writer);
	otx::lua::setTableValue(L, "text", iType.text);
	otx::lua::setTableValue(L, "attack", iType.attack);
	otx::lua::setTableValue(L, "reduceskillloss", iType.reduceSkillLoss);
	otx::lua::setTableValue(L, "extraAttack", iType.extraAttack);
	otx::lua::setTableValue(L, "defense", iType.defense);
	otx::lua::setTableValue(L, "extraDefense", iType.extraDefense);
	otx::lua::setTableValue(L, "armor", iType.armor);
	otx::lua::setTableValue(L, "breakChance", iType.breakChance);
	otx::lua::setTableValue(L, "hitChance", iType.hitChance);
	otx::lua::setTableValue(L, "maxHitChance", iType.maxHitChance);
	otx::lua::setTableValue(L, "runeLevel", iType.runeLevel);
	otx::lua::setTableValue(L, "runeMagicLevel", iType.runeMagLevel);
	otx::lua::setTableValue(L, "lightLevel", iType.lightLevel);
	otx::lua::setTableValue(L, "lightColor", iType.lightColor);
	otx::lua::setTableValue(L, "decayTo", iType.decayTo);
	otx::lua::setTableValue(L, "rotateTo", iType.rotateTo);
	otx::lua::setTableValue(L, "alwaysOnTopOrder", iType.alwaysOnTopOrder);
	otx::lua::setTableValue(L, "shootRange", iType.shootRange);
	otx::lua::setTableValue(L, "charges", iType.charges);
	otx::lua::setTableValue(L, "decayTime", iType.decayTime);
	otx::lua::setTableValue(L, "attackSpeed", iType.attackSpeed);
	otx::lua::setTableValue(L, "wieldInfo", iType.wieldInfo);
	otx::lua::setTableValue(L, "minRequiredLevel", iType.minReqLevel);
	otx::lua::setTableValue(L, "minRequiredMagicLevel", iType.minReqMagicLevel);
	otx::lua::setTableValue(L, "worth", iType.worth);
	otx::lua::setTableValue(L, "levelDoor", iType.levelDoor);
	otx::lua::setTableValue(L, "name", iType.name);
	otx::lua::setTableValue(L, "plural", iType.pluralName);
	otx::lua::setTableValue(L, "article", iType.article);
	otx::lua::setTableValue(L, "description", iType.description);
	otx::lua::setTableValue(L, "runeSpellName", iType.runeSpellName);
	otx::lua::setTableValue(L, "vocationString", iType.vocationString);
	otx::lua::setTableValue(L, "weight", iType.weight);

	lua_createtable(L, CHANGE_LAST, 0);
	int index = 0;
	for (uint8_t i = CHANGE_FIRST; i <= CHANGE_LAST; ++i) {
		lua_pushboolean(L, iType.floorChange[i - 1]);
		lua_rawseti(L, -2, ++index);
	}
	lua_setfield(L, -2, "floorChange");

	lua_createtable(L, 0, 2);
	otx::lua::setTableValue(L, "female", iType.transformBed[PLAYERSEX_FEMALE]);
	otx::lua::setTableValue(L, "male", iType.transformBed[PLAYERSEX_MALE]);
	lua_setfield(L, -2, "transformBed");

	lua_createtable(L, 0, 13);
	otx::lua::setTableValue(L, "manaShield", iType.hasAbilities() ? iType.abilities->manaShield : false);
	otx::lua::setTableValue(L, "invisible", iType.hasAbilities() ? iType.abilities->invisible : false);
	otx::lua::setTableValue(L, "regeneration", iType.hasAbilities() ? iType.abilities->regeneration : false);
	otx::lua::setTableValue(L, "preventLoss", iType.hasAbilities() ? iType.abilities->preventLoss : false);
	otx::lua::setTableValue(L, "preventDrop", iType.hasAbilities() ? iType.abilities->preventDrop : false);
	otx::lua::setTableValue(L, "elementType", iType.hasAbilities() ? iType.abilities->elementType : 0);
	otx::lua::setTableValue(L, "elementDamage", iType.hasAbilities() ? iType.abilities->elementDamage : 0);
	otx::lua::setTableValue(L, "speed", iType.hasAbilities() ? iType.abilities->speed : 0);
	otx::lua::setTableValue(L, "healthGain", iType.hasAbilities() ? iType.abilities->healthGain : 0);
	otx::lua::setTableValue(L, "healthTicks", iType.hasAbilities() ? iType.abilities->healthTicks : 0);
	otx::lua::setTableValue(L, "manaGain", iType.hasAbilities() ? iType.abilities->manaGain : 0);
	otx::lua::setTableValue(L, "manaTicks", iType.hasAbilities() ? iType.abilities->manaTicks : 0);
	otx::lua::setTableValue(L, "conditionSuppressions", iType.hasAbilities() ? iType.abilities->conditionSuppressions : 0);
	// TODO: absorb, increment, reflect, skills, skillsPercent, stats, statsPercent
	lua_setfield(L, -2, "abilities");
	return 1;
}

static int luaGetItemAttribute(lua_State* L)
{
	// getItemAttribute(uid, key)
	Item* item = otx::lua::getItem(L, 1);
	if (!item) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_ITEM_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	const std::string key = otx::lua::getString(L, 2);

	boost::any value = item->getAttribute(key.data());
	if (value.type() == typeid(std::string)) {
		otx::lua::pushString(L, boost::any_cast<std::string>(value));
	} else if (value.type() == typeid(int32_t)) {
		lua_pushnumber(L, boost::any_cast<int32_t>(value));
	} else if (value.type() == typeid(float)) {
		lua_pushnumber(L, boost::any_cast<float>(value));
	} else if (value.type() == typeid(bool)) {
		lua_pushboolean(L, boost::any_cast<bool>(value));
	} else {
		lua_pushnil(L);
	}
	return 1;
}

static int luaDoItemSetAttribute(lua_State* L)
{
	// doItemSetAttribute(uid, key, value)
	Item* item = otx::lua::getItem(L, 1);
	if (!item) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_ITEM_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	const std::string key = otx::lua::getString(L, 2);

	if (key == "uid" || key == "aid") {
		if (!otx::lua::isNumber(L, 3)) {
			otx::lua::reportErrorEx(L, "Protected key must have a number value.");
			lua_pushnil(L);
			return 1;
		}

		const auto value = otx::lua::getNumber<int32_t>(L, 3);
		if (key == "uid") {
			if (value < 1000 || value > 0xFFFF) {
				otx::lua::reportErrorEx(L, "UniqueId must be in range of 1000 to 65535.");
				lua_pushnil(L);
				return 1;
			}

			item->setUniqueId(value);
		} else {
			item->setActionId(value);
		}
	}

	if (otx::lua::isNumber(L, 3)) {
		item->setAttribute(key.data(), otx::lua::getNumber<int32_t>(L, 3));
	} else if (otx::lua::isBoolean(L, 3)) {
		item->setAttribute(key.data(), otx::lua::getBoolean(L, 3));
	} else if (otx::lua::isString(L, 3)) {
		item->setAttribute(key.data(), otx::lua::getString(L, 3));
	} else {
		otx::lua::reportErrorEx(L, "Invalid attribute value.");
		lua_pushnil(L);
		return 1;
	}

	lua_pushboolean(L, true);
	return 1;
}

static int luaDoItemEraseAttribute(lua_State* L)
{
	// doItemEraseAttribute(uid, key)
	Item* item = otx::lua::getItem(L, 1);
	if (!item) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_ITEM_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	const std::string key = otx::lua::getString(L, 2);

	bool ret = true;
	if (key == "uid") {
		otx::lua::reportErrorEx(L, "Attempt to erase protected key \"uid\"");
		ret = false;
	} else if (key != "aid") {
		item->eraseAttribute(key.data());
	} else {
		item->resetActionId();
	}

	lua_pushboolean(L, ret);
	return 1;
}

static int luaDoItemSetDuration(lua_State* L)
{
	// doItemSetDuration(uid, duration)
	if (Item* item = otx::lua::getItem(L, 1)) {
		const auto duration = otx::lua::getNumber<int32_t>(L, 2);
		item->setDuration(duration * 1000);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_ITEM_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetItemDurationTime(lua_State* L)
{
	// getItemDurationTime(uid)
	if (Item* item = otx::lua::getItem(L, 1)) {
		lua_pushnumber(L, item->getDuration() / 1000);
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_ITEM_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaGetItemWeight(lua_State* L)
{
	// getItemWeight(uid[, precise = true])
	Item* item = otx::lua::getItem(L, 1);
	if (!item) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_ITEM_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	const auto precise = otx::lua::getBoolean(L, 2, true);

	double weight = item->getWeight();
	if (precise) {
		std::ostringstream ws;
		ws << std::fixed << std::setprecision(2) << weight;
		weight = atof(ws.str().c_str());
	}

	lua_pushnumber(L, weight);
	return 1;
}

static int luaGetItemParent(lua_State* L)
{
	// getItemParent(uid)
	Thing* thing = otx::lua::getThing(L, 1);
	if (!thing) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_THING_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	Cylinder* cylinder = thing->getParent();
	if (!cylinder) {
		otx::lua::pushThing(L, nullptr);
		return 1;
	}

	if (Item* container = cylinder->getItem()) {
		otx::lua::pushThing(L, container);
	} else if (Creature* creature = cylinder->getCreature()) {
		otx::lua::pushThing(L, creature);
	} else if (Tile* tile = cylinder->getTile()) {
		if (tile->ground && tile->ground != thing) {
			otx::lua::pushThing(L, tile->ground);
		} else {
			otx::lua::pushThing(L, nullptr);
		}
	} else {
		otx::lua::pushThing(L, nullptr);
	}
	return 1;
}

static int luaHasItemProperty(lua_State* L)
{
	// hasItemProperty(uid, prop)
	Item* item = otx::lua::getItem(L, 1);
	if (!item) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_ITEM_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	const auto prop = static_cast<ITEMPROPERTY>(otx::lua::getNumber<uint8_t>(L, 2));

	// Check if the item is a tile, so we can get more accurate properties
	const Tile* tile = item->getTile();
	if (tile && tile->ground == item) {
		lua_pushboolean(L, tile->hasProperty(prop));
	} else {
		lua_pushboolean(L, item->hasProperty(prop));
	}
	return 1;
}

static int luaHasMonsterRaid(lua_State* L)
{
	// hasMonsterRaid(cid)
	if (Monster* monster = otx::lua::getMonster(L, 1)) {
		lua_pushboolean(L, monster->hasRaid());
	} else {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_MONSTER_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

static int luaIsIpBanished(lua_State* L)
{
	// isIpBanished(ip[, mask = 0xFFFFFFFF])
	const auto ip = otx::lua::getNumber<uint32_t>(L, 1);
	const auto mask = otx::lua::getNumber<uint32_t>(L, 2, 0xFFFFFFFF);
	lua_pushboolean(L, IOBan::getInstance()->isIpBanished(ip, mask));
	return 1;
}

static int luaIsPlayerBanished(lua_State* L)
{
	// isPlayerBanished(name/guid, banType)
	const auto banType = static_cast<PlayerBan_t>(otx::lua::getNumber<uint8_t>(L, 1));
	if (otx::lua::isNumber(L, 1)) {
		lua_pushboolean(L, IOBan::getInstance()->isPlayerBanished(otx::lua::getNumber<uint32_t>(L, 1), banType));
	} else {
		lua_pushboolean(L, IOBan::getInstance()->isPlayerBanished(otx::lua::getString(L, 1), banType));
	}
	return 1;
}

static int luaIsAccountBanished(lua_State* L)
{
	// isAccountBanished(accountId[, playerId = 0])
	const auto accountId = otx::lua::getNumber<uint32_t>(L, 1);
	const auto playerId = otx::lua::getNumber<uint32_t>(L, 2, 0);
	lua_pushboolean(L, IOBan::getInstance()->isAccountBanished(accountId, playerId));
	return 1;
}

static int luaDoAddIpBanishment(lua_State* L)
{
	// doAddIpBanishment(ip[, mask = 0xFFFFFFFF, length = *, reason = 21, comment = "", admin = 0, statement = ""])
	const auto ip = otx::lua::getNumber<uint32_t>(L, 1);
	const auto mask = otx::lua::getNumber<uint32_t>(L, 2, 0xFFFFFFFF);
	const auto length = otx::lua::getNumber<int64_t>(L, 3, time(nullptr) + otx::config::getInteger(otx::config::IPBAN_LENGTH));
	const auto reason = otx::lua::getNumber<uint32_t>(L, 4, 21);
	const std::string comment = otx::lua::getString(L, 5);
	const auto admin = otx::lua::getNumber<uint32_t>(L, 6, 0);
	const std::string statement = otx::lua::getString(L, 7);
	lua_pushboolean(L, IOBan::getInstance()->addIpBanishment(ip, length, reason, comment, admin, mask, statement));
	return 1;
}

static int luaDoAddPlayerBanishment(lua_State* L)
{
	// doAddPlayerBanishment(name/guid[, banType = PLAYERBAN_LOCK,
	//                       length = -1, reason = 21, action = ACTION_NAMELOCK, comment = "", admin = 0, statement = ""])
	const auto banType = static_cast<PlayerBan_t>(otx::lua::getNumber<uint8_t>(L, 2, PLAYERBAN_LOCK));
	const auto length = otx::lua::getNumber<int64_t>(L, 3, -1);
	const auto reason = otx::lua::getNumber<uint32_t>(L, 4, 21);
	const auto action = static_cast<ViolationAction_t>(otx::lua::getNumber<uint8_t>(L, 5, ACTION_NAMELOCK));
	const std::string comment = otx::lua::getString(L, 6);
	const auto admin = otx::lua::getNumber<uint32_t>(L, 7, 0);
	const std::string statement = otx::lua::getString(L, 8);
	if (otx::lua::isNumber(L, 1)) {
		lua_pushboolean(L, IOBan::getInstance()->addPlayerBanishment(otx::lua::getNumber<uint32_t>(L, 1), length, reason, action, comment, admin, banType, statement));
	} else {
		lua_pushboolean(L, IOBan::getInstance()->addPlayerBanishment(otx::lua::getString(L, 1), length, reason, action, comment, admin, banType, statement));
	}
	return 1;
}

static int luaDoAddAccountBanishment(lua_State* L)
{
	// doAddAccountBanishment(accountId[, playerId = 0, length = *,
	//                        reason = 21, action = ACTION_BANISHMENT, comment = "", admin = 0, statement = ""])
	const auto accountId = otx::lua::getNumber<uint32_t>(L, 1);
	const auto playerId = otx::lua::getNumber<uint32_t>(L, 2, 0);
	const auto length = otx::lua::getNumber<int64_t>(L, 3, time(nullptr) + otx::config::getInteger(otx::config::BAN_LENGTH));
	const auto reason = otx::lua::getNumber<uint32_t>(L, 4, 21);
	const auto action = static_cast<ViolationAction_t>(otx::lua::getNumber<uint8_t>(L, 5, ACTION_BANISHMENT));
	const std::string comment = otx::lua::getString(L, 6);
	const auto admin = otx::lua::getNumber<uint32_t>(L, 7, 0);
	const std::string statement = otx::lua::getString(L, 8);
	lua_pushboolean(L, IOBan::getInstance()->addAccountBanishment(accountId, length, reason, action, comment, admin, playerId, statement));
	return 1;
}

static int luaDoAddAccountWarnings(lua_State* L)
{
	// doAddAccountWarnings(accountId[, warnings = 1])
	const auto accountId = otx::lua::getNumber<uint32_t>(L, 1);
	const auto warnings = otx::lua::getNumber<int32_t>(L, 2, 1);

	Account account = IOLoginData::getInstance()->loadAccount(accountId, true);
	account.warnings = std::clamp<int32_t>(account.warnings + warnings, 0, 0xFFFF);

	IOLoginData::getInstance()->saveAccount(account);
	lua_pushboolean(L, true);
	return 1;
}

static int luaDoAddNotation(lua_State* L)
{
	// doAddNotation(accountId[, playerId = 0, reason = 21, comment = "", admin = 0, statement = ""])
	const auto accountId = otx::lua::getNumber<uint32_t>(L, 1);
	const auto playerId = otx::lua::getNumber<uint32_t>(L, 2, 0);
	const auto reason = otx::lua::getNumber<uint32_t>(L, 3, 21);
	const std::string comment = otx::lua::getString(L, 4);
	const auto admin = otx::lua::getNumber<uint32_t>(L, 5, 0);
	const std::string statement = otx::lua::getString(L, 6);
	lua_pushboolean(L, IOBan::getInstance()->addNotation(accountId, reason, comment, admin, playerId, statement));
	return 1;
}

static int luaDoAddStatement(lua_State* L)
{
	// doAddStatement(name/guid[, channelId = -1, reason = 21, comment = "", admin = 0, statement = ""])
	const auto channelId = otx::lua::getNumber<int16_t>(L, 2, -1);
	const auto reason = otx::lua::getNumber<uint32_t>(L, 3, 21);
	const std::string comment = otx::lua::getString(L, 4);
	const auto admin = otx::lua::getNumber<uint32_t>(L, 5, 0);
	const std::string statement = otx::lua::getString(L, 6);

	if (otx::lua::isNumber(L, 1)) {
		lua_pushboolean(L, IOBan::getInstance()->addStatement(otx::lua::getNumber<uint32_t>(L, 1), reason, comment, admin, channelId, statement));
	} else {
		lua_pushboolean(L, IOBan::getInstance()->addStatement(otx::lua::getString(L, 1), reason, comment, admin, channelId, statement));
	}
	return 1;
}

static int luaDoRemoveIpBanishment(lua_State* L)
{
	// doRemoveIpBanishment(ip[, mask = 0xFFFFFFFF])
	const auto ip = otx::lua::getNumber<uint32_t>(L, 1);
	const auto mask = otx::lua::getNumber<uint32_t>(L, 2, 0xFFFFFFFF);
	lua_pushboolean(L, IOBan::getInstance()->removeIpBanishment(ip, mask));
	return 1;
}

static int luaDoRemovePlayerBanishment(lua_State* L)
{
	// doRemovePlayerBanishment(name/guid, banType)
	const auto banType = static_cast<PlayerBan_t>(otx::lua::getNumber<uint8_t>(L, 2));
	if (otx::lua::isNumber(L, 1)) {
		lua_pushboolean(L, IOBan::getInstance()->removePlayerBanishment(otx::lua::getNumber<uint32_t>(L, 1), banType));
	} else {
		lua_pushboolean(L, IOBan::getInstance()->removePlayerBanishment(otx::lua::getString(L, 1), banType));
	}
	return 1;
}

static int luaDoRemoveAccountBanishment(lua_State* L)
{
	// doRemoveAccountBanishment(accountId[, playerId = 0])
	const auto accountId = otx::lua::getNumber<uint32_t>(L, 1);
	const auto playerId = otx::lua::getNumber<uint32_t>(L, 2, 0);
	lua_pushboolean(L, IOBan::getInstance()->removeAccountBanishment(accountId, playerId));
	return 1;
}

static int luaDoRemoveNotations(lua_State* L)
{
	// doRemoveNotations(accountId[, playerId = 0])
	const auto accountId = otx::lua::getNumber<uint32_t>(L, 1);
	const auto playerId = otx::lua::getNumber<uint32_t>(L, 2, 0);
	lua_pushboolean(L, IOBan::getInstance()->removeNotations(accountId, playerId));
	return 1;
}

static int luaGetAccountWarnings(lua_State* L)
{
	// getAccountWarnings(accountId)
	Account account = IOLoginData::getInstance()->loadAccount(otx::lua::getNumber<uint32_t>(L, 1));
	lua_pushnumber(L, account.warnings);
	return 1;
}

static int luaGetNotationsCount(lua_State* L)
{
	// getNotationsCount(accountId[, playerId = 0])
	const auto accountId = otx::lua::getNumber<uint32_t>(L, 1);
	const auto playerId = otx::lua::getNumber<uint32_t>(L, 2, 0);
	lua_pushnumber(L, IOBan::getInstance()->getNotationsCount(accountId, playerId));
	return 1;
}

static int luaGetBanData(lua_State* L)
{
	// getBanData(value[, banType = BAN_NONE, param = 0])
	const auto value = otx::lua::getNumber<uint32_t>(L, 1);
	const auto banType = static_cast<Ban_t>(otx::lua::getNumber<uint8_t>(L, 2, BAN_NONE));
	const auto param = otx::lua::getNumber<uint32_t>(L, 3, 0);

	Ban tmp;
	tmp.value = value;
	tmp.type = banType;
	tmp.param = param;

	if (!IOBan::getInstance()->getData(tmp)) {
		lua_pushnil(L);
		return 1;
	}

	lua_createtable(L, 0, 8);
	otx::lua::setTableValue(L, "id", tmp.id);
	otx::lua::setTableValue(L, "type", tmp.type);
	otx::lua::setTableValue(L, "value", tmp.value);
	otx::lua::setTableValue(L, "param", tmp.param);
	otx::lua::setTableValue(L, "added", tmp.added);
	otx::lua::setTableValue(L, "expires", tmp.expires);
	otx::lua::setTableValue(L, "adminId", tmp.adminId);
	otx::lua::setTableValue(L, "comment", tmp.comment);
	return 1;
}

static int luaGetBanList(lua_State* L)
{
	// getBanList(banType[, value = 0, param = 0])
	const auto banType = static_cast<Ban_t>(otx::lua::getNumber<uint8_t>(L, 1));
	const auto value = otx::lua::getNumber<uint32_t>(L, 2, 0);
	const auto param = otx::lua::getNumber<uint32_t>(L, 3, 0);

	const auto bans = IOBan::getInstance()->getList(banType, value, param);
	lua_createtable(L, bans.size(), 0);

	int index = 0;
	for (const Ban& ban : bans) {
		lua_createtable(L, 0, 8);
		otx::lua::setTableValue(L, "id", ban.id);
		otx::lua::setTableValue(L, "type", ban.type);
		otx::lua::setTableValue(L, "value", ban.value);
		otx::lua::setTableValue(L, "param", ban.param);
		otx::lua::setTableValue(L, "added", ban.added);
		otx::lua::setTableValue(L, "expires", ban.expires);
		otx::lua::setTableValue(L, "adminId", ban.adminId);
		otx::lua::setTableValue(L, "comment", ban.comment);
		lua_rawseti(L, -2, ++index);
	}
	return 1;
}

static int luaGetExperienceStage(lua_State* L)
{
	// getExperienceStage(level[, divider = 1.0])
	const auto level = otx::lua::getNumber<uint32_t>(L, 1);
	const auto divider = otx::lua::getNumber<double>(L, 2, 1.0);
	lua_pushnumber(L, g_game.getExperienceStage(level, divider));
	return 1;
}

static int luaGetDataDir(lua_State* L)
{
	// getDataDir()
	otx::lua::pushString(L, getFilePath(FILE_TYPE_OTHER, ""));
	return 1;
}

static int luaGetLogsDir(lua_State* L)
{
	// getLogsDir()
	otx::lua::pushString(L, getFilePath(FILE_TYPE_LOG, ""));
	return 1;
}

static int luaGetConfigFile(lua_State* L)
{
	// getConfigFile()
	otx::lua::pushString(L, otx::config::getString(otx::config::CONFIG_FILE));
	return 1;
}

static int luaDoPlayerSetWalkthrough(lua_State* L)
{
	// doPlayerSetWalkthrough(cid, uid, walkthrough)
	Player* player = otx::lua::getPlayer(L, 1);
	if (!player) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	Creature* creature = otx::lua::getCreature(L, 2);
	if (!creature) {
		otx::lua::reportErrorEx(L, otx::lua::getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	const bool walkthrough = otx::lua::getBoolean(L, 3);
	if (player != creature) {
		player->setWalkthrough(creature, walkthrough);
		lua_pushboolean(L, true);
	} else {
		lua_pushboolean(L, false);
	}
	return 1;
}

static int luaDoGuildAddEnemy(lua_State* L)
{
	// doGuildAddEnemy(guild, enemy, warId, warType)
	const auto guild = otx::lua::getNumber<uint32_t>(L, 1);
	const auto enemy = otx::lua::getNumber<uint32_t>(L, 2);
	const auto warId = otx::lua::getNumber<uint32_t>(L, 3);
	const auto warType = static_cast<WarType_t>(otx::lua::getNumber<uint8_t>(L, 4));

	War_t war;
	war.war = warId;
	war.type = warType;

	uint32_t count = 0;
	for (const auto& it : g_game.getPlayers()) {
		if (it.second->getGuildId() != guild) {
			continue;
		}

		++count;
		it.second->addEnemy(enemy, war);
		g_game.updateCreatureEmblem(it.second);
	}

	lua_pushnumber(L, count);
	return 1;
}

static int luaDoGuildRemoveEnemy(lua_State* L)
{
	// doGuildRemoveEnemy(guild, enemy)
	const auto guild = otx::lua::getNumber<uint32_t>(L, 1);
	const auto enemy = otx::lua::getNumber<uint32_t>(L, 2);

	uint32_t count = 0;
	for (const auto& it : g_game.getPlayers()) {
		if (it.second->isRemoved() || it.second->getGuildId() != guild) {
			continue;
		}

		++count;
		it.second->removeEnemy(enemy);
		g_game.updateCreatureEmblem(it.second);
	}

	lua_pushnumber(L, count);
	return 1;
}

static int luaGetConfigValue(lua_State* L)
{
	// getConfigValue(key)
	const std::string name = otx::lua::getString(L, 1);
	lua_getglobal(L, "g_config");
	if (otx::lua::isTable(L, -1)) {
		lua_getfield(L, -1, name.data());
	} else {
		lua_pushnil(L);
	}

	lua_remove(L, -2);
	return 1;
}

static int luaL_dodirectory(lua_State* L)
{
	// dodirectory(dir[, recursively = false, loadSystems = true])
	const std::string dir = otx::lua::getString(L, 1);
	const auto recursively = otx::lua::getBoolean(L, 2, false);
	const auto loadSystems = otx::lua::getBoolean(L, 3, true);

	if (!otx::lua::getScriptEnv().getInterface()->loadDirectory(dir, recursively, loadSystems, nullptr)) {
		otx::lua::reportErrorEx(L, "Failed to load directory " + dir);
		lua_pushboolean(L, false);
	} else {
		lua_pushboolean(L, true);
	}
	return 1;
}

static int luaErrors(lua_State* L)
{
	// errors(enabled)
	bool prev = g_lua.isErrorsEnabled();
	g_lua.setErrorsEnabled(otx::lua::getBoolean(L, 1));
	lua_pushboolean(L, prev);
	return 1;
}

//
// std
//
static int luaStdCout(lua_State* L)
{
	// std.cout(...)
	const int nparams = lua_gettop(L);
	for (int i = 1; i <= nparams; ++i) {
		std::cout << (i > 1 ? '\t' : '\0') << otx::lua::getString(L, i);
	}

	std::cout << std::flush;
	lua_pushnumber(L, nparams);
	return 1;
}

static int luaStdClog(lua_State* L)
{
	// std.clog(...)
	const int nparams = lua_gettop(L);
	for (int i = 1; i <= nparams; ++i) {
		std::clog << (i > 1 ? '\t' : '\0') << otx::lua::getString(L, i);
	}

	std::clog << std::flush;
	lua_pushnumber(L, nparams);
	return 1;
}

static int luaStdCerr(lua_State* L)
{
	// std.cerr(...)
	const int nparams = lua_gettop(L);
	for (int i = 1; i <= nparams; ++i) {
		std::cerr << (i > 1 ? '\t' : '\0') << otx::lua::getString(L, i);
	}

	std::cerr << std::flush;
	lua_pushnumber(L, nparams);
	return 1;
}

static int luaStdSHA1(lua_State* L)
{
	// std.sha1(string)
	otx::lua::pushString(L, transformToSHA1(otx::lua::getString(L, 1)));
	return 1;
}

static int luaStdCheckName(lua_State* L)
{
	// std.checkName(name[, forceUppercaseOnFirstLetter = true])
	const std::string name = otx::lua::getString(L, 1);
	const auto forceUppercaseOnFirstLetter = otx::lua::getBoolean(L, 2, true);
	lua_pushboolean(L, isValidName(name, forceUppercaseOnFirstLetter));
	return 1;
}

//
// os
//
static int luaSystemTime(lua_State* L)
{
	// os.mtime()
	lua_pushnumber(L, otx::util::mstime());
	return 1;
}

//
// db
//
static int luaDatabaseExecute(lua_State* L)
{
	// db.query(query)
	lua_pushboolean(L, g_database.executeQuery(otx::lua::getString(L, 1)));
	return 1;
}

static int luaDatabaseStoreQuery(lua_State* L)
{
	// db.storeQuery(query)
	if (DBResultPtr result = g_database.storeQuery(otx::lua::getString(L, 1))) {
		lua_pushnumber(L, addDBResult(result));
	} else {
		lua_pushnil(L);
	}
	return 1;
}

static int luaDatabaseEscapeString(lua_State* L)
{
	// db.escapeString(str)
	otx::lua::pushString(L, g_database.escapeString(otx::lua::getString(L, 1)));
	return 1;
}

static int luaDatabaseEscapeBlob(lua_State* L)
{
	// db.escapeBlob(s, length)
	const std::string s = otx::lua::getString(L, 1);
	const auto length = otx::lua::getNumber<uint32_t>(L, 2);
	otx::lua::pushString(L, g_database.escapeBlob(s.data(), length));
	return 1;
}

static int luaDatabaseLastInsertId(lua_State* L)
{
	// db.lastInsertId()
	lua_pushnumber(L, g_database.getLastInsertId());
	return 1;
}

static int luaDatabaseTableExists(lua_State* L)
{
	// db.tableExists(table)
	lua_pushboolean(L, DatabaseManager::getInstance()->tableExists(otx::lua::getString(L, 1)));
	return 1;
}

static int luaDatabaseTransBegin(lua_State* L)
{
	// db.transBegin()
	lua_pushboolean(L, g_database.beginTransaction());
	return 1;
}

static int luaDatabaseTransRollback(lua_State* L)
{
	// db.transRollback()
	lua_pushboolean(L, g_database.rollback());
	return 1;
}

static int luaDatabaseTransCommit(lua_State* L)
{
	// db.transCommit()
	lua_pushboolean(L, g_database.commit());
	return 1;
}

//
// result
//
static int luaResultGetNumber(lua_State* L)
{
	// result.getNumber(res, s)
	DBResultPtr result = getDBResultById(otx::lua::getNumber<uint32_t>(L, 1));
	if (!result) {
		lua_pushnil(L);
		return 1;
	}

	const std::string s = otx::lua::getString(L, 2);
	lua_pushnumber(L, result->getNumber<int64_t>(s));
	return 1;
}

static int luaResultGetString(lua_State* L)
{
	// result.getString(res, s)
	DBResultPtr result = getDBResultById(otx::lua::getNumber<uint32_t>(L, 1));
	if (!result) {
		lua_pushnil(L);
		return 1;
	}

	const std::string s = otx::lua::getString(L, 2);
	otx::lua::pushString(L, result->getString(s));
	return 1;
}

static int luaResultGetStream(lua_State* L)
{
	// result.getStream(res, s)
	DBResultPtr result = getDBResultById(otx::lua::getNumber<uint32_t>(L, 1));
	if (!result) {
		lua_pushnil(L);
		return 1;
	}

	const std::string s = otx::lua::getString(L, 2);

	unsigned long length = 0;
	otx::lua::pushString(L, result->getStream(s, length));
	lua_pushnumber(L, length);
	return 2;
}

static int luaResultNext(lua_State* L)
{
	// result.next(res)
	DBResultPtr result = getDBResultById(otx::lua::getNumber<uint32_t>(L, 1));
	lua_pushboolean(L, result && result->next());
	return 1;
}

static int luaResultFree(lua_State* L)
{
	// result.free(res)
	lua_pushboolean(L, removeDBResultById(otx::lua::getNumber<uint32_t>(L, 1)));
	return 1;
}

//
// bit
//
static int luaBitNot(lua_State* L)
{
	// bit.bnot(value)
	const auto value = ~otx::lua::getNumber<int32_t>(L, 1);
	lua_pushnumber(L, value);
	return 1;
}

static int luaBitUNot(lua_State* L)
{
	// bit.ubnot(value)
	const auto value = ~otx::lua::getNumber<uint32_t>(L, 1);
	lua_pushnumber(L, value);
	return 1;
}

static int luaBitAnd(lua_State* L)
{
	// bit.band(...)
	auto value = otx::lua::getNumber<int32_t>(L, 1);
	const int nparams = lua_gettop(L);
	for (int i = 2; i <= nparams; ++i) {
		value &= otx::lua::getNumber<int32_t>(L, i);
	}

	lua_pushnumber(L, value);
	return 1;
}

static int luaBitOr(lua_State* L)
{
	// bit.bor(...)
	auto value = otx::lua::getNumber<int32_t>(L, 1);
	const int nparams = lua_gettop(L);
	for (int i = 2; i <= nparams; ++i) {
		value |= otx::lua::getNumber<int32_t>(L, i);
	}

	lua_pushnumber(L, value);
	return 1;
}

static int luaBitXor(lua_State* L)
{
	// bit.bxor(...)
	auto value = otx::lua::getNumber<int32_t>(L, 1);
	const int nparams = lua_gettop(L);
	for (int i = 2; i <= nparams; ++i) {
		value ^= otx::lua::getNumber<int32_t>(L, i);
	}

	lua_pushnumber(L, value);
	return 1;
}

static int luaBitUAnd(lua_State* L)
{
	// bit.uband(...)
	auto value = otx::lua::getNumber<uint32_t>(L, 1);
	const int nparams = lua_gettop(L);
	for (int i = 2; i <= nparams; ++i) {
		value &= otx::lua::getNumber<uint32_t>(L, i);
	}

	lua_pushnumber(L, value);
	return 1;
}

static int luaBitUOr(lua_State* L)
{
	// bit.ubor(...)
	auto value = otx::lua::getNumber<uint32_t>(L, 1);
	const int nparams = lua_gettop(L);
	for (int i = 2; i <= nparams; ++i) {
		value |= otx::lua::getNumber<uint32_t>(L, i);
	}

	lua_pushnumber(L, value);
	return 1;
}

static int luaBitUXor(lua_State* L)
{
	// bit.ubxor(...)
	auto value = otx::lua::getNumber<uint32_t>(L, 1);
	const int nparams = lua_gettop(L);
	for (int i = 2; i <= nparams; ++i) {
		value ^= otx::lua::getNumber<uint32_t>(L, i);
	}

	lua_pushnumber(L, value);
	return 1;
}

static int luaBitLeftShift(lua_State* L)
{
	// bit.lshift(v1, v2)
	const auto value = otx::lua::getNumber<int32_t>(L, 1) << otx::lua::getNumber<int32_t>(L, 2);
	lua_pushnumber(L, value);
	return 1;
}

static int luaBitRightShift(lua_State* L)
{
	// bit.rshift(v1, v2)
	const auto value = otx::lua::getNumber<int32_t>(L, 1) >> otx::lua::getNumber<int32_t>(L, 2);
	lua_pushnumber(L, value);
	return 1;
}

static int luaBitULeftShift(lua_State* L)
{
	// bit.ulshift(v1, v2)
	const auto value = otx::lua::getNumber<uint32_t>(L, 1) << otx::lua::getNumber<uint32_t>(L, 2);
	lua_pushnumber(L, value);
	return 1;
}

static int luaBitURightShift(lua_State* L)
{
	// bit.urshift(v1, v2)
	const auto value = otx::lua::getNumber<uint32_t>(L, 1) >> otx::lua::getNumber<uint32_t>(L, 2);
	lua_pushnumber(L, value);
	return 1;
}

#define registerEnum(value)                                                             \
	{                                                                                   \
		std::string enumName = #value;                                                  \
		registerGlobalVariable(enumName.substr(enumName.find_last_of(':') + 1), value); \
	}

const luaL_Reg luaSystemTable[] = {
	// os.mtime()
	{ "mtime", luaSystemTime },

	{ nullptr, nullptr }
};

const luaL_Reg luaDatabaseTable[] = {
	{ "query", luaDatabaseExecute },
	{ "storeQuery", luaDatabaseStoreQuery },
	{ "escapeString", luaDatabaseEscapeString },
	{ "escapeBlob", luaDatabaseEscapeBlob },
	{ "lastInsertId", luaDatabaseLastInsertId },
	{ "tableExists", luaDatabaseTableExists },
	{ "transBegin", luaDatabaseTransBegin },
	{ "transRollback", luaDatabaseTransRollback },
	{ "transCommit", luaDatabaseTransCommit },

	{ nullptr, nullptr }
};

const luaL_Reg luaResultTable[] = {
	{ "getNumber", luaResultGetNumber },
	{ "getString", luaResultGetString },
	{ "getStream", luaResultGetStream },
	{ "next", luaResultNext },
	{ "free", luaResultFree },

	// backwards-compatibility
	{ "getDataLong", luaResultGetNumber },
	{ "getDataInt", luaResultGetNumber },
	{ "getDataString", luaResultGetString },
	{ "getDataStream", luaResultGetStream },

	{ nullptr, nullptr }
};

const luaL_Reg luaBitTable[] = {
	{ "bnot", luaBitNot },
	{ "band", luaBitAnd },
	{ "bor", luaBitOr },
	{ "bxor", luaBitXor },
	{ "lshift", luaBitLeftShift },
	{ "rshift", luaBitRightShift },

	{ "ubnot", luaBitUNot },
	{ "uband", luaBitUAnd },
	{ "ubor", luaBitUOr },
	{ "ubxor", luaBitUXor },
	{ "ulshift", luaBitULeftShift },
	{ "urshift", luaBitURightShift },

	{ nullptr, nullptr }
};

const luaL_Reg luaStdTable[] = {
	{ "cout", luaStdCout },
	{ "clog", luaStdClog },
	{ "cerr", luaStdCerr },
	{ "sha1", luaStdSHA1 },
	{ "checkName", luaStdCheckName },
	{ nullptr, nullptr }
};

void otx::lua::registerFunctions()
{
	lua_State* L = g_lua.getLuaState();

	// Game
	lua_register(L, "existMonsterByName", luaExistMonsterByName);
	lua_register(L, "getInstantSpellInfo", luaGetInstantSpellInfo);
	lua_register(L, "getStorageList", luaGetStorageList);
	lua_register(L, "getStorage", luaGetStorage);
	lua_register(L, "doSetStorage", luaDoSetStorage);
	lua_register(L, "getChannelUsers", luaGetChannelUsers);
	lua_register(L, "getPlayersOnline", luaGetPlayersOnline);
	lua_register(L, "getTileInfo", luaGetTileInfo);
	lua_register(L, "getThingFromPosition", luaGetThingFromPosition);
	lua_register(L, "doTileQueryAdd", luaDoTileQueryAdd);
	lua_register(L, "getTileItemById", luaGetTileItemById);
	lua_register(L, "getTileItemByType", luaGetTileItemByType);
	lua_register(L, "getTileThingByPos", luaGetTileThingByPos);
	lua_register(L, "getTopCreature", luaGetTopCreature);
	lua_register(L, "getSearchString", luaGetSearchString);
	lua_register(L, "doSendMagicEffect", luaDoSendMagicEffect);
	lua_register(L, "doSendDistanceShoot", luaDoSendDistanceShoot);
	lua_register(L, "doSendAnimatedText", luaDoSendAnimatedText);
	lua_register(L, "getCreatureByName", luaGetCreatureByName);
	lua_register(L, "doCreateItem", luaDoCreateItem);
	lua_register(L, "doCreateItemEx", luaDoCreateItemEx);
	lua_register(L, "doTileAddItemEx", luaDoTileAddItemEx);
	lua_register(L, "doRelocate", luaDoRelocate);
	lua_register(L, "doCleanTile", luaDoCleanTile);
	lua_register(L, "doCreateTeleport", luaDoCreateTeleport);
	lua_register(L, "doCreateMonster", luaDoCreateMonster);
	lua_register(L, "doCreateNpc", luaDoCreateNpc);
	lua_register(L, "getMonsterInfo", luaGetMonsterInfo);
	lua_register(L, "getOutfitIdByLooktype", luaGetOutfitIdByLooktype);
	lua_register(L, "getHouseInfo", luaGetHouseInfo);
	lua_register(L, "getHouseAccessList", luaGetHouseAccessList);
	lua_register(L, "getHouseByPlayerGUID", luaGetHouseByPlayerGUID);
	lua_register(L, "getHouseFromPosition", luaGetHouseFromPosition);
	lua_register(L, "setHouseAccessList", luaSetHouseAccessList);
	lua_register(L, "setHouseOwner", luaSetHouseOwner);
	lua_register(L, "getWorldType", luaGetWorldType);
	lua_register(L, "setWorldType", luaSetWorldType);
	lua_register(L, "getWorldTime", luaGetWorldTime);
	lua_register(L, "getWorldLight", luaGetWorldLight);
	lua_register(L, "getWorldCreatures", luaGetWorldCreatures);
	lua_register(L, "getWorldUpTime", luaGetWorldUpTime);
	lua_register(L, "getGuildId", luaGetGuildId);
	lua_register(L, "getGuildMotd", luaGetGuildMotd);
	lua_register(L, "doCombatAreaHealth", luaDoCombatAreaHealth);
	lua_register(L, "doTargetCombatHealth", luaDoTargetCombatHealth);
	lua_register(L, "doCombatAreaMana", luaDoCombatAreaMana);
	lua_register(L, "doTargetCombatMana", luaDoTargetCombatMana);
	lua_register(L, "doCombatAreaCondition", luaDoCombatAreaCondition);
	lua_register(L, "doTargetCombatCondition", luaDoTargetCombatCondition);
	lua_register(L, "doCombatAreaDispel", luaDoCombatAreaDispel);
	lua_register(L, "doTargetCombatDispel", luaDoTargetCombatDispel);
	lua_register(L, "isSightClear", luaIsSightClear);
	lua_register(L, "addEvent", luaAddEvent);
	lua_register(L, "stopEvent", luaStopEvent);
	lua_register(L, "getAccountIdByName", luaGetAccountIdByName);
	lua_register(L, "getAccountByName", luaGetAccountByName);
	lua_register(L, "getAccountIdByAccount", luaGetAccountIdByAccount);
	lua_register(L, "getAccountByAccountId", luaGetAccountByAccountId);
	lua_register(L, "getAccountFlagValue", luaGetAccountFlagValue);
	lua_register(L, "getAccountCustomFlagValue", luaGetAccountCustomFlagValue);
	lua_register(L, "getIpByName", luaGetIpByName);
	lua_register(L, "getPlayersByIp", luaGetPlayersByIp);
	lua_register(L, "getTownId", luaGetTownId);
	lua_register(L, "getTownName", luaGetTownName);
	lua_register(L, "getTownTemplePosition", luaGetTownTemplePosition);
	lua_register(L, "getTownHouses", luaGetTownHouses);
	lua_register(L, "getSpectators", luaGetSpectators);
	lua_register(L, "getVocationInfo", luaGetVocationInfo);
	lua_register(L, "getGroupInfo", luaGetGroupInfo);
	lua_register(L, "getVocationList", luaGetVocationList);
	lua_register(L, "getGroupList", luaGetGroupList);
	lua_register(L, "getChannelList", luaGetChannelList);
	lua_register(L, "getTownList", luaGetTownList);
	lua_register(L, "getWaypointList", luaGetWaypointList);
	lua_register(L, "getTalkActionList", luaGetTalkActionList);
	lua_register(L, "getExperienceStageList", luaGetExperienceStageList);
	lua_register(L, "getItemIdByName", luaGetItemIdByName);
	lua_register(L, "getItemInfo", luaGetItemInfo);
	lua_register(L, "isIpBanished", luaIsIpBanished);
	lua_register(L, "isPlayerBanished", luaIsPlayerBanished);
	lua_register(L, "isAccountBanished", luaIsAccountBanished);
	lua_register(L, "doAddIpBanishment", luaDoAddIpBanishment);
	lua_register(L, "doAddPlayerBanishment", luaDoAddPlayerBanishment);
	lua_register(L, "doAddAccountBanishment", luaDoAddAccountBanishment);
	lua_register(L, "doAddAccountWarnings", luaDoAddAccountWarnings);
	lua_register(L, "getAccountWarnings", luaGetAccountWarnings);
	lua_register(L, "doAddNotation", luaDoAddNotation);
	lua_register(L, "doAddStatement", luaDoAddStatement);
	lua_register(L, "doRemoveIpBanishment", luaDoRemoveIpBanishment);
	lua_register(L, "doRemovePlayerBanishment", luaDoRemovePlayerBanishment);
	lua_register(L, "doRemoveAccountBanishment", luaDoRemoveAccountBanishment);
	lua_register(L, "doRemoveNotations", luaDoRemoveNotations);
	lua_register(L, "getNotationsCount", luaGetNotationsCount);
	lua_register(L, "getBanData", luaGetBanData);
	lua_register(L, "getBanList", luaGetBanList);
	lua_register(L, "getExperienceStage", luaGetExperienceStage);
	lua_register(L, "getDataDir", luaGetDataDir);
	lua_register(L, "getLogsDir", luaGetLogsDir);
	lua_register(L, "getConfigFile", luaGetConfigFile);
	lua_register(L, "getConfigValue", luaGetConfigValue);
	lua_register(L, "getHighscoreString", luaGetHighscoreString);
	lua_register(L, "getWaypointPosition", luaGetWaypointPosition);
	lua_register(L, "doWaypointAddTemporial", luaDoWaypointAddTemporial);
	lua_register(L, "getGameState", luaGetGameState);
	lua_register(L, "doSetGameState", luaDoSetGameState);
	lua_register(L, "doExecuteRaid", luaDoExecuteRaid);
	lua_register(L, "doReloadInfo", luaDoReloadInfo);
	lua_register(L, "doSaveServer", luaDoSaveServer);
	lua_register(L, "doSaveHouse", luaDoSaveHouse);
	lua_register(L, "doCleanHouse", luaDoCleanHouse);
	lua_register(L, "doCleanMap", luaDoCleanMap);
	lua_register(L, "doRefreshMap", luaDoRefreshMap);
	lua_register(L, "doGuildAddEnemy", luaDoGuildAddEnemy);
	lua_register(L, "doGuildRemoveEnemy", luaDoGuildRemoveEnemy);
	lua_register(L, "doUpdateHouseAuctions", luaDoUpdateHouseAuctions);
	lua_register(L, "dodirectory", luaL_dodirectory);
	lua_register(L, "errors", luaErrors);

	// Creatures
	lua_register(L, "getClosestFreeTile", luaGetClosestFreeTile);
	lua_register(L, "getCreatureHealth", luaGetCreatureHealth);
	lua_register(L, "getCreatureMaxHealth", luaGetCreatureMaxHealth);
	lua_register(L, "getCreatureMana", luaGetCreatureMana);
	lua_register(L, "getCreatureMaxMana", luaGetCreatureMaxMana);
	lua_register(L, "getCreatureHideHealth", luaGetCreatureHideHealth);
	lua_register(L, "doCreatureSetHideHealth", luaDoCreatureSetHideHealth);
	lua_register(L, "getCreatureSpeakType", luaGetCreatureSpeakType);
	lua_register(L, "doCreatureSetSpeakType", luaDoCreatureSetSpeakType);
	lua_register(L, "getCreatureLookDirection", luaGetCreatureLookDirection);
	lua_register(L, "doCreatureCastSpell", luaDoCreatureCastSpell);
	lua_register(L, "getCreatureStorageList", luaGetCreatureStorageList);
	lua_register(L, "getCreatureStorage", luaGetCreatureStorage);
	lua_register(L, "doCreatureSetStorage", luaDoCreatureSetStorage);
	lua_register(L, "doCreatureSay", luaDoCreatureSay);
	lua_register(L, "setCreatureMaxMana", luaSetCreatureMaxMana);
	lua_register(L, "doCreatureChannelSay", luaDoCreatureChannelSay);
	lua_register(L, "doSummonMonster", luaDoSummonMonster);
	lua_register(L, "doSendCreatureSquare", luaDoSendCreatureSquare);
	lua_register(L, "doCreatureAddHealth", luaDoCreatureAddHealth);
	lua_register(L, "doCreatureAddMana", luaDoCreatureAddMana);
	lua_register(L, "setCreatureMaxHealth", luaSetCreatureMaxHealth);
	lua_register(L, "doConvinceCreature", luaDoConvinceCreature);
	lua_register(L, "doAddCondition", luaDoAddCondition);
	lua_register(L, "doRemoveCondition", luaDoRemoveCondition);
	lua_register(L, "doRemoveConditions", luaDoRemoveConditions);
	lua_register(L, "doRemoveCreature", luaDoRemoveCreature);
	lua_register(L, "doMoveCreature", luaDoMoveCreature);
	lua_register(L, "doSteerCreature", luaDoSteerCreature);
	lua_register(L, "hasCreatureCondition", luaHasCreatureCondition);
	lua_register(L, "getCreatureConditionInfo", luaGetCreatureConditionInfo);
	lua_register(L, "doCreatureSetDropLoot", luaDoCreatureSetDropLoot);
	lua_register(L, "isCreature", luaIsCreature);
	lua_register(L, "registerCreatureEvent", luaRegisterCreatureEvent);
	lua_register(L, "unregisterCreatureEvent", luaUnregisterCreatureEvent);
	lua_register(L, "unregisterCreatureEventType", luaUnregisterCreatureEventType);
	lua_register(L, "doChallengeCreature", luaDoChallengeCreature);
	lua_register(L, "doChangeSpeed", luaDoChangeSpeed);
	lua_register(L, "doCreatureChangeOutfit", luaDoCreatureChangeOutfit);
	lua_register(L, "doSetMonsterOutfit", luaSetMonsterOutfit);
	lua_register(L, "doSetItemOutfit", luaSetItemOutfit);
	lua_register(L, "doSetCreatureOutfit", luaSetCreatureOutfit);
	lua_register(L, "getCreatureOutfit", luaGetCreatureOutfit);
	lua_register(L, "getCreatureLastPosition", luaGetCreatureLastPosition);
	lua_register(L, "getCreatureName", luaGetCreatureName);
	lua_register(L, "getCreatureSpeed", luaGetCreatureSpeed);
	lua_register(L, "setCreatureSpeed", luaSetCreatureSpeed);
	lua_register(L, "getCreatureBaseSpeed", luaGetCreatureBaseSpeed);
	lua_register(L, "getCreatureTarget", luaGetCreatureTarget);
	lua_register(L, "doCreatureExecuteTalkAction", luaDoCreatureExecuteTalkAction);
	lua_register(L, "doCreatureSetLookDirection", luaDoCreatureSetLookDir);
	lua_register(L, "getCreatureGuildEmblem", luaGetCreatureGuildEmblem);
	lua_register(L, "doCreatureSetGuildEmblem", luaDoCreatureSetGuildEmblem);
	lua_register(L, "getCreaturePartyShield", luaGetCreaturePartyShield);
	lua_register(L, "doCreatureSetPartyShield", luaDoCreatureSetPartyShield);
	lua_register(L, "getCreatureSkullType", luaGetCreatureSkullType);
	lua_register(L, "doCreatureSetSkullType", luaDoCreatureSetSkullType);
	lua_register(L, "getCreatureNoMove", luaGetCreatureNoMove);
	lua_register(L, "doCreatureSetNoMove", luaDoCreatureSetNoMove);
	lua_register(L, "getCreatureMaster", luaGetCreatureMaster);
	lua_register(L, "getCreatureSummons", luaGetCreatureSummons);

	// Player
	lua_register(L, "getPlayerLevel", luaGetPlayerLevel);
	lua_register(L, "getPlayerExperience", luaGetPlayerExperience);
	lua_register(L, "getPlayerMagLevel", luaGetPlayerMagLevel);
	lua_register(L, "getPlayerSpentMana", luaGetPlayerSpentMana);
	lua_register(L, "getPlayerFood", luaGetPlayerFood);
	lua_register(L, "getPlayerAccess", luaGetPlayerAccess);
	lua_register(L, "getPlayerGhostAccess", luaGetPlayerGhostAccess);
	lua_register(L, "getPlayerSkillLevel", luaGetPlayerSkillLevel);
	lua_register(L, "getPlayerSkillTries", luaGetPlayerSkillTries);
	lua_register(L, "doPlayerSetOfflineTrainingSkill", luaDoPlayerSetOfflineTrainingSkill);
	lua_register(L, "getPlayerTown", luaGetPlayerTown);
	lua_register(L, "getPlayerVocation", luaGetPlayerVocation);
	lua_register(L, "getPlayerIp", luaGetPlayerIp);
	lua_register(L, "getPlayerRequiredMana", luaGetPlayerRequiredMana);
	lua_register(L, "getPlayerRequiredSkillTries", luaGetPlayerRequiredSkillTries);
	lua_register(L, "getPlayerItemCount", luaGetPlayerItemCount);
	lua_register(L, "getPlayerMoney", luaGetPlayerMoney);
	lua_register(L, "getPlayerSoul", luaGetPlayerSoul);
	lua_register(L, "getPlayerFreeCap", luaGetPlayerFreeCap);
	lua_register(L, "getPlayerLight", luaGetPlayerLight);
	lua_register(L, "getPlayerSlotItem", luaGetPlayerSlotItem);
	lua_register(L, "getPlayerWeapon", luaGetPlayerWeapon);
	lua_register(L, "getPlayerItemById", luaGetPlayerItemById);
	lua_register(L, "getPlayerDepotItems", luaGetPlayerDepotItems);
	lua_register(L, "getPlayerGuildId", luaGetPlayerGuildId);
	lua_register(L, "getPlayerGuildName", luaGetPlayerGuildName);
	lua_register(L, "getPlayerGuildRankId", luaGetPlayerGuildRankId);
	lua_register(L, "getPlayerGuildRank", luaGetPlayerGuildRank);
	lua_register(L, "getPlayerGuildNick", luaGetPlayerGuildNick);
	lua_register(L, "getPlayerGuildLevel", luaGetPlayerGuildLevel);
	lua_register(L, "getPlayerGUID", luaGetPlayerGUID);
	lua_register(L, "getPlayerNameDescription", luaGetPlayerNameDescription);
	lua_register(L, "doPlayerSetNameDescription", luaDoPlayerSetNameDescription);
	lua_register(L, "getPlayerSpecialDescription", luaGetPlayerSpecialDescription);
	lua_register(L, "doPlayerSetSpecialDescription", luaDoPlayerSetSpecialDescription);
	lua_register(L, "getPlayerAccountId", luaGetPlayerAccountId);
	lua_register(L, "getPlayerAccount", luaGetPlayerAccount);
	lua_register(L, "getPlayerFlagValue", luaGetPlayerFlagValue);
	lua_register(L, "getPlayerCustomFlagValue", luaGetPlayerCustomFlagValue);
	lua_register(L, "getPlayerPromotionLevel", luaGetPlayerPromotionLevel);
	lua_register(L, "doPlayerSetPromotionLevel", luaDoPlayerSetPromotionLevel);
	lua_register(L, "getPlayerGroupId", luaGetPlayerGroupId);
	lua_register(L, "doPlayerSetGroupId", luaDoPlayerSetGroupId);
	lua_register(L, "doPlayerSendOutfitWindow", luaDoPlayerSendOutfitWindow);
	lua_register(L, "doPlayerLearnInstantSpell", luaDoPlayerLearnInstantSpell);
	lua_register(L, "doPlayerUnlearnInstantSpell", luaDoPlayerUnlearnInstantSpell);
	lua_register(L, "getPlayerLearnedInstantSpell", luaGetPlayerLearnedInstantSpell);
	lua_register(L, "getPlayerInstantSpellCount", luaGetPlayerInstantSpellCount);
	lua_register(L, "getPlayerInstantSpellInfo", luaGetPlayerInstantSpellInfo);
	lua_register(L, "getPlayerSpectators", luaGetPlayerSpectators);
	lua_register(L, "doPlayerSetSpectators", luaDoPlayerSetSpectators);
	lua_register(L, "doPlayerAddSkillTry", luaDoPlayerAddSkillTry);
	lua_register(L, "doPlayerFeed", luaDoPlayerFeed);
	lua_register(L, "doPlayerSendCancel", luaDoPlayerSendCancel);
	lua_register(L, "doPlayerSendCastList", luaDoPlayerSendCastList);
	lua_register(L, "doPlayerSendDefaultCancel", luaDoSendDefaultCancel);
	lua_register(L, "doPlayerSetMaxCapacity", luaDoPlayerSetMaxCapacity);
	lua_register(L, "doPlayerAddSpentMana", luaDoPlayerAddSpentMana);
	lua_register(L, "doPlayerAddSoul", luaDoPlayerAddSoul);
	lua_register(L, "doPlayerSetExtraAttackSpeed", luaDoPlayerSetExtraAttackSpeed);
	lua_register(L, "doPlayerAddItem", luaDoPlayerAddItem);
	lua_register(L, "doPlayerAddItemEx", luaDoPlayerAddItemEx);
	lua_register(L, "doPlayerSendTextMessage", luaDoPlayerSendTextMessage);
	lua_register(L, "doPlayerSendChannelMessage", luaDoPlayerSendChannelMessage);
	lua_register(L, "doPlayerOpenChannel", luaDoPlayerOpenChannel);
	lua_register(L, "doPlayerOpenPrivateChannel", luaDoPlayerOpenPrivateChannel);
	lua_register(L, "doPlayerSendChannels", luaDoPlayerSendChannels);
	lua_register(L, "doPlayerAddMoney", luaDoPlayerAddMoney);
	lua_register(L, "doPlayerRemoveMoney", luaDoPlayerRemoveMoney);
	lua_register(L, "doPlayerTransferMoneyTo", luaDoPlayerTransferMoneyTo);
	lua_register(L, "doShowTextDialog", luaDoShowTextDialog);
	lua_register(L, "doPlayerSetPzLocked", luaDoPlayerSetPzLocked);
	lua_register(L, "doPlayerSetTown", luaDoPlayerSetTown);
	lua_register(L, "doPlayerSetVocation", luaDoPlayerSetVocation);
	lua_register(L, "doPlayerRemoveItem", luaDoPlayerRemoveItem);
	lua_register(L, "doPlayerAddExperience", luaDoPlayerAddExperience);
	lua_register(L, "doPlayerSetGuildId", luaDoPlayerSetGuildId);
	lua_register(L, "doPlayerSetGuildLevel", luaDoPlayerSetGuildLevel);
	lua_register(L, "doPlayerSetGuildNick", luaDoPlayerSetGuildNick);
	lua_register(L, "doPlayerAddOutfit", luaDoPlayerAddOutfit);
	lua_register(L, "doPlayerRemoveOutfit", luaDoPlayerRemoveOutfit);
	lua_register(L, "doPlayerAddOutfitId", luaDoPlayerAddOutfitId);
	lua_register(L, "doPlayerRemoveOutfitId", luaDoPlayerRemoveOutfitId);
	lua_register(L, "canPlayerWearOutfit", luaCanPlayerWearOutfit);
	lua_register(L, "canPlayerWearOutfitId", luaCanPlayerWearOutfitId);
	lua_register(L, "getPlayerLossPercent", luaGetPlayerLossPercent);
	lua_register(L, "doPlayerSetLossPercent", luaDoPlayerSetLossPercent);
	lua_register(L, "doPlayerSetLossSkill", luaDoPlayerSetLossSkill);
	lua_register(L, "getPlayerLossSkill", luaGetPlayerLossSkill);
	lua_register(L, "doPlayerSwitchSaving", luaDoPlayerSwitchSaving);
	lua_register(L, "doPlayerSave", luaDoPlayerSave);
	lua_register(L, "doPlayerSaveItems", luaDoPlayerSaveItems);
	lua_register(L, "isPlayerPzLocked", luaIsPlayerPzLocked);
	lua_register(L, "isPlayerSaving", luaIsPlayerSaving);
	lua_register(L, "isPlayerProtected", luaIsPlayerProtected);
	lua_register(L, "getPlayerByName", luaGetPlayerByName);
	lua_register(L, "getPlayerByGUID", luaGetPlayerByGUID);
	lua_register(L, "getPlayerByNameWildcard", luaGetPlayerByNameWildcard);
	lua_register(L, "getPlayerGUIDByName", luaGetPlayerGUIDByName);
	lua_register(L, "getPlayerNameByGUID", luaGetPlayerNameByGUID);
	lua_register(L, "doPlayerChangeName", luaDoPlayerChangeName);
	lua_register(L, "getPlayerSex", luaGetPlayerSex);
	lua_register(L, "doPlayerSetSex", luaDoPlayerSetSex);
	lua_register(L, "getPlayerSkullEnd", luaGetPlayerSkullEnd);
	lua_register(L, "doPlayerSetSkullEnd", luaDoPlayerSetSkullEnd);
	lua_register(L, "sendPlayerProgressBar", luaSendProgressbar);
	lua_register(L, "getPlayersByAccountId", luaGetPlayersByAccountId);
	lua_register(L, "doPlayerPopupFYI", luaDoPlayerPopupFYI);
	lua_register(L, "doPlayerSendTutorial", luaDoPlayerSendTutorial);
	lua_register(L, "doPlayerSendMailByName", luaDoPlayerSendMailByName);
	lua_register(L, "doPlayerAddMapMark", luaDoPlayerAddMapMark);
	lua_register(L, "doPlayerAddPremiumDays", luaDoPlayerAddPremiumDays);
	lua_register(L, "getPlayerPremiumDays", luaGetPlayerPremiumDays);
	lua_register(L, "getPlayerBlessing", luaGetPlayerBlessing);
	lua_register(L, "doPlayerAddBlessing", luaDoPlayerAddBlessing);
	lua_register(L, "getPlayerPVPBlessing", luaGetPlayerPVPBlessing);
	lua_register(L, "doPlayerSetPVPBlessing", luaDoPlayerSetPVPBlessing);
	lua_register(L, "getPlayerStamina", luaGetPlayerStamina);
	lua_register(L, "doPlayerSetStamina", luaDoPlayerSetStamina);
	lua_register(L, "getPlayerBalance", luaGetPlayerBalance);
	lua_register(L, "doPlayerSetBalance", luaDoPlayerSetBalance);
	lua_register(L, "doUpdatePlayerStats", luaUpdatePlayerStats);
	lua_register(L, "getPlayerDamageMultiplier", luaGetPlayerDamageMultiplier);
	lua_register(L, "setPlayerDamageMultiplier", luaSetPlayerDamageMultiplier);
	lua_register(L, "getPlayerIdleTime", luaGetPlayerIdleTime);
	lua_register(L, "doPlayerSetIdleTime", luaDoPlayerSetIdleTime);
	lua_register(L, "getPlayerLastLoad", luaGetPlayerLastLoad);
	lua_register(L, "getPlayerLastLogin", luaGetPlayerLastLogin);
	lua_register(L, "getPlayerAccountManager", luaGetPlayerAccountManager);
	lua_register(L, "getPlayerTradeState", luaGetPlayerTradeState);
	lua_register(L, "getPlayerOperatingSystem", luaGetPlayerOperatingSystem);
	lua_register(L, "getPlayerClientVersion", luaGetPlayerClientVersion);
	lua_register(L, "getPlayerModes", luaGetPlayerModes);
	lua_register(L, "getPlayerRates", luaGetPlayerRates);
	lua_register(L, "doPlayerSetRate", luaDoPlayerSetRate);
	lua_register(L, "getPlayerPartner", luaGetPlayerPartner);
	lua_register(L, "doPlayerSetPartner", luaDoPlayerSetPartner);
	lua_register(L, "doPlayerFollowCreature", luaDoPlayerFollowCreature);
	lua_register(L, "getPlayerParty", luaGetPlayerParty);
	lua_register(L, "doPlayerJoinParty", luaDoPlayerJoinParty);
	lua_register(L, "doPlayerLeaveParty", luaDoPlayerLeaveParty);
	lua_register(L, "getPartyMembers", luaGetPartyMembers);
	lua_register(L, "hasPlayerClient", luaHasPlayerClient);
	lua_register(L, "isPlayerUsingOtclient", luaIsPlayerUsingOtclient);
	lua_register(L, "doSendPlayerExtendedOpcode", luaDoSendPlayerExtendedOpcode);
	lua_register(L, "doPlayerSetWalkthrough", luaDoPlayerSetWalkthrough);

	// Monster
	lua_register(L, "getMonsterTargetList", luaGetMonsterTargetList);
	lua_register(L, "getMonsterFriendList", luaGetMonsterFriendList);
	lua_register(L, "doMonsterSetTarget", luaDoMonsterSetTarget);
	lua_register(L, "doMonsterChangeTarget", luaDoMonsterChangeTarget);
	lua_register(L, "hasMonsterRaid", luaHasMonsterRaid);

	// Item
	lua_register(L, "doRemoveItem", luaDoRemoveItem);
	lua_register(L, "doItemRaidUnref", luaDoItemRaidUnref);
	lua_register(L, "doItemSetDestination", luaDoItemSetDestination);
	lua_register(L, "doTransformItem", luaDoTransformItem);
	lua_register(L, "doDecayItem", luaDoDecayItem);
	lua_register(L, "getContainerSize", luaGetContainerSize);
	lua_register(L, "getContainerCap", luaGetContainerCap);
	lua_register(L, "getContainerItem", luaGetContainerItem);
	lua_register(L, "doAddContainerItem", luaDoAddContainerItem);
	lua_register(L, "isItemDoor", luaisItemDoor);
	lua_register(L, "getItemAttribute", luaGetItemAttribute);
	lua_register(L, "doItemSetAttribute", luaDoItemSetAttribute);
	lua_register(L, "doItemEraseAttribute", luaDoItemEraseAttribute);
	lua_register(L, "doItemSetDuration", luaDoItemSetDuration);
	lua_register(L, "getItemDurationTime", luaGetItemDurationTime);
	lua_register(L, "getItemWeight", luaGetItemWeight);
	lua_register(L, "getItemParent", luaGetItemParent);
	lua_register(L, "hasItemProperty", luaHasItemProperty);
	lua_register(L, "doAddContainerItemEx", luaDoAddContainerItemEx);

	// Thing
	lua_register(L, "isMovable", luaIsMovable);
	lua_register(L, "getThing", luaGetThing);
	lua_register(L, "getThingPosition", luaGetThingPosition);
	lua_register(L, "doTeleportThing", luaDoTeleportThing);

	// Combat
	lua_register(L, "createCombatObject", luaCreateCombatObject);
	lua_register(L, "createCombatArea", luaCreateCombatArea);
	lua_register(L, "setCombatArea", luaSetCombatArea);
	lua_register(L, "setCombatCondition", luaSetCombatCondition);
	lua_register(L, "setCombatParam", luaSetCombatParam);
	lua_register(L, "setCombatCallback", luaSetCombatCallback);
	lua_register(L, "setCombatFormula", luaSetCombatFormula);
	lua_register(L, "doCombat", luaDoCombat);

	// Condition
	lua_register(L, "createConditionObject", luaCreateConditionObject);
	lua_register(L, "setConditionParam", luaSetConditionParam);
	lua_register(L, "addDamageCondition", luaAddDamageCondition);
	lua_register(L, "addOutfitCondition", luaAddOutfitCondition);
	lua_register(L, "setConditionFormula", luaSetConditionFormula);

	// Variant
	lua_register(L, "numberToVariant", luaNumberToVariant);
	lua_register(L, "stringToVariant", luaStringToVariant);
	lua_register(L, "positionToVariant", luaPositionToVariant);
	lua_register(L, "targetPositionToVariant", luaTargetPositionToVariant);
	lua_register(L, "variantToNumber", luaVariantToNumber);
	lua_register(L, "variantToString", luaVariantToString);
	lua_register(L, "variantToPosition", luaVariantToPosition);

	// os table
	luaL_register(L, "os", luaSystemTable);
	lua_pop(L, 1);

	// db table
	luaL_register(L, "db", luaDatabaseTable);
	lua_pop(L, 1);

	// result table
	luaL_register(L, "result", luaResultTable);
	lua_pop(L, 1);

	// bit table
	luaL_register(L, "bit", luaBitTable);
	lua_pop(L, 1);

	// std table
	luaL_register(L, "std", luaStdTable);
	lua_pop(L, 1);
}
