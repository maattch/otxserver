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

#include "cylinder.h"
#include "item.h"

class Depot;
class Container;

using ItemDeque = std::deque<Item*>;

class ContainerIterator final
{
public:
	bool hasNext() const { return !over.empty(); }

	void advance();
	Item* operator*();

private:
	std::list<const Container*> over;
	ItemDeque::const_iterator cur;

	friend class Container;
};

class Container : public Item, public Cylinder
{
public:
	Container(uint16_t type);
	Container(uint16_t type, uint16_t size);
	virtual ~Container();

	Item* clone() const override final;

	Container* getContainer() override final { return this; }
	const Container* getContainer() const override final { return this; }

	virtual Depot* getDepot() { return nullptr; }
	virtual const Depot* getDepot() const { return nullptr; }

	Attr_ReadValue readAttr(AttrTypes_t attr, PropStream& propStream) override;
	bool unserializeItemNode(FileLoader& f, NODE node, PropStream& propStream) override;

	std::string getContentDescription() const;
	uint32_t getItemHoldingCount() const;
	double getWeight() const override final;

	uint32_t capacity() const { return maxSize; }
	uint32_t size() const { return static_cast<uint32_t>(itemlist.size()); }
	bool empty() const { return itemlist.empty(); }
	bool full() const { return size() >= capacity(); }

	void addItem(Item* item);
	Item* getItemByIndex(uint32_t index) const;
	bool isHoldingItem(const Item* item) const;

	ContainerIterator iterator() const;

	const ItemDeque& getItemList() const { return itemlist; }

	ItemDeque::const_reverse_iterator getReversedItems() const { return itemlist.rbegin(); }
	ItemDeque::const_reverse_iterator getReversedEnd() const { return itemlist.rend(); }

	// cylinder implementations
	virtual Cylinder* getParent() { return Thing::getParent(); }
	virtual const Cylinder* getParent() const { return Thing::getParent(); }
	virtual bool isRemoved() const { return Thing::isRemoved(); }
	virtual Position getPosition() const { return Thing::getPosition(); }
	virtual Tile* getTile() { return Thing::getTile(); }
	virtual const Tile* getTile() const { return Thing::getTile(); }
	virtual Item* getItem() { return this; }
	virtual const Item* getItem() const { return this; }
	virtual Creature* getCreature() { return nullptr; }
	virtual const Creature* getCreature() const { return nullptr; }

	virtual ReturnValue __queryAdd(int32_t index, const Thing* thing, uint32_t count,
		uint32_t flags, Creature* actor = nullptr) const;
	virtual ReturnValue __queryMaxCount(int32_t index, const Thing* thing, uint32_t count, uint32_t& maxQueryCount,
		uint32_t flags) const;
	virtual ReturnValue __queryRemove(const Thing* thing, uint32_t count, uint32_t flags, Creature* actor = nullptr) const;
	virtual Cylinder* __queryDestination(int32_t& index, const Thing* thing, Item** destItem, uint32_t& flags);

	virtual void __addThing(Creature* actor, Thing* thing);
	virtual void __addThing(Creature* actor, int32_t index, Thing* thing);

	virtual void __updateThing(Thing* thing, uint16_t itemId, uint32_t count);
	virtual void __replaceThing(uint32_t index, Thing* thing);

	virtual void __removeThing(Thing* thing, uint32_t count);

	virtual int32_t __getIndexOfThing(const Thing* thing) const;
	virtual Thing* __getThing(uint32_t index) const;

	virtual int32_t __getFirstIndex() const;
	virtual int32_t __getLastIndex() const;

	virtual uint32_t __getItemTypeCount(uint16_t itemId, int32_t subType = -1) const;
	virtual std::map<uint32_t, uint32_t>& __getAllItemTypeCount(std::map<uint32_t, uint32_t>& countMap) const;

	virtual void postAddNotification(Creature* actor, Thing* thing, const Cylinder* oldParent,
		int32_t index, CylinderLink_t link = LINK_OWNER);
	virtual void postRemoveNotification(Creature* actor, Thing* thing, const Cylinder* newParent,
		int32_t index, bool isCompleteRemoval, CylinderLink_t link = LINK_OWNER);

	virtual void __internalAddThing(Thing* thing);
	virtual void __internalAddThing(uint32_t index, Thing* thing);
	virtual void __startDecaying();

private:
	void onAddContainerItem(Item* item) const;
	void onUpdateContainerItem(uint32_t index, Item* oldItem, Item* newItem) const;
	void onRemoveContainerItem(uint32_t index, Item* item) const;

	Container* getParentContainer();
	void updateItemWeight(double diff);

	ItemDeque itemlist;

protected:
	double totalWeight = 0;
	uint32_t maxSize = 0;
	uint32_t serializationCount = 0;

	friend class ContainerIterator;
	friend class IOMapSerialize;
};
