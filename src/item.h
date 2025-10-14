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

#include "fileloader.h"
#include "items.h"
#include "raids.h"
#include "thing.h"

#include <variant>

class Creature;
class Player;
class Container;
class Depot;
class TrashHolder;
class Mailbox;
class Teleport;
class MagicField;
class Door;
class BedItem;

static constexpr uint16_t ITEM_ATTRIBUTES_VERSION = 1;

enum ITEMPROPERTY
{
	BLOCKSOLID = 0,
	HASHEIGHT,
	BLOCKPROJECTILE,
	BLOCKPATH,
	ISVERTICAL,
	ISHORIZONTAL,
	MOVABLE,
	IMMOVABLEBLOCKSOLID,
	IMMOVABLEBLOCKPATH,
	IMMOVABLENOFIELDBLOCKPATH,
	NOFIELDBLOCKPATH,
	SUPPORTHANGABLE,
	FLOORCHANGEDOWN,
	FLOORCHANGEUP
};

enum TradeEvents_t
{
	ON_TRADE_TRANSFER,
	ON_TRADE_CANCEL,
};

enum ItemDecayState_t
{
	DECAYING_FALSE = 0,
	DECAYING_TRUE,
	DECAYING_PENDING
};

enum AttrTypes_t
{
	ATTR_END = 0,
	// ATTR_DESCRIPTION = 1,
	// ATTR_EXT_FILE = 2,
	ATTR_TILE_FLAGS = 3,
	ATTR_ACTION_ID = 4,
	ATTR_UNIQUE_ID = 5,
	ATTR_TEXT = 6,
	ATTR_DESC = 7,
	ATTR_TELE_DEST = 8,
	ATTR_ITEM = 9,
	ATTR_DEPOT_ID = 10,
	// ATTR_EXT_SPAWN_FILE = 11,
	ATTR_RUNE_CHARGES = 12,
	// ATTR_EXT_HOUSE_FILE = 13,
	ATTR_HOUSEDOORID = 14,
	ATTR_COUNT = 15,
	ATTR_DURATION = 16,
	ATTR_DECAYING_STATE = 17,
	ATTR_WRITTENDATE = 18,
	ATTR_WRITTENBY = 19,
	ATTR_SLEEPERGUID = 20,
	ATTR_SLEEPSTART = 21,
	ATTR_CHARGES = 22,
	ATTR_CONTAINER_ITEMS = 23,
	ATTR_NAME = 30,
	ATTR_PLURALNAME = 31,
	ATTR_ATTACK = 33,
	ATTR_EXTRAATTACK = 34,
	ATTR_DEFENSE = 35,
	ATTR_EXTRADEFENSE = 36,
	ATTR_ARMOR = 37,
	ATTR_ATTACKSPEED = 38,
	ATTR_HITCHANCE = 39,
	ATTR_SHOOTRANGE = 40,
	ATTR_ARTICLE = 41,
	ATTR_SCRIPTPROTECTED = 42,
	ATTR_DUALWIELD = 43,
	ATTR_CRITICALHITCHANCE = 44, // unused
	ATTR_REDUCE_SKILL_LOSS = 45, // unused

	ATTR_ATTRIBUTE_MAP_OLD = 128,
	ATTR_ATTRIBUTE_MAP = 129,
};

enum Attr_ReadValue
{
	ATTR_READ_CONTINUE,
	ATTR_READ_ERROR,
	ATTR_READ_END
};

typedef std::list<Item*> ItemList;
typedef std::vector<Item*> ItemVector;

class ItemAttributes final
{
public:
	constexpr ItemAttributes() = default;

	template<typename T>
	explicit ItemAttributes(const T& v) : value(v) {}

	constexpr bool operator==(const ItemAttributes& o) const noexcept { return value == o.value; }
	constexpr bool operator!=(const ItemAttributes& o) const noexcept { return !(*this == o); }

	constexpr bool isBlank() noexcept { return value.index() == 0; }
	constexpr bool isString() noexcept { return value.index() == 1; }
	constexpr bool isInt() noexcept { return value.index() == 2; }
	constexpr bool isDouble() noexcept { return value.index() == 3; }
	constexpr bool isBool() noexcept { return value.index() == 4; }

	constexpr const std::string& getString() const { return std::get<1>(value); }
	constexpr int64_t getInt() const { return std::get<2>(value); }
	constexpr double getDouble() const { return std::get<3>(value); }
	constexpr bool getBool() const { return std::get<4>(value); }

	void pushToLua(lua_State* L) const {
		struct PushLuaVisitor
		{
			explicit PushLuaVisitor(lua_State* L) : L(L) {}
			void operator()(std::monostate) { lua_pushnil(L); }
			void operator()(const std::string& v) { lua_pushlstring(L, v.data(), v.size()); }
			void operator()(int64_t v) { lua_pushnumber(L, v); }
			void operator()(double v) { lua_pushnumber(L, v); }
			void operator()(bool v) { lua_pushboolean(L, v); }
			lua_State* L;
		};
		std::visit(PushLuaVisitor(L), value);
	}

	void serialize(PropWriteStream& propWriteStream) const {
		propWriteStream.addByte(static_cast<uint8_t>(value.index()));
		struct SerializeVisitor
		{
			explicit SerializeVisitor(PropWriteStream& propWriteStream) : propWriteStream(propWriteStream) {}
			void operator()(std::monostate) {}
			void operator()(const std::string& v) { propWriteStream.addLongString(v); }
			void operator()(int64_t v) { propWriteStream.addType<int64_t>(v); }
			void operator()(double v) { propWriteStream.addType<double>(v); }
			void operator()(bool v) { propWriteStream.addType<bool>(v); }
			PropWriteStream& propWriteStream;
		};
		std::visit(SerializeVisitor(propWriteStream), value);
	}

	bool unserialize_old(PropStream& propStream) {
		uint8_t type;
		if (!propStream.getByte(type)) {
			return false;
		}

		switch (type) {
			case 1: {
				// String
				std::string v;
				if (!propStream.getLongString(v)) {
					return false;
				}

				value = v;
				break;
			}
			case 2: {
				// Integer
				int32_t v;
				if (!propStream.getType<int32_t>(v)) {
					return false;
				}

				value = static_cast<int64_t>(v);
				break;
			}
			case 3: {
				// Double
				float v;
				if (!propStream.getType<float>(v)) {
					return false;
				}

				value = static_cast<double>(v);
				break;
			}
			case 4: {
				// Bool
				uint8_t v;
				if (!propStream.getType<uint8_t>(v)) {
					return false;
				}

				value = (v != 0);
				break;
			}

			default: {
				value = std::monostate();
				return false;
			}
		}
		return true;
	}

	bool unserialize(PropStream& propStream) {
		uint8_t type;
		if (!propStream.getByte(type)) {
			return false;
		}

		switch (type) {
			case 1: {
				// String
				std::string v;
				if (!propStream.getLongString(v)) {
					return false;
				}

				value = v;
				break;
			}
			case 2: {
				// Integer
				int64_t v;
				if (!propStream.getType<int64_t>(v)) {
					return false;
				}

				value = v;
				break;
			}
			case 3: {
				// Double
				double v;
				if (!propStream.getType<double>(v)) {
					return false;
				}

				value = v;
				break;
			}
			case 4: {
				// Bool
				bool v;
				if (!propStream.getType<bool>(v)) {
					return false;
				}

				value = v;
				break;
			}

			default: {
				value = std::monostate();
				return false;
			}
		}
		return true;
	}

private:
	std::variant<std::monostate, std::string, int64_t, double, bool> value;
};


class Item : virtual public Thing
{
public:
	Item(const uint16_t type, uint16_t amount = 0);
	Item(const Item& i) : Thing(), m_id(i.m_id), m_count(i.m_count) {}
	virtual ~Item() {}

	static Items items;

	// Factory member to create item of right type based on type
	static Item* CreateItem(const uint16_t type, uint16_t amount = 0);
	static Item* CreateItem(PropStream& propStream);

	virtual Item* clone() const;
	virtual void copyAttributes(Item* item);
	void makeUnique(Item* parent);

	Item* getItem() override { return this; }
	const Item* getItem() const override { return this; }

	virtual Container* getContainer() { return nullptr; }
	virtual const Container* getContainer() const { return nullptr; }

	virtual Teleport* getTeleport() { return nullptr; }
	virtual const Teleport* getTeleport() const { return nullptr; }

	virtual TrashHolder* getTrashHolder() { return nullptr; }
	virtual const TrashHolder* getTrashHolder() const { return nullptr; }

	virtual Mailbox* getMailbox() { return nullptr; }
	virtual const Mailbox* getMailbox() const { return nullptr; }

	virtual Door* getDoor() { return nullptr; }
	virtual const Door* getDoor() const { return nullptr; }

	virtual MagicField* getMagicField() { return nullptr; }
	virtual const MagicField* getMagicField() const { return nullptr; }

	virtual BedItem* getBed() { return nullptr; }
	virtual const BedItem* getBed() const { return nullptr; }

	uint16_t getID() const { return m_id; }
	void setID(uint16_t newid);
	uint16_t getClientID() const { return items[m_id].clientId; }

	static std::string getDescription(const ItemType& it, int32_t lookDistance, const Item* item = nullptr, int32_t subType = -1, bool addArticle = true);
	static std::string getNameDescription(const ItemType& it, const Item* item = nullptr, int32_t subType = -1, bool addArticle = true);
	static std::string getWeightDescription(double weight, bool stackable, uint32_t count = 1);

	virtual std::string getDescription(int32_t lookDistance) const { return getDescription(items[m_id], lookDistance, this); }
	std::string getNameDescription() const { return getNameDescription(items[m_id], this); }
	std::string getWeightDescription() const { return getWeightDescription(getWeight(), items[m_id].stackable && items[m_id].showCount, m_count); }

	Player* getHoldingPlayer();
	const Player* getHoldingPlayer() const;

	// attributes
	ItemAttributes* getAttribute(std::string_view key) const;
	bool eraseAttribute(std::string_view key);

	void setStrAttr(const std::string& key, const std::string& value);
	void setIntAttr(const std::string& key, int64_t value);
	void setDoubleAttr(const std::string& key, double value);
	void setBoolAttr(const std::string& key, bool value);

	bool hasStrAttr(std::string_view key) const;
	bool hasIntAttr(std::string_view key) const;
	bool hasDoubleAttr(std::string_view key) const;
	bool hasBoolAttr(std::string_view key) const;

	// serialization
	virtual Attr_ReadValue readAttr(AttrTypes_t attr, PropStream& propStream);
	virtual bool unserializeAttr(PropStream& propStream);
	virtual bool serializeAttr(PropWriteStream& propWriteStream) const;
	virtual bool unserializeItemNode(FileLoader&, NODE, PropStream& propStream) { return unserializeAttr(propStream); }

	// Item attributes
	void setDuration(int32_t time) { m_duration = time; }
	void decreaseDuration(int32_t time) { m_duration -= time; }
	int32_t getDuration() const { return m_duration; }

	void setSpecialDescription(const std::string& description) { setStrAttr("description", description); }
	void resetSpecialDescription() { eraseAttribute("description"); }
	const std::string& getSpecialDescription() const;

	void setText(const std::string& text) { setStrAttr("text", text); }
	void resetText() { eraseAttribute("text"); }
	const std::string& getText() const;

	void setDate(time_t date) { setIntAttr("date", date); }
	void resetDate() { eraseAttribute("date"); }
	time_t getDate() const;

	void setWriter(const std::string& writer) { setStrAttr("writer", writer); }
	void resetWriter() { eraseAttribute("writer"); }
	const std::string& getWriter() const;

	void setActionId(int32_t aid, bool callEvent = true);
	void resetActionId(bool callEvent = true);
	int32_t getActionId() const;

	void setUniqueId(int32_t uid);
	int32_t getUniqueId() const;

	void setCharges(uint16_t charges) { setIntAttr("charges", charges); }
	void resetCharges() { eraseAttribute("charges"); }
	uint16_t getCharges() const;

	void setFluidType(uint16_t fluidType) { setIntAttr("fluidtype", fluidType); }
	void resetFluidType() { eraseAttribute("fluidtype"); }
	uint16_t getFluidType() const;

	void setOwner(uint32_t owner) { setIntAttr("owner", owner); }
	uint32_t getOwner() const;

	void setCorpseOwner(uint32_t corpseOwner) { setIntAttr("corpseowner", corpseOwner); }
	uint32_t getCorpseOwner();

	void setDecaying(ItemDecayState_t state) { setIntAttr("decaying", state); }
	ItemDecayState_t getDecaying() const;

	const std::string& getName() const;
	const std::string& getPluralName() const;
	const std::string& getArticle() const;

	bool isScriptProtected() const;
	bool isDualWield() const;

	int32_t getAttack() const;
	int32_t getReduceSkillLoss() const;
	int32_t getExtraAttack() const;
	int32_t getDefense() const;
	int32_t getExtraDefense() const;

	int32_t getArmor() const;
	int32_t getAttackSpeed() const;
	int32_t getHitChance() const;
	int32_t getShootRange() const;

	Ammo_t getAmmoType() const { return items[m_id].ammoType; }
	WeaponType_t getWeaponType() const { return items[m_id].weaponType; }
	int32_t getSlotPosition() const { return items[m_id].slotPosition; }
	int32_t getWieldPosition() const { return items[m_id].wieldPosition; }

	virtual double getWeight() const;
	void getLight(LightInfo& lightInfo);

	int32_t getMaxWriteLength() const { return items[m_id].maxTextLength; }
	int32_t getWorth() const { return m_count * items[m_id].worth; }
	virtual int32_t getThrowRange() const { return (isPickupable() ? 15 : 2); }

	bool floorChange(FloorChange_t change = CHANGE_NONE) const;
	bool forceSerialize() const { return items[m_id].forceSerialize || canWriteText() || isContainer() || isBed() || isDoor(); }

	bool hasProperty(enum ITEMPROPERTY prop) const;
	bool hasSubType() const { return items[m_id].hasSubType(); }
	bool hasCharges() const { return hasIntAttr("charges"); }

	bool canDecay();
	virtual bool canRemove() const { return true; }
	virtual bool canTransform() const { return true; }
	bool canWriteText() const { return items[m_id].canWriteText; }

	virtual bool isPushable() const { return isMovable(); }
	virtual bool isBlocking(const Creature*) const { return items[m_id].blockSolid; }
	bool isGroundTile() const { return items[m_id].isGroundTile(); }
	bool isContainer() const { return items[m_id].isContainer(); }
	bool isSplash() const { return items[m_id].isSplash(); }
	bool isFluidContainer() const { return (items[m_id].isFluidContainer()); }
	bool isDoor() const { return items[m_id].isDoor(); }
	bool isMagicField() const { return items[m_id].isMagicField(); }
	bool isTeleport() const { return items[m_id].isTeleport(); }
	bool isKey() const { return items[m_id].isKey(); }
	bool isDepot() const { return items[m_id].isDepot(); }
	bool isMailbox() const { return items[m_id].isMailbox(); }
	bool isTrashHolder() const { return items[m_id].isTrashHolder(); }
	bool isBed() const { return items[m_id].isBed(); }
	bool isRune() const { return items[m_id].isRune(); }
	bool isStackable() const { return items[m_id].stackable; }
	bool isAlwaysOnTop() const { return items[m_id].alwaysOnTop; }
	bool isMovable() const { return items[m_id].movable; }
	bool isPickupable() const { return items[m_id].pickupable; }
	bool isUsable() const { return items[m_id].usable; }
	bool isHangable() const { return items[m_id].isHangable; }
	bool isRoteable() const {
		const ItemType& it = items[m_id];
		return it.rotable && it.rotateTo != 0;
	}
	bool isWeapon() const { return (items[m_id].weaponType != WEAPON_NONE); }
	bool isReadable() const { return items[m_id].canReadText; }
	bool isWare() const { return items[m_id].wareId != 0; }
	bool isPremiumScroll() const { return items[m_id].premiumDays > 0; }

	bool isLoadedFromMap() const { return m_loadedFromMap; }
	void setLoadedFromMap(bool value) { m_loadedFromMap = value; }

	CombatType_t getElementType() const { return items[m_id].hasAbilities() ? items[m_id].abilities->elementType : COMBAT_NONE; }
	int32_t getElementDamage() const { return items[m_id].hasAbilities() ? items[m_id].abilities->elementDamage : 0; }

	uint16_t getItemCount() const { return m_count; }
	void setItemCount(uint16_t n) { m_count = std::max<uint16_t>(1, n); }

	uint16_t getSubType() const;
	void setSubType(uint16_t n);

	uint32_t getDefaultDuration() const { return items[m_id].decayTime * 1000; }
	void setDefaultDuration()
	{
		uint32_t duration = getDefaultDuration();
		if (duration) {
			setDuration(duration);
		}
	}

	void setDefaultSubtype();

	Raid* getRaid() { return m_raid; }
	void setRaid(Raid* raid) { m_raid = raid; }

	virtual void __startDecaying();
	virtual void onRemoved();
	virtual bool onTradeEvent(TradeEvents_t, Player*, Player*) { return true; }

	static uint32_t countByType(const Item* item, int32_t checkType);

#ifdef _MSC_VER
	// MSVC C++17 does not support transparent lookup in unordered_map yet (only C++20 above)
	struct StringViewLess
	{
		using is_transparent = void;
		bool operator()(std::string_view lhs, std::string_view rhs) const noexcept {
			return lhs < rhs;
		}
	};
	using AttributeMap = std::map<std::string, ItemAttributes, StringViewLess>;
#else
	struct StringViewHash
	{
		using is_transparent = void;
		std::size_t operator()(const std::string_view& sv) const {
			return std::hash<std::string_view>{}(sv);
		}
	};
	struct StringViewEqual
	{
		using is_transparent = void;
		bool operator()(const std::string_view& lhs, const std::string_view& rhs) const noexcept {
			return lhs == rhs;
		}
	};
	using AttributeMap = std::unordered_map<std::string, ItemAttributes, StringViewHash, StringViewEqual>;
#endif

	AttributeMap& getAttributes() {
		if (!m_attributes) {
			m_attributes.reset(new AttributeMap);
		}
		return *m_attributes;
	}

protected:
	Raid* m_raid = nullptr; // TOOD: move it out of item class
	uint16_t m_id;
	uint8_t m_count;

private:
	std::unique_ptr<AttributeMap> m_attributes;
	int32_t m_duration = 0; // TOOD: move it out of item class
	bool m_loadedFromMap = false;
};
