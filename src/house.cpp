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

#include "house.h"

#include "beds.h"
#include "configmanager.h"
#include "database.h"
#include "game.h"
#include "ioguild.h"
#include "iologindata.h"
#include "iomapserialize.h"
#include "town.h"

#include "otx/util.hpp"

House::House(uint32_t houseId)
{
	guild = pendingTransfer = false;
	name = "OTX headquarter (Flat 1, Area 42)";
	entry = Position();
	id = houseId;
	rent = price = townId = paidUntil = owner = ownerAccountId = rentWarnings = lastWarning = 0;
	syncFlags = HOUSE_SYNC_NAME | HOUSE_SYNC_TOWN | HOUSE_SYNC_SIZE | HOUSE_SYNC_PRICE | HOUSE_SYNC_RENT | HOUSE_SYNC_GUILD;
}

void House::addTile(HouseTile* tile)
{
	tile->setFlag(TILESTATE_PROTECTIONZONE);
	houseTiles.push_back(tile);
}

void House::addBed(BedItem* bed)
{
	bedsList.push_back(bed);
	bed->setHouse(this);
}

void House::addDoor(Door* door)
{
	door->addRef();
	doorList.push_back(door);

	door->setHouse(this);
	updateDoorDescription("", door);
}

void House::removeDoor(Door* door)
{
	HouseDoorList::iterator it = std::find(doorList.begin(), doorList.end(), door);
	if (it != doorList.end()) {
		(*it)->unRef();
		doorList.erase(it);
	}
}

Door* House::getDoorByNumber(uint8_t doorId) const
{
	for (HouseDoorList::const_iterator it = doorList.begin(); it != doorList.end(); ++it) {
		if ((*it)->getDoorId() == doorId) {
			return (*it);
		}
	}

	return nullptr;
}

Door* House::getDoorByPosition(const Position& pos)
{
	for (HouseDoorList::iterator it = doorList.begin(); it != doorList.end(); ++it) {
		if ((*it)->getPosition() == pos) {
			return (*it);
		}
	}

	return nullptr;
}

void House::setOwner(uint32_t guid)
{
	owner = guid;
	updateDoorDescription();
}

bool House::setOwnerEx(uint32_t guid, bool transfer)
{
	if (owner == guid) {
		return true;
	}

	if (isGuild() && guid) {
		Player* player = g_game.getPlayerByGuidEx(guid);
		if (!player) {
			return false;
		}

		guid = player->getGuildId();
		if (player->isVirtual()) {
			delete player;
		}
	}

	if (owner) {
		ownerAccountId = rentWarnings = paidUntil = 0;
		if (transfer) {
			clean();
		}

		setAccessList(SUBOWNER_LIST, "", !transfer);
		setAccessList(GUEST_LIST, "", !transfer);
		for (HouseDoorList::iterator it = doorList.begin(); it != doorList.end(); ++it) {
			(*it)->setAccessList("");
		}
	}

	setOwner(guid);
	lastWarning = guid ? time(nullptr) : 0;

	DBTransaction trans;
	if (!trans.begin()) {
		return false;
	}

	IOMapSerialize::getInstance()->saveHouse(this);
	return trans.commit();
}

bool House::isGuild() const
{
	return g_config.getBool(ConfigManager::GUILD_HALLS) && guild;
}

bool House::isBidded() const
{
	DBResultPtr result;

	std::ostringstream query;
	query << "SELECT `house_id` FROM `house_auctions` WHERE `house_id` = " << id << " LIMIT 1";
	if (!(result = g_database.storeQuery(query.str()))) {
		return false;
	}

	return true;
}

void House::updateDoorDescription(std::string _name /* = ""*/, Door* door /* = nullptr*/)
{
	std::string tmp = "house";
	if (isGuild()) {
		tmp = "hall";
	}

	char houseDescription[200];
	if (owner) {
		if (isGuild()) {
			IOGuild::getInstance()->getGuildById(_name, owner);
		} else if (_name.empty()) {
			IOLoginData::getInstance()->getNameByGuid(owner, _name);
		}

		// CUSTOM: Protect House
		ownerAccountId = IOLoginData::getInstance()->getAccountIdByName(_name);
		sprintf(houseDescription, "It belongs to %s '%s'. %s owns this %s.", tmp.c_str(), name.c_str(), _name.c_str(), tmp.c_str());
	} else {
		sprintf(houseDescription, "It belongs to %s '%s'. Nobody owns this %s. It costs %d gold coins.", tmp.c_str(), name.c_str(), tmp.c_str(), price);
	}

	if (!door) {
		for (HouseDoorList::iterator it = doorList.begin(); it != doorList.end(); ++it) {
			(*it)->setSpecialDescription(houseDescription);
		}
	} else {
		door->setSpecialDescription(houseDescription);
	}
}

void House::removePlayer(Player* player, bool ignoreRights)
{
	if (!ignoreRights && player->hasFlag(PlayerFlag_CanEditHouses)) {
		return;
	}

	Position curPos = player->getPosition(), newPos = g_game.getClosestFreeTile(player, entry, false, false);
	if (g_game.internalTeleport(player, newPos, false) == RET_NOERROR && !player->isGhost()) {
		g_game.addMagicEffect(curPos, MAGIC_EFFECT_POFF);
		g_game.addMagicEffect(newPos, MAGIC_EFFECT_TELEPORT);
	}
}

void House::removePlayers(bool ignoreInvites)
{
	std::vector<Player*> kickList;
	for (HouseTileList::iterator it = houseTiles.begin(); it != houseTiles.end(); ++it) {
		CreatureVector* creatures = (*it)->getCreatures();
		if (!creatures) {
			continue;
		}

		Player* player = nullptr;
		for (CreatureVector::iterator cit = creatures->begin(); cit != creatures->end(); ++cit) {
			if ((player = (*cit)->getPlayer()) && !player->isRemoved()
				&& (ignoreInvites || !isInvited(player))) {
				kickList.push_back(player);
			}
		}
	}

	if (kickList.empty()) {
		return;
	}

	for (auto it = kickList.begin(); it != kickList.end(); ++it) {
		removePlayer((*it), false);
	}
}

bool House::kickPlayer(Player* player, Player* target)
{
	if (!target || target->isRemoved()) {
		return false;
	}

	HouseTile* houseTile = target->getTile()->getHouseTile();
	if (!houseTile || houseTile->getHouse() != this) {
		return false;
	}

	bool self = player == target;
	if (getHouseAccessLevel(player) < getHouseAccessLevel(target) && !self) {
		return false;
	}

	removePlayer(target, self);
	return true;
}

void House::clean()
{
	for (HouseBedList::iterator bit = bedsList.begin(); bit != bedsList.end(); ++bit) {
		if ((*bit)->getSleeper()) {
			(*bit)->wakeUp();
		}
	}

	removePlayers(true);
	transferToDepot();
}

bool House::transferToDepot()
{
	if (!townId) {
		return false;
	}

	Player* player = nullptr;
	if (owner) {
		uint32_t tmp = owner;
		if (isGuild() && !IOGuild::getInstance()->swapGuildIdToOwner(tmp)) {
			tmp = 0;
		}

		if (tmp) {
			player = g_game.getPlayerByGuidEx(tmp);
		}
	}

	Container* tmpContainer = nullptr;
	TileItemVector* items = nullptr;

	ItemList moveList;
	for (HouseTileList::iterator it = houseTiles.begin(); it != houseTiles.end(); ++it) {
		if (!(items = (*it)->getItemList())) {
			continue;
		}

		for (ItemVector::iterator iit = items->begin(); iit != items->end(); ++iit) {
			if ((*iit)->isPickupable()) {
				moveList.push_back(*iit);
			} else if ((tmpContainer = (*iit)->getContainer())) {
				for (ItemList::const_iterator cit = tmpContainer->getItems(); cit != tmpContainer->getEnd(); ++cit) {
					moveList.push_back(*cit);
				}
			}
		}
	}

	if (player) {
		Depot* depot = player->getDepot(townId, true);
		for (ItemList::iterator it = moveList.begin(); it != moveList.end(); ++it) {
			g_game.internalMoveItem(nullptr, (*it)->getParent(), depot, INDEX_WHEREEVER, (*it), (*it)->getItemCount(), nullptr, FLAG_NOLIMIT);
		}

		if (player->isVirtual()) {
			IOLoginData::getInstance()->savePlayer(player);
			delete player;
		}
	} else {
		for (ItemList::iterator it = moveList.begin(); it != moveList.end(); ++it) {
			g_game.internalRemoveItem(nullptr, (*it), (*it)->getItemCount(), false, FLAG_NOLIMIT);
		}
	}

	return true;
}

bool House::isInvited(const Player* player)
{
	return getHouseAccessLevel(player) != HOUSE_NO_INVITED;
}

AccessHouseLevel_t House::getHouseAccessLevel(const Player* player)
{
	if (!player) {
		return HOUSE_NO_INVITED;
	}

	// CUSTOM: Protect House
	if (g_config.getBool(ConfigManager::HOUSE_OWNED_BY_ACCOUNT) && player->getAccount() == ownerAccountId) {
		return HOUSE_OWNER;
	}

	if (player->hasFlag(PlayerFlag_CanEditHouses)) {
		return HOUSE_OWNER;
	}

	if (!owner) {
		return HOUSE_NO_INVITED;
	}

	AccessHouseLevel_t tmp = HOUSE_NO_INVITED;
	if (isGuild()) {
		if (player->getGuildId() == owner) {
			switch (player->getGuildLevel()) {
				case GUILDLEVEL_LEADER:
					return HOUSE_OWNER;
				case GUILDLEVEL_VICE:
					return HOUSE_SUBOWNER;
				default:
					tmp = HOUSE_GUEST;
			}
		}
	} else if (player->getGUID() == owner || player->marriage == owner) {
		return HOUSE_OWNER;
	}

	if (subOwnerList.isInList(player)) {
		return HOUSE_SUBOWNER;
	}

	if (guestList.isInList(player)) {
		return HOUSE_GUEST;
	}

	return tmp;
}

bool House::canEditAccessList(uint32_t listId, const Player* player)
{
	switch (getHouseAccessLevel(player)) {
		case HOUSE_OWNER:
			return true;
		case HOUSE_SUBOWNER:
			return listId == GUEST_LIST;
		default:
			break;
	}

	return false;
}

bool House::getAccessList(uint32_t listId, std::string& list) const
{
	if (listId == GUEST_LIST) {
		guestList.getList(list);
		return true;
	}

	if (listId == SUBOWNER_LIST) {
		subOwnerList.getList(list);
		return true;
	}

	if (Door* door = getDoorByNumber(listId)) {
		return door->getAccessList(list);
	}

#ifdef __DEBUG_HOUSES__
	std::clog << "[Failure - House::getAccessList] door == nullptr, listId = " << listId << std::endl;
#endif
	return false;
}

void House::setAccessList(uint32_t listId, const std::string& textlist, bool teleport /* = true*/)
{
	if (listId == GUEST_LIST) {
		guestList.parseList(textlist);
	} else if (listId == SUBOWNER_LIST) {
		subOwnerList.parseList(textlist);
	} else {
		if (Door* door = getDoorByNumber(listId)) {
			door->setAccessList(textlist);
		}
#ifdef __DEBUG_HOUSES__
		else {
			std::clog << "[Failure - House::setAccessList] door == nullptr, listId = " << listId << std::endl;
		}
#endif

		return;
	}

	if (teleport) {
		removePlayers(false);
	}
}

TransferItem* TransferItem::createTransferItem(House* house)
{
	TransferItem* transferItem = new TransferItem(house);
	transferItem->addRef();
	transferItem->setID(ITEM_HOUSE_TRANSFER);

	char buffer[150];
	sprintf(buffer, "It is a %s transfer document for '%s'.", house->isGuild() ? "guild hall" : "house", house->getName().c_str());
	transferItem->setSpecialDescription(buffer);

	transferItem->setSubType(1);
	return transferItem;
}

bool TransferItem::onTradeEvent(TradeEvents_t event, Player* owner, Player* seller)
{
	switch (event) {
		case ON_TRADE_TRANSFER: {
			if (house) {
				house->setOwnerEx(owner->getGUID(), true);
			}

			g_game.internalRemoveItem(nullptr, this, getItemCount());
			seller->transferContainer.setParent(nullptr);
			break;
		}

		case ON_TRADE_CANCEL: {
			owner->transferContainer.setParent(nullptr);
			owner->transferContainer.__removeThing(this, getItemCount());
			g_game.freeThing(this);
			break;
		}

		default:
			return false;
	}

	return true;
}

void AccessList::getList(std::string& _list) const
{
	_list = list;
}

bool AccessList::parseList(const std::string& _list)
{
	playerList.clear();
	guildList.clear();

	list = _list;
	if (_list.empty()) {
		return true;
	}

	std::stringstream listStream(_list);
	std::string line;
	while (getline(listStream, line)) {
		otx::util::trim_string(line, ' ');
		otx::util::trim_string(line, '\t');
		otx::util::to_lower_string(line);
		if (line.substr(0, 1) == "#" || line.length() > 100) {
			continue;
		}

		if (line.find("@") != std::string::npos) {
			std::string::size_type pos = line.find("@");
			addGuild(line.substr(pos + 1), line.substr(0, pos));
		} else if (line == "*") {
			m_allowEveryone = true;
		} else if (line.find("!") != std::string::npos || line.find("*") != std::string::npos || line.find("?") != std::string::npos) {
			// regex expressions no longer supported
			continue;
		} else {
			addPlayer(line);
		}
	}

	return true;
}

bool AccessList::isInList(const Player* player)
{
	if (m_allowEveryone) {
		return true;
	}

	if (playerList.find(player->getGUID()) != playerList.end()) {
		return true;
	}

	for (GuildList::iterator git = guildList.begin(); git != guildList.end(); ++git) {
		if (git->first == player->getGuildId() && ((uint32_t)git->second == player->getRankId() || git->second == -1)) {
			return true;
		}
	}
	return false;
}

bool AccessList::addPlayer(std::string& name)
{
	std::string tmp = name;
	uint32_t guid;
	if (!IOLoginData::getInstance()->getGuidByName(guid, tmp) || playerList.find(guid) != playerList.end()) {
		return false;
	}

	playerList.insert(guid);
	return true;
}

bool AccessList::addGuild(const std::string& guildName, const std::string& rankName)
{
	uint32_t guildId;
	if (!IOGuild::getInstance()->getGuildId(guildId, guildName)) {
		return false;
	}

	std::string tmp = rankName;
	int32_t rankId = IOGuild::getInstance()->getRankIdByName(guildId, tmp);
	if (!rankId && (tmp.find("?") == std::string::npos || tmp.find("!") == std::string::npos || tmp.find("*") == std::string::npos)) {
		rankId = -1;
	}

	if (!rankId) {
		return false;
	}

	for (GuildList::iterator git = guildList.begin(); git != guildList.end(); ++git) {
		if (git->first == guildId && git->second == rankId) {
			return true;
		}
	}

	guildList.push_back(std::make_pair(guildId, rankId));
	return true;
}

Door::~Door()
{
	delete accessList;
}

Attr_ReadValue Door::readAttr(AttrTypes_t attr, PropStream& propStream)
{
	if (attr != ATTR_HOUSEDOORID) {
		return Item::readAttr(attr, propStream);
	}

	uint8_t _doorId = 0;
	if (!propStream.getByte(_doorId)) {
		return ATTR_READ_ERROR;
	}

	doorId = _doorId;
	return ATTR_READ_CONTINUE;
}

void Door::copyAttributes(Item* item)
{
	Item::copyAttributes(item);
	if (Door* door = item->getDoor()) {
		doorId = door->getDoorId();
		std::string list;
		if (door->getAccessList(list)) {
			setAccessList(list);
		}
	}
}

void Door::onRemoved()
{
	Item::onRemoved();
	if (house) {
		house->removeDoor(this);
	}
}

bool Door::canUse(const Player* player)
{
	if (!house || house->getHouseAccessLevel(player) >= HOUSE_SUBOWNER) {
		return true;
	}

	return accessList->isInList(player);
}

void Door::setHouse(House* _house)
{
	if (house) {
		return;
	}

	house = _house;
	if (!accessList) {
		accessList = new AccessList();
	}
}

bool Door::getAccessList(std::string& list) const
{
	if (!house) {
		return false;
	}

	accessList->getList(list);
	return true;
}

void Door::setAccessList(const std::string& textlist)
{
	if (!accessList) {
		accessList = new AccessList();
	}

	accessList->parseList(textlist);
}

Houses::Houses()
{
	rentPeriod = RENTPERIOD_NEVER;
	std::string strValue = otx::util::as_lower_string(g_config.getString(ConfigManager::HOUSE_RENT_PERIOD));
	if (strValue == "yearly") {
		rentPeriod = RENTPERIOD_YEARLY;
	} else if (strValue == "monthly") {
		rentPeriod = RENTPERIOD_MONTHLY;
	} else if (strValue == "weekly") {
		rentPeriod = RENTPERIOD_WEEKLY;
	} else if (strValue == "daily") {
		rentPeriod = RENTPERIOD_DAILY;
	}
}

bool Houses::loadFromXml(std::string filename)
{
	xmlDocPtr doc = xmlParseFile(filename.c_str());
	if (!doc) {
		std::clog << "[Warning - Houses::loadFromXml] Cannot load houses file." << std::endl;
		std::clog << getLastXMLError() << std::endl;
		return false;
	}

	xmlNodePtr houseNode, root = xmlDocGetRootElement(doc);
	if (xmlStrcmp(root->name, (const xmlChar*)"houses")) {
		std::clog << "[Error - Houses::loadFromXml] Malformed houses file." << std::endl;
		xmlFreeDoc(doc);
		return false;
	}

	int32_t intValue;
	std::string strValue;

	houseNode = root->children;
	while (houseNode) {
		if (xmlStrcmp(houseNode->name, (const xmlChar*)"house")) {
			houseNode = houseNode->next;
			continue;
		}

		int32_t houseId = 0;
		if (!readXMLInteger(houseNode, "houseid", houseId)) {
			std::clog << "[Error - Houses::loadFromXml] Could not read houseId" << std::endl;
			xmlFreeDoc(doc);
			return false;
		}

		House* house = Houses::getInstance()->getHouse(houseId);
		if (!house) {
			std::clog << "[Error - Houses::loadFromXml] Unknown house with id: " << houseId << std::endl;
			xmlFreeDoc(doc);
			return false;
		}

		Position entry(0, 0, 0);
		if (readXMLInteger(houseNode, "entryx", intValue)) {
			entry.x = intValue;
		}

		if (readXMLInteger(houseNode, "entryy", intValue)) {
			entry.y = intValue;
		}

		if (readXMLInteger(houseNode, "entryz", intValue)) {
			entry.z = intValue;
		}

		if (readXMLString(houseNode, "name", strValue)) {
			house->setName(strValue);
		} else {
			house->resetSyncFlag(House::HOUSE_SYNC_NAME);
		}

		house->setEntry(entry);
		if (!entry.x || !entry.y) {
			std::clog << "[Warning - Houses::loadFromXml] House entry not set for: "
					  << house->getName() << " (" << houseId << ")" << std::endl;
		}

		if (readXMLInteger(houseNode, "townid", intValue)) {
			house->setTownId(intValue);
		} else {
			house->resetSyncFlag(House::HOUSE_SYNC_TOWN);
		}

		if (readXMLInteger(houseNode, "size", intValue)) {
			house->setSize(intValue);
		} else {
			house->resetSyncFlag(House::HOUSE_SYNC_SIZE);
		}

		if (readXMLString(houseNode, "guildhall", strValue)) {
			house->setGuild(booleanString(strValue));
		} else {
			house->resetSyncFlag(House::HOUSE_SYNC_GUILD);
		}

		uint32_t rent = 0;
		if (readXMLInteger(houseNode, "rent", intValue)) {
			rent = intValue;
		}

		uint32_t price = (house->getSize() + house->getBedsCount()) * g_config.getNumber(ConfigManager::HOUSE_PRICE);
		// we should let players to pay only for walkable tiles + beds as single units not two items.
		if (g_config.getBool(ConfigManager::HOUSE_RENTASPRICE) && rent) {
			price = rent;
		}

		house->setPrice(price);
		if (g_config.getBool(ConfigManager::HOUSE_PRICEASRENT)) {
			house->setRent(price);
		} else {
			house->setRent(rent);
		}

		house->setOwner(0);
		houseNode = houseNode->next;
	}

	xmlFreeDoc(doc);
	return true;
}

void Houses::check()
{
	uint64_t start = otx::util::mstime();
	std::clog << "> Checking houses..." << std::endl;

	time_t currentTime = time(nullptr);
	for (HouseMap::iterator it = houseMap.begin(); it != houseMap.end(); ++it) {
		payHouse(it->second, currentTime, 0);
	}

	std::clog << "Houses checked in " << (otx::util::mstime() - start) / (1000.) << " seconds." << std::endl;
}

bool Houses::payRent(Player* player, House* house, uint32_t bid, time_t _time /* = 0*/)
{
	if (rentPeriod == RENTPERIOD_NEVER || !house->getOwner() || house->getPaidUntil() > _time || !house->getRent() || player->hasCustomFlag(PlayerCustomFlag_IgnoreHouseRent)) {
		return true;
	}

	Town* town = Towns::getInstance()->getTown(house->getTownId());
	if (!town) {
		return false;
	}

	bool paid = false;
	uint32_t amount = house->getRent() + bid;
	if (g_config.getBool(ConfigManager::BANK_SYSTEM) && player->balance >= amount) {
		player->balance -= amount;
		paid = true;
	} else if (Depot* depot = player->getDepot(town->getID(), true)) {
		paid = g_game.removeMoney(depot, amount, FLAG_NOLIMIT);
	}

	if (!paid) {
		return false;
	}

	if (!_time) {
		_time = time(nullptr);
	}

	uint32_t paidUntil = _time;
	switch (rentPeriod) {
		case RENTPERIOD_DAILY:
			paidUntil += 86400;
			break;
		case RENTPERIOD_WEEKLY:
			paidUntil += 7 * 86400;
			break;
		case RENTPERIOD_MONTHLY:
			paidUntil += 30 * 86400;
			break;
		case RENTPERIOD_YEARLY:
			paidUntil += 365 * 86400;
			break;
		default:
			break;
	}

	house->setLastWarning(0);
	house->setRentWarnings(0);

	house->setPaidUntil(paidUntil);
	return true;
}

bool Houses::payHouse(House* house, time_t _time, uint32_t bid)
{
	if (rentPeriod == RENTPERIOD_NEVER || !house->getOwner() || house->getPaidUntil() > _time || !house->getRent()) {
		return true;
	}

	Town* town = Towns::getInstance()->getTown(house->getTownId());
	if (!town) {
		return false;
	}

	uint32_t owner = house->getOwner();
	if (house->isGuild() && !IOGuild::getInstance()->swapGuildIdToOwner(owner)) {
		house->setOwnerEx(0, true);
		return false;
	}

	std::string name;
	if (!IOLoginData::getInstance()->getNameByGuid(owner, name)) {
		house->setOwnerEx(0, true);
		return false;
	}

	Player* player = g_game.getPlayerByNameEx(name);
	if (!player) {
		return false;
	}

	if (!player->isPremium() && g_config.getBool(ConfigManager::HOUSE_NEED_PREMIUM)) {
		house->setOwnerEx(0, true);
		if (player->isVirtual()) {
			delete player;
		}

		return false;
	}

	int32_t loginClean = g_config.getNumber(ConfigManager::HOUSE_CLEAN_OLD);
	if (loginClean && _time >= (player->getLastLogin() + loginClean)) {
		house->setOwnerEx(0, true);
		if (player->isVirtual()) {
			delete player;
		}

		return false;
	}

	if (payRent(player, house, bid, _time) || _time < (house->getLastWarning() + 86400)) {
		if (player->isVirtual()) {
			IOLoginData::getInstance()->savePlayer(player);
			delete player;
		}

		return true;
	}

	uint32_t warningsLimit = 7;
	switch (rentPeriod) {
		case RENTPERIOD_DAILY:
			warningsLimit = 1;
			break;
		case RENTPERIOD_WEEKLY:
			warningsLimit = 3;
			break;
		case RENTPERIOD_YEARLY:
			warningsLimit = 14;
			break;
		default:
			break;
	}

	uint32_t warnings = house->getRentWarnings();
	if (warnings >= warningsLimit) {
		house->setOwnerEx(0, true);
		if (player->isVirtual()) {
			delete player;
		}

		return false;
	}

	if (Depot* depot = player->getDepot(town->getID(), true)) {
		if (Item* letter = Item::CreateItem(ITEM_LETTER_STAMPED)) {
			if (g_game.internalAddItem(nullptr, depot, letter, INDEX_WHEREEVER, FLAG_NOLIMIT) == RET_NOERROR) {
				letter->setWriter(g_config.getString(ConfigManager::SERVER_NAME));
				letter->setDate(std::time(nullptr));
				std::ostringstream s;

				s << "Warning!\nThe ";
				switch (rentPeriod) {
					case RENTPERIOD_DAILY:
						s << "daily";
						break;
					case RENTPERIOD_WEEKLY:
						s << "weekly";
						break;
					case RENTPERIOD_MONTHLY:
						s << "monthly";
						break;
					case RENTPERIOD_YEARLY:
						s << "annual";
						break;
					default:
						break;
				}

				s << " rent of " << house->getRent() << " gold for your "
				  << (house->isGuild() ? "guild hall" : "house") << " \"" << house->getName()
				  << "\" has to be paid. Have it within " << (warningsLimit - warnings)
				  << " days or you will lose your " << (house->isGuild() ? "guild hall" : "house") << ".";

				letter->setText(s.str().c_str());
				if (player->isVirtual()) {
					IOLoginData::getInstance()->savePlayer(player);
				}
			}

			else {
				g_game.freeThing(letter);
			}
		}
	}

	house->setLastWarning(_time);
	house->setRentWarnings(++warnings);
	if (player->isVirtual()) {
		delete player;
	}

	return false;
}

House* Houses::getHouse(uint32_t houseId, bool add /*= false*/)
{
	HouseMap::iterator it = houseMap.find(houseId);
	if (it != houseMap.end()) {
		return it->second;
	}

	if (!add) {
		return nullptr;
	}

	houseMap[houseId] = new House(houseId);
	return houseMap[houseId];
}

House* Houses::getHouseByPlayer(Player* player)
{
	if (!player || player->isRemoved()) {
		return nullptr;
	}

	HouseTile* houseTile = player->getTile()->getHouseTile();
	if (!houseTile) {
		return nullptr;
	}

	if (House* house = houseTile->getHouse()) {
		return house;
	}

	return nullptr;
}

House* Houses::getHouseByPlayerId(uint32_t playerId)
{
	for (HouseMap::iterator it = houseMap.begin(); it != houseMap.end(); ++it) {
		if (!it->second->isGuild() && it->second->getOwner() == playerId) {
			return it->second;
		}
	}
	return nullptr;
}

House* Houses::getHouseByGuildId(uint32_t guildId)
{
	for (HouseMap::iterator it = houseMap.begin(); it != houseMap.end(); ++it) {
		if (it->second->isGuild() && it->second->getOwner() == guildId) {
			return it->second;
		}
	}

	return nullptr;
}

uint32_t Houses::getHousesCount(uint32_t accId)
{
	std::ostringstream query;
	query << "SELECT `id` FROM `players` WHERE `account_id` = " << accId << " AND `deleted` = 0";

	DBResultPtr result = g_database.storeQuery(query.str());
	if (!result) {
		return 0;
	}

	uint32_t count = 0;
	do {
		if (getHouseByPlayerId(result->getNumber<uint32_t>("id"))) {
			++count;
		}
	} while (result->next());
	return count;
}
