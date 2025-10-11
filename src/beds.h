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

#include "item.h"

class House;
class Player;

class BedItem final : public Item
{
public:
	BedItem(uint16_t type) : Item(type) { internalRemoveSleeper(); }

	BedItem* getBed() override { return this; }
	const BedItem* getBed() const override { return this; }

	Attr_ReadValue readAttr(AttrTypes_t attr, PropStream& propStream) override;
	bool serializeAttr(PropWriteStream& propWriteStream) const override;

	bool canRemove() const override { return m_house != nullptr; }

	uint32_t getSleeper() const { return m_sleeperGUID; }
	void setSleeper(uint32_t guid) { m_sleeperGUID = guid; }

	House* getHouse() const { return m_house; }
	void setHouse(House* h) { m_house = h; }

	bool canUse(Player* player);

	void sleep(Player* player);
	void wakeUp();

	BedItem* getNextBedItem();

private:
	void updateAppearance(const Player* player);
	void regeneratePlayer(Player* player) const;

	void internalSetSleeper(const Player* player);
	void internalRemoveSleeper();

	House* m_house = nullptr;
	uint32_t m_sleeperGUID = 0;
};
