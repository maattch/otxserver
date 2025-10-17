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

#include "item.h"

#include "beds.h"
#include "combat.h"
#include "configmanager.h"
#include "container.h"
#include "depot.h"
#include "game.h"
#include "house.h"
#include "lua_definitions.h"
#include "mailbox.h"
#include "movement.h"
#include "teleport.h"
#include "tools.h"
#include "trashholder.h"

#include "otx/util.hpp"

Items Item::items;

Item* Item::CreateItem(const uint16_t type, uint16_t amount /* = 0*/)
{
	const ItemType& it = Item::items[type];
	if (it.group == ITEM_GROUP_DEPRECATED) {
		return nullptr;
	}

	if (it.id == 0) {
		return nullptr;
	}

	Item* newItem = nullptr;
	if (it.isDepot()) {
		newItem = new Depot(type);
	} else if (it.isContainer()) {
		newItem = new Container(type);
	} else if (it.isTeleport()) {
		newItem = new Teleport(type);
	} else if (it.isMagicField()) {
		newItem = new MagicField(type);
	} else if (it.isDoor()) {
		newItem = new Door(type);
	} else if (it.isTrashHolder()) {
		newItem = new TrashHolder(type, it.magicEffect);
	} else if (it.isMailbox()) {
		newItem = new Mailbox(type);
	} else if (it.isBed()) {
		newItem = new BedItem(type);
	} else if (it.id >= 2210 && it.id <= 2212) {
		newItem = new Item(type - 3, amount);
	} else if (it.id == 2215 || it.id == 2216) {
		newItem = new Item(type - 2, amount);
	} else if (it.id >= 2202 && it.id <= 2206) {
		newItem = new Item(type - 37, amount);
	} else if (it.id == 2640) {
		newItem = new Item(6132, amount);
	} else if (it.id == 6301) {
		newItem = new Item(6300, amount);
	} else {
		newItem = new Item(type, amount);
	}

	newItem->addRef();
	return newItem;
}

Item* Item::CreateItem(PropStream& propStream)
{
	uint16_t type;
	if (!propStream.getShort(type)) {
		return nullptr;
	}
	return Item::CreateItem(type, 0);
}

Item::Item(const uint16_t type, uint16_t amount /* = 0*/) : m_id(type)
{
	setItemCount(1);
	setDefaultDuration();

	const ItemType& it = items[type];
	if (it.isFluidContainer() || it.isSplash()) {
		setFluidType(amount);
	} else if (it.stackable) {
		if (amount) {
			setItemCount(amount);
		} else if (it.charges) {
			setItemCount(it.charges);
		}
	} else if (it.charges) {
		setCharges(amount ? amount : it.charges);
	}
}

Item* Item::clone() const
{
	Item* cloneItem = Item::CreateItem(m_id, m_count);
	if (!cloneItem) {
		return nullptr;
	}

	if (m_attributes && !m_attributes->empty()) {
		cloneItem->m_attributes.reset(new AttributeMap(*m_attributes));
		cloneItem->eraseAttribute("uid");
		cloneItem->eraseAttribute("decaying");
	}
	return cloneItem;
}

void Item::copyAttributes(Item* item)
{
	if (item->m_attributes && !item->m_attributes->empty()) {
		m_attributes.reset(new AttributeMap(*item->m_attributes));
		eraseAttribute("uid");
	} else {
		m_attributes.reset();
	}

	m_duration = 0;
	eraseAttribute("decaying");
}

void Item::makeUnique(Item* parent)
{
	if (!parent) {
		return;
	}

	const uint16_t uniqueId = parent->getUniqueId();
	if (uniqueId == 0) {
		return;
	}

	g_game.removeUniqueItem(uniqueId);
	setUniqueId(uniqueId);
	parent->eraseAttribute("uid");
}

void Item::onRemoved()
{
	if (m_raid) {
		m_raid->unRef();
		m_raid = nullptr;
	}

	otx::lua::removeTempItem(this);

	const uint16_t uniqueId = getUniqueId();
	if (uniqueId != 0) {
		g_game.removeUniqueItem(uniqueId);
	}
}

void Item::setDefaultSubtype()
{
	setItemCount(1);
	const ItemType& it = items[m_id];
	if (it.charges) {
		setCharges(it.charges);
	}
}

void Item::setID(uint16_t newId)
{
	const ItemType& it = Item::items[newId];
	const ItemType& pit = Item::items[m_id];
	m_id = newId;

	uint32_t newDuration = it.decayTime * 1000;
	if (!newDuration && !it.stopTime && it.decayTo == -1) {
		eraseAttribute("decaying");
		m_duration = -1;
	}

	eraseAttribute("corpseowner");
	if (newDuration > 0 && (!pit.stopTime || m_duration == 0)) {
		setDecaying(DECAYING_FALSE);
		setDuration(newDuration);
	}
}

bool Item::floorChange(FloorChange_t change /* = CHANGE_NONE*/) const
{
	if (change < CHANGE_NONE) {
		return Item::items[m_id].floorChange[change];
	}

	for (int32_t i = CHANGE_PRE_FIRST; i < CHANGE_LAST; ++i) {
		if (Item::items[m_id].floorChange[i]) {
			return true;
		}
	}

	return false;
}

Player* Item::getHoldingPlayer()
{
	for (Cylinder* p = getParent(); p; p = p->getParent()) {
		if (p->getCreature()) {
			return p->getCreature()->getPlayer();
		}
	}

	return nullptr;
}

const Player* Item::getHoldingPlayer() const
{
	return const_cast<Item*>(this)->getHoldingPlayer();
}

uint16_t Item::getSubType() const
{
	const ItemType& it = items[m_id];
	if (it.isFluidContainer() || it.isSplash()) {
		return getFluidType();
	}

	if (it.charges) {
		return getCharges();
	}

	return m_count;
}

void Item::setSubType(uint16_t n)
{
	const ItemType& it = items[m_id];
	if (it.isFluidContainer() || it.isSplash()) {
		setFluidType(n);
	} else if (it.charges) {
		setCharges(n);
	} else {
		m_count = n;
	}
}

Attr_ReadValue Item::readAttr(AttrTypes_t attr, PropStream& propStream)
{
	switch (attr) {
		case ATTR_COUNT: {
			uint8_t count;
			if (!propStream.getByte(count)) {
				return ATTR_READ_ERROR;
			}

			setSubType(count);
			break;
		}
		case ATTR_ACTION_ID: {
			uint16_t aid;
			if (!propStream.getType<uint16_t>(aid)) {
				return ATTR_READ_ERROR;
			}

			setIntAttr("aid", aid);
			break;
		}
		case ATTR_UNIQUE_ID: {
			uint16_t uid;
			if (!propStream.getType<uint16_t>(uid)) {
				return ATTR_READ_ERROR;
			}

			setUniqueId(uid);
			break;
		}
		case ATTR_NAME: {
			std::string name;
			if (!propStream.getString(name)) {
				return ATTR_READ_ERROR;
			}

			setStrAttr("name", name);
			break;
		}
		case ATTR_PLURALNAME: {
			std::string name;
			if (!propStream.getString(name)) {
				return ATTR_READ_ERROR;
			}

			setStrAttr("pluralname", name);
			break;
		}
		case ATTR_ARTICLE: {
			std::string article;
			if (!propStream.getString(article)) {
				return ATTR_READ_ERROR;
			}

			setStrAttr("article", article);
			break;
		}
		case ATTR_ATTACK: {
			uint32_t attack;
			if (!propStream.getType<uint32_t>(attack)) {
				return ATTR_READ_ERROR;
			}

			setIntAttr("attack", attack);
			break;
		}
		case ATTR_EXTRAATTACK: {
			uint32_t attack;
			if (!propStream.getType<uint32_t>(attack)) {
				return ATTR_READ_ERROR;
			}

			setIntAttr("extraattack", attack);
			break;
		}
		case ATTR_DEFENSE: {
			uint32_t defense;
			if (!propStream.getType<uint32_t>(defense)) {
				return ATTR_READ_ERROR;
			}

			setIntAttr("defense", defense);
			break;
		}
		case ATTR_EXTRADEFENSE: {
			uint32_t defense;
			if (!propStream.getType<uint32_t>(defense)) {
				return ATTR_READ_ERROR;
			}

			setIntAttr("extradefense", defense);
			break;
		}
		case ATTR_ARMOR: {
			uint32_t armor;
			if (!propStream.getType<uint32_t>(armor)) {
				return ATTR_READ_ERROR;
			}

			setIntAttr("armor", armor);
			break;
		}
		case ATTR_ATTACKSPEED: {
			uint32_t attackSpeed;
			if (!propStream.getType<uint32_t>(attackSpeed)) {
				return ATTR_READ_ERROR;
			}

			setIntAttr("attackspeed", attackSpeed);
			break;
		}
		case ATTR_HITCHANCE: {
			uint32_t hitChance;
			if (!propStream.getType<uint32_t>(hitChance)) {
				return ATTR_READ_ERROR;
			}

			setIntAttr("hitchance", hitChance);
			break;
		}
		case ATTR_SCRIPTPROTECTED: {
			uint8_t protection;
			if (!propStream.getByte(protection)) {
				return ATTR_READ_ERROR;
			}

			setBoolAttr("scriptprotected", protection != 0);
			break;
		}
		case ATTR_DUALWIELD: {
			uint8_t wield;
			if (!propStream.getByte(wield)) {
				return ATTR_READ_ERROR;
			}

			setBoolAttr("dualwield", wield != 0);
			break;
		}
		case ATTR_TEXT: {
			std::string text;
			if (!propStream.getString(text)) {
				return ATTR_READ_ERROR;
			}

			setStrAttr("text", text);
			break;
		}
		case ATTR_WRITTENDATE: {
			uint32_t date;
			if (!propStream.getType<uint32_t>(date)) {
				return ATTR_READ_ERROR;
			}

			setIntAttr("date", date);
			break;
		}
		case ATTR_WRITTENBY: {
			std::string writer;
			if (!propStream.getString(writer)) {
				return ATTR_READ_ERROR;
			}

			setStrAttr("writer", writer);
			break;
		}
		case ATTR_DESC: {
			std::string text;
			if (!propStream.getString(text)) {
				return ATTR_READ_ERROR;
			}

			setStrAttr("description", text);
			break;
		}
		case ATTR_RUNE_CHARGES: {
			uint8_t charges;
			if (!propStream.getByte(charges)) {
				return ATTR_READ_ERROR;
			}

			setSubType(charges);
			break;
		}
		case ATTR_CHARGES: {
			uint16_t charges;
			if (!propStream.getShort(charges)) {
				return ATTR_READ_ERROR;
			}

			setSubType(charges);
			break;
		}
		case ATTR_DURATION: {
			uint32_t duration;
			if (!propStream.getType<uint32_t>(duration)) {
				return ATTR_READ_ERROR;
			}

			// setAttribute("duration", duration);
			m_duration = duration;
			break;
		}
		case ATTR_DECAYING_STATE: {
			uint8_t state;
			if (!propStream.getByte(state)) {
				return ATTR_READ_ERROR;
			}

			if (state != DECAYING_FALSE) {
				setIntAttr("decaying", DECAYING_PENDING);
			}
			break;
		}

		// these should be handled through derived classes
		// if these are called then something has changed in the items.otb since the map was saved
		// just read the values
		case ATTR_DEPOT_ID: {
			if (!propStream.skip(2)) {
				return ATTR_READ_ERROR;
			}
			break;
		}
		case ATTR_HOUSEDOORID: {
			if (!propStream.skip(1)) {
				return ATTR_READ_ERROR;
			}
			break;
		}
		case ATTR_TELE_DEST: {
			if (!propStream.skip(5)) {
				return ATTR_READ_ERROR;
			}
			break;
		}
		case ATTR_SLEEPERGUID:
		case ATTR_SLEEPSTART:
		case ATTR_CONTAINER_ITEMS:
		case ATTR_CRITICALHITCHANCE:
		case ATTR_REDUCE_SKILL_LOSS: {
			if (!propStream.skip(4)) {
				return ATTR_READ_ERROR;
			}
			break;
		}

		// attributes
		case ATTR_ATTRIBUTE_MAP_OLD: {
			uint16_t size;
			if (!propStream.getType<uint16_t>(size)) {
				return ATTR_READ_ERROR;
			}

			const bool unique = (getUniqueId() != 0);

			auto& attributes = getAttributes();
			for (uint16_t i = 0; i < size; i++) {
				std::string key;
				if (!propStream.getString(key)) {
					return ATTR_READ_ERROR;
				}

				ItemAttributes itemAttr;
				if (!itemAttr.unserialize_old(propStream)) {
					return ATTR_READ_ERROR;
				}

				attributes.insert_or_assign(std::move(key), std::move(itemAttr));
			}

			ItemAttributes* itemAttr = getAttribute("uid");
			if (!unique && itemAttr && itemAttr->isInt()) {
				// unfortunately we have to do this
				g_game.addUniqueItem(itemAttr->getInt(), this);
			}

			// this attribute has a custom behavior as well
			if (getDecaying() != DECAYING_FALSE) {
				setDecaying(DECAYING_PENDING);
			}
			break;
		}

		case ATTR_ATTRIBUTE_MAP: {
			uint16_t version;
			if (!propStream.getType<uint16_t>(version)) {
				return ATTR_READ_ERROR;
			}

			UNUSED(version);

			uint16_t size;
			if (!propStream.getType<uint16_t>(size)) {
				return ATTR_READ_ERROR;
			}

			const bool unique = (getUniqueId() != 0);

			auto& attributes = getAttributes();
			for (uint16_t i = 0; i < size; i++) {
				std::string key;
				if (!propStream.getString(key)) {
					return ATTR_READ_ERROR;
				}

				ItemAttributes itemAttr;
				if (!itemAttr.unserialize(propStream)) {
					return ATTR_READ_ERROR;
				}

				attributes.insert_or_assign(std::move(key), std::move(itemAttr));
			}

			ItemAttributes* itemAttr = getAttribute("uid");
			if (!unique && itemAttr && itemAttr->isInt()) {
				// unfortunately we have to do this
				g_game.addUniqueItem(itemAttr->getInt(), this);
			}

			// this attribute has a custom behavior as well
			if (getDecaying() != DECAYING_FALSE) {
				setDecaying(DECAYING_PENDING);
			}
			break;
		}

		default:
			return ATTR_READ_ERROR;
	}
	return ATTR_READ_CONTINUE;
}

bool Item::unserializeAttr(PropStream& propStream)
{
	uint8_t attrType = ATTR_END;
	while (propStream.getByte(attrType) && attrType != ATTR_END) {
		switch (readAttr(static_cast<AttrTypes_t>(attrType), propStream)) {
			case ATTR_READ_ERROR:
				return false;

			case ATTR_READ_END:
				return true;

			default:
				break;
		}
	}

	return true;
}

bool Item::serializeAttr(PropWriteStream& propWriteStream) const
{
	if (isStackable() || isFluidContainer() || isSplash()) {
		propWriteStream.addByte(ATTR_COUNT);
		propWriteStream.addByte(getSubType());
	}

	if (m_duration != 0) {
		propWriteStream.addByte(ATTR_DURATION);
		propWriteStream.addType(m_duration);
	}

	if (m_attributes && !m_attributes->empty()) {
		propWriteStream.addByte(ATTR_ATTRIBUTE_MAP);
		propWriteStream.addType<uint16_t>(ITEM_ATTRIBUTES_VERSION); // serialization version, for future changes
		propWriteStream.addType<uint16_t>(m_attributes->size());

		for (const auto& [key, attr] : *m_attributes) {
			propWriteStream.addString(key);
			attr.serialize(propWriteStream);
		}
	}
	return true;
}

bool Item::hasProperty(enum ITEMPROPERTY prop) const
{
	const ItemType& it = items[m_id];
	switch (prop) {
		case BLOCKSOLID:
			if (it.blockSolid) {
				return true;
			}

			break;

		case MOVABLE:
			if (it.movable && (!m_loadedFromMap || (!getUniqueId() && (!getActionId() || !getContainer())))) {
				return true;
			}

			break;

		case HASHEIGHT:
			if (it.hasHeight) {
				return true;
			}

			break;

		case BLOCKPROJECTILE:
			if (it.blockProjectile) {
				return true;
			}

			break;

		case BLOCKPATH:
			if (it.blockPathFind) {
				return true;
			}

			break;

		case ISVERTICAL:
			if (it.isVertical) {
				return true;
			}

			break;

		case ISHORIZONTAL:
			if (it.isHorizontal) {
				return true;
			}

			break;

		case IMMOVABLEBLOCKSOLID:
			if (it.blockSolid && (!it.movable || (m_loadedFromMap && (getUniqueId() || (getActionId() && getContainer()))))) {
				return true;
			}

			break;

		case IMMOVABLEBLOCKPATH:
			if (it.blockPathFind && (!it.movable || (m_loadedFromMap && (getUniqueId() || (getActionId() && getContainer()))))) {
				return true;
			}

			break;

		case SUPPORTHANGABLE:
			if (it.isHorizontal || it.isVertical) {
				return true;
			}

			break;

		case IMMOVABLENOFIELDBLOCKPATH:
			if (!it.isMagicField() && it.blockPathFind && (!it.movable || (m_loadedFromMap && (getUniqueId() || (getActionId() && getContainer()))))) {
				return true;
			}

			break;

		case NOFIELDBLOCKPATH:
			if (!it.isMagicField() && it.blockPathFind) {
				return true;
			}

			break;

		case FLOORCHANGEDOWN:
			if (it.floorChange[CHANGE_DOWN]) {
				return true;
			}

			break;

		case FLOORCHANGEUP:
			for (uint16_t i = CHANGE_FIRST; i <= CHANGE_PRE_LAST; ++i) {
				if (it.floorChange[i]) {
					return true;
				}
			}

			break;

		default:
			break;
	}

	return false;
}

double Item::getWeight() const
{
	if (isStackable()) {
		return items[m_id].weight * std::max<int32_t>(1, m_count);
	}
	return items[m_id].weight;
}

std::string Item::getDescription(const ItemType& it, int32_t lookDistance, const Item* item /* = nullptr*/,
	int32_t subType /* = -1*/, bool addArticle /* = true*/)
{
	std::ostringstream s;
	s << getNameDescription(it, item, subType, addArticle);
	if (item) {
		subType = item->getSubType();
	}

	bool dot = true;
	if (it.isRune()) {
		if (!it.runeSpellName.empty()) {
			s << "(\"" << it.runeSpellName << "\")";
		}

		if (it.runeLevel != 0 || it.runeMagLevel != 0 || (it.vocationString != "" && it.wieldInfo == 0)) {
			s << "." << std::endl
			  << "It can only be used";
			if (it.vocationString != "" && it.wieldInfo == 0) {
				s << " by " << it.vocationString;
			}

			bool begin = true;
			if (otx::config::getBoolean(otx::config::USE_RUNE_REQUIREMENTS) && it.runeLevel != 0) {
				begin = false;
				s << " with level " << it.runeLevel;
			}

			if (otx::config::getBoolean(otx::config::USE_RUNE_REQUIREMENTS) && it.runeMagLevel != 0) {
				begin = false;
				s << " " << (begin ? "with" : "and") << " magic level " << it.runeMagLevel;
			}

			if (otx::config::getBoolean(otx::config::USE_RUNE_REQUIREMENTS) && !begin) {
				s << " or higher";
			}
		}
	} else if (it.weaponType != WEAPON_NONE) {
		bool begin = true;
		if (it.weaponType == WEAPON_DIST && it.ammoType != AMMO_NONE) {
			begin = false;
			s << " (Range:" << (item ? item->getShootRange() : it.shootRange);
			if (it.attack || it.extraAttack || (item && (item->getAttack() || item->getExtraAttack()))) {
				s << ", Atk " << std::showpos << (item ? item->getAttack() : it.attack);
				if (it.extraAttack || (item && item->getExtraAttack())) {
					s << " " << std::showpos << (item ? item->getExtraAttack() : it.extraAttack) << std::noshowpos;
				}
			}

			if (it.hitChance != -1 || (item && item->getHitChance() != -1)) {
				s << ", Hit% " << std::showpos << (item ? item->getHitChance() : it.hitChance) << std::noshowpos;
			}

			if (it.attackSpeed || (item && item->getAttackSpeed())) {
				s << ", AS: " << (item ? item->getAttackSpeed() : it.attackSpeed);
			}
		} else if (it.weaponType != WEAPON_AMMO && it.weaponType != WEAPON_WAND) {
			int32_t attack, defense, extraDefense;
			if (item) {
				attack = item->getAttack();
				defense = item->getDefense();
				extraDefense = item->getExtraDefense();
			} else {
				attack = it.attack;
				defense = it.defense;
				extraDefense = it.extraDefense;
			}

			if (attack != 0) {
				begin = false;
				s << " (Atk:" << attack;

				if (it.abilities && it.abilities->elementType != COMBAT_NONE && it.abilities->elementDamage != 0) {
					s << " physical + " << it.abilities->elementDamage << ' ' << getCombatName(it.abilities->elementType);
				}
			}

			if (defense != 0 || extraDefense != 0) {
				if (begin) {
					begin = false;
					s << " (";
				} else {
					s << ", ";
				}

				s << "Def:" << defense;
				if (extraDefense != 0) {
					s << ' ' << std::showpos << extraDefense << std::noshowpos;
				}
			}
		}

		if (it.attackSpeed || (item && item->getAttackSpeed())) {
			if (begin) {
				begin = false;
				s << " (";
			} else {
				s << ", ";
			}

			s << "AS: " << (item ? item->getAttackSpeed() : it.attackSpeed);
		}

		if (it.hasAbilities()) {
			for (uint16_t i = SKILL_FIRST; i <= SKILL_LAST; ++i) {
				if (!it.abilities->skills[i]) {
					continue;
				}

				if (begin) {
					begin = false;
					s << " (";
				} else {
					s << ", ";
				}

				s << getSkillName(i) << " " << std::showpos << it.abilities->skills[i] << std::noshowpos;
			}

			if (it.abilities->stats[STAT_MAGICLEVEL]) {
				if (begin) {
					begin = false;
					s << " (";
				} else {
					s << ", ";
				}

				s << "magic level " << std::showpos << it.abilities->stats[STAT_MAGICLEVEL] << std::noshowpos;
			}

			int32_t show = it.abilities->absorb[COMBAT_ALL];
			if (!show) {
				bool tmp = true;
				for (uint16_t i = COMBATINDEX_FIRST; i <= COMBATINDEX_LAST; ++i) {
					if (!it.abilities->absorb[i]) {
						continue;
					}

					if (tmp) {
						tmp = false;
						if (begin) {
							begin = false;
							s << " (";
						} else {
							s << ", ";
						}

						s << "protection ";
					} else {
						s << ", ";
					}

					s << getCombatName(otx::util::index_combat(i)) << " " << std::showpos << it.abilities->absorb[i] << std::noshowpos << "%";
				}
			} else {
				if (begin) {
					begin = false;
					s << " (";
				} else {
					s << ", ";
				}

				s << "protection all " << std::showpos << show << std::noshowpos << "%";
			}

			show = it.abilities->fieldAbsorb[COMBAT_ALL];
			if (!show) {
				bool tmp = true;
				for (uint16_t i = COMBATINDEX_FIRST; i <= COMBATINDEX_LAST; ++i) {
					if (!it.abilities->fieldAbsorb[i]) {
						continue;
					}

					if (tmp) {
						tmp = false;
						if (begin) {
							begin = false;
							s << " (";
						} else {
							s << ", ";
						}

						s << "protection ";
					} else {
						s << ", ";
					}

					s << getCombatName(otx::util::index_combat(i)) << " field " << std::showpos << it.abilities->absorb[i] << std::noshowpos << "%";
				}
			} else {
				if (begin) {
					begin = false;
					s << " (";
				} else {
					s << ", ";
				}

				s << "protection all fields " << std::showpos << show << std::noshowpos << "%";
			}

			show = it.abilities->reflect[REFLECT_CHANCE][COMBAT_ALL];
			if (!show) {
				bool tmp = true;
				for (uint16_t i = COMBATINDEX_FIRST; i <= COMBATINDEX_LAST; ++i) {
					if (!it.abilities->reflect[REFLECT_CHANCE][i] || !it.abilities->reflect[REFLECT_PERCENT][i]) {
						continue;
					}

					if (tmp) {
						tmp = false;
						if (begin) {
							begin = false;
							s << " (";
						} else {
							s << ", ";
						}

						s << "reflect: ";
					} else {
						s << ", ";
					}

					s << it.abilities->reflect[REFLECT_CHANCE][i] << "% for ";
					if (it.abilities->reflect[REFLECT_PERCENT][i] > 99) {
						s << "whole";
					} else if (it.abilities->reflect[REFLECT_PERCENT][i] >= 75) {
						s << "huge";
					} else if (it.abilities->reflect[REFLECT_PERCENT][i] >= 50) {
						s << "medium";
					} else if (it.abilities->reflect[REFLECT_PERCENT][i] >= 25) {
						s << "small";
					} else {
						s << "tiny";
					}

					s << getCombatName(otx::util::index_combat(i));
				}

				if (!tmp) {
					s << " damage";
				}
			} else {
				if (begin) {
					begin = false;
					s << " (";
				} else {
					s << ", ";
				}

				int32_t tmp = it.abilities->reflect[REFLECT_PERCENT][COMBAT_ALL];
				s << "reflect: " << show << "% for ";
				if (tmp) {
					if (tmp > 99) {
						s << "whole";
					} else if (tmp >= 75) {
						s << "huge";
					} else if (tmp >= 50) {
						s << "medium";
					} else if (tmp >= 25) {
						s << "small";
					} else {
						s << "tiny";
					}
				} else {
					s << "mixed";
				}

				s << " damage";
			}

			if (it.abilities->speed) {
				if (begin) {
					begin = false;
					s << " (";
				} else {
					s << ", ";
				}

				s << "speed " << std::showpos << (it.abilities->speed / 2) << std::noshowpos;
			}

			if (it.abilities->invisible) {
				if (begin) {
					begin = false;
					s << " (";
				} else {
					s << ", ";
				}

				s << "invisibility";
			}

			if (it.abilities->regeneration) {
				if (begin) {
					begin = false;
					s << " (";
				} else {
					s << ", ";
				}

				s << "faster regeneration";
			}

			if (it.abilities->manaShield) {
				if (begin) {
					begin = false;
					s << " (";
				} else {
					s << ", ";
				}

				s << "mana shield";
			}

			if (hasBitSet(CONDITION_DRUNK, it.abilities->conditionSuppressions)) {
				if (begin) {
					begin = false;
					s << " (";
				} else {
					s << ", ";
				}

				s << "hard drinking";
			}
		}

		if (it.dualWield || (item && item->isDualWield())) {
			if (begin) {
				begin = false;
				s << " (";
			} else {
				s << ", ";
			}

			s << "dual wielding";
		}

		if (!begin) {
			s << ")";
		}
	} else if (it.armor || (item && item->getArmor()) || it.showAttributes) {
		int32_t tmp = it.armor;
		if (item) {
			tmp = item->getArmor();
		}

		bool begin = true;
		if (tmp) {
			s << " (Arm:" << tmp;
			begin = false;
		}

		if (it.isContainer()) {
			if (begin) {
				begin = false;
				s << " (";
			} else {
				s << ", ";
			}

			s << "Vol:" << it.maxItems;
		}

		if (it.hasAbilities()) {
			for (uint16_t i = SKILL_FIRST; i <= SKILL_LAST; ++i) {
				if (!it.abilities->skills[i]) {
					continue;
				}

				if (begin) {
					begin = false;
					s << " (";
				} else {
					s << ", ";
				}

				s << getSkillName(i) << " " << std::showpos << it.abilities->skills[i] << std::noshowpos;
			}

			if (it.abilities->stats[STAT_MAGICLEVEL]) {
				if (begin) {
					begin = false;
					s << " (";
				} else {
					s << ", ";
				}

				s << "magic level " << std::showpos << it.abilities->stats[STAT_MAGICLEVEL] << std::noshowpos;
			}

			int32_t show = it.abilities->absorb[COMBAT_ALL];
			if (!show) {
				bool tmp2 = true;
				for (uint16_t i = COMBATINDEX_FIRST; i <= COMBATINDEX_LAST; ++i) {
					if (!it.abilities->absorb[i]) {
						continue;
					}

					if (tmp2) {
						tmp2 = false;
						if (begin) {
							begin = false;
							s << " (";
						} else {
							s << ", ";
						}

						s << "protection ";
					} else {
						s << ", ";
					}

					s << getCombatName(otx::util::index_combat(i)) << " " << std::showpos << it.abilities->absorb[i] << std::noshowpos << "%";
				}
			} else {
				if (begin) {
					begin = false;
					s << " (";
				} else {
					s << ", ";
				}

				s << "protection all " << std::showpos << show << std::noshowpos << "%";
			}

			show = it.abilities->reflect[REFLECT_CHANCE][COMBAT_ALL];
			if (!show) {
				bool tmp2 = true;
				for (uint16_t i = COMBATINDEX_FIRST; i <= COMBATINDEX_LAST; ++i) {
					if (!it.abilities->reflect[REFLECT_CHANCE][i] || !it.abilities->reflect[REFLECT_PERCENT][i]) {
						continue;
					}

					if (tmp2) {
						tmp2 = false;
						if (begin) {
							begin = false;
							s << " (";
						} else {
							s << ", ";
						}

						s << "reflect: ";
					} else {
						s << ", ";
					}

					s << it.abilities->reflect[REFLECT_CHANCE][i] << "% for ";
					if (it.abilities->reflect[REFLECT_PERCENT][i] > 99) {
						s << "whole";
					} else if (it.abilities->reflect[REFLECT_PERCENT][i] >= 75) {
						s << "huge";
					} else if (it.abilities->reflect[REFLECT_PERCENT][i] >= 50) {
						s << "medium";
					} else if (it.abilities->reflect[REFLECT_PERCENT][i] >= 25) {
						s << "small";
					} else {
						s << "tiny";
					}

					s << getCombatName(otx::util::index_combat(i));
				}

				if (!tmp2) {
					s << " damage";
				}
			} else {
				if (begin) {
					begin = false;
					s << " (";
				} else {
					s << ", ";
				}

				tmp = it.abilities->reflect[REFLECT_PERCENT][COMBAT_ALL];
				s << "reflect: " << show << "% for ";
				if (tmp) {
					if (tmp > 99) {
						s << "whole";
					} else if (tmp >= 75) {
						s << "huge";
					} else if (tmp >= 50) {
						s << "medium";
					} else if (tmp >= 25) {
						s << "small";
					} else {
						s << "tiny";
					}
				} else {
					s << "mixed";
				}

				s << " damage";
			}

			if (it.abilities->speed) {
				if (begin) {
					begin = false;
					s << " (";
				} else {
					s << ", ";
				}

				s << "speed " << std::showpos << (it.abilities->speed / 2) << std::noshowpos;
			}

			if (it.abilities->invisible) {
				if (begin) {
					begin = false;
					s << " (";
				} else {
					s << ", ";
				}

				s << "invisibility";
			}

			if (it.abilities->regeneration) {
				if (begin) {
					begin = false;
					s << " (";
				} else {
					s << ", ";
				}

				s << "faster regeneration";
			}

			if (it.abilities->manaShield) {
				if (begin) {
					begin = false;
					s << " (";
				} else {
					s << ", ";
				}

				s << "mana shield";
			}

			if (hasBitSet(CONDITION_DRUNK, it.abilities->conditionSuppressions)) {
				if (begin) {
					begin = false;
					s << " (";
				} else {
					s << ", ";
				}

				s << "hard drinking";
			}

			if (!begin) {
				s << ")";
			}
		}
	} else if (it.isContainer()) {
		s << " (Vol:" << it.maxItems << ')';
	} else if (it.isKey()) {
		s << " (Key:" << (item ? item->getActionId() : 0) << ")";
	} else if (it.isFluidContainer()) {
		if (subType > 0) {
			s << " of " << (items[subType].name.length() ? items[subType].name : "unknown");
		} else {
			s << ". It is empty";
		}
	} else if (it.isSplash()) {
		s << " of ";
		if (subType > 0 && items[subType].name.length()) {
			s << items[subType].name;
		} else {
			s << "unknown";
		}
	} else if (it.allowDistRead) {
		s << "." << std::endl;
		if (item && !item->getText().empty()) {
			if (lookDistance <= 4) {
				if (!item->getWriter().empty()) {
					s << item->getWriter() << " wrote";
					time_t date = item->getDate();
					if (date > 0) {
						s << " on " << formatDate(date);
					}

					s << ": ";
				} else {
					s << "You read: ";
				}

				std::string text = item->getText();
				s << text;

				char end = *text.rbegin();
				if (end == '?' || end == '!' || end == '.') {
					dot = false;
				}
			} else {
				s << "You are too far away to read it";
			}
		} else {
			s << "Nothing is written on it";
		}
	} else if (it.levelDoor && item && item->getActionId() >= static_cast<int32_t>(it.levelDoor) && item->getActionId() <= (static_cast<int32_t>(it.levelDoor) + otx::config::getInteger(otx::config::MAXIMUM_DOOR_LEVEL))) {
		s << " for level " << item->getActionId() - it.levelDoor;
	}

	if (it.showCharges) {
		s << " that has " << subType << " charge" << (subType != 1 ? "s" : "") << " left";
	}

	if (it.showDuration) {
		int32_t duration = item ? item->getDuration() / 1000 : 0;
		if (duration != 0) {
			s << " that will expire in ";
			if (duration >= 86400) {
				uint16_t days = duration / 86400;
				uint16_t hours = (duration % 86400) / 3600;
				s << days << " day" << (days > 1 ? "s" : "");
				if (hours > 0) {
					s << " and " << hours << " hour" << (hours > 1 ? "s" : "");
				}
			} else if (duration >= 3600) {
				uint16_t hours = duration / 3600;
				uint16_t minutes = (duration % 3600) / 60;
				s << hours << " hour" << (hours > 1 ? "s" : "");
				if (hours > 0) {
					s << " and " << minutes << " minute" << (minutes > 1 ? "s" : "");
				}
			} else if (duration >= 60) {
				uint16_t minutes = duration / 60;
				uint16_t seconds = duration % 60;
				s << minutes << " minute" << (minutes > 1 ? "s" : "");
				if (seconds > 0) {
					s << " and " << seconds << " second" << (seconds > 1 ? "s" : "");
				}
			} else {
				s << duration << " second" << (duration > 1 ? "s" : "");
			}
		} else {
			s << " that is brand-new";
		}
	}

	if (dot) {
		s << ".";
	}

	if (it.wieldInfo) {
		s << std::endl
		  << "It can only be wielded properly by ";
		if (it.wieldInfo & WIELDINFO_PREMIUM) {
			s << "premium ";
		}

		if (it.wieldInfo & WIELDINFO_VOCREQ) {
			s << it.vocationString;
		} else {
			s << "players";
		}

		if (it.wieldInfo & WIELDINFO_LEVEL) {
			s << " of level " << it.minReqLevel << " or higher";
		}

		if (it.wieldInfo & WIELDINFO_MAGLV) {
			if (it.wieldInfo & WIELDINFO_LEVEL) {
				s << " and";
			} else {
				s << " of";
			}

			s << " magic level " << it.minReqMagicLevel << " or higher";
		}

		s << ".";
	}

	if (lookDistance <= 3 && it.pickupable) {
		std::string tmp;
		if (!item) {
			tmp = getWeightDescription(it.weight, it.stackable && it.showCount, subType);
		} else {
			tmp = item->getWeightDescription();
		}

		if (!tmp.empty()) {
			s << std::endl
			  << tmp;
		}
	}

	if (item && !item->getSpecialDescription().empty()) {
		s << std::endl
		  << item->getSpecialDescription();
	} else if (!it.description.empty() && lookDistance <= 3) {
		s << std::endl
		  << it.description;
	}

	std::string str = s.str();
	if (str.find("|PLAYERNAME|") != std::string::npos) {
		std::string tmp = "You";
		if (item) {
			if (const Player* player = item->getHoldingPlayer()) {
				tmp = player->getName();
			}
		}

		otx::util::replace_all(str, "|PLAYERNAME|", tmp);
	}

	if (str.find("|TIME|") != std::string::npos || str.find("|DATE|") != std::string::npos || str.find("|DAY|") != std::string::npos || str.find("|MONTH|") != std::string::npos || str.find("|YEAR|") != std::string::npos || str.find("|HOUR|") != std::string::npos || str.find("|MINUTES|") != std::string::npos || str.find("|SECONDS|") != std::string::npos || str.find("|WEEKDAY|") != std::string::npos || str.find("|YEARDAY|") != std::string::npos) {
		time_t now = time(nullptr);
		tm* ts = localtime(&now);

		std::ostringstream ss;
		ss << ts->tm_sec;
		otx::util::replace_all(str, "|SECONDS|", ss.str());

		ss.str("");
		ss << ts->tm_min;
		otx::util::replace_all(str, "|MINUTES|", ss.str());

		ss.str("");
		ss << ts->tm_hour;
		otx::util::replace_all(str, "|HOUR|", ss.str());

		ss.str("");
		ss << ts->tm_mday;
		otx::util::replace_all(str, "|DAY|", ss.str());

		ss.str("");
		ss << (ts->tm_mon + 1);
		otx::util::replace_all(str, "|MONTH|", ss.str());

		ss.str("");
		ss << (ts->tm_year + 1900);
		otx::util::replace_all(str, "|YEAR|", ss.str());

		ss.str("");
		ss << ts->tm_wday;
		otx::util::replace_all(str, "|WEEKDAY|", ss.str());

		ss.str("");
		ss << ts->tm_yday;
		otx::util::replace_all(str, "|YEARDAY|", ss.str());

		ss.str("");
		ss << ts->tm_hour << ":" << ts->tm_min << ":" << ts->tm_sec;
		otx::util::replace_all(str, "|TIME|", ss.str());

		ss.str("");
		otx::util::replace_all(str, "|DATE|", formatDateEx(now));
	}

	return str;
}

std::string Item::getNameDescription(const ItemType& it, const Item* item /* = nullptr*/, int32_t subType /* = -1*/, bool addArticle /* = true*/)
{
	if (item) {
		subType = item->getSubType();
	}

	std::ostringstream s;
	if (it.loaded || (item && !item->getName().empty())) {
		if (subType > 1 && it.stackable && it.showCount) {
			s << subType << " " << (item ? item->getPluralName() : it.pluralName);
		} else {
			if (addArticle) {
				if (item && !item->getArticle().empty()) {
					s << item->getArticle() << " ";
				} else if (!it.article.empty()) {
					s << it.article << " ";
				}
			}

			s << (item ? item->getName() : it.name);
		}
	} else if (it.name.empty()) {
		s << "an item of type " << it.id << ", please report it to gamemaster";
	} else {
		s << "an item '" << it.name << "', please report it to gamemaster";
	}

	return s.str();
}

std::string Item::getWeightDescription(double weight, bool stackable, uint32_t count /* = 1*/)
{
	if (weight <= 0) {
		return "";
	}

	std::ostringstream s;
	if (stackable && count > 1) {
		s << "They weigh " << std::fixed << std::setprecision(2) << weight << " oz.";
	} else {
		s << "It weighs " << std::fixed << std::setprecision(2) << weight << " oz.";
	}

	return s.str();
}

void Item::setActionId(int32_t aid, bool callEvent /* = true*/)
{
	Tile* tile = nullptr;
	if (callEvent) {
		tile = getTile();
	}

	if (tile && getActionId()) {
		g_moveEvents.onRemoveTileItem(tile, this);
	}

	setIntAttr("aid", aid);
	if (tile) {
		g_moveEvents.onAddTileItem(tile, this);
	}
}

void Item::resetActionId(bool callEvent /* = true*/)
{
	if (!getActionId()) {
		return;
	}

	Tile* tile = nullptr;
	if (callEvent) {
		tile = getTile();
	}

	eraseAttribute("aid");
	if (tile) {
		g_moveEvents.onAddTileItem(tile, this);
	}
}

void Item::setUniqueId(int32_t uid)
{
	if (getUniqueId() != 0) {
		return;
	}

	if (g_game.addUniqueItem(uid, this)) {
		setIntAttr("uid", uid);
	}
}

bool Item::canDecay()
{
	if (isRemoved()) {
		return false;
	}

	const ItemType& it = Item::items[m_id];
	if (it.decayTo < 0 || it.decayTime == 0) {
		return false;
	}
	return true;
}

void Item::getLight(LightInfo& lightInfo)
{
	const ItemType& it = items[m_id];
	lightInfo.color = it.lightColor;
	lightInfo.level = it.lightLevel;
}

void Item::__startDecaying()
{
	g_game.startDecay(this);
}

uint32_t Item::countByType(const Item* item, int32_t checkType)
{
	if (checkType != -1 && checkType != static_cast<int32_t>(item->getSubType())) {
		return 0;
	}
	return item->getItemCount();
}

//
// ItemAttributes
//
ItemAttributes* Item::getAttribute(std::string_view key) const
{
	if (!m_attributes) {
		return nullptr;
	}

	auto it = m_attributes->find(key);
	if (it != m_attributes->end()) {
		return &it->second;
	}
	return nullptr;
}

bool Item::eraseAttribute(std::string_view key)
{
	if (!m_attributes) {
		return false;
	}

	auto it = m_attributes->find(key);
	if (it != m_attributes->end()) {
		m_attributes->erase(it);
		if (m_attributes->empty()) {
			m_attributes.reset();
		}
		return true;
	}
	return false;
}

void Item::setStrAttr(const std::string& key, const std::string& value)
{
	getAttributes()[key] = ItemAttributes(value);
}

void Item::setIntAttr(const std::string& key, int64_t value)
{
	getAttributes()[key] = ItemAttributes(value);
}

void Item::setDoubleAttr(const std::string& key, double value)
{
	getAttributes()[key] = ItemAttributes(value);
}

void Item::setBoolAttr(const std::string& key, bool value)
{
	getAttributes()[key] = ItemAttributes(value);
}

bool Item::hasStrAttr(std::string_view key) const
{
	if (ItemAttributes* attr = getAttribute(key)) {
		return attr->isString();
	}
	return false;
}

bool Item::hasIntAttr(std::string_view key) const
{
	if (ItemAttributes* attr = getAttribute(key)) {
		return attr->isInt();
	}
	return false;
}

bool Item::hasDoubleAttr(std::string_view key) const
{
	if (ItemAttributes* attr = getAttribute(key)) {
		return attr->isDouble();
	}
	return false;
}

bool Item::hasBoolAttr(std::string_view key) const
{
	if (ItemAttributes* attr = getAttribute(key)) {
		return attr->isBool();
	}
	return false;
}

const std::string& Item::getName() const
{
	ItemAttributes* attr = getAttribute("name");
	if (attr && attr->isString()) {
		return attr->getString();
	}
	return items[m_id].name;
}

const std::string& Item::getPluralName() const
{
	ItemAttributes* attr = getAttribute("pluralname");
	if (attr && attr->isString()) {
		return attr->getString();
	}
	return items[m_id].pluralName;
}

const std::string& Item::getArticle() const
{
	ItemAttributes* attr = getAttribute("article");
	if (attr && attr->isString()) {
		return attr->getString();
	}
	return items[m_id].article;
}

bool Item::isScriptProtected() const
{
	ItemAttributes* attr = getAttribute("scriptprotected");
	if (attr && attr->isBool()) {
		return attr->getBool();
	}
	return false;
}

int32_t Item::getAttack() const
{
	ItemAttributes* attr = getAttribute("attack");
	if (attr && attr->isInt()) {
		return static_cast<int32_t>(attr->getInt());
	}
	return items[m_id].attack;
}

int32_t Item::getExtraAttack() const
{
	ItemAttributes* attr = getAttribute("extraattack");
	if (attr && attr->isInt()) {
		return static_cast<int32_t>(attr->getInt());
	}
	return items[m_id].extraAttack;
}

int32_t Item::getDefense() const
{
	ItemAttributes* attr = getAttribute("defense");
	if (attr && attr->isInt()) {
		return static_cast<int32_t>(attr->getInt());
	}
	return items[m_id].defense;
}

int32_t Item::getExtraDefense() const
{
	ItemAttributes* attr = getAttribute("extradefense");
	if (attr && attr->isInt()) {
		return static_cast<int32_t>(attr->getInt());
	}
	return items[m_id].extraDefense;
}

int32_t Item::getArmor() const
{
	ItemAttributes* attr = getAttribute("armor");
	if (attr && attr->isInt()) {
		return static_cast<int32_t>(attr->getInt());
	}
	return items[m_id].armor;
}

int32_t Item::getAttackSpeed() const
{
	ItemAttributes* attr = getAttribute("attackspeed");
	if (attr && attr->isInt()) {
		return static_cast<int32_t>(attr->getInt());
	}
	return items[m_id].attackSpeed;
}

int32_t Item::getHitChance() const
{
	ItemAttributes* attr = getAttribute("hitchance");
	if (attr && attr->isInt()) {
		return static_cast<int32_t>(attr->getInt());
	}
	return items[m_id].hitChance;
}

int32_t Item::getShootRange() const
{
	ItemAttributes* attr = getAttribute("shootrange");
	if (attr && attr->isInt()) {
		return static_cast<int32_t>(attr->getInt());
	}
	return items[m_id].shootRange;
}

bool Item::isDualWield() const
{
	ItemAttributes* attr = getAttribute("dualwield");
	if (attr && attr->isBool()) {
		return attr->getBool();
	}
	return items[m_id].dualWield;
}

const std::string& Item::getSpecialDescription() const
{
	ItemAttributes* attr = getAttribute("description");
	if (attr && attr->isString()) {
		return attr->getString();
	}

	static const std::string emptyString;
	return emptyString;
}

const std::string& Item::getText() const
{
	ItemAttributes* attr = getAttribute("text");
	if (attr && attr->isString()) {
		return attr->getString();
	}
	return items[m_id].text;
}

time_t Item::getDate() const
{
	ItemAttributes* attr = getAttribute("date");
	if (attr && attr->isInt()) {
		return static_cast<int32_t>(attr->getInt());
	}
	return items[m_id].date;
}

const std::string& Item::getWriter() const
{
	ItemAttributes* attr = getAttribute("writer");
	if (attr && attr->isString()) {
		return attr->getString();
	}
	return items[m_id].writer;
}

int32_t Item::getActionId() const
{
	ItemAttributes* attr = getAttribute("aid");
	if (attr && attr->isInt()) {
		return static_cast<int32_t>(attr->getInt());
	}
	return 0;
}

int32_t Item::getUniqueId() const
{
	ItemAttributes* attr = getAttribute("uid");
	if (attr && attr->isInt()) {
		return static_cast<int32_t>(attr->getInt());
	}
	return 0;
}

uint16_t Item::getCharges() const
{
	ItemAttributes* attr = getAttribute("charges");
	if (attr && attr->isInt()) {
		return static_cast<uint16_t>(attr->getInt());
	}
	return 0;
}

uint16_t Item::getFluidType() const
{
	ItemAttributes* attr = getAttribute("fluidtype");
	if (attr && attr->isInt()) {
		return static_cast<uint16_t>(attr->getInt());
	}
	return 0;
}

uint32_t Item::getOwner() const
{
	ItemAttributes* attr = getAttribute("owner");
	if (attr && attr->isInt()) {
		return static_cast<uint32_t>(attr->getInt());
	}
	return 0;
}

uint32_t Item::getCorpseOwner()
{
	ItemAttributes* attr = getAttribute("corpseowner");
	if (attr && attr->isInt()) {
		return static_cast<uint32_t>(attr->getInt());
	}
	return 0;
}

ItemDecayState_t Item::getDecaying() const
{
	ItemAttributes* attr = getAttribute("decaying");
	if (attr && attr->isInt()) {
		return static_cast<ItemDecayState_t>(attr->getInt());
	}
	return DECAYING_FALSE;
}
