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

#include "items.h"

#include "condition.h"
#include "configmanager.h"
#include "movement.h"
#include "tools.h"
#include "weapons.h"

#include "otx/util.hpp"

uint32_t Items::dwMajorVersion = 0;
uint32_t Items::dwMinorVersion = 0;
uint32_t Items::dwBuildNumber = 0;

Items::Items()
{
	nameToItems.reserve(20000);
	items.reserve(20000);
	clientIdToServerId.reserve(20000);
}

void Items::clear()
{
	moneyMap.clear();
	nameToItems.clear();
	clientIdToServerId.clear();
	items.clear();
}

bool Items::reload()
{
	clear();

	loadFromOtb(getFilePath(FILE_TYPE_OTHER, "items/items.otb"));
	if (!loadFromXml()) {
		return false;
	}

	g_moveEvents.reload();
	g_weapons.reload();
	return true;
}

bool Items::loadFromOtb(const std::string& file)
{
	FileLoader f;
	if (!f.openFile(file.data(), "OTBI", false, true)) {
		return false;
	}

	uint32_t type;
	NODE node = f.getChildNode(NO_NODE, type);

	PropStream props;
	if (f.getProps(node, props)) {
		// 4 byte flags
		// attributes
		// 0x01 = version data
		uint32_t flags;
		if (!props.getLong(flags)) {
			return false;
		}

		attribute_t attr;
		if (!props.getType(attr)) {
			return false;
		}

		if (attr == ROOT_ATTR_VERSION) {
			datasize_t length = 0;
			if (!props.getType(length)) {
				return false;
			}

			if (length != sizeof(VERSIONINFO)) {
				return false;
			}

			VERSIONINFO vi;
			if (!props.getType(vi)) {
				return false;
			}

			Items::dwMajorVersion = vi.dwMajorVersion; // items otb format file version
			Items::dwMinorVersion = vi.dwMinorVersion; // client version
			Items::dwBuildNumber = vi.dwBuildNumber; // revision
		}
	}

	if (Items::dwMajorVersion == 0xFFFFFFFF) {
		std::clog << "[Warning - Items::loadFromOtb] items.otb using generic client version." << std::endl;
	} else if (Items::dwMajorVersion != 3) {
		std::clog << "[Error - Items::loadFromOtb] Incorrect version detected, please use official items.otb." << std::endl;
		return false;
	} else if (!otx::config::getBoolean(otx::config::SKIP_ITEMS_VERSION) && Items::dwMinorVersion != CLIENT_VERSION_ITEMS) {
		std::clog << "[Error - Items::loadFromOtb] Another client version of items.otb is required." << std::endl;
		return false;
	}

	for (node = f.getChildNode(node, type); node != NO_NODE; node = f.getNextNode(node, type)) {
		if (!f.getProps(node, props)) {
			return false;
		}

		// read 4 byte flags
		flags_t flags;
		if (!props.getType(flags)) {
			return false;
		}

		uint16_t serverId = 0;
		uint16_t clientId = 0;
		uint16_t speed = 0;
		uint16_t lightLevel = 0;
		uint16_t lightColor = 0;
		uint16_t wareId = 0;
		uint8_t topOrder = 0;

		attribute_t attr;
		while (props.getType(attr)) {
			// size of data
			datasize_t length = 0;
			if (!props.getType(length)) {
				return false;
			}

			switch (attr) {
				case ITEM_ATTR_SERVERID: {
					if (length != sizeof(uint16_t)) {
						return false;
					}

					if (!props.getShort(serverId)) {
						return false;
					}
					break;
				}
				case ITEM_ATTR_CLIENTID: {
					if (length != sizeof(uint16_t)) {
						return false;
					}

					if (!props.getShort(clientId)) {
						return false;
					}
					break;
				}
				case ITEM_ATTR_SPEED: {
					if (length != sizeof(uint16_t)) {
						return false;
					}

					if (!props.getShort(speed)) {
						return false;
					}
					break;
				}
				case ITEM_ATTR_LIGHT2: {
					if (length != sizeof(uint16_t) * 2) {
						return false;
					}

					if (!props.getShort(lightLevel) || !props.getShort(lightColor)) {
						return false;
					}
					break;
				}
				case ITEM_ATTR_TOPORDER: {
					if (length != sizeof(uint8_t)) {
						return false;
					}

					if (!props.getByte(topOrder)) {
						return false;
					}
					break;
				}
				case ITEM_ATTR_WAREID: {
					if (length != sizeof(uint16_t)) {
						return false;
					}

					if (!props.getShort(wareId)) {
						return false;
					}
					break;
				}
				default: {
					// skip unknown attributes
					if (!props.skip(length)) {
						return false;
					}
					break;
				}
			}
		}

		// store the found item
		if (serverId >= items.size()) {
			items.resize(serverId + 1);
		}

		if (clientId >= clientIdToServerId.size()) {
			clientIdToServerId.resize(clientId + 1, 0);
		}

		clientIdToServerId[clientId] = serverId;

		ItemType& iType = items[serverId];
		iType.id = serverId;
		iType.clientId = clientId;
		iType.speed = speed;
		iType.lightLevel = lightLevel;
		iType.lightColor = lightColor;
		iType.alwaysOnTopOrder = topOrder;
		iType.group = static_cast<itemgroup_t>(type);

		switch (type) {
			case ITEM_GROUP_CONTAINER:
				iType.type = ITEM_TYPE_CONTAINER;
				break;
			case ITEM_GROUP_DOOR:
				// not used
				iType.type = ITEM_TYPE_DOOR;
				break;
			case ITEM_GROUP_MAGICFIELD:
				// not used
				iType.type = ITEM_TYPE_MAGICFIELD;
				break;
			case ITEM_GROUP_TELEPORT:
				// not used
				iType.type = ITEM_TYPE_TELEPORT;
				break;
			case ITEM_GROUP_NONE:
			case ITEM_GROUP_GROUND:
			case ITEM_GROUP_SPLASH:
			case ITEM_GROUP_FLUID:
			case ITEM_GROUP_CHARGES:
			case ITEM_GROUP_DEPRECATED:
				break;

			default:
				return false;
		}

		iType.blockSolid = hasBitSet(FLAG_BLOCK_SOLID, flags);
		iType.blockProjectile = hasBitSet(FLAG_BLOCK_PROJECTILE, flags);
		iType.blockPathFind = hasBitSet(FLAG_BLOCK_PATHFIND, flags);
		iType.hasHeight = hasBitSet(FLAG_HAS_HEIGHT, flags);
		iType.usable = hasBitSet(FLAG_USABLE, flags);
		iType.pickupable = hasBitSet(FLAG_PICKUPABLE, flags);
		iType.movable = hasBitSet(FLAG_MOVABLE, flags);
		iType.stackable = hasBitSet(FLAG_STACKABLE, flags);

		iType.alwaysOnTop = hasBitSet(FLAG_ALWAYSONTOP, flags);
		iType.isVertical = hasBitSet(FLAG_VERTICAL, flags);
		iType.isHorizontal = hasBitSet(FLAG_HORIZONTAL, flags);
		iType.isHangable = hasBitSet(FLAG_HANGABLE, flags);
		iType.allowDistRead = hasBitSet(FLAG_ALLOWDISTREAD, flags);
		iType.rotable = hasBitSet(FLAG_ROTABLE, flags);
		iType.canReadText = hasBitSet(FLAG_READABLE, flags);
		iType.lookThrough = hasBitSet(FLAG_LOOKTHROUGH, flags);
		iType.isAnimation = hasBitSet(FLAG_ANIMATION, flags);
		iType.walkStack = !hasBitSet(FLAG_WALKSTACK, flags);
	}

	items.shrink_to_fit();
	clientIdToServerId.shrink_to_fit();
	return true;
}

bool Items::loadFromXml()
{
	xmlDocPtr doc = xmlParseFile(getFilePath(FILE_TYPE_OTHER, "items/items.xml").c_str());
	if (!doc) {
		std::clog << "[Warning - Items::loadFromXml] Cannot load items file."
				  << std::endl
				  << getLastXMLError() << std::endl;
		return false;
	}

	xmlNodePtr root = xmlDocGetRootElement(doc);
	if (xmlStrcmp(root->name, reinterpret_cast<const xmlChar*>("items"))) {
		xmlFreeDoc(doc);
		std::clog << "[Warning - Items::loadFromXml] Malformed items file." << std::endl;
		return false;
	}

	std::string strValue, endValue;
	for (xmlNodePtr node = root->children; node; node = node->next) {
		if (xmlStrcmp(node->name, reinterpret_cast<const xmlChar*>("item"))) {
			continue;
		}

		if (readXMLString(node, "id", strValue)) {
			auto strVector = explodeString(strValue, ";");
			for (auto it = strVector.begin(); it != strVector.end(); ++it) {
				auto intVector = vectorAtoi(explodeString(*it, "-"));
				if (intVector.size() > 1) {
					int32_t i = intVector[0];
					while (i <= intVector[1]) {
						parseItemNode(node, i++);
					}
				} else {
					parseItemNode(node, atoi((*it).c_str()));
				}
			}
		} else if (readXMLString(node, "fromid", strValue) && readXMLString(node, "toid", endValue)) {
			auto intVector = vectorAtoi(explodeString(strValue, ";"));
			auto endVector = vectorAtoi(explodeString(endValue, ";"));
			if (intVector[0] && intVector.size() == endVector.size()) {
				size_t size = intVector.size();
				for (size_t i = 0; i < size; ++i) {
					while (intVector[i] <= endVector[i]) {
						parseItemNode(node, intVector[i]++);
					}
				}
			} else {
				std::clog << "[Warning - Items::loadFromXml] Malformed entry (from: \"" << strValue << "\", to: \"" << endValue << "\")" << std::endl;
			}
		} else {
			std::clog << "[Warning - Items::loadFromXml] No itemid found at line " << node->line << std::endl;
		}
	}

	// lets do some checks...
	for (const ItemType& it : items) {
		if (it.id == 0) {
			continue;
		}

		// check bed items
		if ((it.transformBed[PLAYERSEX_FEMALE] || it.transformBed[PLAYERSEX_MALE]) && it.type != ITEM_TYPE_BED) {
			std::clog << "[Warning - Items::loadFromXml] Item " << it.id << " is not set as a bed-type." << std::endl;
		}
	}

	xmlFreeDoc(doc);
	return true;
}

void Items::parseItemNode(xmlNodePtr itemNode, uint16_t id)
{
	ItemType& it = getItemType(id);
	if (it.loaded) {
		std::clog << "[Warning - Items::parseItemNode] Duplicate registered item with id " << id << " (" << it.name << ')' << std::endl;
		return;
	}

	it.loaded = true;

	int32_t intValue;
	std::string strValue;
	if (readXMLString(itemNode, "name", strValue)) {
		it.name = strValue;
	}

	if (!it.name.empty()) {
		nameToItems.emplace(otx::util::as_lower_string(it.name), id);
	}

	if (readXMLString(itemNode, "article", strValue)) {
		it.article = strValue;
	}

	if (readXMLString(itemNode, "plural", strValue)) {
		it.pluralName = strValue;
	}

	for (xmlNodePtr itemAttributesNode = itemNode->children; itemAttributesNode; itemAttributesNode = itemAttributesNode->next) {
		if (!readXMLString(itemAttributesNode, "key", strValue)) {
			continue;
		}

#ifdef _MSC_VER
		bool notLoaded = false;
#endif
		std::string tmpStrValue = otx::util::as_lower_string(strValue);
		if (tmpStrValue == "type") {
			if (readXMLString(itemAttributesNode, "value", strValue)) {
				tmpStrValue = otx::util::as_lower_string(strValue);
				if (tmpStrValue == "container") {
					it.type = ITEM_TYPE_CONTAINER;
					it.group = ITEM_GROUP_CONTAINER;
				} else if (tmpStrValue == "key") {
					it.type = ITEM_TYPE_KEY;
				} else if (tmpStrValue == "magicfield") {
					it.type = ITEM_TYPE_MAGICFIELD;
				} else if (tmpStrValue == "depot") {
					it.type = ITEM_TYPE_DEPOT;
				} else if (tmpStrValue == "mailbox") {
					it.type = ITEM_TYPE_MAILBOX;
				} else if (tmpStrValue == "trashholder") {
					it.type = ITEM_TYPE_TRASHHOLDER;
				} else if (tmpStrValue == "teleport") {
					it.type = ITEM_TYPE_TELEPORT;
				} else if (tmpStrValue == "door") {
					it.type = ITEM_TYPE_DOOR;
				} else if (tmpStrValue == "bed") {
					it.type = ITEM_TYPE_BED;
				} else if (tmpStrValue == "rune") {
					it.type = ITEM_TYPE_RUNE;
				} else {
					std::clog << "[Warning - Items::loadFromXml] Unknown type " << strValue << std::endl;
				}
			}
		} else if (tmpStrValue == "name") {
			if (readXMLString(itemAttributesNode, "value", strValue)) {
				it.name = strValue;
			}
		} else if (tmpStrValue == "article") {
			if (readXMLString(itemAttributesNode, "value", strValue)) {
				it.article = strValue;
			}
		} else if (tmpStrValue == "plural") {
			if (readXMLString(itemAttributesNode, "value", strValue)) {
				it.pluralName = strValue;
			}
		} else if (tmpStrValue == "clientid") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.clientId = intValue;
				if (it.group == ITEM_GROUP_DEPRECATED) {
					it.group = ITEM_GROUP_NONE;
				}
			}
		} else if (tmpStrValue == "cache") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.cache = (intValue != 0);
			}
		} else if (tmpStrValue == "wareid") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.wareId = intValue;
			}
		} else if (tmpStrValue == "blocksolid" || tmpStrValue == "blocking") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.blockSolid = (intValue != 0);
			}
		} else if (tmpStrValue == "blockprojectile") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.blockProjectile = (intValue != 0);
			}
		} else if (tmpStrValue == "blockpathfind" || tmpStrValue == "blockpathing" || tmpStrValue == "blockpath") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.blockPathFind = (intValue != 0);
			}
		} else if (tmpStrValue == "lightlevel") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.lightLevel = intValue;
			}
		} else if (tmpStrValue == "lightcolor") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.lightColor = intValue;
			}
		} else if (tmpStrValue == "description") {
			if (readXMLString(itemAttributesNode, "value", strValue)) {
				it.description = strValue;
			}
		} else if (tmpStrValue == "runespellname") {
			if (readXMLString(itemAttributesNode, "value", strValue)) {
				it.runeSpellName = strValue;
			}
		} else if (tmpStrValue == "weight") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.weight = intValue / 100.f;
			}
		} else if (tmpStrValue == "showcount") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.showCount = (intValue != 0);
			}
		} else if (tmpStrValue == "armor") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.armor = intValue;
			}
		} else if (tmpStrValue == "defense") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.defense = intValue;
			}
		} else if (tmpStrValue == "extradefense" || tmpStrValue == "extradef") {
			if (readXMLInteger(itemAttributesNode, "chance", intValue)) {
				it.extraDefenseChance = intValue;
			}
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.extraDefense = intValue;
			}
		} else if (tmpStrValue == "attack") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.attack = intValue;
			}
		} else if (tmpStrValue == "extraattack" || tmpStrValue == "extraatk") {
			if (readXMLInteger(itemAttributesNode, "chance", intValue)) {
				it.extraAttackChance = intValue;
			}
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.extraAttack = intValue;
			}
		} else if (tmpStrValue == "attackspeed") {
			if (readXMLInteger(itemAttributesNode, "chance", intValue)) {
				it.attackSpeedChance = intValue;
			}
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.attackSpeed = intValue;
			}
		} else if (tmpStrValue == "rotateto") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.rotateTo = intValue;
			}
		} else if (tmpStrValue == "movable" || tmpStrValue == "moveable") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.movable = (intValue != 0);
			}
		} else if (tmpStrValue == "vertical" || tmpStrValue == "isvertical") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.isVertical = (intValue != 0);
			}
		} else if (tmpStrValue == "horizontal" || tmpStrValue == "ishorizontal") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.isHorizontal = (intValue != 0);
			}
		} else if (tmpStrValue == "pickupable") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.pickupable = (intValue != 0);
			}
		} else if (tmpStrValue == "allowpickupable") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.allowPickupable = (intValue != 0);
			}
		} else if (tmpStrValue == "floorchange") {
			if (readXMLString(itemAttributesNode, "value", strValue)) {
				tmpStrValue = otx::util::as_lower_string(strValue);
				if (tmpStrValue == "down") {
					it.floorChange[CHANGE_DOWN] = true;
				} else if (tmpStrValue == "north") {
					it.floorChange[CHANGE_NORTH] = true;
				} else if (tmpStrValue == "south") {
					it.floorChange[CHANGE_SOUTH] = true;
				} else if (tmpStrValue == "west") {
					it.floorChange[CHANGE_WEST] = true;
				} else if (tmpStrValue == "east") {
					it.floorChange[CHANGE_EAST] = true;
				} else if (tmpStrValue == "northex") {
					it.floorChange[CHANGE_NORTH_EX] = true;
				} else if (tmpStrValue == "southex") {
					it.floorChange[CHANGE_SOUTH_EX] = true;
				} else if (tmpStrValue == "westex") {
					it.floorChange[CHANGE_WEST_EX] = true;
				} else if (tmpStrValue == "eastex") {
					it.floorChange[CHANGE_EAST_EX] = true;
				}
			}
		} else if (tmpStrValue == "corpsetype") {
			tmpStrValue = otx::util::as_lower_string(strValue);
			if (readXMLString(itemAttributesNode, "value", strValue)) {
				tmpStrValue = otx::util::as_lower_string(strValue);
				if (tmpStrValue == "venom") {
					it.corpseType = RACE_VENOM;
				} else if (tmpStrValue == "blood") {
					it.corpseType = RACE_BLOOD;
				} else if (tmpStrValue == "undead") {
					it.corpseType = RACE_UNDEAD;
				} else if (tmpStrValue == "fire") {
					it.corpseType = RACE_FIRE;
				} else if (tmpStrValue == "energy") {
					it.corpseType = RACE_ENERGY;
				} else {
					std::clog << "[Warning - Items::loadFromXml] Unknown corpseType " << strValue << std::endl;
				}
			}
		} else if (tmpStrValue == "containersize") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.maxItems = intValue;
				if (it.group == ITEM_GROUP_NONE) {
					it.group = ITEM_GROUP_CONTAINER;
					it.type = ITEM_TYPE_CONTAINER;
				}
			}
		} else if (tmpStrValue == "fluidsource") {
			if (readXMLString(itemAttributesNode, "value", strValue)) {
				tmpStrValue = otx::util::as_lower_string(strValue);
				FluidTypes_t fluid = getFluidType(tmpStrValue);
				if (fluid != FLUID_NONE) {
					it.fluidSource = fluid;
				} else {
					std::clog << "[Warning - Items::loadFromXml] Unknown fluidSource " << strValue << std::endl;
				}
			}
		} else if (tmpStrValue == "writeable" || tmpStrValue == "writable") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.canWriteText = (intValue != 0);
				it.canReadText = (intValue != 0);
			}
		} else if (tmpStrValue == "readable") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.canReadText = (intValue != 0);
			}
		} else if (tmpStrValue == "maxtextlen" || tmpStrValue == "maxtextlength") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.maxTextLength = intValue;
			}
		} else if (tmpStrValue == "text") {
			if (readXMLString(itemAttributesNode, "value", strValue)) {
				it.text = strValue;
			}
		} else if (tmpStrValue == "author" || tmpStrValue == "writer") {
			if (readXMLString(itemAttributesNode, "value", strValue)) {
				it.writer = strValue;
			}
		} else if (tmpStrValue == "date") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.date = intValue;
			}
		} else if (tmpStrValue == "writeonceitemid") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.writeOnceItemId = intValue;
			}
		} else if (tmpStrValue == "wareid") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.wareId = intValue;
			}
		} else if (tmpStrValue == "worth") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				if (moneyMap.find(intValue) != moneyMap.end()) {
					std::clog << "[Warning - Items::loadFromXml] Duplicated money item " << id << " with worth " << intValue << "!" << std::endl;
				} else {
					moneyMap[intValue] = id;
					it.worth = intValue;
				}
			}
		} else if (tmpStrValue == "forceserialize" || tmpStrValue == "forceserialization" || tmpStrValue == "forcesave") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.forceSerialize = (intValue != 0);
			}
		} else if (tmpStrValue == "leveldoor") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.levelDoor = intValue;
			}
		} else if (tmpStrValue == "specialdoor") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.specialDoor = (intValue != 0);
			}
		} else if (tmpStrValue == "closingdoor") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.closingDoor = (intValue != 0);
			}
		} else if (tmpStrValue == "weapontype") {
			if (readXMLString(itemAttributesNode, "value", strValue)) {
				tmpStrValue = otx::util::as_lower_string(strValue);
				if (tmpStrValue == "sword") {
					it.weaponType = WEAPON_SWORD;
				} else if (tmpStrValue == "club") {
					it.weaponType = WEAPON_CLUB;
				} else if (tmpStrValue == "axe") {
					it.weaponType = WEAPON_AXE;
				} else if (tmpStrValue == "shield") {
					it.weaponType = WEAPON_SHIELD;
				} else if (tmpStrValue == "distance" || tmpStrValue == "dist") {
					it.weaponType = WEAPON_DIST;
				} else if (tmpStrValue == "wand" || tmpStrValue == "rod") {
					it.weaponType = WEAPON_WAND;
				} else if (tmpStrValue == "ammunition" || tmpStrValue == "ammo") {
					it.weaponType = WEAPON_AMMO;
				} else if (tmpStrValue == "fist") {
					it.weaponType = WEAPON_FIST;
				} else {
					std::clog << "[Warning - Items::loadFromXml] Unknown weaponType " << strValue << std::endl;
				}
			}
		} else if (tmpStrValue == "slottype") {
			if (readXMLString(itemAttributesNode, "value", strValue)) {
				tmpStrValue = otx::util::as_lower_string(strValue);
				if (tmpStrValue == "head") {
					it.slotPosition |= SLOTP_HEAD;
					it.wieldPosition = SLOT_HEAD;
				} else if (tmpStrValue == "body") {
					it.slotPosition |= SLOTP_ARMOR;
					it.wieldPosition = SLOT_ARMOR;
				} else if (tmpStrValue == "legs") {
					it.slotPosition |= SLOTP_LEGS;
					it.wieldPosition = SLOT_LEGS;
				} else if (tmpStrValue == "feet") {
					it.slotPosition |= SLOTP_FEET;
					it.wieldPosition = SLOT_FEET;
				} else if (tmpStrValue == "backpack") {
					it.slotPosition |= SLOTP_BACKPACK;
					it.wieldPosition = SLOT_BACKPACK;
				} else if (tmpStrValue == "two-handed") {
					it.slotPosition |= SLOTP_TWO_HAND;
					it.wieldPosition = SLOT_HAND;
				} else if (tmpStrValue == "right-hand") {
					it.slotPosition &= ~SLOTP_LEFT;
					it.wieldPosition = SLOT_RIGHT;
				} else if (tmpStrValue == "left-hand") {
					it.slotPosition &= ~SLOTP_RIGHT;
					it.wieldPosition = SLOT_LEFT;
				} else if (tmpStrValue == "necklace") {
					it.slotPosition |= SLOTP_NECKLACE;
					it.wieldPosition = SLOT_NECKLACE;
				} else if (tmpStrValue == "ring") {
					it.slotPosition |= SLOTP_RING;
					it.wieldPosition = SLOT_RING;
				} else if (tmpStrValue == "ammo") {
					it.slotPosition |= SLOTP_AMMO;
					it.wieldPosition = SLOT_AMMO;
				} else if (tmpStrValue == "hand") {
					it.wieldPosition = SLOT_HAND;
				} else {
					std::clog << "[Warning - Items::loadFromXml] Unknown slotType " << strValue << std::endl;
				}
			}
		} else if (tmpStrValue == "ammotype") {
			if (readXMLString(itemAttributesNode, "value", strValue)) {
				it.ammoType = getAmmoType(strValue);
				if (it.ammoType == AMMO_NONE) {
					std::clog << "[Warning - Items::loadFromXml] Unknown ammoType " << strValue << std::endl;
				}
			}
		} else if (tmpStrValue == "shoottype") {
			if (readXMLString(itemAttributesNode, "value", strValue)) {
				ShootEffect_t shoot = getShootType(strValue);
				if (shoot != SHOOT_EFFECT_NONE) {
					it.shootType = shoot;
				} else {
					std::clog << "[Warning - Items::loadFromXml] Unknown shootType " << strValue << std::endl;
				}
			}
		} else if (tmpStrValue == "effect") {
			if (readXMLString(itemAttributesNode, "value", strValue)) {
				MagicEffect_t effect = getMagicEffect(strValue);
				if (effect != MAGIC_EFFECT_NONE) {
					it.magicEffect = effect;
				} else {
					std::clog << "[Warning - Items::loadFromXml] Unknown effect " << strValue << std::endl;
				}
			}
		} else if (tmpStrValue == "range") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.shootRange = intValue;
			}
		} else if (tmpStrValue == "stopduration") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.stopTime = (intValue != 0);
			}
		} else if (tmpStrValue == "decayto") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.decayTo = intValue;
			}
		} else if (tmpStrValue == "transformequipto" || tmpStrValue == "onequipto") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.transformEquipTo = intValue;
			}
		} else if (tmpStrValue == "transformdeequipto" || tmpStrValue == "ondeequipto") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.transformDeEquipTo = intValue;
			}
		} else if (tmpStrValue == "duration") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.decayTime = std::max<int32_t>(0, intValue);
			}
		} else if (tmpStrValue == "showduration") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.showDuration = (intValue != 0);
			}
		} else if (tmpStrValue == "charges") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.charges = intValue;
			}
		} else if (tmpStrValue == "showcharges") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.showCharges = (intValue != 0);
			}
		} else if (tmpStrValue == "showattributes") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.showAttributes = (intValue != 0);
			}
		} else if (tmpStrValue == "breakchance") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.breakChance = std::max(0, std::min(100, intValue));
			}
		} else if (tmpStrValue == "ammoaction") {
			if (readXMLString(itemAttributesNode, "value", strValue)) {
				AmmoAction_t ammo = getAmmoAction(strValue);
				if (ammo != AMMOACTION_NONE) {
					it.ammoAction = ammo;
				} else {
					std::clog << "[Warning - Items::loadFromXml] Unknown ammoAction " << strValue << std::endl;
				}
			}
		} else if (tmpStrValue == "hitchance") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.hitChance = std::max(-100, std::min(100, intValue));
			}
		} else if (tmpStrValue == "maxhitchance") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.maxHitChance = std::max(0, std::min(100, intValue));
			}
		} else if (tmpStrValue == "dualwield") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.dualWield = (intValue != 0);
			}
		} else if (tmpStrValue == "preventloss") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().preventLoss = (intValue != 0);
			}
		} else if (tmpStrValue == "preventdrop") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().preventDrop = (intValue != 0);
			}
		} else if (tmpStrValue == "invisible") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().invisible = (intValue != 0);
			}
		} else if (tmpStrValue == "speed") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().speed = intValue;
			}
		} else if (tmpStrValue == "healthgain") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().regeneration = true;
				it.getAbilities().healthGain = intValue;
			}
		} else if (tmpStrValue == "healthticks") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().regeneration = true;
				it.getAbilities().healthTicks = intValue;
			}
		} else if (tmpStrValue == "managain") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().regeneration = true;
				it.getAbilities().manaGain = intValue;
			}
		} else if (tmpStrValue == "manaticks") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().regeneration = true;
				it.getAbilities().manaTicks = intValue;
			}
		} else if (tmpStrValue == "manashield") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().manaShield = (intValue != 0);
			}
		} else if (tmpStrValue == "skillsword") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().skills[SKILL_SWORD] = intValue;
			}
		} else if (tmpStrValue == "skillaxe") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().skills[SKILL_AXE] = intValue;
			}
		} else if (tmpStrValue == "skillclub") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().skills[SKILL_CLUB] = intValue;
			}
		} else if (tmpStrValue == "skilldist") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().skills[SKILL_DIST] = intValue;
			}
		} else if (tmpStrValue == "skillfish") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().skills[SKILL_FISH] = intValue;
			}
		} else if (tmpStrValue == "skillshield") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().skills[SKILL_SHIELD] = intValue;
			}
		} else if (tmpStrValue == "skillfist") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().skills[SKILL_FIST] = intValue;
			}
		} else if (tmpStrValue == "maxhealthpoints" || tmpStrValue == "maxhitpoints") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().stats[STAT_MAXHEALTH] = intValue;
			}
		} else if (tmpStrValue == "maxhealthpercent" || tmpStrValue == "maxhitpointspercent") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().statsPercent[STAT_MAXHEALTH] = intValue;
			}
		} else if (tmpStrValue == "maxmanapoints") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().stats[STAT_MAXMANA] = intValue;
			}
		} else if (tmpStrValue == "maxmanapercent" || tmpStrValue == "maxmanapointspercent") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().statsPercent[STAT_MAXMANA] = intValue;
			}
		} else if (tmpStrValue == "soulpoints") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().stats[STAT_SOUL] = intValue;
			}
		} else if (tmpStrValue == "soulpercent" || tmpStrValue == "soulpointspercent") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().statsPercent[STAT_SOUL] = intValue;
			}
		} else if (tmpStrValue == "magiclevelpoints" || tmpStrValue == "magicpoints") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().stats[STAT_MAGICLEVEL] = intValue;
			}
		} else if (tmpStrValue == "magiclevelpercent" || tmpStrValue == "magicpointspercent") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().statsPercent[STAT_MAGICLEVEL] = intValue;
			}
		} else if (tmpStrValue == "increasemagicvalue") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().increment[MAGIC_VALUE] = intValue;
			}
		} else if (tmpStrValue == "increasemagicpercent") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().increment[MAGIC_PERCENT] = intValue;
			}
		} else if (tmpStrValue == "increasehealingvalue") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().increment[HEALING_VALUE] = intValue;
			}
		} else if (tmpStrValue == "increasehealingpercent") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().increment[HEALING_PERCENT] = intValue;
			}
		} else if (tmpStrValue == "fieldabsorbpercentenergy") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().fieldAbsorb[COMBATINDEX_ENERGYDAMAGE] += intValue;
			}
		} else if (tmpStrValue == "fieldabsorbpercentfire") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().fieldAbsorb[COMBATINDEX_FIREDAMAGE] += intValue;
			}
		} else if (tmpStrValue == "fieldabsorbpercentpoison" || tmpStrValue == "fieldabsorbpercentearth") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().fieldAbsorb[COMBATINDEX_EARTHDAMAGE] += intValue;
			}
		} else if (tmpStrValue == "absorbpercentall") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				for (uint16_t i = COMBATINDEX_FIRST; i <= COMBATINDEX_LAST; ++i) {
					it.getAbilities().absorb[i] += intValue;
				}
			}
		} else if (tmpStrValue == "absorbpercentelements") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().absorb[COMBATINDEX_ENERGYDAMAGE] += intValue;
				it.getAbilities().absorb[COMBATINDEX_FIREDAMAGE] += intValue;
				it.getAbilities().absorb[COMBATINDEX_EARTHDAMAGE] += intValue;
				it.getAbilities().absorb[COMBATINDEX_ICEDAMAGE] += intValue;
			}
		} else if (tmpStrValue == "absorbpercentmagic") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().absorb[COMBATINDEX_ENERGYDAMAGE] += intValue;
				it.getAbilities().absorb[COMBATINDEX_FIREDAMAGE] += intValue;
				it.getAbilities().absorb[COMBATINDEX_EARTHDAMAGE] += intValue;
				it.getAbilities().absorb[COMBATINDEX_ICEDAMAGE] += intValue;
				it.getAbilities().absorb[COMBATINDEX_HOLYDAMAGE] += intValue;
				it.getAbilities().absorb[COMBATINDEX_DEATHDAMAGE] += intValue;
			}
		} else if (tmpStrValue == "absorbpercentenergy") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().absorb[COMBATINDEX_ENERGYDAMAGE] += intValue;
			}
		} else if (tmpStrValue == "absorbpercentfire") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().absorb[COMBATINDEX_FIREDAMAGE] += intValue;
			}
		} else if (tmpStrValue == "absorbpercentpoison" || tmpStrValue == "absorbpercentearth") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().absorb[COMBATINDEX_EARTHDAMAGE] += intValue;
			}
		} else if (tmpStrValue == "absorbpercentice") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().absorb[COMBATINDEX_ICEDAMAGE] += intValue;
			}
		} else if (tmpStrValue == "absorbpercentholy") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().absorb[COMBATINDEX_HOLYDAMAGE] += intValue;
			}
		} else if (tmpStrValue == "absorbpercentdeath") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().absorb[COMBATINDEX_DEATHDAMAGE] += intValue;
			}
		} else if (tmpStrValue == "absorbpercentlifedrain") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().absorb[COMBATINDEX_LIFEDRAIN] += intValue;
			}
		} else if (tmpStrValue == "absorbpercentmanadrain") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().absorb[COMBATINDEX_MANADRAIN] += intValue;
			}
		} else if (tmpStrValue == "absorbpercentdrown") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().absorb[COMBATINDEX_DROWNDAMAGE] += intValue;
			}
		} else if (tmpStrValue == "absorbpercentphysical") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().absorb[COMBATINDEX_PHYSICALDAMAGE] += intValue;
			}
		} else if (tmpStrValue == "absorbpercenthealing") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().absorb[COMBATINDEX_HEALING] += intValue;
			}
		} else if (tmpStrValue == "absorbpercentundefined") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().absorb[COMBATINDEX_UNDEFINEDDAMAGE] += intValue;
			}
		}
#ifndef _MSC_VER
		else if (tmpStrValue == "reflectpercentall")
#else
		else
			notLoaded = true;

		if (!notLoaded) {
			continue;
		}

		if (tmpStrValue == "reflectpercentall")
#endif
		{
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				for (uint16_t i = COMBATINDEX_FIRST; i <= COMBATINDEX_LAST; ++i) {
					it.getAbilities().reflect[REFLECT_PERCENT][i] += intValue;
				}
			}
		} else if (tmpStrValue == "reflectpercentelements") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().reflect[REFLECT_PERCENT][COMBATINDEX_ENERGYDAMAGE] += intValue;
				it.getAbilities().reflect[REFLECT_PERCENT][COMBATINDEX_FIREDAMAGE] += intValue;
				it.getAbilities().reflect[REFLECT_PERCENT][COMBATINDEX_EARTHDAMAGE] += intValue;
				it.getAbilities().reflect[REFLECT_PERCENT][COMBATINDEX_ICEDAMAGE] += intValue;
			}
		} else if (tmpStrValue == "reflectpercentmagic") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().reflect[REFLECT_PERCENT][COMBATINDEX_ENERGYDAMAGE] += intValue;
				it.getAbilities().reflect[REFLECT_PERCENT][COMBATINDEX_FIREDAMAGE] += intValue;
				it.getAbilities().reflect[REFLECT_PERCENT][COMBATINDEX_EARTHDAMAGE] += intValue;
				it.getAbilities().reflect[REFLECT_PERCENT][COMBATINDEX_ICEDAMAGE] += intValue;
				it.getAbilities().reflect[REFLECT_PERCENT][COMBATINDEX_HOLYDAMAGE] += intValue;
				it.getAbilities().reflect[REFLECT_PERCENT][COMBATINDEX_DEATHDAMAGE] += intValue;
			}
		} else if (tmpStrValue == "reflectpercentenergy") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().reflect[REFLECT_PERCENT][COMBATINDEX_ENERGYDAMAGE] += intValue;
			}
		} else if (tmpStrValue == "reflectpercentfire") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().reflect[REFLECT_PERCENT][COMBATINDEX_FIREDAMAGE] += intValue;
			}
		} else if (tmpStrValue == "reflectpercentpoison" || tmpStrValue == "reflectpercentearth") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().reflect[REFLECT_PERCENT][COMBATINDEX_EARTHDAMAGE] += intValue;
			}
		} else if (tmpStrValue == "reflectpercentice") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().reflect[REFLECT_PERCENT][COMBATINDEX_ICEDAMAGE] += intValue;
			}
		} else if (tmpStrValue == "reflectpercentholy") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().reflect[REFLECT_PERCENT][COMBATINDEX_HOLYDAMAGE] += intValue;
			}
		} else if (tmpStrValue == "reflectpercentdeath") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().reflect[REFLECT_PERCENT][COMBATINDEX_DEATHDAMAGE] += intValue;
			}
		} else if (tmpStrValue == "reflectpercentlifedrain") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().reflect[REFLECT_PERCENT][COMBATINDEX_LIFEDRAIN] += intValue;
			}
		} else if (tmpStrValue == "reflectpercentmanadrain") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().reflect[REFLECT_PERCENT][COMBATINDEX_MANADRAIN] += intValue;
			}
		} else if (tmpStrValue == "reflectpercentdrown") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().reflect[REFLECT_PERCENT][COMBATINDEX_DROWNDAMAGE] += intValue;
			}
		} else if (tmpStrValue == "reflectpercentphysical") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().reflect[REFLECT_PERCENT][COMBATINDEX_PHYSICALDAMAGE] += intValue;
			}
		} else if (tmpStrValue == "reflectpercenthealing") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().reflect[REFLECT_PERCENT][COMBATINDEX_HEALING] += intValue;
			}
		} else if (tmpStrValue == "reflectpercentundefined") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().reflect[REFLECT_PERCENT][COMBATINDEX_UNDEFINEDDAMAGE] += intValue;
			}
		} else if (tmpStrValue == "reflectchanceall") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				for (uint16_t i = COMBATINDEX_FIRST; i <= COMBATINDEX_LAST; ++i) {
					it.getAbilities().reflect[REFLECT_CHANCE][i] += intValue;
				}
			}
		} else if (tmpStrValue == "reflectchanceelements") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().reflect[REFLECT_CHANCE][COMBATINDEX_ENERGYDAMAGE] += intValue;
				it.getAbilities().reflect[REFLECT_CHANCE][COMBATINDEX_FIREDAMAGE] += intValue;
				it.getAbilities().reflect[REFLECT_CHANCE][COMBATINDEX_EARTHDAMAGE] += intValue;
				it.getAbilities().reflect[REFLECT_CHANCE][COMBATINDEX_ICEDAMAGE] += intValue;
			}
		} else if (tmpStrValue == "reflectchancemagic") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().reflect[REFLECT_CHANCE][COMBATINDEX_ENERGYDAMAGE] += intValue;
				it.getAbilities().reflect[REFLECT_CHANCE][COMBATINDEX_FIREDAMAGE] += intValue;
				it.getAbilities().reflect[REFLECT_CHANCE][COMBATINDEX_EARTHDAMAGE] += intValue;
				it.getAbilities().reflect[REFLECT_CHANCE][COMBATINDEX_ICEDAMAGE] += intValue;
				it.getAbilities().reflect[REFLECT_CHANCE][COMBATINDEX_HOLYDAMAGE] += intValue;
				it.getAbilities().reflect[REFLECT_CHANCE][COMBATINDEX_DEATHDAMAGE] += intValue;
			}
		} else if (tmpStrValue == "reflectchanceenergy") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().reflect[REFLECT_CHANCE][COMBATINDEX_ENERGYDAMAGE] += intValue;
			}
		} else if (tmpStrValue == "reflectchancefire") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().reflect[REFLECT_CHANCE][COMBATINDEX_FIREDAMAGE] += intValue;
			}
		} else if (tmpStrValue == "reflectchancepoison" || tmpStrValue == "reflectchanceearth") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().reflect[REFLECT_CHANCE][COMBATINDEX_EARTHDAMAGE] += intValue;
			}
		} else if (tmpStrValue == "reflectchanceice") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().reflect[REFLECT_CHANCE][COMBATINDEX_ICEDAMAGE] += intValue;
			}
		} else if (tmpStrValue == "reflectchanceholy") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().reflect[REFLECT_CHANCE][COMBATINDEX_HOLYDAMAGE] += intValue;
			}
		} else if (tmpStrValue == "reflectchancedeath") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().reflect[REFLECT_CHANCE][COMBATINDEX_DEATHDAMAGE] += intValue;
			}
		} else if (tmpStrValue == "reflectchancelifedrain") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().reflect[REFLECT_CHANCE][COMBATINDEX_LIFEDRAIN] += intValue;
			}
		} else if (tmpStrValue == "reflectchancemanadrain") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().reflect[REFLECT_CHANCE][COMBATINDEX_MANADRAIN] += intValue;
			}
		} else if (tmpStrValue == "reflectchancedrown") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().reflect[REFLECT_CHANCE][COMBATINDEX_DROWNDAMAGE] += intValue;
			}
		} else if (tmpStrValue == "reflectchancephysical") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().reflect[REFLECT_CHANCE][COMBATINDEX_PHYSICALDAMAGE] += intValue;
			}
		} else if (tmpStrValue == "reflectchancehealing") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().reflect[REFLECT_CHANCE][COMBATINDEX_HEALING] += intValue;
			}
		} else if (tmpStrValue == "reflectchanceundefined") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().reflect[REFLECT_CHANCE][COMBATINDEX_UNDEFINEDDAMAGE] += intValue;
			}
		} else if (tmpStrValue == "suppressshock" || tmpStrValue == "suppressenergy") {
			if (readXMLInteger(itemAttributesNode, "value", intValue) && intValue != 0) {
				it.getAbilities().conditionSuppressions |= CONDITION_ENERGY;
			}
		} else if (tmpStrValue == "suppressburn" || tmpStrValue == "suppressfire") {
			if (readXMLInteger(itemAttributesNode, "value", intValue) && intValue != 0) {
				it.getAbilities().conditionSuppressions |= CONDITION_FIRE;
			}
		} else if (tmpStrValue == "suppresspoison" || tmpStrValue == "suppressearth") {
			if (readXMLInteger(itemAttributesNode, "value", intValue) && intValue != 0) {
				it.getAbilities().conditionSuppressions |= CONDITION_POISON;
			}
		} else if (tmpStrValue == "suppressfreeze" || tmpStrValue == "suppressice") {
			if (readXMLInteger(itemAttributesNode, "value", intValue) && intValue != 0) {
				it.getAbilities().conditionSuppressions |= CONDITION_FREEZING;
			}
		} else if (tmpStrValue == "suppressdazzle" || tmpStrValue == "suppressholy") {
			if (readXMLInteger(itemAttributesNode, "value", intValue) && intValue != 0) {
				it.getAbilities().conditionSuppressions |= CONDITION_DAZZLED;
			}
		} else if (tmpStrValue == "suppresscurse" || tmpStrValue == "suppressdeath") {
			if (readXMLInteger(itemAttributesNode, "value", intValue) && intValue != 0) {
				it.getAbilities().conditionSuppressions |= CONDITION_CURSED;
			}
		} else if (tmpStrValue == "suppressdrown") {
			if (readXMLInteger(itemAttributesNode, "value", intValue) && intValue != 0) {
				it.getAbilities().conditionSuppressions |= CONDITION_DROWN;
			}
		} else if (tmpStrValue == "suppressphysical") {
			if (readXMLInteger(itemAttributesNode, "value", intValue) && intValue != 0) {
				it.getAbilities().conditionSuppressions |= CONDITION_BLEEDING;
			}
		} else if (tmpStrValue == "suppresshaste") {
			if (readXMLInteger(itemAttributesNode, "value", intValue) && intValue != 0) {
				it.getAbilities().conditionSuppressions |= CONDITION_HASTE;
			}
		} else if (tmpStrValue == "suppressparalyze") {
			if (readXMLInteger(itemAttributesNode, "value", intValue) && intValue != 0) {
				it.getAbilities().conditionSuppressions |= CONDITION_PARALYZE;
			}
		} else if (tmpStrValue == "suppressdrunk") {
			if (readXMLInteger(itemAttributesNode, "value", intValue) && intValue != 0) {
				it.getAbilities().conditionSuppressions |= CONDITION_DRUNK;
			}
		} else if (tmpStrValue == "suppressregeneration") {
			if (readXMLInteger(itemAttributesNode, "value", intValue) && intValue != 0) {
				it.getAbilities().conditionSuppressions |= CONDITION_REGENERATION;
			}
		} else if (tmpStrValue == "suppresssoul") {
			if (readXMLInteger(itemAttributesNode, "value", intValue) && intValue != 0) {
				it.getAbilities().conditionSuppressions |= CONDITION_SOUL;
			}
		} else if (tmpStrValue == "suppressoutfit") {
			if (readXMLInteger(itemAttributesNode, "value", intValue) && intValue != 0) {
				it.getAbilities().conditionSuppressions |= CONDITION_OUTFIT;
			}
		} else if (tmpStrValue == "suppressinvisible") {
			if (readXMLInteger(itemAttributesNode, "value", intValue) && intValue != 0) {
				it.getAbilities().conditionSuppressions |= CONDITION_INVISIBLE;
			}
		} else if (tmpStrValue == "suppressinfight") {
			if (readXMLInteger(itemAttributesNode, "value", intValue) && intValue != 0) {
				it.getAbilities().conditionSuppressions |= CONDITION_INFIGHT;
			}
		} else if (tmpStrValue == "suppressexhaust") {
			if (readXMLInteger(itemAttributesNode, "value", intValue) && intValue != 0) {
				it.getAbilities().conditionSuppressions |= CONDITION_EXHAUST;
			}
		} else if (tmpStrValue == "suppressmuted") {
			if (readXMLInteger(itemAttributesNode, "value", intValue) && intValue != 0) {
				it.getAbilities().conditionSuppressions |= CONDITION_MUTED;
			}
		} else if (tmpStrValue == "suppresspacified") {
			if (readXMLInteger(itemAttributesNode, "value", intValue) && intValue != 0) {
				it.getAbilities().conditionSuppressions |= CONDITION_PACIFIED;
			}
		} else if (tmpStrValue == "suppresslight") {
			if (readXMLInteger(itemAttributesNode, "value", intValue) && intValue != 0) {
				it.getAbilities().conditionSuppressions |= CONDITION_LIGHT;
			}
		} else if (tmpStrValue == "suppressattributes") {
			if (readXMLInteger(itemAttributesNode, "value", intValue) && intValue != 0) {
				it.getAbilities().conditionSuppressions |= CONDITION_ATTRIBUTES;
			}
		} else if (tmpStrValue == "suppressmanashield") {
			if (readXMLInteger(itemAttributesNode, "value", intValue) && intValue != 0) {
				it.getAbilities().conditionSuppressions |= CONDITION_MANASHIELD;
			}
		} else if (tmpStrValue == "field") {
			it.group = ITEM_GROUP_MAGICFIELD;
			it.type = ITEM_TYPE_MAGICFIELD;

			CombatType_t combatType = COMBAT_NONE;
			std::unique_ptr<ConditionDamage> conditionDamage;
			if (readXMLString(itemAttributesNode, "value", strValue)) {
				tmpStrValue = otx::util::as_lower_string(strValue);
				if (tmpStrValue == "fire") {
					conditionDamage = std::make_unique<ConditionDamage>(CONDITIONID_COMBAT, CONDITION_FIRE, false, 0);
					combatType = COMBAT_FIREDAMAGE;
				} else if (tmpStrValue == "energy") {
					conditionDamage = std::make_unique<ConditionDamage>(CONDITIONID_COMBAT, CONDITION_ENERGY, false, 0);
					combatType = COMBAT_ENERGYDAMAGE;
				} else if (tmpStrValue == "earth" || tmpStrValue == "poison") {
					conditionDamage = std::make_unique<ConditionDamage>(CONDITIONID_COMBAT, CONDITION_POISON, false, 0);
					combatType = COMBAT_EARTHDAMAGE;
				} else if (tmpStrValue == "ice" || tmpStrValue == "freezing") {
					conditionDamage = std::make_unique<ConditionDamage>(CONDITIONID_COMBAT, CONDITION_FREEZING, false, 0);
					combatType = COMBAT_ICEDAMAGE;
				} else if (tmpStrValue == "holy" || tmpStrValue == "dazzled") {
					conditionDamage = std::make_unique<ConditionDamage>(CONDITIONID_COMBAT, CONDITION_DAZZLED, false, 0);
					combatType = COMBAT_HOLYDAMAGE;
				} else if (tmpStrValue == "death" || tmpStrValue == "cursed") {
					conditionDamage = std::make_unique<ConditionDamage>(CONDITIONID_COMBAT, CONDITION_CURSED, false, 0);
					combatType = COMBAT_DEATHDAMAGE;
				} else if (tmpStrValue == "drown") {
					conditionDamage = std::make_unique<ConditionDamage>(CONDITIONID_COMBAT, CONDITION_DROWN, false, 0);
					combatType = COMBAT_DROWNDAMAGE;
				} else if (tmpStrValue == "physical" || tmpStrValue == "bleed") {
					conditionDamage = std::make_unique<ConditionDamage>(CONDITIONID_COMBAT, CONDITION_BLEEDING, false, 0);
					combatType = COMBAT_PHYSICALDAMAGE;
				} else {
					std::clog << "[Warning - Items::loadFromXml] Unknown field value " << strValue << std::endl;
				}

				if (combatType != COMBAT_NONE) {
					uint32_t ticks = 0;
					int32_t damage = 0, start = 0, count = 1;
					for (xmlNodePtr fieldAttributesNode = itemAttributesNode->children; fieldAttributesNode; fieldAttributesNode = fieldAttributesNode->next) {
						if (!readXMLString(fieldAttributesNode, "key", strValue)) {
							continue;
						}

						tmpStrValue = otx::util::as_lower_string(strValue);
						if (tmpStrValue == "ticks") {
							if (readXMLInteger(fieldAttributesNode, "value", intValue)) {
								ticks = std::max(0, intValue);
							}
						}

						if (tmpStrValue == "count") {
							if (readXMLInteger(fieldAttributesNode, "value", intValue)) {
								count = std::max(1, intValue);
							}
						}

						if (tmpStrValue == "start") {
							if (readXMLInteger(fieldAttributesNode, "value", intValue)) {
								start = std::max(0, intValue);
							}
						}

						if (tmpStrValue == "damage") {
							if (readXMLInteger(fieldAttributesNode, "value", intValue)) {
								damage = -intValue;
								if (start > 0) {
									std::list<int32_t> damageList;
									ConditionDamage::generateDamageList(damage, start, damageList);
									for (int32_t value : damageList) {
										conditionDamage->addDamage(1, ticks, -value);
									}

									start = 0;
								} else {
									conditionDamage->addDamage(count, ticks, damage);
								}
							}
						}
					}

					conditionDamage->setParam(CONDITIONPARAM_FIELD, true);
					if (conditionDamage->getTotalDamage() > 0) {
						conditionDamage->setParam(CONDITIONPARAM_FORCEUPDATE, true);
					}

					it.combatType = combatType;
					it.condition = std::move(conditionDamage);
				}
			}
		} else if (tmpStrValue == "elementphysical") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().elementDamage = intValue;
				it.getAbilities().elementType = COMBAT_PHYSICALDAMAGE;
			}
		} else if (tmpStrValue == "elementfire") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().elementDamage = intValue;
				it.getAbilities().elementType = COMBAT_FIREDAMAGE;
			}
		} else if (tmpStrValue == "elementenergy") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().elementDamage = intValue;
				it.getAbilities().elementType = COMBAT_ENERGYDAMAGE;
			}
		} else if (tmpStrValue == "elementearth") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().elementDamage = intValue;
				it.getAbilities().elementType = COMBAT_EARTHDAMAGE;
			}
		} else if (tmpStrValue == "elementice") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().elementDamage = intValue;
				it.getAbilities().elementType = COMBAT_ICEDAMAGE;
			}
		} else if (tmpStrValue == "elementholy") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().elementDamage = intValue;
				it.getAbilities().elementType = COMBAT_HOLYDAMAGE;
			}
		} else if (tmpStrValue == "elementdeath") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().elementDamage = intValue;
				it.getAbilities().elementType = COMBAT_DEATHDAMAGE;
			}
		} else if (tmpStrValue == "elementlifedrain") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().elementDamage = intValue;
				it.getAbilities().elementType = COMBAT_LIFEDRAIN;
			}
		} else if (tmpStrValue == "elementmanadrain") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().elementDamage = intValue;
				it.getAbilities().elementType = COMBAT_MANADRAIN;
			}
		} else if (tmpStrValue == "elementhealing") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().elementDamage = intValue;
				it.getAbilities().elementType = COMBAT_HEALING;
			}
		} else if (tmpStrValue == "elementundefined") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.getAbilities().elementDamage = intValue;
				it.getAbilities().elementType = COMBAT_UNDEFINEDDAMAGE;
			}
		} else if (tmpStrValue == "replacable" || tmpStrValue == "replaceable") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.replacable = (intValue != 0);
			}
		} else if (tmpStrValue == "partnerdirection") {
			if (readXMLString(itemAttributesNode, "value", strValue)) {
				it.bedPartnerDir = getDirection(strValue);
			}
		} else if (tmpStrValue == "maletransformto") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.transformBed[PLAYERSEX_MALE] = intValue;
				ItemType& ot = getItemType(intValue);
				if (!ot.transformUseTo) {
					ot.transformUseTo = it.id;
				}

				if (!it.transformBed[PLAYERSEX_FEMALE]) {
					it.transformBed[PLAYERSEX_FEMALE] = intValue;
				}
			}
		} else if (tmpStrValue == "femaletransformto") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.transformBed[PLAYERSEX_FEMALE] = intValue;
				ItemType& ot = getItemType(intValue);
				if (!ot.transformUseTo) {
					ot.transformUseTo = it.id;
				}

				if (!it.transformBed[PLAYERSEX_MALE]) {
					it.transformBed[PLAYERSEX_MALE] = intValue;
				}
			}
		} else if (tmpStrValue == "transformto" || tmpStrValue == "transformuseto" || tmpStrValue == "onuseto") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.transformUseTo = intValue;
			}
		} else if (tmpStrValue == "walkstack") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.walkStack = (intValue != 0);
			}
		} else if (tmpStrValue == "premiumdays") {
			if (readXMLInteger(itemAttributesNode, "value", intValue)) {
				it.premiumDays = intValue;
			}
		} else {
			std::clog << "[Warning - Items::loadFromXml] Unknown key value " << strValue << std::endl;
		}
	}

	if (it.pluralName.empty() && !it.name.empty()) {
		it.pluralName = it.name;
		if (it.showCount) {
			it.pluralName += "s";
		}
	}

	it.getAbilities().absorb[COMBAT_ALL] = it.getAbilities().absorb[COMBATINDEX_FIRST];
	it.getAbilities().reflect[REFLECT_PERCENT][COMBAT_ALL] = it.getAbilities().reflect[REFLECT_PERCENT][COMBATINDEX_FIRST];
	it.getAbilities().reflect[REFLECT_CHANCE][COMBAT_ALL] = it.getAbilities().reflect[REFLECT_CHANCE][COMBATINDEX_FIRST];
	for (uint16_t i = COMBATINDEX_FIRST + 1; i <= COMBATINDEX_LAST; ++i) {
		if (it.getAbilities().absorb[COMBAT_ALL] != it.getAbilities().absorb[i]) {
			it.getAbilities().absorb[COMBAT_ALL] = 0;
		}

		if (it.getAbilities().reflect[REFLECT_PERCENT][COMBAT_ALL] != it.getAbilities().reflect[REFLECT_PERCENT][i]) {
			it.getAbilities().reflect[REFLECT_PERCENT][COMBAT_ALL] = 0;
		}

		if (it.getAbilities().reflect[REFLECT_CHANCE][COMBAT_ALL] != it.getAbilities().reflect[REFLECT_CHANCE][i]) {
			it.getAbilities().reflect[REFLECT_CHANCE][COMBAT_ALL] = 0;
		}
	}
}

ItemType& Items::getItemType(uint16_t id)
{
	if (id < items.size()) {
		return items[id];
	}
	return items.front();
}

const ItemType& Items::getItemType(uint16_t id) const
{
	if (id < items.size()) {
		return items[id];
	}
	return items.front();
}

const ItemType& Items::getItemIdByClientId(uint16_t spriteId) const
{
	if (spriteId >= 100 && spriteId < clientIdToServerId.size()) {
		return getItemType(clientIdToServerId[spriteId]);
	}
	return items.front();
}

uint16_t Items::getItemIdByName(const std::string& name)
{
	if (name.empty()) {
		return 0;
	}

	auto it = nameToItems.find(otx::util::as_lower_string(name));
	if (it == nameToItems.end()) {
		return 0;
	}
	return it->second;
}
