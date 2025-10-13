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

#include "beds.h"

#include "configmanager.h"
#include "game.h"
#include "house.h"
#include "iologindata.h"
#include "player.h"

Attr_ReadValue BedItem::readAttr(AttrTypes_t attr, PropStream& propStream)
{
	switch (attr) {
		case ATTR_SLEEPERGUID: {
			if (!propStream.getLong(m_sleeperGUID)) {
				return ATTR_READ_ERROR;
			}

			if (m_sleeperGUID != 0) {
				std::string name;
				if (IOLoginData::getInstance()->getNameByGuid(m_sleeperGUID, name)) {
					setSpecialDescription(name + " is sleeping there.");
					g_game.setBedSleeper(this, m_sleeperGUID);
				}
			}
			return ATTR_READ_CONTINUE;
		}

		case ATTR_SLEEPSTART: {
			uint32_t sleepStart;
			if (!propStream.getLong(sleepStart)) {
				return ATTR_READ_ERROR;
			}

			setIntAttr("sleepstart", sleepStart);
			return ATTR_READ_CONTINUE;
		}

		default:
			break;
	}
	return Item::readAttr(attr, propStream);
}

bool BedItem::serializeAttr(PropWriteStream& propWriteStream) const
{
	if (m_sleeperGUID != 0) {
		propWriteStream.addByte(ATTR_SLEEPERGUID);
		propWriteStream.addLong(m_sleeperGUID);
	}
	return Item::serializeAttr(propWriteStream);
}

BedItem* BedItem::getNextBedItem()
{
	if (Tile* tile = g_game.getTile(getNextPosition(Item::items[getID()].bedPartnerDir, getPosition()))) {
		return tile->getBedItem();
	}
	return nullptr;
}

bool BedItem::canUse(Player* player)
{
	if (!m_house || !player || player->isRemoved() || (!player->isPremium() && otx::config::getBoolean(otx::config::BED_REQUIRE_PREMIUM))) {
		return false;
	}

	if (m_sleeperGUID == 0 || m_house->getHouseAccessLevel(player) == HOUSE_OWNER) {
		return isBed();
	}

	Player* _player = g_game.getPlayerByGuidEx(m_sleeperGUID);
	if (!_player) {
		return isBed();
	}

	bool ret = m_house->getHouseAccessLevel(_player) <= m_house->getHouseAccessLevel(player);
	if (_player->isVirtual()) {
		delete _player;
	}
	return ret;
}

void BedItem::sleep(Player* player)
{
	if (!m_house || !player || player->isRemoved()) {
		return;
	}

	if (m_sleeperGUID == 0) {
		g_game.setBedSleeper(this, player->getGUID());
		internalSetSleeper(player);

		BedItem* nextBedItem = getNextBedItem();
		if (nextBedItem) {
			nextBedItem->internalSetSleeper(player);
		}

		updateAppearance(player);
		if (nextBedItem) {
			nextBedItem->updateAppearance(player);
		}

		player->getTile()->moveCreature(nullptr, player, getTile());
		g_game.addMagicEffect(player->getPosition(), MAGIC_EFFECT_SLEEP);
		addSchedulerTask(SCHEDULER_MINTICKS, [playerID = player->getID()]() { g_game.kickPlayer(playerID, false); });
	} else if (Item::items[getID()].transformUseTo) {
		wakeUp();
		g_game.addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
	} else {
		player->sendCancelMessage(RET_NOTPOSSIBLE);
	}
}

void BedItem::wakeUp()
{
	if (!m_house) {
		return;
	}

	if (m_sleeperGUID != 0) {
		if (Player* player = g_game.getPlayerByGuidEx(m_sleeperGUID)) {
			regeneratePlayer(player);
			if (player->isVirtual()) {
				IOLoginData::getInstance()->savePlayer(player);
				delete player;
			} else {
				g_game.addCreatureHealth(player);
			}
		}
	}

	g_game.removeBedSleeper(m_sleeperGUID);
	internalRemoveSleeper();

	BedItem* nextBedItem = getNextBedItem();
	if (nextBedItem) {
		nextBedItem->internalRemoveSleeper();
	}

	updateAppearance(nullptr);
	if (nextBedItem) {
		nextBedItem->updateAppearance(nullptr);
	}
}

void BedItem::regeneratePlayer(Player* player) const
{
	int32_t sleepStart = 0;
	ItemAttributes* attr = getAttribute("sleepstart");
	if (attr && attr->isInt()) {
		sleepStart = attr->getInt();
	}

	const int32_t sleptTime = time(nullptr) - sleepStart;
	if (Condition* condition = player->getCondition(CONDITION_REGENERATION, CONDITIONID_DEFAULT)) {
		int32_t regenAmount = sleptTime / 30;
		if (condition->getTicks() != -1) {
			regenAmount = std::min((condition->getTicks() / 1000), sleptTime) / 30;
			const int32_t newTicks = condition->getTicks() - (regenAmount * 30000);
			if (newTicks <= 0) {
				player->removeCondition(condition);
			} else {
				condition->setTicks(newTicks);
			}
		}

		if (regenAmount > 0) {
			player->changeHealth(regenAmount);
			player->changeMana(regenAmount);
		}
	}

	if (sleptTime > 0) {
		player->changeSoul(sleptTime / (60 * 15));
	}
}

void BedItem::updateAppearance(const Player* player)
{
	const ItemType& it = Item::items[getID()];
	if (it.type != ITEM_TYPE_BED) {
		return;
	}

	if (player && it.transformBed[player->getSex(false)]) {
		if (Item::items[it.transformBed[player->getSex(false)]].type == ITEM_TYPE_BED) {
			g_game.transformItem(this, it.transformBed[player->getSex(false)]);
		}
	} else if (it.transformUseTo) {
		if (Item::items[it.transformUseTo].type == ITEM_TYPE_BED) {
			g_game.transformItem(this, it.transformUseTo);
		}
	}
}

void BedItem::internalSetSleeper(const Player* player)
{
	m_sleeperGUID = player->getGUID();
	setIntAttr("sleepstart", time(nullptr));
	setSpecialDescription(player->getName() + " is sleeping there.");
}

void BedItem::internalRemoveSleeper()
{
	m_sleeperGUID = 0;
	eraseAttribute("sleepstart");
	setSpecialDescription("Nobody is sleeping there.");
}
