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

#include "container.h"

#include "game.h"
#include "iomap.h"
#include "player.h"

Container::Container(uint16_t type) : Container(type, items[type].maxItems)
{
	//
}

Container::Container(uint16_t type, uint16_t size) :
	Item(type),
	maxSize(size)
{
	//
}

Container::~Container()
{
	for (Item* item : itemlist) {
		item->setParent(nullptr);
		item->unRef();
	}
}

Item* Container::clone() const
{
	Container* cloneContainer = static_cast<Container*>(Item::clone());
	for (Item* item : itemlist) {
		cloneContainer->addItem(item->clone());
	}

	cloneContainer->totalWeight = totalWeight;
	return cloneContainer;
}

Container* Container::getParentContainer()
{
	if (Cylinder* cylinder = getParent()) {
		if (Item* item = cylinder->getItem()) {
			return item->getContainer();
		}
	}
	return nullptr;
}

void Container::addItem(Item* item)
{
	itemlist.push_back(item);
	item->setParent(this);
}

Attr_ReadValue Container::readAttr(AttrTypes_t attr, PropStream& propStream)
{
	switch (attr) {
		case ATTR_CONTAINER_ITEMS: {
			uint32_t count;
			if (!propStream.getLong(count)) {
				return ATTR_READ_ERROR;
			}

			serializationCount = count;
			return ATTR_READ_END;
		}

		default:
			break;
	}

	return Item::readAttr(attr, propStream);
}

bool Container::unserializeItemNode(FileLoader& f, NODE node, PropStream& propStream)
{
	if (!Item::unserializeItemNode(f, node, propStream)) {
		return false;
	}

	uint32_t type;
	for (NODE nodeItem = f.getChildNode(node, type); nodeItem; nodeItem = f.getNextNode(nodeItem, type)) {
		// load container items
		if (type != OTBM_ITEM) {
			return false;
		}

		PropStream itemPropStream;
		f.getProps(nodeItem, itemPropStream);

		Item* item = Item::CreateItem(itemPropStream);
		if (!item) {
			return false;
		}

		if (!item->unserializeItemNode(f, nodeItem, itemPropStream)) {
			return false;
		}

		addItem(item);
		updateItemWeight(item->getWeight());
	}
	return true;
}

void Container::updateItemWeight(double diff)
{
	totalWeight += diff;
	if (Container* parent = getParentContainer()) {
		parent->updateItemWeight(diff);
	}
}

double Container::getWeight() const
{
	return Item::getWeight() + totalWeight;
}

std::string Container::getContentDescription() const
{
	std::ostringstream ss;

	bool begin = true;
	for (ContainerIterator it = iterator(); it.hasNext(); it.advance()) {
		Container* container = (*it)->getContainer();
		if (container && !container->empty()) {
			continue;
		}

		if (!begin) {
			ss << ", ";
		} else {
			begin = false;
		}

		ss << (*it)->getNameDescription();
	}

	if (begin) {
		ss << "nothing";
	}
	return ss.str();
}

Item* Container::getItemByIndex(uint32_t index) const
{
	if (index >= size()) {
		return nullptr;
	}
	return itemlist[index];
}

uint32_t Container::getItemHoldingCount() const
{
	uint32_t counter = 0;
	for (ContainerIterator it = iterator(); it.hasNext(); it.advance()) {
		++counter;
	}
	return counter;
}

bool Container::isHoldingItem(const Item* item) const
{
	for (ContainerIterator it = iterator(); it.hasNext(); it.advance()) {
		if ((*it) == item) {
			return true;
		}
	}
	return false;
}

void Container::onAddContainerItem(Item* item) const
{
	const Position& cylinderMapPos = getPosition();

	SpectatorVec spectators;
	g_game.getSpectators(spectators, cylinderMapPos, false, true, 1, 1, 1, 1);

	// send to client
	for (Creature* spectator : spectators) {
		assert(dynamic_cast<Player*>(spectator) != nullptr);
		static_cast<Player*>(spectator)->sendAddContainerItem(this, item);
	}

	// event methods
	for (Creature* spectator : spectators) {
		assert(dynamic_cast<Player*>(spectator) != nullptr);
		static_cast<Player*>(spectator)->onAddContainerItem(this, item);
	}
}

void Container::onUpdateContainerItem(uint32_t index, Item* oldItem, Item* newItem) const
{
	const Position& cylinderMapPos = getPosition();

	SpectatorVec spectators;
	g_game.getSpectators(spectators, cylinderMapPos, false, true, 1, 1, 1, 1);

	// send to client
	for (Creature* spectator : spectators) {
		assert(dynamic_cast<Player*>(spectator) != nullptr);
		static_cast<Player*>(spectator)->sendUpdateContainerItem(this, index, newItem);
	}

	// event methods
	for (Creature* spectator : spectators) {
		assert(dynamic_cast<Player*>(spectator) != nullptr);
		static_cast<Player*>(spectator)->onUpdateContainerItem(this, oldItem, newItem);
	}
}

void Container::onRemoveContainerItem(uint32_t index, Item* item) const
{
	const Position& cylinderMapPos = getPosition();

	SpectatorVec spectators;
	g_game.getSpectators(spectators, cylinderMapPos, false, true, 1, 1, 1, 1);

	// send change to client
	for (Creature* spectator : spectators) {
		assert(dynamic_cast<Player*>(spectator) != nullptr);
		static_cast<Player*>(spectator)->sendRemoveContainerItem(this, index, item);
	}

	// event methods
	for (Creature* spectator : spectators) {
		assert(dynamic_cast<Player*>(spectator) != nullptr);
		static_cast<Player*>(spectator)->onRemoveContainerItem(this, index, item);
	}
}

ReturnValue Container::__queryAdd(int32_t index, const Thing* thing, uint32_t count,
	uint32_t flags, Creature* actor /* = nullptr*/) const
{
	if ((flags & FLAG_CHILDISOWNER) == FLAG_CHILDISOWNER) {
		// a child container is querying, since we are the top container (not carried by a player)
		// just return with no error.
		return RET_NOERROR;
	}

	const Item* item = thing->getItem();
	if (!item) {
		return RET_NOTPOSSIBLE;
	}

	if (!item->isPickupable()) {
		return RET_CANNOTPICKUP;
	}

	if (item == this) {
		return RET_THISISIMPOSSIBLE;
	}

	if (const Container* container = item->getContainer()) {
		for (const Cylinder* cylinder = getParent(); cylinder; cylinder = cylinder->getParent()) {
			if (cylinder == container) {
				return RET_THISISIMPOSSIBLE;
			}
		}
	}

	if ((flags & FLAG_NOLIMIT) != FLAG_NOLIMIT && (index == INDEX_WHEREEVER && size() >= capacity())) {
		return RET_CONTAINERNOTENOUGHROOM;
	}

	const Cylinder* topParent = getTopParent();
	if (topParent != this) {
		return topParent->__queryAdd(INDEX_WHEREEVER, item, count, flags | FLAG_CHILDISOWNER, actor);
	}
	return RET_NOERROR;
}

ReturnValue Container::__queryMaxCount(int32_t index, const Thing* thing, uint32_t count,
	uint32_t& maxQueryCount, uint32_t flags) const
{
	const Item* item = thing->getItem();
	if (!item) {
		maxQueryCount = 0;
		return RET_NOTPOSSIBLE;
	}

	if (flags & FLAG_NOLIMIT) {
		maxQueryCount = std::max<uint32_t>(1, count);
		return RET_NOERROR;
	}

	uint32_t freeSlots = std::max<int32_t>(0, capacity() - size());
	if (item->isStackable()) {
		uint32_t n = 0;
		if (index == INDEX_WHEREEVER) {
			// Iterate through every item and check how much free stackable slots there is.
			uint32_t slotIndex = 0;
			for (Item* containerItem : itemlist) {
				if (containerItem != item && containerItem->getID() == item->getID() && containerItem->getItemCount() < 100) {
					if (__queryAdd(slotIndex++, item, count, flags) == RET_NOERROR) {
						n += 100 - containerItem->getItemCount();
					}
				}
			}
		} else {
			const Item* destItem = getItemByIndex(index);
			if (destItem && destItem->getID() == item->getID() && destItem->getItemCount() < 100) {
				if (__queryAdd(index, item, count, flags) == RET_NOERROR) {
					n += 100 - destItem->getItemCount();
				}
			}
		}

		maxQueryCount = freeSlots * 100 + n;
		if (maxQueryCount < count) {
			return RET_CONTAINERNOTENOUGHROOM;
		}
	} else {
		maxQueryCount = freeSlots;
		if (maxQueryCount == 0) {
			return RET_CONTAINERNOTENOUGHROOM;
		}
	}
	return RET_NOERROR;
}

ReturnValue Container::__queryRemove(const Thing* thing, uint32_t count, uint32_t flags, Creature*) const
{
	int32_t index = __getIndexOfThing(thing);
	if (index == -1) {
		return RET_NOTPOSSIBLE;
	}

	const Item* item = thing->getItem();
	if (!item) {
		return RET_NOTPOSSIBLE;
	}

	if (count == 0 || (item->isStackable() && count > item->getItemCount())) {
		return RET_NOTPOSSIBLE;
	}

	if (!item->isMovable() && !hasBitSet(FLAG_IGNORENOTMOVABLE, flags)) {
		return RET_NOTMOVABLE;
	}
	return RET_NOERROR;
}

Cylinder* Container::__queryDestination(int32_t& index, const Thing* thing, Item** destItem,
	uint32_t& flags)
{
	if (index == 254 /*move up*/) {
		index = INDEX_WHEREEVER;
		*destItem = nullptr;

		Container* parentContainer = dynamic_cast<Container*>(getParent());
		if (parentContainer) {
			return parentContainer;
		}

		return this;
	}

	if (index == 255 /*add wherever*/) {
		index = INDEX_WHEREEVER;
		*destItem = nullptr;
	} else if (index >= static_cast<int32_t>(capacity())) {
		/*
		if you have a container, maximize it to show all 20 slots
		then you open a bag that is inside the container you will have a bag with 8 slots
		and a "grey" area where the other 12 slots where from the container
		if you drop the item on that grey area the client calculates the slot position
		as if the bag has 20 slots
		*/

		index = INDEX_WHEREEVER;
		*destItem = nullptr;
	}

	const Item* item = thing->getItem();
	if (!item) {
		return this;
	}

	if (index != INDEX_WHEREEVER) {
		Item* itemFromIndex = getItemByIndex(index);
		if (itemFromIndex) {
			*destItem = itemFromIndex;
		}

		if (Cylinder* subCylinder = dynamic_cast<Cylinder*>(*destItem)) {
			index = INDEX_WHEREEVER;
			*destItem = nullptr;
			return subCylinder;
		}
	}

	const bool autoStack = !hasBitSet(FLAG_IGNOREAUTOSTACK, flags);
	if (autoStack && item->isStackable() && item->getParent() != this) {
		if (*destItem && (*destItem)->getID() == item->getID() && (*destItem)->getItemCount() < 100) {
			return this;
		}

		// try to find a suitable item to stack with
		uint32_t n = 0;
		for (Item* listItem : itemlist) {
			if (listItem != item && listItem->getID() == item->getID() && listItem->getItemCount() < 100) {
				*destItem = listItem;
				index = n;
				return this;
			}
			++n;
		}
	}
	return this;
}

void Container::__addThing(Creature* actor, Thing* thing)
{
	return __addThing(actor, 0, thing);
}

void Container::__addThing(Creature*, int32_t index, Thing* thing)
{
	if (index >= static_cast<int32_t>(capacity())) {
		return /*RET_NOTPOSSIBLE*/;
	}

	Item* item = thing->getItem();
	if (!item) {
		return /*RET_NOTPOSSIBLE*/;
	}

	item->setParent(this);
	itemlist.push_front(item);
	updateItemWeight(item->getWeight());

	// send change to client
	Cylinder* parent = getParent();
	if (parent && parent != VirtualCylinder::virtualCylinder) {
		onAddContainerItem(item);
	}
}

void Container::__updateThing(Thing* thing, uint16_t itemId, uint32_t count)
{
	int32_t index = __getIndexOfThing(thing);
	if (index == -1) {
		return /*RET_NOTPOSSIBLE*/;
	}

	Item* item = thing->getItem();
	if (!item) {
		return /*RET_NOTPOSSIBLE*/;
	}

	const double oldWeight = item->getWeight();
	item->setID(itemId);
	item->setSubType(count);
	updateItemWeight(-oldWeight + item->getWeight());

	// send change to client
	if (getParent()) {
		onUpdateContainerItem(index, item, item);
	}
}

void Container::__replaceThing(uint32_t index, Thing* thing)
{
	Item* item = thing->getItem();
	if (!item) {
		return /*RET_NOTPOSSIBLE*/;
	}

	Item* replacedItem = getItemByIndex(index);
	if (!replacedItem) {
		return /*RETURNVALUE_NOTPOSSIBLE*/;
	}

	itemlist[index] = item;
	item->setParent(this);
	updateItemWeight(-replacedItem->getWeight() + item->getWeight());

	// send change to client
	if (getParent()) {
		onUpdateContainerItem(index, replacedItem, item);
	}
}

void Container::__removeThing(Thing* thing, uint32_t count)
{
	Item* item = thing->getItem();
	if (!item) {
		return /*RET_NOTPOSSIBLE*/;
	}

	const int32_t index = __getIndexOfThing(thing);
	if (index == -1) {
		return /*RETURNVALUE_NOTPOSSIBLE*/;
	}

	if (item->isStackable() && count != item->getItemCount()) {
		const double oldWeight = item->getWeight();
		item->setItemCount(std::max<int32_t>(0, item->getItemCount() - count));
		updateItemWeight(-oldWeight + item->getWeight());

		// send change to client
		if (getParent()) {
			onUpdateContainerItem(index, item, item);
		}
	} else {
		updateItemWeight(-item->getWeight());

		// send change to client
		if (getParent()) {
			onRemoveContainerItem(index, item);
		}

		item->setParent(nullptr);
		itemlist.erase(itemlist.begin() + index);
	}
}

Thing* Container::__getThing(uint32_t index) const
{
	return getItemByIndex(index);
}

int32_t Container::__getIndexOfThing(const Thing* thing) const
{
	int32_t index = 0;
	for (Item* item : itemlist) {
		if (item == thing) {
			return index;
		}
		++index;
	}
	return -1;
}

int32_t Container::__getFirstIndex() const
{
	return 0;
}

int32_t Container::__getLastIndex() const
{
	return size();
}

uint32_t Container::__getItemTypeCount(uint16_t itemId, int32_t subType /*= -1*/) const
{
	uint32_t count = 0;
	for (Item* item : itemlist) {
		if (item->getID() == itemId) {
			count += Item::countByType(item, subType);
		}
	}
	return count;
}

std::map<uint32_t, uint32_t>& Container::__getAllItemTypeCount(std::map<uint32_t, uint32_t>& countMap) const
{
	for (Item* item : itemlist) {
		countMap[item->getID()] += item->getItemCount();
	}
	return countMap;
}

void Container::postAddNotification(Creature* actor, Thing* thing, const Cylinder* oldParent,
	int32_t index, CylinderLink_t /* link = LINK_OWNER*/)
{
	Cylinder* topParent = getTopParent();
	if (!topParent->getCreature()) {
		if (topParent == this) {
			// let the tile class notify surrounding players
			if (topParent->getParent()) {
				topParent->getParent()->postAddNotification(actor, thing, oldParent, index, LINK_NEAR);
			}
		} else {
			topParent->postAddNotification(actor, thing, oldParent, index, LINK_PARENT);
		}
	} else {
		topParent->postAddNotification(actor, thing, oldParent, index, LINK_TOPPARENT);
	}
}

void Container::postRemoveNotification(Creature* actor, Thing* thing, const Cylinder* newParent,
	int32_t index, bool isCompleteRemoval, CylinderLink_t /* link = LINK_OWNER*/)
{
	Cylinder* topParent = getTopParent();
	if (!topParent->getCreature()) {
		if (topParent == this) {
			// let the tile class notify surrounding players
			if (topParent->getParent()) {
				topParent->getParent()->postRemoveNotification(actor, thing, newParent, index, isCompleteRemoval, LINK_NEAR);
			}
		} else {
			topParent->postRemoveNotification(actor, thing, newParent, index, isCompleteRemoval, LINK_PARENT);
		}
	} else {
		topParent->postRemoveNotification(actor, thing, newParent, index, isCompleteRemoval, LINK_TOPPARENT);
	}
}

void Container::__internalAddThing(Thing* thing)
{
	__internalAddThing(0, thing);
}

void Container::__internalAddThing(uint32_t, Thing* thing)
{
	if (!thing) {
		return;
	}

	Item* item = thing->getItem();
	if (!item) {
		return;
	}

	itemlist.push_front(item);
	item->setParent(this);
	updateItemWeight(item->getWeight());
}

void Container::__startDecaying()
{
	Item::__startDecaying();
	for (Item* item : itemlist) {
		item->__startDecaying();
	}
}

ContainerIterator Container::iterator() const
{
	ContainerIterator cit;
	if (!itemlist.empty()) {
		cit.over.push_back(this);
		cit.cur = itemlist.begin();
	}
	return cit;
}

Item* ContainerIterator::operator*()
{
	return *cur;
}

void ContainerIterator::advance()
{
	if (Item* i = *cur) {
		if (Container* c = i->getContainer()) {
			if (!c->empty()) {
				over.push_back(c);
			}
		}
	}

	if (++cur == over.front()->itemlist.end()) {
		over.pop_front();
		if (!over.empty()) {
			cur = over.front()->itemlist.begin();
		}
	}
}
