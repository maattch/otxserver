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

#include "teleport.h"

#include "game.h"

Attr_ReadValue Teleport::readAttr(AttrTypes_t attr, PropStream& propStream)
{
	if (attr != ATTR_TELE_DEST) {
		return Item::readAttr(attr, propStream);
	}

	uint16_t x, y;
	uint8_t z;
	if (!propStream.getType<uint16_t>(x) || !propStream.getType<uint16_t>(y) || !propStream.getType<uint8_t>(z)) {
		return ATTR_READ_ERROR;
	}

	setDestination(Position(x, y, z));
	return ATTR_READ_CONTINUE;
}

bool Teleport::serializeAttr(PropWriteStream& propWriteStream) const
{
	bool ret = Item::serializeAttr(propWriteStream);
	propWriteStream.addByte(ATTR_TELE_DEST);
	propWriteStream.addType<uint16_t>(destination.x);
	propWriteStream.addType<uint16_t>(destination.y);
	propWriteStream.addByte(destination.z);
	return ret;
}

void Teleport::__addThing(Creature* actor, int32_t, Thing* thing)
{
	if (!thing || thing->isRemoved()) {
		return;
	}

	Tile* destTile = g_game.getTile(destination);
	if (!destTile) {
		return;
	}

	if (destTile->hasFlag(TILESTATE_TELEPORT)) {
		std::clog << "[Warning - Teleport] The teleport at " << getPosition() << " leads to another teleport, this could cause a crash!" << std::endl;
		return;
	}

	if (Creature* creature = thing->getCreature()) {
		g_game.addMagicEffect(creature->getPosition(), MAGIC_EFFECT_TELEPORT, creature->isGhost());
		creature->getTile()->moveCreature(actor, creature, destTile);
		g_game.addMagicEffect(destTile->getPosition(), MAGIC_EFFECT_TELEPORT, creature->isGhost());
	} else if (Item* item = thing->getItem()) {
		g_game.addMagicEffect(item->getPosition(), MAGIC_EFFECT_TELEPORT);
		g_game.internalMoveItem(actor, item->getTile(), destTile, INDEX_WHEREEVER, item, item->getItemCount(), nullptr);
		g_game.addMagicEffect(destTile->getPosition(), MAGIC_EFFECT_TELEPORT);
	}
}
