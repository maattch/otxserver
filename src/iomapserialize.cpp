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

#include "iomapserialize.h"

#include "configmanager.h"
#include "game.h"
#include "house.h"
#include "iologindata.h"

#include "otx/util.hpp"

bool IOMapSerialize::loadMap(Map* map)
{
	std::string config = otx::util::as_lower_string(otx::config::getString(otx::config::HOUSE_STORAGE));
	bool result = false;
	if (config == "binary-tilebased") {
		result = loadMapBinaryTileBased(map);
	} else if (config == "binary") {
		result = loadMapBinary(map);
	} else {
		result = loadMapRelational(map);
	}

	if (!result) {
		return false;
	}

	for (const auto& it : Houses::getInstance()->getHouses()) {
		if (!it.second->hasSyncFlag(House::HOUSE_SYNC_UPDATE)) {
			continue;
		}

		it.second->resetSyncFlag(House::HOUSE_SYNC_UPDATE);
		it.second->updateDoorDescription();
	}
	return true;
}

bool IOMapSerialize::saveMap(Map* map)
{
	std::string config = otx::util::as_lower_string(otx::config::getString(otx::config::HOUSE_STORAGE));
	if (config == "binary-tilebased") {
		return saveMapBinaryTileBased(map);
	} else if (config == "binary") {
		return saveMapBinary(map);
	}

	return saveMapRelational(map);
}

bool IOMapSerialize::updateAuctions()
{
	std::ostringstream query;

	time_t now = time(nullptr);
	query << "SELECT `house_id`, `player_id`, `bid` FROM `house_auctions` WHERE `endtime` < " << now;

	DBResultPtr result;
	if (!(result = g_database.storeQuery(query.str()))) {
		return true;
	}

	bool success = true;
	House* house = nullptr;
	do {
		query.str("");
		query << "DELETE FROM `house_auctions` WHERE `house_id` = " << result->getNumber<int32_t>("house_id");
		if (!(house = Houses::getInstance()->getHouse(result->getNumber<int32_t>(
				  "house_id")))
			|| !g_database.executeQuery(query.str())) {
			success = false;
			continue;
		}

		house->setOwner(result->getNumber<int32_t>("player_id"));
		Houses::getInstance()->payHouse(house, now, result->getNumber<int32_t>("bid"));
	} while (result->next());
	return success;
}

bool IOMapSerialize::loadHouses()
{
	DBResultPtr result = g_database.storeQuery("SELECT * FROM `houses`");
	if (!result) {
		return false;
	}

	House* house = nullptr;
	do {
		if (!(house = Houses::getInstance()->getHouse(result->getNumber<int32_t>("id")))) {
			continue;
		}

		house->setRentWarnings(result->getNumber<int32_t>("warnings"));
		house->setLastWarning(result->getNumber<int32_t>("lastwarning"));

		house->setPaidUntil(result->getNumber<int32_t>("paid"));

		if (result->getNumber<int32_t>("clear") == 1) {
			house->setPendingTransfer(true);
		}

		house->setOwner(result->getNumber<int32_t>("owner"));
		if (house->getOwner() && house->hasSyncFlag(House::HOUSE_SYNC_UPDATE)) {
			house->resetSyncFlag(House::HOUSE_SYNC_UPDATE);
		}
	} while (result->next());

	std::ostringstream query;
	for (const auto& it : Houses::getInstance()->getHouses()) {
		if (!(house = it.second) || !house->getId() || !house->getOwner()) {
			continue;
		}

		query.str("");
		query << "SELECT `listid`, `list` FROM `house_lists` WHERE `house_id` = " << house->getId();
		if (!(result = g_database.storeQuery(query.str()))) {
			continue;
		}

		do {
			house->setAccessList(result->getNumber<int32_t>("listid"), result->getString("list"));
		} while (result->next());
	}

	return true;
}

bool IOMapSerialize::updateHouses()
{
	std::ostringstream query;

	House* house = nullptr;
	for (const auto& it : Houses::getInstance()->getHouses()) {
		if (!(house = it.second)) {
			continue;
		}

		query << "SELECT `price` FROM `houses` WHERE `id` = " << house->getId() << " LIMIT 1";
		if (DBResultPtr result = g_database.storeQuery(query.str())) {
			if ((uint32_t)result->getNumber<int32_t>("price") != house->getPrice()) {
				house->setSyncFlag(House::HOUSE_SYNC_UPDATE);
			}

			query.str("");

			query << "UPDATE `houses` SET ";
			if (house->hasSyncFlag(House::HOUSE_SYNC_NAME)) {
				query << "`name` = " << g_database.escapeString(house->getName()) << ", ";
			}

			if (house->hasSyncFlag(House::HOUSE_SYNC_TOWN)) {
				query << "`town` = " << house->getTownId() << ", ";
			}

			if (house->hasSyncFlag(House::HOUSE_SYNC_SIZE)) {
				query << "`size` = " << house->getSize() << ", ";
			}

			if (house->hasSyncFlag(House::HOUSE_SYNC_PRICE)) {
				query << "`price` = " << house->getPrice() << ", ";
			}

			if (house->hasSyncFlag(House::HOUSE_SYNC_RENT)) {
				query << "`rent` = " << house->getRent() << ", ";
			}

			query << "`doors` = " << house->getDoorsCount() << ", `beds` = "
				  << house->getBedsCount() << ", `tiles` = " << house->getTilesCount();
			if (house->hasSyncFlag(House::HOUSE_SYNC_GUILD)) {
				query << ", `guild` = " << house->isGuild();
			}

			query << " WHERE `id` = " << house->getId() << " LIMIT 1";
		} else {
			query.str("");
			query << "INSERT INTO `houses` (`id`, `owner`, `name`, `town`, `size`, `price`, `rent`, `doors`, `beds`, `tiles`, `guild`) VALUES ("
				  << house->getId() << ", 0, "
				  // we need owner for compatibility reasons (field doesn't have a default value)
				  << g_database.escapeString(house->getName()) << ", " << house->getTownId() << ", "
				  << house->getSize() << ", " << house->getPrice() << ", " << house->getRent() << ", "
				  << house->getDoorsCount() << ", " << house->getBedsCount() << ", "
				  << house->getTilesCount() << ", " << house->isGuild() << ")";
		}

		if (!g_database.executeQuery(query.str())) {
			return false;
		}

		query.str("");
	}

	return true;
}

bool IOMapSerialize::saveHouses()
{
	DBTransaction trans;
	if (!trans.begin()) {
		return false;
	}

	for (const auto& it : Houses::getInstance()->getHouses()) {
		saveHouse(it.second);
	}

	return trans.commit();
}

bool IOMapSerialize::saveHouse(House* house)
{
	std::ostringstream query;
	query << "UPDATE `houses` SET `owner` = " << house->getOwner() << ", `paid` = "
		  << house->getPaidUntil() << ", `warnings` = " << house->getRentWarnings() << ", `lastwarning` = "
		  << house->getLastWarning() << ", `clear` = 0 WHERE `id` = " << house->getId() << " LIMIT 1";
	if (!g_database.executeQuery(query.str())) {
		return false;
	}

	query.str("");
	query << "DELETE FROM `house_lists` WHERE `house_id` = " << house->getId();
	if (!g_database.executeQuery(query.str())) {
		return false;
	}

	DBInsert queryInsert;
	queryInsert.setQuery("INSERT INTO `house_lists` (`house_id`, `listid`, `list`) VALUES ");

	std::string listText;
	if (house->getAccessList(GUEST_LIST, listText) && !listText.empty()) {
		query.str("");
		query << house->getId() << ", " << GUEST_LIST << ", " << g_database.escapeString(listText);
		if (!queryInsert.addRow(query.str())) {
			return false;
		}
	}

	if (house->getAccessList(SUBOWNER_LIST, listText) && !listText.empty()) {
		query.str("");
		query << house->getId() << ", " << SUBOWNER_LIST << ", " << g_database.escapeString(listText);
		if (!queryInsert.addRow(query.str())) {
			return false;
		}
	}

	for (Door* door : house->getHouseDoors()) {
		if (!door->getAccessList(listText) || listText.empty()) {
			continue;
		}

		query.str("");
		query << house->getId() << ", " << (int32_t)door->getDoorId() << ", " << g_database.escapeString(listText);
		if (!queryInsert.addRow(query.str())) {
			return false;
		}
	}

	return query.str().empty() || queryInsert.execute();
}

bool IOMapSerialize::saveHouseItems(House* house)
{
	std::string config = otx::util::as_lower_string(otx::config::getString(otx::config::HOUSE_STORAGE));
	if (config == "binary-tilebased") {
		std::ostringstream query;
		query << "DELETE FROM `tile_store` WHERE `house_id` = " << house->getId();
		if (!g_database.executeQuery(query.str())) {
			return false;
		}

		DBInsert stmt;
		stmt.setQuery("INSERT INTO `tile_store` (`house_id`, `data`) VALUES ");
		return saveHouseBinaryTileBased(stmt, house) && stmt.execute();
	} else if (config == "binary") {
		std::ostringstream query;
		query << "DELETE FROM `house_data` WHERE `house_id` = " << house->getId();
		if (!g_database.executeQuery(query.str())) {
			return false;
		}

		DBInsert stmt;
		stmt.setQuery("INSERT INTO `house_data` (`house_id`, `data`) VALUES ");
		return saveHouseBinary(stmt, house) && stmt.execute();
	}

	std::ostringstream query;
	query << "DELETE FROM `tile_items` WHERE `tile_id` IN (SELECT `id` FROM `tiles` WHERE `house_id` = " << house->getId() << ')';
	if (!g_database.executeQuery(query.str())) {
		return false;
	}

	query.str("");
	query << "DELETE FROM `tiles` WHERE `house_id` = " << house->getId();
	if (!g_database.executeQuery(query.str())) {
		return false;
	}

	query.str("");
	query << "SELECT `id` FROM `tiles` ORDER BY `id` DESC LIMIT 1";

	DBResultPtr result;
	if (!(result = g_database.storeQuery(query.str()))) {
		return false;
	}

	uint32_t tileId = result->getNumber<int32_t>("id") + 1;
	return saveHouseRelational(house, tileId);
}

bool IOMapSerialize::loadMapRelational(Map* map)
{
	std::ostringstream query; // lock mutex!

	House* house = nullptr;
	for (const auto& it : Houses::getInstance()->getHouses()) {
		if (!(house = it.second)) {
			continue;
		}

		query.str("");
		query << "SELECT * FROM `tiles` WHERE `house_id` = " << house->getId();
		DBResultPtr result = g_database.storeQuery(query.str());
		if (result) {
			do {
				query.str("");
				query << "SELECT * FROM `tile_items` WHERE `tile_id` = " << result->getNumber<int32_t>("id") << " ORDER BY `sid` DESC";
				if (DBResultPtr itemsResult = g_database.storeQuery(query.str())) {
					if (house->hasPendingTransfer()) {
						if (Player* player = g_game.getPlayerByGuidEx(house->getOwner())) {
							Depot* depot = player->getDepot(house->getTownId(), true);
							loadItems(itemsResult, depot, true);
							if (player->isVirtual()) {
								IOLoginData::getInstance()->savePlayer(player);
								delete player;
							}
						}
					} else {
						Position pos(result->getNumber<int32_t>("x"), result->getNumber<int32_t>("y"), result->getNumber<int32_t>("z"));
						if (Tile* tile = map->getTile(pos)) {
							loadItems(itemsResult, tile, false);
						} else {
							std::clog << "[Error - IOMapSerialize::loadMapRelational] Unserialization"
									  << " of invalid tile at position " << pos << std::endl;
						}
					}
				}
			} while (result->next());
		} else // backward compatibility
		{
			for (HouseTile* tile : house->getHouseTiles()) {
				query.str("");
				query << "SELECT `id` FROM `tiles` WHERE `x` = " << tile->getPosition().x << " AND `y` = "
					  << tile->getPosition().y << " AND `z` = " << tile->getPosition().z << " LIMIT 1";
				result = g_database.storeQuery(query.str());
				if (result) {
					query.str("");
					query << "SELECT * FROM `tile_items` WHERE `tile_id` = " << result->getNumber<int32_t>("id") << " ORDER BY `sid` DESC";
					if (DBResultPtr itemsResult = g_database.storeQuery(query.str())) {
						if (house->hasPendingTransfer()) {
							if (Player* player = g_game.getPlayerByGuidEx(house->getOwner())) {
								Depot* depot = player->getDepot(house->getTownId(), true);
								loadItems(itemsResult, depot, true);
								if (player->isVirtual()) {
									IOLoginData::getInstance()->savePlayer(player);
									delete player;
								}
							}
						} else {
							loadItems(itemsResult, tile, false);
						}
					}
				}
			}
		}
	}

	return true;
}

bool IOMapSerialize::saveMapRelational(Map*)
{
	// Start the transaction
	DBTransaction trans;
	if (!trans.begin()) {
		return false;
	}

	// clear old tile data
	if (!g_database.executeQuery("DELETE FROM `tile_items`")) {
		return false;
	}

	if (!g_database.executeQuery("DELETE FROM `tiles`")) {
		return false;
	}

	uint32_t tileId = 0;
	for (const auto& it : Houses::getInstance()->getHouses()) {
		saveHouseRelational(it.second, tileId);
	}

	// End the transaction
	return trans.commit();
}

bool IOMapSerialize::loadMapBinary(Map* map)
{
	DBResultPtr result = g_database.storeQuery("SELECT `house_id`, `data` FROM `house_data`");
	if (!result) {
		return false;
	}

	House* house = nullptr;
	do {
		int32_t houseId = result->getNumber<int32_t>("house_id");
		house = Houses::getInstance()->getHouse(houseId);

		unsigned long attrSize = 0;
		const char* attr = result->getStream("data", attrSize);

		PropStream propStream;
		propStream.init(attr, attrSize);
		while (propStream.size()) {
			uint16_t x = 0, y = 0;
			uint8_t z = 0;

			propStream.getShort(x);
			propStream.getShort(y);
			propStream.getByte(z);

			uint32_t itemCount = 0;
			propStream.getLong(itemCount);

			Position pos(x, y, (int16_t)z);
			if (house && house->hasPendingTransfer()) {
				if (Player* player = g_game.getPlayerByGuidEx(house->getOwner())) {
					Depot* depot = player->getDepot(player->getTown(), true);
					while (itemCount--) {
						loadItem(propStream, depot, true);
					}

					if (player->isVirtual()) {
						IOLoginData::getInstance()->savePlayer(player);
						delete player;
					}
				}
			} else if (Tile* tile = map->getTile(pos)) {
				while (itemCount--) {
					loadItem(propStream, tile, false);
				}
			} else {
				std::clog << "[Error - IOMapSerialize::loadMapBinary] Unserialization of invalid tile"
						  << " at position " << pos << std::endl;
				break;
			}
		}
	} while (result->next());
	return true;
}

bool IOMapSerialize::saveMapBinary(Map*)
{
	// Start the transaction
	DBTransaction transaction;
	if (!transaction.begin()) {
		return false;
	}

	if (!g_database.executeQuery("DELETE FROM `house_data`")) {
		return false;
	}

	DBInsert stmt;
	stmt.setQuery("INSERT INTO `house_data` (`house_id`, `data`) VALUES ");
	for (const auto& it : Houses::getInstance()->getHouses()) {
		saveHouseBinary(stmt, it.second);
	}

	if (!stmt.execute()) {
		return false;
	}

	// End the transaction
	return transaction.commit();
}

bool IOMapSerialize::loadMapBinaryTileBased(Map* map)
{
	DBResultPtr result = g_database.storeQuery("SELECT `house_id`, `data` FROM `tile_store`");
	if (!result) {
		return false;
	}

	House* house = nullptr;
	do {
		int32_t houseId = result->getNumber<int32_t>("house_id");
		house = Houses::getInstance()->getHouse(houseId);

		unsigned long attrSize = 0;
		const char* attr = result->getStream("data", attrSize);

		PropStream propStream;
		propStream.init(attr, attrSize);
		while (propStream.size()) {
			uint16_t x = 0, y = 0;
			uint8_t z = 0;

			propStream.getShort(x);
			propStream.getShort(y);
			propStream.getByte(z);

			uint32_t itemCount = 0;
			propStream.getLong(itemCount);

			Position pos(x, y, (int16_t)z);
			if (house && house->hasPendingTransfer()) {
				if (Player* player = g_game.getPlayerByGuidEx(house->getOwner())) {
					Depot* depot = player->getDepot(player->getTown(), true);
					while (itemCount--) {
						loadItem(propStream, depot, true);
					}

					if (player->isVirtual()) {
						IOLoginData::getInstance()->savePlayer(player);
						delete player;
					}
				}
			} else if (Tile* tile = map->getTile(pos)) {
				while (itemCount--) {
					loadItem(propStream, tile, false);
				}
			} else {
				std::clog << "[Error - IOMapSerialize::loadMapBinary] Unserialization of invalid tile"
						  << " at position " << pos << std::endl;
				break;
			}
		}
	} while (result->next());
	return true;
}

bool IOMapSerialize::saveMapBinaryTileBased(Map*)
{
	// Start the transaction
	DBTransaction transaction;
	if (!transaction.begin()) {
		return false;
	}

	if (!g_database.executeQuery("DELETE FROM `tile_store`")) {
		return false;
	}

	DBInsert stmt;
	stmt.setQuery("INSERT INTO `tile_store` (`house_id`, `data`) VALUES ");
	for (const auto& it : Houses::getInstance()->getHouses()) {
		saveHouseBinaryTileBased(stmt, it.second);
	}

	if (!stmt.execute()) {
		return false;
	}

	// End the transaction
	return transaction.commit();
}

bool IOMapSerialize::saveHouseRelational(House* house, uint32_t& tileId)
{
	for (HouseTile* tile : house->getHouseTiles()) {
		saveItems(tileId, house->getId(), tile);
	}
	return true;
}

bool IOMapSerialize::saveHouseBinary(DBInsert& stmt, House* house)
{
	PropWriteStream stream;
	for (HouseTile* tile : house->getHouseTiles()) {
		if (!saveTile(stream, tile)) {
			continue;
		}
	}

	uint32_t attributesSize = 0;
	const char* attributes = stream.getStream(attributesSize);
	if (!attributesSize) {
		return true;
	}

	std::ostringstream query;
	query << house->getId() << ", " << g_database.escapeBlob(attributes, attributesSize);
	return stmt.addRow(query.str());
}

bool IOMapSerialize::saveHouseBinaryTileBased(DBInsert& stmt, House* house)
{
	for (HouseTile* tile : house->getHouseTiles()) {
		PropWriteStream stream;
		if (!saveTile(stream, tile)) {
			continue;
		}

		uint32_t attributesSize = 0;
		const char* attributes = stream.getStream(attributesSize);
		if (!attributesSize) {
			continue;
		}

		std::ostringstream query;
		query << house->getId() << ", " << g_database.escapeBlob(attributes, attributesSize);
		if (!stmt.addRow(query.str())) {
			return false;
		}
	}
	return true;
}

bool IOMapSerialize::loadItems(DBResultPtr result, Cylinder* parent, bool depotTransfer /* = false*/)
{
	ItemMap itemMap;
	Tile* tile = nullptr;
	if (!parent->getItem()) {
		tile = parent->getTile();
	}

	Item* item = nullptr;
	int32_t sid, pid, id, count;
	do {
		sid = result->getNumber<int32_t>("sid");
		pid = result->getNumber<int32_t>("pid");
		id = result->getNumber<int32_t>("itemtype");
		count = result->getNumber<int32_t>("count");

		item = nullptr;
		unsigned long attrSize = 0;
		const char* attr = result->getStream("attributes", attrSize);

		PropStream propStream;
		propStream.init(attr, attrSize);

		const ItemType& iType = Item::items[id];
		if (iType.movable || iType.forceSerialize || pid) {
			if (!(item = Item::CreateItem(id, count))) {
				continue;
			}

			if (item->unserializeAttr(propStream)) {
				if (!pid) {
					parent->__internalAddThing(item);
					item->__startDecaying();
				}
			} else {
				std::clog << "[Warning - IOMapSerialize::loadItems] Unserialization error [0] for item type " << id << std::endl;
			}
		} else if (tile) {
			// find this type in the tile
			if (TileItemVector* items = tile->getItemList()) {
				for (ItemVector::iterator it = items->begin(); it != items->end(); ++it) {
					if ((*it)->getID() == id) {
						item = *it;
						break;
					}

					if (iType.isBed() && (*it)->getBed()) {
						item = *it;
						break;
					}

					if (iType.isDoor() && (*it)->getDoor()) {
						item = *it;
						break;
					}
				}
			}
		}

		if (item) {
			if (item->unserializeAttr(propStream)) {
				if (!item->getDoor() || item->getID() == iType.transformUseTo) {
					item = g_game.transformItem(item, id);
				}

				if (item) {
					itemMap[sid] = std::make_pair(item, pid);
				}
			} else {
				std::clog << "[Warning - IOMapSerialize::loadItems] Unserialization error [1] for item type " << id << std::endl;
			}
		} else if ((item = Item::CreateItem(id))) {
			item->unserializeAttr(propStream);
			if (!depotTransfer) {
				std::clog << "[Warning - IOMapSerialize::loadItems] nullptr item at "
						  << tile->getPosition() << " (type = " << id << ", sid = "
						  << sid << ", pid = " << pid << ")" << std::endl;
			} else {
				itemMap[sid] = std::make_pair(parent->getItem(), pid);
			}

			delete item;
			item = nullptr;
		}
	} while (result->next());

	ItemMap::iterator it;
	for (ItemMap::reverse_iterator rit = itemMap.rbegin(); rit != itemMap.rend(); ++rit) {
		if (!(item = rit->second.first)) {
			continue;
		}

		pid = rit->second.second;
		it = itemMap.find(pid);
		if (it == itemMap.end()) {
			continue;
		}

		if (Container* container = it->second.first->getContainer()) {
			container->__internalAddThing(item);
			g_game.startDecay(item);
		}
	}

	return true;
}

bool IOMapSerialize::saveItems(uint32_t& tileId, uint32_t houseId, const Tile* tile)
{
	int32_t thingCount = tile->getThingCount();
	if (!thingCount) {
		return true;
	}

	Item* item = nullptr;
	int32_t runningId = 0, parentId = 0;
	ContainerStackList containerStackList;

	bool stored = false;
	DBInsert stmt;
	stmt.setQuery("INSERT INTO `tile_items` (`tile_id`, `sid`, `pid`, `itemtype`, `count`, `attributes`, `serial`) VALUES ");

	std::ostringstream query;
	for (int32_t i = 0; i < thingCount; ++i) {
		if (!(item = tile->__getThing(i)->getItem()) || (!item->isMovable() && !item->forceSerialize())) {
			continue;
		}

		if (!stored) {
			Position tilePosition = tile->getPosition();
			query << "INSERT INTO `tiles` (`id`, `house_id`, `x`, `y`, `z`) VALUES ("
				  << tileId << ", " << houseId << ", "
				  << tilePosition.x << ", " << tilePosition.y << ", " << tilePosition.z << ")";
			if (!g_database.executeQuery(query.str())) {
				return false;
			}

			stored = true;
			query.str("");
		}

		PropWriteStream propWriteStream;
		item->serializeAttr(propWriteStream);

		std::string serial;
		ItemAttributes* attr = item->getAttribute("serial");
		if (attr && attr->isString()) {
			serial = attr->getString();
			item->eraseAttribute("serial");
		} else {
			serial = generateSerial();
		}

		uint32_t attributesSize = 0;
		const char* attributes = propWriteStream.getStream(attributesSize);

		query << tileId << ", " << ++runningId << ", " << parentId << ", "
			  << item->getID() << ", " << item->getSubType() << ", " << g_database.escapeBlob(attributes, attributesSize) << ", " << g_database.escapeString(serial);
		if (!stmt.addRow(query.str())) {
			return false;
		}

		query.str("");
		if (item->getContainer()) {
			containerStackList.push_back(std::make_pair(item->getContainer(), runningId));
		}
	}

	Container* container = nullptr;
	for (ContainerStackList::iterator cit = containerStackList.begin(); cit != containerStackList.end(); ++cit) {
		container = cit->first;
		parentId = cit->second;
		for (Item* containerItem : container->getItemList()) {
			PropWriteStream propWriteStream;
			containerItem->serializeAttr(propWriteStream);

			uint32_t attributesSize = 0;
			const char* attributes = propWriteStream.getStream(attributesSize);

			query << tileId << ", " << ++runningId << ", " << parentId << ", "
				  << containerItem->getID() << ", " << containerItem->getSubType() << ", " << g_database.escapeBlob(attributes, attributesSize);
			if (!stmt.addRow(query.str())) {
				return false;
			}

			query.str("");
			if (containerItem->getContainer()) {
				containerStackList.emplace_back(containerItem->getContainer(), runningId);
			}
		}
	}

	if (stored) {
		++tileId;
	}

	return stmt.execute();
}

bool IOMapSerialize::loadContainer(PropStream& propStream, Container* container)
{
	while (container->serializationCount > 0) {
		if (!loadItem(propStream, container, false)) {
			std::clog << "[Warning - IOMapSerialize::loadContainer] Unserialization error [0] for item in container " << container->getID() << std::endl;
			return false;
		}

		container->serializationCount--;
	}

	uint8_t endAttr = ATTR_END;
	propStream.getByte(endAttr);
	if (endAttr == ATTR_END) {
		return true;
	}

	std::clog << "[Warning - IOMapSerialize::loadContainer] Unserialization error [1] for item in container " << container->getID() << std::endl;
	return false;
}

bool IOMapSerialize::loadItem(PropStream& propStream, Cylinder* parent, bool depotTransfer /* = false*/)
{
	Tile* tile = nullptr;
	if (!parent->getItem()) {
		tile = parent->getTile();
	}

	uint16_t id = 0;
	propStream.getShort(id);
	Item* item = nullptr;

	const ItemType& iType = Item::items[id];
	if (iType.movable || iType.forceSerialize || (!depotTransfer && !tile)) {
		if (!(item = Item::CreateItem(id))) {
			return true;
		}

		if (!item->unserializeAttr(propStream)) {
			std::clog << "[Warning - IOMapSerialize::loadItem] Unserialization error [0] for item type " << id << std::endl;
			delete item;
			return false;
		}

		if (Container* container = item->getContainer()) {
			if (!loadContainer(propStream, container)) {
				delete item;
				return false;
			}
		}

		if (parent) {
			parent->__internalAddThing(item);
			item->__startDecaying();
		} else {
			delete item;
		}

		return true;
	}

	if (tile) {
		// Stationary items
		if (TileItemVector* items = tile->getItemList()) {
			for (ItemVector::iterator it = items->begin(); it != items->end(); ++it) {
				if ((*it)->getID() == id) {
					item = *it;
					break;
				}

				if (iType.isBed() && (*it)->getBed()) {
					item = *it;
					break;
				}

				if (iType.isDoor() && (*it)->getDoor()) {
					item = *it;
					break;
				}
			}
		}
	}

	if (item) {
		if (item->unserializeAttr(propStream)) {
			Container* container = item->getContainer();
			if (container && !loadContainer(propStream, container)) {
				return false;
			}

			if (!item->getDoor() || item->getID() == iType.transformUseTo) {
				item = g_game.transformItem(item, id);
			}
		} else {
			std::clog << "[Warning - IOMapSerialize::loadItem] Unserialization error [1] for item type " << id << std::endl;
		}

		return true;
	}

	// The map changed since the last save, just read the attributes
	if (!(item = Item::CreateItem(id))) {
		return true;
	}

	item->unserializeAttr(propStream);
	if (Container* container = item->getContainer()) {
		if (!loadContainer(propStream, container)) {
			delete item;
			return false;
		}

		if (depotTransfer) {
			for (Item* containerItem : container->getItemList()) {
				parent->__addThing(nullptr, containerItem);
			}
			container->itemlist.clear();
		}
	}

	delete item;
	return true;
}

bool IOMapSerialize::saveTile(PropWriteStream& stream, const Tile* tile)
{
	int32_t tileCount = tile->getThingCount();
	if (!tileCount) {
		return true;
	}

	std::vector<Item*> items;
	Item* item = nullptr;
	for (; tileCount > 0; --tileCount) {
		if ((item = tile->__getThing(tileCount - 1)->getItem()) && // CHECKME: wouldn't it be better to use TileItemVector in here?
			(item->isMovable() || item->forceSerialize())) {
			items.push_back(item);
		}
	}

	tileCount = items.size(); // lame, but at least we don't need a new variable
	if (tileCount > 0) {
		stream.addShort(tile->getPosition().x);
		stream.addShort(tile->getPosition().y);
		stream.addByte(tile->getPosition().z);

		stream.addLong(tileCount);
		for (std::vector<Item*>::iterator it = items.begin(); it != items.end(); ++it) {
			saveItem(stream, (*it));
		}
	}

	return true;
}

bool IOMapSerialize::saveItem(PropWriteStream& stream, const Item* item)
{
	stream.addShort(item->getID());
	item->serializeAttr(stream);
	if (const Container* container = item->getContainer()) {
		stream.addByte(ATTR_CONTAINER_ITEMS);
		stream.addLong(container->size());
		for (auto rit = container->getReversedItems(); rit != container->getReversedEnd(); ++rit) {
			saveItem(stream, (*rit));
		}
	}

	stream.addByte(ATTR_END);
	return true;
}
