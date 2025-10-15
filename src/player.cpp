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

#include "player.h"

#include "beds.h"
#include "chat.h"
#include "combat.h"
#include "configmanager.h"
#include "creatureevent.h"
#include "game.h"
#include "house.h"
#include "ioban.h"
#include "ioguild.h"
#include "iologindata.h"
#include "movement.h"
#include "npc.h"
#include "quests.h"
#include "spectators.h"
#include "weapons.h"

#include "otx/util.hpp"

uint32_t Player::playerAutoID = 0x10000000;

#if ENABLE_SERVER_DIAGNOSTIC > 0
uint32_t Player::playerCount = 0;
#endif
MuteCountMap Player::muteCountMap;

namespace
{
	bool checkText(std::string text, std::string_view str)
	{
		otx::util::trim_string(text);
		return caseInsensitiveEqual(text, str);
	}

} // namespace

Player::Player(const std::string& name, ProtocolGame_ptr p) :
	Creature(),
	m_transferContainer(ITEM_LOCKER),
	m_name(name),
	m_nameDescription(name),
	m_client(new Spectators(p)),
	m_lastLoad(otx::util::mstime()),
	m_lastPong(m_lastLoad),
	m_lastPing(m_lastLoad),
	m_lastAttack(m_lastLoad),
	m_lastMail(m_lastLoad)
{
#if ENABLE_SERVER_DIAGNOSTIC > 0
	++Player::playerCount;
#endif

	if (m_client->getOwner()) {
		p->setPlayer(this);
	}
}

Player::~Player()
{
#if ENABLE_SERVER_DIAGNOSTIC > 0
	--Player::playerCount;
#endif

	delete m_client;
	for (int32_t i = 0; i < 11; ++i) {
		if (!m_inventory[i]) {
			continue;
		}

		m_inventory[i]->setParent(nullptr);
		m_inventory[i]->unRef();

		m_inventory[i] = nullptr;
		m_inventoryAbilities[i] = false;
	}

	setWriteItem(nullptr);
	setNextWalkActionTask(nullptr);

	m_transferContainer.setParent(nullptr);
	for (DepotMap::iterator it = m_depots.begin(); it != m_depots.end(); ++it) {
		it->second.first->unRef();
	}
}

bool Player::setVocation(uint32_t id)
{
	const Vocation* vocation = g_vocations.getVocation(id);
	if (!vocation) {
		return false;
	}

	m_vocationId = id;
	m_vocation = vocation;

	Creature::setDropLoot(vocation->dropLoot ? LOOT_DROP_FULL : LOOT_DROP_PREVENT);
	Creature::setLossSkill(vocation->skillLoss);

	m_soulMax = vocation->gain[GAIN_SOUL];
	if (Condition* condition = getCondition(CONDITION_REGENERATION, CONDITIONID_DEFAULT)) {
		condition->setParam(CONDITIONPARAM_HEALTHGAIN, vocation->gainAmount[GAIN_HEALTH]);
		condition->setParam(CONDITIONPARAM_HEALTHTICKS, (vocation->gainTicks[GAIN_HEALTH] * 1000));
		condition->setParam(CONDITIONPARAM_MANAGAIN, vocation->gainAmount[GAIN_MANA]);
		condition->setParam(CONDITIONPARAM_MANATICKS, (vocation->gainTicks[GAIN_MANA] * 1000));
	}

	// TODO: update basespeed and skills tries to prevent overflow
	return true;
}

void Player::setDropLoot(lootDrop_t _lootDrop)
{
	if (m_vocation && !m_vocation->dropLoot) {
		_lootDrop = LOOT_DROP_PREVENT;
	}

	Creature::setDropLoot(_lootDrop);
}

void Player::setLossSkill(bool _skillLoss)
{
	if (m_vocation && !m_vocation->skillLoss) {
		_skillLoss = false;
	}

	Creature::setLossSkill(_skillLoss);
}

bool Player::isPushable() const
{
	return m_accountManager == MANAGER_NONE && !hasFlag(PlayerFlag_CannotBePushed) && Creature::isPushable();
}

std::string Player::getDescription(int32_t lookDistance) const
{
	std::ostringstream s;
	if (lookDistance == -1) {
		s << "yourself.";
		if (hasFlag(PlayerFlag_ShowGroupNameInsteadOfVocation)) {
			s << " You are " << m_group->name;
		} else if (m_vocationId != 0) {
			s << " You are " << m_vocation->description;
		} else {
			s << " You have no vocation";
		}
	} else {
		s << m_nameDescription;
		if (!hasCustomFlag(PlayerCustomFlag_HideLevel)) {
			s << " (Level " << m_level << ")";
		}

		s << ". " << (m_sex % 2 ? "He" : "She");
		if (hasFlag(PlayerFlag_ShowGroupNameInsteadOfVocation)) {
			s << " is " << m_group->name;
		} else if (m_vocationId != 0) {
			s << " is " << m_vocation->description;
		} else {
			s << " has no vocation";
		}
	}

	std::string tmp;
	if (m_marriage && IOLoginData::getInstance()->getNameByGuid(m_marriage, tmp)) {
		s << ", ";
		if (m_vocationId == 0) {
			if (lookDistance == -1) {
				s << "and you are";
			} else {
				s << "and is";
			}

			s << " ";
		}

		s << (m_sex % 2 ? "husband" : "wife") << " of " << tmp;
	}

	s << ".";
	if (m_guildId) {
		if (lookDistance == -1) {
			s << " You are ";
		} else {
			s << " " << (m_sex % 2 ? "He" : "She") << " is ";
		}

		s << (m_rankName.empty() ? "a member" : m_rankName) << " of the " << m_guildName;
		if (!m_guildNick.empty()) {
			s << " (" << m_guildNick << ")";
		}

		s << ".";
	}

	s << getSpecialDescription();
	return s.str();
}

Item* Player::getInventoryItem(Slots_t slot) const
{
	if (slot >= SLOT_FIRST && slot <= SLOT_LAST) {
		return m_inventory[slot];
	}

	if (slot == SLOT_HAND) {
		return m_inventory[SLOT_LEFT] ? m_inventory[SLOT_LEFT] : m_inventory[SLOT_RIGHT];
	}
	return nullptr;
}

Item* Player::getEquippedItem(Slots_t slot) const
{
	Item* item = getInventoryItem(slot);
	if (!item) {
		return nullptr;
	}

	switch (slot) {
		case SLOT_LEFT:
		case SLOT_RIGHT:
			if (item->getWieldPosition() == SLOT_HAND) {
				return item;
			}
			break;

		default:
			break;
	}

	return item->getWieldPosition() == slot ? item : nullptr;
}

void Player::setConditionSuppressions(uint32_t conditions, bool remove)
{
	if (remove) {
		m_conditionSuppressions &= ~conditions;
	} else {
		m_conditionSuppressions |= conditions;
	}
}

Item* Player::getWeapon(Slots_t slot, bool ignoreAmmo) const
{
	Item* item = m_inventory[slot];
	if (!item) {
		return nullptr;
	}

	const WeaponType_t weaponType = item->getWeaponType();
	if (weaponType == WEAPON_NONE || weaponType == WEAPON_SHIELD || weaponType == WEAPON_AMMO) {
		return nullptr;
	}

	if (!ignoreAmmo && weaponType == WEAPON_DIST) {
		const ItemType& it = Item::items[item->getID()];
		if (it.ammoType != AMMO_NONE) {
			Item* ammoItem = m_inventory[SLOT_AMMO];
			if (!ammoItem || ammoItem->getAmmoType() != it.ammoType) {
				return nullptr;
			}
			item = ammoItem;
		}
	}
	return item;
}

Item* Player::getWeapon(bool ignoreAmmo/* = false*/) const
{
	Item* item = getWeapon(SLOT_LEFT, ignoreAmmo);
	if (!item) {
		item = getWeapon(SLOT_RIGHT, ignoreAmmo);
	}
	return item;
}

WeaponType_t Player::getWeaponType()
{
	if (Item* item = getWeapon()) {
		return item->getWeaponType();
	}
	return WEAPON_NONE;
}

int32_t Player::getWeaponSkill(const Item* item) const
{
	if (!item) {
		return getSkillLevel(SKILL_FIST);
	}

	switch (item->getWeaponType()) {
		case WEAPON_SWORD:
			return getSkillLevel(SKILL_SWORD);

		case WEAPON_CLUB:
			return getSkillLevel(SKILL_CLUB);

		case WEAPON_AXE:
			return getSkillLevel(SKILL_AXE);

		case WEAPON_FIST:
			return getSkillLevel(SKILL_FIST);

		case WEAPON_DIST:
		case WEAPON_AMMO:
			return getSkillLevel(SKILL_DIST);

		default:
			break;
	}

	return 0;
}

int32_t Player::getArmor() const
{
	int32_t i = SLOT_FIRST, armor = 0;
	for (; i <= SLOT_LAST; ++i) {
		if (Item* item = getInventoryItem(static_cast<Slots_t>(i))) {
			if (item->getWieldPosition() == i) {
				armor += item->getArmor();
			}
		}
	}
	return armor * m_vocation->formulaMultipliers[MULTIPLIER_ARMOR];
}

int32_t Player::getDefense() const
{
	int32_t baseDefense = 5;
	int32_t defenseValue = 0;
	int32_t defenseSkill = 0;
	int32_t extraDefense = 0;

	const float defenseFactor = getDefenseFactor();

	Item* weapon = nullptr;
	Item* shield = nullptr;

	for (uint8_t slot = SLOT_RIGHT; slot <= SLOT_LEFT; slot++) {
		Item* item = m_inventory[slot];
		if (!item) {
			continue;
		}

		switch (item->getWeaponType()) {
			case WEAPON_NONE: {
				break;
			}
			case WEAPON_SHIELD: {
				if (!shield || item->getDefense() > shield->getDefense()) {
					shield = item;
				}
				break;
			}

			default: {
				// weapons that are not shields
				weapon = item;
				break;
			}
		}
	}

	if (weapon) {
		extraDefense = weapon->getExtraDefense();
		defenseValue = baseDefense + weapon->getDefense();
		defenseSkill = getWeaponSkill(weapon);
	}

	if (shield && shield->getDefense() > defenseValue) {
		if (shield->getExtraDefense() > extraDefense) {
			extraDefense = shield->getExtraDefense();
		}

		defenseValue = baseDefense + shield->getDefense();
		defenseSkill = getSkillLevel(SKILL_SHIELD);
	}

	if (defenseSkill == 0) {
		return 0;
	}

	defenseValue += extraDefense;
	defenseValue *= m_vocation->formulaMultipliers[MULTIPLIER_DEFENSE];
	return std::ceil(((defenseSkill * (defenseValue * 0.015)) + (defenseValue * 0.1)) * defenseFactor);
}

float Player::getAttackFactor() const
{
	switch (m_fightMode) {
		case FIGHTMODE_BALANCED:
			return 1.2f;

		case FIGHTMODE_DEFENSE:
			return 2.0f;

		case FIGHTMODE_ATTACK:
		default:
			break;
	}

	return 1.0f;
}

float Player::getDefenseFactor() const
{
	switch (m_fightMode) {
		case FIGHTMODE_BALANCED:
			return 1.2f;

		case FIGHTMODE_DEFENSE: {
			if ((otx::util::mstime() - m_lastAttack) < getAttackSpeed()) { // attacking will cause us to get into normal defense
				return 1.0f;
			}

			return 2.0f;
		}

		case FIGHTMODE_ATTACK:
		default:
			break;
	}

	return 1.0f;
}

void Player::sendIcons() const
{
	if (!m_client) {
		return;
	}

	uint32_t icons = ICON_NONE;
	for (ConditionList::const_iterator it = m_conditions.begin(); it != m_conditions.end(); ++it) {
		if (!isSuppress((*it)->getType())) {
			icons |= (*it)->getIcons();
		}
	}

	if (getZone() == ZONE_PROTECTION) {
		icons |= ICON_PZ;

		if (hasBitSet(ICON_SWORDS, icons)) {
			icons &= ~ICON_SWORDS;
		}
	}

	if (m_pzLocked) {
		icons |= ICON_PZBLOCK;
	}

	// Tibia client debugs with 10 or more icons
	// so let's prevent that from happening.
	std::bitset<20> icon_bitset(icons);
	for (size_t i = 0, size = icon_bitset.size(); i < size; ++i) {
		if (icon_bitset.count() < 10) {
			break;
		}

		if (icon_bitset[i]) {
			icon_bitset.reset(i);
		}
	}
	m_client->sendIcons(icon_bitset.to_ulong());
}

void Player::updateInventoryWeight()
{
	m_inventoryWeight = 0.00;
	if (hasFlag(PlayerFlag_HasInfiniteCapacity)
		|| !otx::config::getBoolean(otx::config::USE_CAPACITY)) {
		return;
	}

	for (int32_t i = SLOT_FIRST; i <= SLOT_LAST; ++i) {
		if (Item* item = getInventoryItem(static_cast<Slots_t>(i))) {
			m_inventoryWeight += item->getWeight();
		}
	}
}

void Player::updateInventoryGoods(uint32_t itemId)
{
	if (Item::items[itemId].worth) {
		sendGoods();
		return;
	}

	for (ShopInfoList::iterator it = m_shopOffer.begin(); it != m_shopOffer.end(); ++it) {
		if (it->itemId != itemId) {
			continue;
		}

		sendGoods();
		break;
	}
}

void Player::addSkillAdvance(Skills_t skill, uint64_t count, bool useMultiplier /* = true*/)
{
	if (count == 0) {
		return;
	}

	// player has reached max skill
	uint64_t currReqTries = m_vocation->getReqSkillTries(skill, m_skills[skill].level);
	uint64_t nextReqTries = m_vocation->getReqSkillTries(skill, m_skills[skill].level + 1);
	if (currReqTries > nextReqTries) {
		return;
	}

	if (useMultiplier) {
		count *=  m_rates[skill] * otx::config::getDouble(otx::config::RATE_SKILL);
	}

	std::ostringstream s;
	while (m_skills[skill].tries + count >= nextReqTries) {
		count -= nextReqTries - m_skills[skill].tries;
		m_skills[skill].tries = m_skills[skill].percent = 0;
		m_skills[skill].level++;

		s.str("");
		s << "You advanced to " << getSkillName(skill) << " level " << m_skills[skill].level << ".";
		sendTextMessage(MSG_EVENT_ADVANCE, s.str().c_str());

		if (hasEventRegistered(CREATURE_EVENT_ADVANCE)) {
			for (CreatureEvent* it : getCreatureEvents(CREATURE_EVENT_ADVANCE)) {
				it->executeAdvance(this, skill, (m_skills[skill].level - 1), m_skills[skill].level);
			}
		}

		currReqTries = nextReqTries;
		nextReqTries = m_vocation->getReqSkillTries(skill, m_skills[skill].level + 1);
		if (currReqTries > nextReqTries) {
			count = 0;
			break;
		}
	}

	if (count) {
		m_skills[skill].tries += count;
	}

	// update percent
	uint16_t newPercent = Player::getPercentLevel(m_skills[skill].tries, nextReqTries);
	if (m_skills[skill].percent != newPercent) {
		m_skills[skill].percent = newPercent;
		sendSkills();
	} else if (!s.str().empty()) {
		sendSkills();
	}
}

void Player::setVarStats(Stats_t stat, int32_t modifier)
{
	m_varStats[stat] += modifier;
	switch (stat) {
		case STAT_MAXHEALTH: {
			if (getHealth() > getMaxHealth()) {
				Creature::changeHealth(getMaxHealth() - getHealth());
			} else {
				g_game.addCreatureHealth(this);
			}

			break;
		}

		case STAT_MAXMANA: {
			if (getMana() > getMaxMana()) {
				changeMana(getMaxMana() - getMana());
			}

			break;
		}

		default:
			break;
	}
}

int32_t Player::getDefaultStats(Stats_t stat)
{
	switch (stat) {
		case STAT_MAGICLEVEL:
			return getMagicLevel() - getVarStats(STAT_MAGICLEVEL);
		case STAT_MAXHEALTH:
			return getMaxHealth() - getVarStats(STAT_MAXHEALTH);
		case STAT_MAXMANA:
			return getMaxMana() - getVarStats(STAT_MAXMANA);
		case STAT_SOUL:
			return getSoul() - getVarStats(STAT_SOUL);
		default:
			break;
	}

	return 0;
}

Container* Player::getContainer(uint32_t cid)
{
	for (ContainerVector::iterator it = m_containerVec.begin(); it != m_containerVec.end(); ++it) {
		if (it->first == cid) {
			return it->second;
		}
	}
	return nullptr;
}

int32_t Player::getContainerID(const Container* container) const
{
	for (ContainerVector::const_iterator cl = m_containerVec.begin(); cl != m_containerVec.end(); ++cl) {
		if (cl->second == container) {
			return cl->first;
		}
	}
	return -1;
}

void Player::addContainer(uint32_t cid, Container* container)
{
	if (cid > 0xF) {
		return;
	}

	for (ContainerVector::iterator cl = m_containerVec.begin(); cl != m_containerVec.end(); ++cl) {
		if (cl->first == cid) {
			cl->second = container;
			return;
		}
	}

	m_containerVec.push_back(std::make_pair(cid, container));
}

void Player::closeContainer(uint32_t cid)
{
	for (ContainerVector::iterator cl = m_containerVec.begin(); cl != m_containerVec.end(); ++cl) {
		if (cl->first == cid) {
			m_containerVec.erase(cl);
			break;
		}
	}
}

bool Player::canOpenCorpse(uint32_t ownerId)
{
	return m_guid == ownerId || (m_party && m_party->canOpenCorpse(ownerId)) || hasCustomFlag(PlayerCustomFlag_GamemasterPrivileges);
}

uint16_t Player::getLookCorpse() const
{
	return (m_sex % 2) ? ITEM_MALE_CORPSE : ITEM_FEMALE_CORPSE;
}

void Player::dropLoot(Container* corpse)
{
	if (!corpse || m_lootDrop != LOOT_DROP_FULL) {
		return;
	}

	uint32_t loss = m_lossPercent[LOSS_CONTAINERS], start = otx::config::getInteger(otx::config::BLESS_REDUCTION_BASE), bless = getBlessings();
	while (bless > 0 && loss > 0) {
		loss -= start;
		start -= otx::config::getInteger(otx::config::BLESS_REDUCTION_DECREMENT);
		--bless;
	}

	uint32_t itemLoss = std::floor((5.0 + loss) * m_lossPercent[LOSS_ITEMS] / 1000.0);
	for (int32_t i = SLOT_FIRST; i <= SLOT_LAST; ++i) {
		Item* item = m_inventory[i];
		if (!item) {
			continue;
		}

		uint32_t tmp = random_range(1, 100);
		if (m_skull > SKULL_WHITE || (item->getContainer() && tmp < loss) || (!item->getContainer() && tmp < itemLoss)) {
			g_game.internalMoveItem(nullptr, this, corpse, INDEX_WHEREEVER, item, item->getItemCount(), 0);
			sendRemoveInventoryItem(i);
		}
	}
}

bool Player::setStorage(const std::string& key, const std::string& value)
{
	uint32_t numericKey = atol(key.c_str());
	if (!IS_IN_KEYRANGE(numericKey, RESERVED_RANGE)) {
		if (!Creature::setStorage(key, value)) {
			return false;
		}

		if (Quests::getInstance()->isQuestStorage(key, value, true)) {
			onUpdateQuest();
		}

		return true;
	}

	if (IS_IN_KEYRANGE(numericKey, OUTFITS_RANGE)) {
		uint32_t lookType = atoi(value.c_str()) >> 16, addons = atoi(value.c_str()) & 0xFF;
		if (addons < 4) {
			Outfit outfit;
			if (Outfits::getInstance()->getOutfit(lookType, outfit)) {
				return addOutfit(outfit.outfitId, addons);
			}
		} else {
			std::clog << "[Warning - Player::setStorage] Invalid addons value key: " << key
					  << ", value: " << value << " for player: " << getName() << std::endl;
		}
	} else if (IS_IN_KEYRANGE(numericKey, OUTFITSID_RANGE)) {
		uint32_t outfitId = atoi(value.c_str()) >> 16, addons = atoi(value.c_str()) & 0xFF;
		if (addons < 4) {
			return addOutfit(outfitId, addons);
		} else {
			std::clog << "[Warning - Player::setStorage] Invalid addons value key: " << key
					  << ", value: " << value << " for player: " << getName() << std::endl;
		}
	} else {
		std::clog << "[Warning - Player::setStorage] Unknown reserved key: " << key << " for player: " << getName() << std::endl;
	}

	return false;
}

void Player::eraseStorage(const std::string& key)
{
	Creature::eraseStorage(key);
	if (IS_IN_KEYRANGE(atol(key.c_str()), RESERVED_RANGE)) {
		std::clog << "[Warning - Player::eraseStorage] Unknown reserved key: " << key << " for player: " << m_name << std::endl;
	}
}

bool Player::canSee(const Position& pos) const
{
	if (m_client) {
		return m_client->canSee(pos);
	}

	return false;
}

bool Player::canSeeCreature(const Creature* creature) const
{
	if (creature == this) {
		return true;
	}

	if (const Player* player = creature->getPlayer()) {
		return !player->isGhost() || getGhostAccess() >= player->getGhostAccess();
	}

	return !creature->isInvisible() || canSeeInvisibility();
}

bool Player::canWalkthrough(const Creature* creature) const
{
	if (creature == this || hasFlag(PlayerFlag_CanPassThroughAllCreatures) || creature->isWalkable() || std::find(m_forceWalkthrough.begin(), m_forceWalkthrough.end(), creature->getID()) != m_forceWalkthrough.end()
		|| (creature->getMaster() && creature->getMaster() != this && canWalkthrough(creature->getMaster()))) {
		return true;
	}

	const Player* player = creature->getPlayer();
	if (!player) {
		return false;
	}

	if (((g_game.getWorldType() == WORLDTYPE_OPTIONAL && !player->isEnemy(this, true) && !player->isProtected())
			|| player->getTile()->hasFlag(TILESTATE_PROTECTIONZONE) || player->isProtected())
		&& player->getTile()->ground
		&& Item::items[player->getTile()->ground->getID()].walkStack && (!player->hasCustomFlag(PlayerCustomFlag_GamemasterPrivileges) || player->getAccess() <= getAccess())) {
		return true;
	}

	return (player->isGhost() && getGhostAccess() < player->getGhostAccess())
		|| (isGhost() && getGhostAccess() > player->getGhostAccess());
}

void Player::setWalkthrough(const Creature* creature, bool walkthrough)
{
	std::vector<uint32_t>::iterator it = std::find(m_forceWalkthrough.begin(),
		m_forceWalkthrough.end(), creature->getID());
	bool update = false;
	if (walkthrough && it == m_forceWalkthrough.end()) {
		m_forceWalkthrough.push_back(creature->getID());
		update = true;
	} else if (!walkthrough && it != m_forceWalkthrough.end()) {
		m_forceWalkthrough.erase(it);
		update = true;
	}

	if (update) {
		sendCreatureWalkthrough(creature, !walkthrough ? canWalkthrough(creature) : walkthrough);
	}
}

Depot* Player::getDepot(uint32_t depotId, bool autoCreateDepot)
{
	DepotMap::iterator it = m_depots.find(depotId);
	if (it != m_depots.end()) {
		return it->second.first;
	}

	// create a new depot?
	if (autoCreateDepot) {
		Item* locker = Item::CreateItem(ITEM_LOCKER);
		if (Container* container = locker->getContainer()) {
			if (Depot* depot = container->getDepot()) {
				container->__internalAddThing(Item::CreateItem(ITEM_DEPOT));
				internalAddDepot(depot, depotId);
				return depot;
			}
		}

		g_game.freeThing(locker);
		std::clog << "Failure: Creating a new depot with id: " << depotId << ", for player: " << getName() << std::endl;
	}

	return nullptr;
}

bool Player::addDepot(Depot* depot, uint32_t depotId)
{
	if (getDepot(depotId, false)) {
		return false;
	}

	internalAddDepot(depot, depotId);
	return true;
}

void Player::internalAddDepot(Depot* depot, uint32_t depotId)
{
	m_depots[depotId] = std::make_pair(depot, false);
	depot->setMaxDepotLimit((m_group != nullptr ? m_group->getDepotLimit(isPremium()) : 1000));
}

void Player::useDepot(uint32_t depotId, bool value)
{
	DepotMap::iterator it = m_depots.find(depotId);
	if (it != m_depots.end()) {
		m_depots[depotId] = std::make_pair(it->second.first, value);
	}
}

void Player::sendCancelMessage(ReturnValue message) const
{
	switch (message) {
		case RET_DESTINATIONOUTOFREACH:
			sendCancel("Destination is out of reach.");
			break;

		case RET_NOTMOVABLE:
			sendCancel("You cannot move this object.");
			break;

		case RET_DROPTWOHANDEDITEM:
			sendCancel("Drop the double-handed object first.");
			break;

		case RET_BOTHHANDSNEEDTOBEFREE:
			sendCancel("Both hands need to be free.");
			break;

		case RET_CANNOTBEDRESSED:
			sendCancel("You cannot dress this object there.");
			break;

		case RET_PUTTHISOBJECTINYOURHAND:
			sendCancel("Put this object in your hand.");
			break;

		case RET_PUTTHISOBJECTINBOTHHANDS:
			sendCancel("Put this object in both hands.");
			break;

		case RET_CANONLYUSEONEWEAPON:
			sendCancel("You may use only one weapon.");
			break;

		case RET_TOOFARAWAY:
			sendCancel("Too far away.");
			break;

		case RET_FIRSTGODOWNSTAIRS:
			sendCancel("First go downstairs.");
			break;

		case RET_FIRSTGOUPSTAIRS:
			sendCancel("First go upstairs.");
			break;

		case RET_NOTENOUGHCAPACITY:
			sendCancel("This object is too heavy for you to carry.");
			break;

		case RET_CONTAINERNOTENOUGHROOM:
			sendCancel("You cannot put more objects in this container.");
			break;

		case RET_NEEDEXCHANGE:
		case RET_NOTENOUGHROOM:
			sendCancel("There is not enough room.");
			break;

		case RET_CANNOTPICKUP:
			sendCancel("You cannot take this object.");
			break;

		case RET_CANNOTTHROW:
			sendCancel("You cannot throw there.");
			break;

		case RET_THEREISNOWAY:
			sendCancel("There is no way.");
			break;

		case RET_THISISIMPOSSIBLE:
			sendCancel("This is impossible.");
			break;

		case RET_PLAYERISPZLOCKED:
			sendCancel("You cannot enter a protection zone after attacking another player.");
			break;

		case RET_PLAYERISNOTINVITED:
			sendCancel("You are not invited.");
			break;

		case RET_HOUSEPROTECTED:
			sendCancel("This house is protected, you cannot move or put or remove items!");
			break;

		case RET_CREATUREDOESNOTEXIST:
			sendCancel("Creature does not exist.");
			break;

		case RET_DEPOTISFULL:
			sendCancel("Your depot is full. Remove surplus items before storing new ones.");
			break;

		case RET_CANNOTUSETHISOBJECT:
			sendCancel("You cannot use this object.");
			break;

		case RET_PLAYERWITHTHISNAMEISNOTONLINE:
			sendCancel("A player with this name is not online.");
			break;

		case RET_NOTREQUIREDLEVELTOUSERUNE:
			sendCancel("You do not have the required magic level to use this rune.");
			break;

		case RET_YOUAREALREADYTRADING:
			sendCancel("You are already trading. Finish this trade first.");
			break;

		case RET_THISPLAYERISALREADYTRADING:
			sendCancel("This person is already trading.");
			break;

		case RET_YOUMAYNOTLOGOUTDURINGAFIGHT:
			sendCancel("You may not logout during or immediately after a fight.");
			break;

		case RET_DIRECTPLAYERSHOOT:
			sendCancel("You are not allowed to shoot directly on players.");
			break;

		case RET_NOTENOUGHLEVEL:
			sendCancel("You do not have enough level.");
			break;

		case RET_NOTENOUGHMAGICLEVEL:
			sendCancel("You do not have enough magic level.");
			break;

		case RET_NOTENOUGHMANA:
			sendCancel("You do not have enough mana.");
			break;

		case RET_NOTENOUGHSOUL:
			sendCancel("You do not have enough soul.");
			break;

		case RET_YOUAREEXHAUSTED:
			sendCancel("You are exhausted.");
			break;

		case RET_CANONLYUSETHISRUNEONCREATURES:
			sendCancel("You can only use this rune on creatures.");
			break;

		case RET_PLAYERISNOTREACHABLE:
			sendCancel("Player is not reachable.");
			break;

		case RET_CREATUREISNOTREACHABLE:
			sendCancel("Creature is not reachable.");
			break;

		case RET_ACTIONNOTPERMITTEDINPROTECTIONZONE:
			sendCancel("This action is not permitted in a protection zone.");
			break;

		case RET_YOUMAYNOTATTACKTHISPLAYER:
			sendCancel("You may not attack this player.");
			break;

		case RET_YOUMAYNOTATTACKTHISCREATURE:
			sendCancel("You may not attack this creature.");
			break;

		case RET_YOUMAYNOTATTACKAPERSONINPROTECTIONZONE:
			sendCancel("You may not attack a person in a protection zone.");
			break;

		case RET_YOUMAYNOTATTACKAPERSONWHILEINPROTECTIONZONE:
			sendCancel("You may not attack a person while you are in a protection zone.");
			break;

		case RET_YOUCANONLYUSEITONCREATURES:
			sendCancel("You can only use it on creatures.");
			break;

		case RET_TURNSECUREMODETOATTACKUNMARKEDPLAYERS:
			sendCancel("Turn secure mode off if you really want to attack unmarked players.");
			break;

		case RET_YOUNEEDPREMIUMACCOUNT:
			sendCancel("You need a premium account.");
			break;

		case RET_YOUNEEDTOLEARNTHISSPELL:
			sendCancel("You need to learn this spell first.");
			break;

		case RET_YOURVOCATIONCANNOTUSETHISSPELL:
			sendCancel("Your vocation cannot use this spell.");
			break;

		case RET_YOUNEEDAWEAPONTOUSETHISSPELL:
			sendCancel("You need to equip a weapon to use this spell.");
			break;

		case RET_PLAYERISPZLOCKEDLEAVEPVPZONE:
			sendCancel("You cannot leave a pvp zone after attacking another player.");
			break;

		case RET_PLAYERISPZLOCKEDENTERPVPZONE:
			sendCancel("You cannot enter a pvp zone after attacking another player.");
			break;

		case RET_ACTIONNOTPERMITTEDINANOPVPZONE:
			sendCancel("This action is not permitted in a safe zone.");
			break;

		case RET_YOUCANNOTLOGOUTHERE:
			sendCancel("You cannot logout here.");
			break;

		case RET_YOUNEEDAMAGICITEMTOCASTSPELL:
			sendCancel("You need a magic item to cast this spell.");
			break;

		case RET_CANNOTCONJUREITEMHERE:
			sendCancel("You cannot conjure items here.");
			break;

		case RET_NAMEISTOOAMBIGUOUS:
			sendCancel("Name is too ambiguous.");
			break;

		case RET_CANONLYUSEONESHIELD:
			sendCancel("You may use only one shield.");
			break;

		case RET_YOUARENOTTHEOWNER:
			sendCancel("You are not the owner.");
			break;

		case RET_YOUMAYNOTCASTAREAONBLACKSKULL:
			sendCancel("You may not cast area spells while you have a black skull.");
			break;

		case RET_TILEISFULL:
			sendCancel("You cannot add more items on this tile.");
			break;

		case RET_NOTENOUGHSKILL:
			sendCancel("You do not have enough skill.");
			break;

		case RET_NOTPOSSIBLE:
			sendCancel("Sorry, not possible.");
			break;

		case RET_YOUMAYNOTATTACKIMMEDIATELYAFTERLOGGINGIN:
			sendCancel("You may not attack immediately after logging in.");
			break;

		case RET_YOUHAVETOWAIT:
			sendCancel("Sorry, you have to wait.");
			break;

		case RET_YOUCANONLYTRADEUPTOX: {
			std::ostringstream s;
			s << "You can only trade up to " << otx::config::getInteger(otx::config::TRADE_LIMIT) << " items at a time.";
			sendCancel(s.str());
		} break;

		default:
			break;
	}
}

void Player::sendStats()
{
	if (m_client) {
		m_client->sendStats();
		m_lastStatsTrainingTime = getOfflineTrainingTime() / 60 / 1000;
	}
}

Item* Player::getWriteItem(uint32_t& _windowTextId, uint16_t& _maxWriteLen)
{
	_windowTextId = m_windowTextId;
	_maxWriteLen = m_maxWriteLen;
	return m_writeItem;
}

void Player::setWriteItem(Item* item, uint16_t _maxLen /* = 0*/)
{
	m_windowTextId++;
	if (m_writeItem) {
		m_writeItem->unRef();
	}

	if (item) {
		m_writeItem = item;
		m_maxWriteLen = _maxLen;
		m_writeItem->addRef();
	} else {
		m_writeItem = nullptr;
		m_maxWriteLen = 0;
	}
}

House* Player::getEditHouse(uint32_t& _windowTextId, uint32_t& _listId)
{
	_windowTextId = m_windowTextId;
	_listId = m_editListId;
	return m_editHouse;
}

void Player::setEditHouse(House* house, uint32_t listId /* = 0*/)
{
	m_windowTextId++;
	m_editHouse = house;
	m_editListId = listId;
}

void Player::sendHouseWindow(House* house, uint32_t listId) const
{
	if (!m_client) {
		return;
	}

	std::string text;
	if (house->getAccessList(listId, text)) {
		m_client->sendHouseWindow(m_windowTextId, text);
	}
}

void Player::sendCreatureChangeVisible(const Creature* creature, Visible_t visible)
{
	if (!m_client) {
		return;
	}

	const Player* player = creature->getPlayer();
	if (player == this || (player && (visible < VISIBLE_GHOST_APPEAR || getGhostAccess() >= player->getGhostAccess()))
		|| (!player && canSeeInvisibility())) {
		sendCreatureChangeOutfit(creature, creature->getCurrentOutfit());
	} else if (visible == VISIBLE_DISAPPEAR || visible == VISIBLE_GHOST_DISAPPEAR) {
		sendCreatureDisappear(creature, creature->getTile()->getClientIndexOfThing(this, creature));
	} else {
		sendCreatureAppear(creature);
	}
}

void Player::sendAddContainerItem(const Container* container, const Item* item)
{
	if (!m_client) {
		return;
	}

	for (ContainerVector::const_iterator cl = m_containerVec.begin(); cl != m_containerVec.end(); ++cl) {
		if (cl->second == container) {
			m_client->sendAddContainerItem(cl->first, item);
		}
	}
}

void Player::sendUpdateContainerItem(const Container* container, uint8_t slot, const Item* item)
{
	if (!m_client) {
		return;
	}

	for (ContainerVector::const_iterator cl = m_containerVec.begin(); cl != m_containerVec.end(); ++cl) {
		if (cl->second == container) {
			m_client->sendUpdateContainerItem(cl->first, slot, item);
		}
	}
}

void Player::sendRemoveContainerItem(const Container* container, uint8_t slot, const Item*)
{
	if (!m_client) {
		return;
	}

	for (ContainerVector::const_iterator cl = m_containerVec.begin(); cl != m_containerVec.end(); ++cl) {
		if (cl->second == container) {
			m_client->sendRemoveContainerItem(cl->first, slot);
		}
	}
}

void Player::sendContainers(ProtocolGame* target)
{
	if (!target) {
		return;
	}

	for (ContainerVector::const_iterator cl = m_containerVec.begin(); cl != m_containerVec.end(); ++cl) {
		target->sendContainer(cl->first, cl->second, dynamic_cast<const Container*>(cl->second->getParent()) != nullptr);
	}
}

void Player::onUpdateTileItem(const Tile* tile, const Position& pos, const Item* oldItem,
	const ItemType& oldType, const Item* newItem, const ItemType& newType)
{
	Creature::onUpdateTileItem(tile, pos, oldItem, oldType, newItem, newType);
	if (oldItem != newItem) {
		onRemoveTileItem(tile, pos, oldType, oldItem);
	}

	if (m_tradeState != TRADE_TRANSFER && m_tradeItem && oldItem == m_tradeItem) {
		g_game.internalCloseTrade(this);
	}
}

void Player::onRemoveTileItem(const Tile* tile, const Position& pos, const ItemType& iType, const Item* item)
{
	Creature::onRemoveTileItem(tile, pos, iType, item);
	if (m_tradeState == TRADE_TRANSFER) {
		return;
	}

	checkTradeState(item);
	if (m_tradeItem) {
		const Container* container = item->getContainer();
		if (container && container->isHoldingItem(m_tradeItem)) {
			g_game.internalCloseTrade(this);
		}
	}
}

void Player::onCreatureAppear(const Creature* creature)
{
	Creature::onCreatureAppear(creature);
	if (creature != this) {
		return;
	}

	Item* item = nullptr;
	for (int32_t slot = SLOT_FIRST; slot <= SLOT_LAST; ++slot) {
		if (!(item = getInventoryItem(static_cast<Slots_t>(slot)))) {
			continue;
		}

		item->__startDecaying();
		g_moveEvents.onPlayerEquip(this, item, static_cast<Slots_t>(slot), false);
	}

	if (BedItem* bed = g_game.getBedBySleeper(m_guid)) {
		bed->wakeUp();
	}

	Outfit outfit;
	if (Outfits::getInstance()->getOutfit(m_defaultOutfit.lookType, outfit)) {
		m_outfitAttributes = Outfits::getInstance()->addAttributes(getID(), outfit.outfitId, m_sex, m_defaultOutfit.lookAddons);
	}

	if (m_lastLogout != 0 && m_stamina < STAMINA_MAX) {
		int64_t ticks = time(nullptr) - m_lastLogout - 600;
		if (ticks > 0) {
			ticks = (ticks * 1000.0) / otx::config::getDouble(otx::config::RATE_STAMINA_GAIN);
			int64_t premium = otx::config::getInteger(otx::config::STAMINA_LIMIT_TOP) * 60000;
			int64_t period = ticks;
			if (static_cast<int64_t>(m_stamina) <= premium) {
				period += m_stamina;
				if (period > premium) {
					period -= premium;
				} else {
					period = 0;
				}

				useStamina(ticks - period);
			}

			if (period > 0) {
				ticks = (otx::config::getDouble(otx::config::RATE_STAMINA_GAIN) * period) / otx::config::getDouble(otx::config::RATE_STAMINA_THRESHOLD);
				if (m_stamina + ticks > STAMINA_MAX) {
					ticks = STAMINA_MAX - m_stamina;
				}

				useStamina(ticks);
			}

			sendStats();
		}
	}

	int32_t offlineTime;
	if (getLastLogout()) {
		offlineTime = std::min<int32_t>(time(nullptr) - getLastLogout(), 86400 * 21);
	} else {
		offlineTime = 0;
	}

	if (m_offlineTrainingSkill != SKILL_NONE) {
		int32_t trainingTime = std::max(0, std::min(offlineTime, std::min(43200, getOfflineTrainingTime() / 1000)));
		if (offlineTime >= 600) {
			removeOfflineTrainingTime(trainingTime * 1000);
			int32_t remainder = offlineTime - trainingTime;
			if (remainder > 0) {
				addOfflineTrainingTime(remainder * 1000);
			}

			if (trainingTime >= 60) {
				std::ostringstream ss;
				ss << "During your absence you trained for ";

				int32_t hours = trainingTime / 3600;
				if (hours > 1) {
					ss << hours << " hours";
				} else if (hours == 1) {
					ss << "1 hour";
				}

				int32_t minutes = (trainingTime % 3600) / 60;
				if (minutes != 0) {
					if (hours != 0) {
						ss << " and ";
					}

					if (minutes > 1) {
						ss << minutes << " minutes";
					} else {
						ss << "1 minute";
					}
				}

				ss << ".";
				sendTextMessage(MSG_EVENT_ADVANCE, ss.str());

				const Vocation* voc = m_vocation;
				// maybe a configurable?...
				if (!isPromoted(m_promotionLevel + 1)) {
					int32_t vocId = g_vocations.getPromotedVocation(voc->id);
					if (vocId > 0) {
						if (Vocation* tmp = g_vocations.getVocation(vocId)) {
							voc = tmp;
						}
					}
				}

				if (m_offlineTrainingSkill == SKILL_CLUB || m_offlineTrainingSkill == SKILL_SWORD || m_offlineTrainingSkill == SKILL_AXE) {
					float modifier = voc->attackSpeed / 1000.f;
					addOfflineTrainingTries(static_cast<Skills_t>(m_offlineTrainingSkill), (trainingTime / modifier) / 2);
				} else if (m_offlineTrainingSkill == SKILL_DIST) {
					float modifier = voc->attackSpeed / 1000.f;
					addOfflineTrainingTries(static_cast<Skills_t>(m_offlineTrainingSkill), (trainingTime / modifier) / 4);
				} else if (m_offlineTrainingSkill == SKILL__MAGLEVEL) {
					int32_t gainTicks = voc->gainTicks[GAIN_MANA] << 1;
					if (!gainTicks) {
						gainTicks = 1;
					}
					addOfflineTrainingTries(SKILL__MAGLEVEL, trainingTime * (voc->gainAmount[GAIN_MANA] / static_cast<double>(gainTicks)));
				}

				if (addOfflineTrainingTries(SKILL_SHIELD, trainingTime / 4)) {
					sendSkills();
				}
			}

			sendStats();
		} else {
			sendTextMessage(MSG_EVENT_ADVANCE, "You must be logged out for more than 10 minutes to start offline training.");
		}
		m_offlineTrainingSkill = SKILL_NONE;
	} else {
		uint16_t oldMinutes = getOfflineTrainingTime() / 60 / 1000;
		addOfflineTrainingTime(offlineTime * 1000);

		uint16_t newMinutes = getOfflineTrainingTime() / 60 / 1000;
		if (oldMinutes != newMinutes) {
			sendStats();
		}
	}

	g_game.checkPlayersRecord(this);

	// update online status for players with ghost protection
	IOLoginData::getInstance()->updateOnlineStatus(m_guid, true);
	for (const auto& it : g_game.getPlayers()) {
		if (it.second->canSeeCreature(this)) {
			it.second->notifyLogIn(this);
		}
	}

	if (otx::config::getBoolean(otx::config::DISPLAY_LOGGING)) {
		std::clog << m_name << " has logged in." << std::endl;
	}
}

void Player::onTargetDisappear(bool isLogout)
{
	sendCancelTarget();
	if (!isLogout) {
		sendTextMessage(MSG_STATUS_SMALL, "Target lost.");
	}
}

void Player::onFollowCreatureDisappear(bool isLogout)
{
	sendCancelTarget();
	if (!isLogout) {
		sendTextMessage(MSG_STATUS_SMALL, "Target lost.");
	}
}

void Player::onChangeZone(ZoneType_t zone)
{
	if (zone == ZONE_PROTECTION && !hasFlag(PlayerFlag_IgnoreProtectionZone)) {
		// fix party system not used in PZ
		if (isPartner(this)) {
			m_party->updateExperienceMult();
			m_party->canUseSharedExperience(this);
		}

		if (m_attackedCreature) {
			setAttackedCreature(nullptr);
			onTargetDisappear(false);
		}

		removeCondition(CONDITION_INFIGHT);
	} else {
		// Fix party system, reactivates out the PZ
		if (isPartner(this)) {
			m_party->updateExperienceMult();
			m_party->canUseSharedExperience(this);
		}
	}
	g_game.updateCreatureWalkthrough(this);
	sendIcons();
}

void Player::onTargetChangeZone(ZoneType_t zone)
{
	if (zone == ZONE_PROTECTION) {
		if (!hasFlag(PlayerFlag_IgnoreProtectionZone)) {
			setAttackedCreature(nullptr);
			onTargetDisappear(false);
		}
	} else if (zone == ZONE_OPTIONAL) {
		if (m_attackedCreature->getPlayer() && !hasFlag(PlayerFlag_IgnoreProtectionZone)) {
			setAttackedCreature(nullptr);
			onTargetDisappear(false);
		}
	} else if (zone == ZONE_OPEN) {
		if (g_game.getWorldType() == WORLDTYPE_OPTIONAL
			&& m_attackedCreature->getPlayer() && !m_attackedCreature->getPlayer()->isEnemy(this, true)) {
			// attackedCreature can leave a pvp zone if not pzlocked
			setAttackedCreature(nullptr);
			onTargetDisappear(false);
		}
	}
}

void Player::onCreatureDisappear(const Creature* creature, bool isLogout)
{
	Creature::onCreatureDisappear(creature, isLogout);
	if (creature != this) {
		return;
	}

	m_client->clear(true);
	m_lastLogout = time(nullptr);

	if (isLogout) {
		m_loginPosition = getPosition();
	}

	Item* item = nullptr;
	for (int32_t slot = SLOT_FIRST; slot <= SLOT_LAST; ++slot) {
		if (!(item = getInventoryItem(static_cast<Slots_t>(slot)))) {
			continue;
		}

		g_moveEvents.onPlayerDeEquip(this, item, static_cast<Slots_t>(slot), false);
	}

	if (m_eventWalk) {
		setFollowCreature(nullptr);
	}

	closeShopWindow();
	if (m_tradePartner) {
		g_game.internalCloseTrade(this);
	}

	clearPartyInvitations();
	if (m_party) {
		m_party->leave(this);
	}

	g_game.cancelRuleViolation(this);
	if (hasFlag(PlayerFlag_CanAnswerRuleViolations)) {
		PlayerVector closeReportList;
		for (RuleViolationsMap::const_iterator it = g_game.getRuleViolations().begin(); it != g_game.getRuleViolations().end(); ++it) {
			if (it->second->gamemaster == this) {
				closeReportList.push_back(it->second->reporter);
			}
		}

		for (PlayerVector::iterator it = closeReportList.begin(); it != closeReportList.end(); ++it) {
			g_game.closeRuleViolation(*it);
		}
	}

	g_chat.removeUserFromChannels(this);
	if (!isGhost()) {
		IOLoginData::getInstance()->updateOnlineStatus(m_guid, false);
	}

	if (otx::config::getBoolean(otx::config::DISPLAY_LOGGING)) {
		std::clog << getName() << " has logged out." << std::endl;
	}

	bool saved = false;
	for (uint32_t tries = 0; !saved && tries < 3; ++tries) {
		if (IOLoginData::getInstance()->savePlayer(this)) {
			saved = true;
		}
	}

	if (!saved) {
		std::clog << "Error while saving player: " << getName() << "." << std::endl;
	}
}

void Player::openShopWindow() const
{
	sendShop();
	sendGoods();
}

void Player::closeShopWindow(bool send /* = true*/)
{
	int32_t onBuy = -1, onSell = -1;
	if (Npc* npc = getShopOwner(onBuy, onSell)) {
		npc->onPlayerEndTrade(this, onBuy, onSell);
	}

	if (m_shopOwner) {
		m_shopOwner = nullptr;
		if (send) {
			sendCloseShop();
		}
	}

	m_purchaseCallback = m_saleCallback = -1;
	m_shopOffer.clear();
}

bool Player::canBuyItem(uint16_t itemId, uint8_t subType)
{
	for (const auto& it : m_shopOffer) {
		if (it.itemId != itemId) {
			continue;
		}

		if (it.buyPrice == 0) {
			return false;
		}

		const ItemType& iType = Item::items[itemId];
		if (iType.isFluidContainer() || iType.isSplash()) {
			if (it.subType != subType) {
				return false;
			}
		}
		return true;
	}
	return false;
}

bool Player::canSellItem(uint16_t itemId)
{
	for (const auto& it : m_shopOffer) {
		if (it.itemId == itemId) {
			return it.sellPrice != 0;
		}
	}
	return false;
}

void Player::onWalk(Direction& dir)
{
	Creature::onWalk(dir);
	setNextActionTask(nullptr);
	setNextAction(otx::util::mstime() + getStepDuration(dir));
}

void Player::onCreatureMove(const Creature* creature, const Tile* newTile, const Position& newPos,
	const Tile* oldTile, const Position& oldPos, bool teleport)
{
	Creature::onCreatureMove(creature, newTile, newPos, oldTile, oldPos, teleport);
	if (creature != this) {
		return;
	}

	if (m_party) {
		m_party->updateSharedExperience();
	}

	// check if we should close trade
	if (m_tradeState != TRADE_TRANSFER && ((m_tradeItem && !Position::areInRange<1, 1, 0>(m_tradeItem->getPosition(), getPosition())) || (m_tradePartner && !Position::areInRange<2, 2, 0>(m_tradePartner->getPosition(), getPosition())))) {
		g_game.internalCloseTrade(this);
	}

	if ((teleport || oldPos.z != newPos.z) && !hasCustomFlag(PlayerCustomFlag_CanStairhop)) {
		int32_t ticks = otx::config::getInteger(otx::config::STAIRHOP_DELAY);
		if (ticks > 0) {
			addExhaust(ticks, EXHAUST_MELEE);
			if (Condition* condition = Condition::createCondition(CONDITIONID_DEFAULT, CONDITION_PACIFIED, ticks)) {
				addCondition(condition);
			}
		}
	}

	// unset editing house
	if (m_editHouse && !newTile->hasFlag(TILESTATE_HOUSE)) {
		m_editHouse = nullptr;
	}

	if (getZone() == ZONE_PROTECTION && newTile->ground && oldTile->ground && Item::items[newTile->ground->getID()].walkStack != Item::items[oldTile->ground->getID()].walkStack) {
		g_game.updateCreatureWalkthrough(this);
	}
}

void Player::onAddContainerItem(const Container* container, const Item* item)
{
	UNUSED(container);
	checkTradeState(item);
}

void Player::onUpdateContainerItem(const Container* container,
	const Item* oldItem, const Item* newItem)
{
	if (m_tradeState == TRADE_TRANSFER) {
		return;
	}

	checkTradeState(oldItem);
	if (oldItem != newItem && m_tradeItem) {
		if (m_tradeItem->getParent() != container && container->isHoldingItem(m_tradeItem)) {
			g_game.internalCloseTrade(this);
		}
	}

	if (m_tradeState != TRADE_TRANSFER) {
		checkTradeState(oldItem);
	}
}

void Player::onRemoveContainerItem(const Container* container, uint8_t, const Item* item)
{
	if (m_tradeState == TRADE_TRANSFER) {
		return;
	}

	checkTradeState(item);
	if (m_tradeItem) {
		if (m_tradeItem->getParent() != container && container->isHoldingItem(m_tradeItem)) {
			g_game.internalCloseTrade(this);
		}
	}
}

void Player::onCloseContainer(const Container* container)
{
	if (!m_client) {
		return;
	}

	for (ContainerVector::const_iterator cl = m_containerVec.begin(); cl != m_containerVec.end(); ++cl) {
		if (cl->second == container) {
			m_client->sendCloseContainer(cl->first);
		}
	}
}

void Player::onSendContainer(const Container* container)
{
	if (!m_client) {
		return;
	}

	bool hasParent = dynamic_cast<const Container*>(container->getParent()) != nullptr;
	for (ContainerVector::const_iterator cl = m_containerVec.begin(); cl != m_containerVec.end(); ++cl) {
		if (cl->second == container) {
			m_client->sendContainer(cl->first, container, hasParent);
		}
	}
}

void Player::onUpdateInventoryItem(Item* oldItem, Item* newItem)
{
	if (m_tradeState == TRADE_TRANSFER) {
		return;
	}

	checkTradeState(oldItem);
	if (oldItem != newItem && m_tradeItem) {
		const Container* container = oldItem->getContainer();
		if (container && container->isHoldingItem(m_tradeItem)) {
			g_game.internalCloseTrade(this);
		}
	}

	if (m_tradeState != TRADE_TRANSFER) {
		checkTradeState(oldItem);
	}
}

void Player::onRemoveInventoryItem(Item* item)
{
	if (m_tradeState == TRADE_TRANSFER) {
		return;
	}

	checkTradeState(item);
	if (m_tradeItem) {
		const Container* container = item->getContainer();
		if (container && container->isHoldingItem(m_tradeItem)) {
			g_game.internalCloseTrade(this);
		}
	}
}

void Player::checkTradeState(const Item* item)
{
	if (!m_tradeItem || m_tradeState == TRADE_TRANSFER) {
		return;
	}

	if (m_tradeItem != item) {
		const Container* container = dynamic_cast<const Container*>(item->getParent());
		while (container != nullptr) {
			if (container == m_tradeItem) {
				g_game.internalCloseTrade(this);
				break;
			}

			container = dynamic_cast<const Container*>(container->getParent());
		}
	} else {
		g_game.internalCloseTrade(this);
	}
}

void Player::setNextWalkActionTask(SchedulerTaskPtr task)
{
	if (m_walkTaskEvent) {
		g_scheduler.stopEvent(m_walkTaskEvent);
		m_walkTaskEvent = 0;
	}

	m_walkTask = std::move(task);
	setIdleTime(0);
}

void Player::setNextActionTask(SchedulerTaskPtr task)
{
	if (m_actionTaskEvent) {
		g_scheduler.stopEvent(m_actionTaskEvent);
		m_actionTaskEvent = 0;
	}

	if (task) {
		m_actionTaskEvent = g_scheduler.addEvent(std::move(task));
		setIdleTime(0);
	}
}

bool Player::canDoAction() const
{
	return m_nextAction <= otx::util::mstime();
}

uint32_t Player::getNextActionTime(bool scheduler /* = true*/) const
{
	if (!scheduler) {
		return std::max<int64_t>(0, m_nextAction - otx::util::mstime());
	}
	return std::max<int64_t>(SCHEDULER_MINTICKS, m_nextAction - otx::util::mstime());
}

bool Player::canDoExAction() const
{
	return m_nextExAction <= otx::util::mstime();
}

void Player::receivePing()
{
	m_lastPong = otx::util::mstime();
}

void Player::onThink(uint32_t interval)
{
	Creature::onThink(interval);
	int64_t timeNow = otx::util::mstime();
	if (timeNow - m_lastPing >= 5000) {
		m_lastPing = timeNow;
		if (hasClient()) {
			m_client->sendPing();
		} else if (otx::config::getBoolean(otx::config::STOP_ATTACK_AT_EXIT)) {
			setAttackedCreature(nullptr);
		}
	}

	if ((timeNow - m_lastPong) >= 60000 && !getTile()->hasFlag(TILESTATE_NOLOGOUT)
		&& !m_isConnecting && !m_pzLocked && !hasCondition(CONDITION_INFIGHT)) {
		if (hasClient()) {
			m_client->logout(true, true);
		} else if (g_creatureEvents.playerLogout(this, false)) {
			if (!isGhost()) {
				g_game.addMagicEffect(getPosition(), MAGIC_EFFECT_POFF);
			}

			g_game.removeCreature(this, true);
		}
	}

	m_messageTicks += interval;
	if (m_messageTicks >= 1500) {
		m_messageTicks = 0;
		addMessageBuffer();
	}

	if (m_lastMail != 0 && m_lastMail < (otx::util::mstime() + otx::config::getInteger(otx::config::MAIL_ATTEMPTS_FADE))) {
		m_mailAttempts = m_lastMail = 0;
	}

	addOfflineTrainingTime(interval);
	if (m_lastStatsTrainingTime != getOfflineTrainingTime() / 60 / 1000) {
		sendStats();
	}
}

bool Player::isMuted(uint16_t channelId, MessageType_t type, int32_t& time)
{
	time = 0;
	if (hasFlag(PlayerFlag_CannotBeMuted)) {
		return false;
	}

	int32_t muteTicks = 0;
	for (ConditionList::iterator it = m_conditions.begin(); it != m_conditions.end(); ++it) {
		if ((*it)->getType() == CONDITION_MUTED && (*it)->getSubId() == 0) {
			if ((*it)->getTicks() == -1) {
				time = -1;
				break;
			}

			if ((*it)->getTicks() > muteTicks) {
				muteTicks = (*it)->getTicks();
			}
		}
	}

	if (muteTicks) {
		time = muteTicks / 1000;
	}
	return type != MSG_NPC_TO && (type != MSG_CHANNEL || (channelId != CHANNEL_GUILD && !g_chat.isPrivateChannel(channelId)));
}

void Player::addMessageBuffer()
{
	if (!hasFlag(PlayerFlag_CannotBeMuted) && otx::config::getInteger(otx::config::MAX_MESSAGEBUFFER) && m_messageBuffer) {
		m_messageBuffer--;
	}
}

void Player::removeMessageBuffer()
{
	if (hasFlag(PlayerFlag_CannotBeMuted)) {
		return;
	}

	int32_t maxBuffer = otx::config::getInteger(otx::config::MAX_MESSAGEBUFFER);
	if (!maxBuffer || m_messageBuffer > maxBuffer + 1 || ++m_messageBuffer <= maxBuffer) {
		return;
	}

	uint32_t muteCount = 1;
	MuteCountMap::iterator it = muteCountMap.find(m_guid);
	if (it != muteCountMap.end()) {
		muteCount = it->second;
	}

	uint32_t muteTime = 5 * muteCount * muteCount;
	muteCountMap[m_guid] = muteCount + 1;
	if (Condition* condition = Condition::createCondition(CONDITIONID_DEFAULT, CONDITION_MUTED, muteTime * 1000)) {
		addCondition(condition);
	}

	char buffer[50];
	sprintf(buffer, "You are muted for %d seconds.", muteTime);
	sendTextMessage(MSG_STATUS_SMALL, buffer);
}

double Player::getFreeCapacity() const
{
	if (hasFlag(PlayerFlag_HasInfiniteCapacity)
		|| !otx::config::getBoolean(otx::config::USE_CAPACITY)) {
		return 10000.00;
	}

	return std::max(0.00, m_capacity - m_inventoryWeight);
}

void Player::drainHealth(Creature* attacker, CombatType_t combatType, int32_t damage)
{
	Creature::drainHealth(attacker, combatType, damage);
	sendStats();
}

void Player::drainMana(Creature* attacker, CombatType_t combatType, int32_t damage)
{
	Creature::drainMana(attacker, combatType, damage);
	sendStats();
}

void Player::addManaSpent(uint64_t amount, bool useMultiplier /* = true*/)
{
	if (!amount) {
		return;
	}

	uint64_t currReqMana = m_vocation->getReqMana(m_magLevel), nextReqMana = m_vocation->getReqMana(m_magLevel + 1);
	if (m_magLevel > 0 && currReqMana > nextReqMana) { // player has reached max magic m_level
		return;
	}

	if (useMultiplier) {
		amount *= m_rates[SKILL__MAGLEVEL] * otx::config::getDouble(otx::config::RATE_MAGIC);
	}

	std::ostringstream s;
	while (m_manaSpent + amount >= nextReqMana) {
		amount -= nextReqMana - m_manaSpent;
		m_manaSpent = 0;

		s.str("");
		s << "You advanced to magic level " << ++m_magLevel << ".";
		sendTextMessage(MSG_EVENT_ADVANCE, s.str());

		if (hasEventRegistered(CREATURE_EVENT_ADVANCE)) {
			for (CreatureEvent* it : getCreatureEvents(CREATURE_EVENT_ADVANCE)) {
				it->executeAdvance(this, SKILL__MAGLEVEL, (m_magLevel - 1), m_magLevel);
			}
		}

		currReqMana = nextReqMana;
		nextReqMana = m_vocation->getReqMana(m_magLevel + 1);
		if (currReqMana > nextReqMana) {
			amount = 0;
			break;
		}
	}

	if (amount) {
		m_manaSpent += amount;
	}

	uint16_t newPercent = Player::getPercentLevel(m_manaSpent, nextReqMana);
	if (m_magLevelPercent != newPercent) {
		m_magLevelPercent = newPercent;
		sendStats();
	} else if (!s.str().empty()) {
		sendStats();
	}
}

void Player::addExperience(uint64_t exp)
{
	bool attackable = isProtected();
	uint32_t prevLevel = m_level;

	uint64_t nextLevelExp = Player::getExpForLevel(m_level + 1);
	if (Player::getExpForLevel(m_level) > nextLevelExp) {
		// player has reached max m_level
		m_levelPercent = 0;
		sendStats();
		return;
	}

	m_experience += exp;
	while (m_experience >= nextLevelExp) {
		++m_level;
		m_healthMax += m_vocation->gain[GAIN_HEALTH];
		m_health += m_vocation->gain[GAIN_HEALTH];
		m_manaMax += m_vocation->gain[GAIN_MANA];
		m_mana += m_vocation->gain[GAIN_MANA];
		m_capacity += m_vocation->capGain;

		nextLevelExp = Player::getExpForLevel(m_level + 1);
		if (Player::getExpForLevel(m_level) > nextLevelExp) { // player has reached max m_level
			break;
		}
	}

	if (prevLevel != m_level) {
		updateBaseSpeed();
		g_game.changeSpeed(this, 0);
		if (m_party) {
			m_party->updateSharedExperience();
		}

		if (hasEventRegistered(CREATURE_EVENT_ADVANCE)) {
			for (CreatureEvent* it : getCreatureEvents(CREATURE_EVENT_ADVANCE)) {
				it->executeAdvance(this, SKILL__LEVEL, prevLevel, m_level);
			}
		}

		std::ostringstream s;
		s << "You advanced from Level " << prevLevel << " to Level " << m_level << ".";

		sendTextMessage(MSG_EVENT_ADVANCE, s.str());
		if (isProtected() != attackable) {
			g_game.updateCreatureWalkthrough(this);
		}
	}

	uint64_t currLevelExp = Player::getExpForLevel(m_level);
	nextLevelExp = Player::getExpForLevel(m_level + 1);
	m_levelPercent = 0;
	if (nextLevelExp > currLevelExp) {
		m_levelPercent = Player::getPercentLevel(m_experience - currLevelExp, nextLevelExp - currLevelExp);
	}

	sendStats();
}

void Player::removeExperience(uint64_t exp, bool updateStats /* = true*/)
{
	uint32_t prevLevel = m_level;
	bool attackable = isProtected();

	m_experience -= std::min(exp, m_experience);
	while (m_level > 1 && m_experience < Player::getExpForLevel(m_level)) {
		--m_level;
		m_healthMax = std::max<int32_t>(0, (m_healthMax - m_vocation->gain[GAIN_HEALTH]));
		m_manaMax = std::max<int32_t>(0, (m_manaMax - m_vocation->gain[GAIN_MANA]));
		m_capacity = std::max<double>(0.0, (m_capacity - m_vocation->capGain));
	}

	if (prevLevel != m_level) {
		if (updateStats) {
			updateBaseSpeed();
			g_game.changeSpeed(this, 0);
			g_game.addCreatureHealth(this);
		}

		if (hasEventRegistered(CREATURE_EVENT_ADVANCE)) {
			for (CreatureEvent* it : getCreatureEvents(CREATURE_EVENT_ADVANCE)) {
				it->executeAdvance(this, SKILL__LEVEL, prevLevel, m_level);
			}
		}

		std::ostringstream s;
		s << "You were downgraded from Level " << prevLevel << " to Level " << m_level << ".";

		sendTextMessage(MSG_EVENT_ADVANCE, s.str());
		if (!isProtected() != attackable) {
			g_game.updateCreatureWalkthrough(this);
		}
	}

	uint64_t currLevelExp = Player::getExpForLevel(m_level),
			 nextLevelExp = Player::getExpForLevel(m_level + 1);
	if (nextLevelExp > currLevelExp) {
		m_levelPercent = Player::getPercentLevel(m_experience - currLevelExp, nextLevelExp - currLevelExp);
	} else {
		m_levelPercent = 0;
	}

	if (updateStats) {
		sendStats();
	}
}

uint16_t Player::getPercentLevel(uint64_t count, uint64_t nextLevelCount)
{
	if (nextLevelCount == 0) {
		return 0;
	}
	return count * 100 / nextLevelCount;
}

void Player::onBlockHit(BlockType_t)
{
	if (m_shieldBlockCount > 0) {
		--m_shieldBlockCount;
		if (hasShield()) {
			addSkillAdvance(SKILL_SHIELD, 1);
		}
	}
}

void Player::onTargetBlockHit(Creature* target, BlockType_t blockType)
{
	Creature::onTargetBlockHit(target, blockType);
	m_lastAttackBlockType = blockType;
	switch (blockType) {
		case BLOCK_NONE: {
			m_bloodHitCount = m_shieldBlockCount = 30;
			m_addAttackSkillPoint = true;
			break;
		}

		case BLOCK_DEFENSE:
		case BLOCK_ARMOR: {
			// need to draw blood every 30 hits
			if (m_bloodHitCount > 0) {
				m_addAttackSkillPoint = true;
				--m_bloodHitCount;
			} else {
				m_addAttackSkillPoint = false;
			}

			break;
		}

		default: {
			m_addAttackSkillPoint = false;
			break;
		}
	}
}

bool Player::hasShield() const
{
	Item* item = getInventoryItem(SLOT_LEFT);
	return (item && item->getWeaponType() == WEAPON_SHIELD) || ((item = getInventoryItem(SLOT_RIGHT)) && item->getWeaponType() == WEAPON_SHIELD);
}

BlockType_t Player::blockHit(Creature* attacker, CombatType_t combatType, int32_t& damage,
	bool checkDefense /* = false*/, bool checkArmor /* = false*/, bool reflect /* = true*/, bool field /* = false*/, bool element /* = false*/)
{
	BlockType_t blockType = Creature::blockHit(attacker, combatType, damage, checkDefense, checkArmor, reflect, field);
	if (attacker && !element) {
		int16_t color = otx::config::getInteger(otx::config::SQUARE_COLOR);
		if (color < 0) {
			color = random_range(0, 254);
		}

		sendCreatureSquare(attacker, color);
	}

	if (blockType != BLOCK_NONE) {
		return blockType;
	}

	if (m_vocation->formulaMultipliers[MULTIPLIER_MAGICDEFENSE] != 1.0 && combatType != COMBAT_PHYSICALDAMAGE && combatType != COMBAT_NONE && combatType != COMBAT_UNDEFINEDDAMAGE && combatType != COMBAT_DROWNDAMAGE) {
		damage -= std::ceil((damage * m_vocation->formulaMultipliers[MULTIPLIER_MAGICDEFENSE]) / 100.);
	}

	if (damage <= 0) {
		return blockType;
	}

	int32_t blocked = 0, reflected = 0;
	if (reflect) {
		reflect = attacker && !attacker->isRemoved() && attacker->getHealth() > 0;
	}

	const uint16_t combatIndex = otx::util::combat_index(combatType);

	Item* item = nullptr;
	for (int32_t slot = SLOT_FIRST; slot <= SLOT_LAST; ++slot) {
		if (!(item = getInventoryItem(static_cast<Slots_t>(slot))) || item->isRemoved() || (g_moveEvents.hasEquipEvent(item) && !isItemAbilityEnabled(slot))) {
			continue;
		}

		const ItemType& it = Item::items[item->getID()];
		if (!it.hasAbilities()) {
			continue;
		}

		bool transform = false;
		if (it.abilities->absorb[combatIndex]) {
			blocked += std::ceil((damage * it.abilities->absorb[combatIndex]) / 100.);
			if (item->hasCharges()) {
				transform = true;
			}
		}

		if (field && it.abilities->fieldAbsorb[combatIndex]) {
			blocked += std::ceil((damage * it.abilities->fieldAbsorb[combatIndex]) / 100.);
			if (item->hasCharges()) {
				transform = true;
			}
		}

		if (reflect && it.abilities->reflect[REFLECT_PERCENT][combatIndex] && it.abilities->reflect[REFLECT_CHANCE][combatIndex] >= random_range(1, 100)) {
			reflected += std::ceil((damage * it.abilities->reflect[REFLECT_PERCENT][combatIndex]) / 100.);
			if (item->hasCharges()) {
				transform = true;
			}
		}

		if (!element && transform) {
			g_game.transformItem(item, item->getID(), std::max<int32_t>(0, item->getCharges() - 1));
		}
	}
	/*
		if(m_outfitAttributes)
		{
			uint32_t tmp = Outfits::getInstance()->getOutfitAbsorb(defaultOutfit.lookType, m_sex, combatType);
			if(tmp)
				blocked += (int32_t)std::ceil((double)(damage * tmp) / 100.);

			if(reflect)
			{
				tmp = Outfits::getInstance()->getOutfitReflect(defaultOutfit.lookType, m_sex, combatType);
				if(tmp)
					reflected += (int32_t)std::ceil((double)(damage * tmp) / 100.);
			}
		}
	*/
	if (m_vocation->absorb[combatIndex]) {
		blocked += std::ceil((damage * m_vocation->absorb[combatIndex]) / 100.);
	}

	if (reflect && m_vocation->getReflect(combatIndex)) {
		reflected += std::ceil((damage * m_vocation->getReflect(combatIndex)) / 100.);
	}

	if (otx::config::getBoolean(otx::config::USE_MAX_ABSORBALL)) {
		double maxAbsorb = (otx::config::getDouble(otx::config::MAX_ABSORB_PERCENT) / 100.0);
		damage -= (blocked > (damage * maxAbsorb) ? (damage * maxAbsorb) : blocked); // set max absorb = 80%
	} else {
		damage -= blocked;
	}
	if (damage <= 0) {
		damage = 0;
		blockType = BLOCK_DEFENSE;
	}

	if (reflected != 0 && !element) {
		if (combatType != COMBAT_HEALING) {
			reflected = -reflected;
		}

		CombatDamage reflectDamage;
		reflectDamage.primary.type = combatType;
		reflectDamage.primary.value = reflected;
		if (!g_game.combatBlockHit(reflectDamage, this, attacker, false, false)) {
			g_game.combatChangeHealth(nullptr, attacker, reflectDamage);
		}
	}
	return blockType;
}

uint32_t Player::getIP() const
{
	if (m_client) {
		return m_client->getIP();
	}

	return m_lastIP;
}

bool Player::onDeath()
{
	int32_t reduceSkillLoss = 0;
	Item* reduceItem = nullptr;
	Item *preventLoss = nullptr, *preventDrop = nullptr;
	if (getZone() == ZONE_HARDCORE) {
		setDropLoot(LOOT_DROP_NONE);
		setLossSkill(false);
	} else if (m_skull < SKULL_RED) {
		Item* item = nullptr;
		for (int32_t i = SLOT_FIRST; ((!preventDrop || !preventLoss) && i <= SLOT_LAST); ++i) {
			if (!(item = getInventoryItem(static_cast<Slots_t>(i))) || item->isRemoved() || (g_moveEvents.hasEquipEvent(item) && !isItemAbilityEnabled(i))) {
				continue;
			}

			const ItemType& it = Item::items[item->getID()];
			if (!it.hasAbilities()) {
				continue;
			}

			if (m_lootDrop == LOOT_DROP_FULL && it.abilities->preventDrop) {
				setDropLoot(LOOT_DROP_PREVENT);
				preventDrop = item;
			}

			if (m_skillLoss && !preventLoss && it.abilities->preventLoss) {
				preventLoss = item;
				if (preventLoss != preventDrop && it.abilities->preventDrop) {
					preventDrop = item;
				}
			}

			reduceItem = getEquippedItem(static_cast<Slots_t>(i));
			if (reduceItem) {
				int32_t tempReduceSkillLoss = reduceItem->getReduceSkillLoss();
				if (tempReduceSkillLoss != 0) {
					reduceSkillLoss += tempReduceSkillLoss;
				}
			}
		}
	}

	if (!Creature::onDeath()) {
		if (preventDrop) {
			setDropLoot(LOOT_DROP_FULL);
		}

		return false;
	}

	uint32_t totalDamage = 0, pvpDamage = 0, opponents = 0;
	for (CountMap::iterator it = m_damageMap.begin(); it != m_damageMap.end(); ++it) {
		if (((otx::util::mstime() - it->second.ticks) / 1000) > otx::config::getInteger(otx::config::FAIRFIGHT_TIMERANGE)) {
			continue;
		}

		totalDamage += it->second.total;
		if (Creature* creature = g_game.getCreatureByID(it->first)) {
			Player* player = creature->getPlayer();
			if (!player) {
				player = creature->getPlayerMaster();
			}

			if (!player) {
				continue;
			}

			opponents += player->getLevel();
			pvpDamage += it->second.total;
		}
	}

	bool usePVPBlessing = false;
	if (preventLoss) {
		setLossSkill(false);
		g_game.transformItem(preventLoss, preventLoss->getID(), std::max<int32_t>(0, preventLoss->getCharges() - 1));
	} else if (m_pvpBlessing && static_cast<int32_t>(std::floor((100. * pvpDamage) / std::max(1U, totalDamage))) >= otx::config::getInteger(otx::config::PVP_BLESSING_THRESHOLD)) {
		usePVPBlessing = true;
	}

	if (preventDrop && preventDrop != preventLoss && !usePVPBlessing) {
		g_game.transformItem(preventDrop, preventDrop->getID(), std::max<int32_t>(0, preventDrop->getCharges() - 1));
	}

	removeConditions(CONDITIONEND_DEATH);
	if (m_skillLoss) {
		double reduction = 1.0;
		if (otx::config::getBoolean(otx::config::FAIRFIGHT_REDUCTION) && opponents > m_level) {
			reduction -= static_cast<double>(m_level) / opponents;
		}

		uint64_t lossExperience = std::floor(reduction * getLostExperience());
		uint64_t currExperience = m_experience;
		if (reduceSkillLoss > 0) {
			lossExperience = (lossExperience - (lossExperience * reduceSkillLoss / 100));
		}

		removeExperience(lossExperience, false);
		double percent = 1.0 - ((currExperience - static_cast<double>(lossExperience)) / std::max<uint64_t>(1, currExperience));

		// magic m_level loss
		uint64_t sumMana = 0, lostMana = 0;
		for (uint32_t i = 1; i <= m_magLevel; ++i) {
			sumMana += m_vocation->getReqMana(i);
		}

		sumMana += m_manaSpent;
		lostMana = std::ceil((percent * m_lossPercent[LOSS_MANA] / 100.) * sumMana);
		while (lostMana > m_manaSpent && m_magLevel > 0) {
			lostMana -= m_manaSpent;
			m_manaSpent = m_vocation->getReqMana(m_magLevel);
			m_magLevel--;
		}

		m_manaSpent -= lostMana;
		if (reduceSkillLoss > 0) {
			m_manaSpent = (m_manaSpent - (m_manaSpent * reduceSkillLoss / 100));
		}

		uint64_t nextReqMana = m_vocation->getReqMana(m_magLevel + 1);
		if (nextReqMana > m_vocation->getReqMana(m_magLevel)) {
			m_magLevelPercent = Player::getPercentLevel(m_manaSpent, nextReqMana);
		} else {
			m_magLevelPercent = 0;
		}

		// skill loss
		uint64_t lostSkillTries, sumSkillTries;
		for (int16_t i = 0; i < 7; ++i) // for each skill
		{
			lostSkillTries = sumSkillTries = 0;
			for (uint32_t c = 11; c <= m_skills[i].level; ++c) { // sum up all required tries for all skill levels
				sumSkillTries += m_vocation->getReqSkillTries(i, c);
			}

			sumSkillTries += m_skills[i].tries;
			lostSkillTries = std::ceil((percent * m_lossPercent[LOSS_SKILLS] / 100.) * sumSkillTries);
			if (reduceSkillLoss > 0) {
				lostSkillTries = (lostSkillTries - (lostSkillTries * reduceSkillLoss / 100));
			}

			while (lostSkillTries > m_skills[i].tries && m_skills[i].level > 10) {
				lostSkillTries -= m_skills[i].tries;
				m_skills[i].tries = m_vocation->getReqSkillTries(i, m_skills[i].level);
				m_skills[i].level--;
			}

			m_skills[i].tries -= lostSkillTries;
		}

		if (usePVPBlessing) {
			m_pvpBlessing = false;
		} else {
			m_blessings = 0;
		}

		m_loginPosition = m_masterPosition;
		if (!m_inventory[SLOT_BACKPACK]) { // FIXME: you should receive the bag after you login back...
			__internalAddThing(SLOT_BACKPACK, Item::CreateItem(otx::config::getInteger(otx::config::DEATH_CONTAINER)));
		}

		sendIcons();
		sendStats();
		sendSkills();

		g_creatureEvents.playerLogout(this, true);
		g_game.removeCreature(this, false);
		sendReLoginWindow();
	} else {
		setLossSkill(true);
		if (preventLoss) {
			m_loginPosition = m_masterPosition;
			g_creatureEvents.playerLogout(this, true);

			g_game.removeCreature(this, false);
			sendReLoginWindow();
		}
	}

	return true;
}

void Player::dropCorpse(DeathList deathList)
{
	if (m_lootDrop == LOOT_DROP_NONE) {
		m_pzLocked = false;
		if (m_health <= 0) {
			m_health = m_healthMax;
			if (getZone() != ZONE_HARDCORE || otx::config::getBoolean(otx::config::PVPZONE_RECOVERMANA)) {
				m_mana = m_manaMax;
			}
		}

		setDropLoot(LOOT_DROP_FULL);
		sendStats();
		sendIcons();

		onIdleStatus();
		g_game.addCreatureHealth(this);
		g_game.internalTeleport(this, m_masterPosition, false);
	} else {
		Creature::dropCorpse(deathList);
		if (otx::config::getBoolean(otx::config::DEATH_LIST)) {
			IOLoginData::getInstance()->playerDeath(this, deathList);
		}
	}
}

Item* Player::createCorpse(DeathList deathList)
{
	Item* corpse = Creature::createCorpse(deathList);
	if (!corpse) {
		return nullptr;
	}

	std::ostringstream ss;
	ss << "You recognize " << m_nameDescription << ". " << (m_sex % 2 ? "He" : "She") << " was killed by ";
	if (deathList[0].isCreatureKill()) {
		ss << deathList[0].getKillerCreature()->getNameDescription();
		if (deathList[0].getKillerCreature()->getMaster()) {
			ss << " summoned by " << deathList[0].getKillerCreature()->getMaster()->getNameDescription();
		}
	} else {
		ss << deathList[0].getKillerName();
	}

	if (deathList.size() > 1 && otx::config::getBoolean(otx::config::MULTIPLE_NAME)) {
		if (deathList[0].getKillerType() != deathList[1].getKillerType()) {
			if (deathList[1].isCreatureKill()) {
				ss << " and by " << deathList[1].getKillerCreature()->getNameDescription();
				if (deathList[1].getKillerCreature()->getMaster()) {
					ss << " summoned by " << deathList[1].getKillerCreature()->getMaster()->getNameDescription();
				}
			} else {
				ss << " and by " << deathList[1].getKillerName();
			}
		} else if (deathList[1].isCreatureKill()) {
			if (deathList[0].getKillerCreature()->getName() != deathList[1].getKillerCreature()->getName()) {
				ss << " and by " << deathList[1].getKillerCreature()->getNameDescription();
				if (deathList[1].getKillerCreature()->getMaster()) {
					ss << " summoned by " << deathList[1].getKillerCreature()->getMaster()->getNameDescription();
				}
			}
		} else if (otx::util::as_lower_string(deathList[0].getKillerName()) != otx::util::as_lower_string(deathList[1].getKillerName())) {
			ss << " and by " << deathList[1].getKillerName();
		}
	}

	ss << ".";
	corpse->setSpecialDescription(ss.str());
	return corpse;
}

void Player::addCooldown(uint32_t ticks, uint16_t spellId)
{
	if (Condition* condition = Condition::createCondition(CONDITIONID_DEFAULT,
			CONDITION_SPELLCOOLDOWN, ticks, 0, false, spellId)) {
		addCondition(condition);
	}
}

void Player::addExhaust(uint32_t ticks, Exhaust_t exhaust)
{
	if (Condition* condition = Condition::createCondition(CONDITIONID_DEFAULT, CONDITION_EXHAUST, ticks, 0, false, exhaust)) {
		addCondition(condition);
	}
}

void Player::addInFightTicks(bool pzLock, int32_t ticks /* = 0*/)
{
	if (hasFlag(PlayerFlag_NotGainInFight) || getZone() == ZONE_PROTECTION) {
		return;
	}

	if (!ticks) {
		ticks = otx::config::getInteger(otx::config::PZ_LOCKED);
	} else {
		ticks = std::max(-1, ticks);
	}

	if (pzLock) {
		m_pzLocked = true;
	}

	if (Condition* condition = Condition::createCondition(CONDITIONID_DEFAULT,
			CONDITION_INFIGHT, ticks)) {
		addCondition(condition);
	}
}

void Player::addDefaultRegeneration(uint32_t addTicks)
{
	Condition* condition = getCondition(CONDITION_REGENERATION, CONDITIONID_DEFAULT);
	if (condition) {
		condition->setTicks(condition->getTicks() + addTicks);
	} else if ((condition = Condition::createCondition(CONDITIONID_DEFAULT, CONDITION_REGENERATION, addTicks))) {
		condition->setParam(CONDITIONPARAM_HEALTHGAIN, m_vocation->gainAmount[GAIN_HEALTH]);
		condition->setParam(CONDITIONPARAM_HEALTHTICKS, m_vocation->gainTicks[GAIN_HEALTH] * 1000);
		condition->setParam(CONDITIONPARAM_MANAGAIN, m_vocation->gainAmount[GAIN_MANA]);
		condition->setParam(CONDITIONPARAM_MANATICKS, m_vocation->gainTicks[GAIN_MANA] * 1000);
		addCondition(condition);
	}
}

void Player::addList()
{
	g_game.addPlayer(this);
}

void Player::removeList()
{
	g_game.removePlayer(this);
	if (!isGhost()) {
		for (const auto& it : g_game.getPlayers()) {
			it.second->notifyLogOut(this);
		}
	} else {
		for (const auto& it : g_game.getPlayers()) {
			if (it.second->canSeeCreature(this)) {
				it.second->notifyLogOut(this);
			}
		}
	}
}

void Player::kick(bool displayEffect, bool forceLogout)
{
	if (!hasClient()) {
		if (g_creatureEvents.playerLogout(this, forceLogout)) {
			m_client->clear(true);
			g_game.removeCreature(this);
		}
	} else {
		m_client->logout(displayEffect, forceLogout);
	}
}

void Player::notifyLogIn(Player* loginPlayer)
{
	if (!m_client) {
		return;
	}

	VIPSet::iterator it = m_VIPList.find(loginPlayer->getGUID());
	if (it != m_VIPList.end()) {
		m_client->sendVIPLogIn(loginPlayer->getGUID());
	}
}

void Player::notifyLogOut(Player* logoutPlayer)
{
	if (!m_client) {
		return;
	}

	VIPSet::iterator it = m_VIPList.find(logoutPlayer->getGUID());
	if (it != m_VIPList.end()) {
		m_client->sendVIPLogOut(logoutPlayer->getGUID());
	}
}

bool Player::removeVIP(uint32_t _guid)
{
	VIPSet::iterator it = m_VIPList.find(_guid);
	if (it == m_VIPList.end()) {
		return false;
	}

	m_VIPList.erase(it);
	return true;
}

bool Player::addVIP(uint32_t _guid, const std::string& name, bool online, bool loading /* = false*/)
{
	if (m_guid == _guid) {
		if (!loading) {
			sendTextMessage(MSG_STATUS_SMALL, "You cannot add yourself.");
		}

		return false;
	}

	if (!loading && m_VIPList.size() > (m_group ? m_group->getMaxVips(isPremium()) : static_cast<uint32_t>(otx::config::getInteger(otx::config::VIPLIST_DEFAULT_LIMIT)))) {
		sendTextMessage(MSG_STATUS_SMALL, "You cannot add more buddies.");
		return false;
	}

	VIPSet::iterator it = m_VIPList.find(_guid);
	if (it != m_VIPList.end()) {
		if (!loading) {
			sendTextMessage(MSG_STATUS_SMALL, "This player is already in your list.");
		}

		return false;
	}

	m_VIPList.insert(_guid);
	if (!loading && m_client) {
		m_client->sendVIP(_guid, name, online);
	}

	return true;
}

// close container and its child containers
void Player::autoCloseContainers(const Container* container)
{
	typedef std::vector<uint32_t> CloseList;
	CloseList closeList;
	for (ContainerVector::iterator it = m_containerVec.begin(); it != m_containerVec.end(); ++it) {
		Container* tmp = it->second;
		while (tmp != nullptr) {
			if (tmp->isRemoved() || tmp == container) {
				closeList.push_back(it->first);
				break;
			}

			tmp = dynamic_cast<Container*>(tmp->getParent());
		}
	}

	for (CloseList::iterator it = closeList.begin(); it != closeList.end(); ++it) {
		closeContainer(*it);
		if (m_client) {
			m_client->sendCloseContainer(*it);
		}
	}
}

bool Player::hasCapacity(const Item* item, uint32_t count) const
{
	if (hasFlag(PlayerFlag_HasInfiniteCapacity) || item->getTopParent() == this) {
		return true;
	}

	double itemWeight = 0;
	if (item->isStackable()) {
		itemWeight = Item::items[item->getID()].weight * count;
	} else {
		itemWeight = item->getWeight();
	}

	return (itemWeight < getFreeCapacity());
}

ReturnValue Player::__queryAdd(int32_t index, const Thing* thing, uint32_t count, uint32_t flags, Creature*) const
{
	const Item* item = thing->getItem();
	if (!item) {
		return RET_NOTPOSSIBLE;
	}

	if (!item->isPickupable() || (hasFlag(PlayerFlag_CannotPickupItem) && item->getParent() && item->getParent() != VirtualCylinder::virtualCylinder)) {
		return RET_CANNOTPICKUP;
	}

	if ((flags & FLAG_CHILDISOWNER) == FLAG_CHILDISOWNER) {
		// a child container is querying the player, just check if enough m_capacity
		if ((flags & FLAG_NOLIMIT) == FLAG_NOLIMIT || hasCapacity(item, count)) {
			return RET_NOERROR;
		}

		return RET_NOTENOUGHCAPACITY;
	}

	ReturnValue ret = RET_NOERROR;
	if ((item->getSlotPosition() & SLOTP_HEAD) || (item->getSlotPosition() & SLOTP_NECKLACE) || (item->getSlotPosition() & SLOTP_BACKPACK) || (item->getSlotPosition() & SLOTP_ARMOR) || (item->getSlotPosition() & SLOTP_LEGS) || (item->getSlotPosition() & SLOTP_FEET) || (item->getSlotPosition() & SLOTP_RING)) {
		ret = RET_CANNOTBEDRESSED;
	} else if (item->getSlotPosition() & SLOTP_TWO_HAND) {
		ret = RET_PUTTHISOBJECTINBOTHHANDS;
	} else if ((item->getSlotPosition() & SLOTP_RIGHT) || (item->getSlotPosition() & SLOTP_LEFT)) {
		if (!otx::config::getBoolean(otx::config::CLASSIC_EQUIPMENT_SLOTS)) {
			ret = RET_CANNOTBEDRESSED;
		} else {
			ret = RET_PUTTHISOBJECTINYOURHAND;
		}
	}

	switch (index) {
		case SLOT_HEAD:
			if (item->getSlotPosition() & SLOTP_HEAD) {
				ret = RET_NOERROR;
			}
			break;
		case SLOT_NECKLACE:
			if (item->getSlotPosition() & SLOTP_NECKLACE) {
				ret = RET_NOERROR;
			}
			break;
		case SLOT_BACKPACK:
			if (item->getSlotPosition() & SLOTP_BACKPACK) {
				ret = RET_NOERROR;
			}
			break;
		case SLOT_ARMOR:
			if (item->getSlotPosition() & SLOTP_ARMOR) {
				ret = RET_NOERROR;
			}
			break;
		case SLOT_RIGHT:
			if (item->getSlotPosition() & SLOTP_RIGHT) {
				if (!otx::config::getBoolean(otx::config::CLASSIC_EQUIPMENT_SLOTS)) {
					if (!item->isWeapon() || (item->getWeaponType() != WEAPON_SHIELD && !item->isDualWield())) {
						ret = RET_CANNOTBEDRESSED;
					} else {
						const Item* leftItem = m_inventory[SLOT_LEFT];
						if (leftItem) {
							if ((leftItem->getSlotPosition() | item->getSlotPosition()) & SLOTP_TWO_HAND) {
								ret = RET_BOTHHANDSNEEDTOBEFREE;
							} else {
								ret = RET_NOERROR;
							}
						} else {
							ret = RET_NOERROR;
						}
					}
				} else if (item->getSlotPosition() & SLOTP_TWO_HAND) {
					if (m_inventory[SLOT_LEFT] && m_inventory[SLOT_LEFT] != item) {
						ret = RET_BOTHHANDSNEEDTOBEFREE;
					} else {
						ret = RET_NOERROR;
					}
				} else if (m_inventory[SLOT_LEFT]) {
					const Item* leftItem = m_inventory[SLOT_LEFT];
					WeaponType_t type = item->getWeaponType(), leftType = leftItem->getWeaponType();
					if (leftItem->getSlotPosition() & SLOTP_TWO_HAND) {
						ret = RET_DROPTWOHANDEDITEM;
					} else if (item == leftItem && count == item->getItemCount()) {
						ret = RET_NOERROR;
					} else if (leftType == WEAPON_SHIELD && type == WEAPON_SHIELD) {
						ret = RET_CANONLYUSEONESHIELD;
					} else if (!leftItem->isWeapon() || !item->isWeapon() || leftType == WEAPON_SHIELD || leftType == WEAPON_AMMO || type == WEAPON_SHIELD || type == WEAPON_AMMO || (leftItem->isDualWield() && item->isDualWield())) {
						ret = RET_NOERROR;
					} else {
						ret = RET_CANONLYUSEONEWEAPON;
					}
				} else {
					ret = RET_NOERROR;
				}
			}
			break;
		case SLOT_LEFT:
			if (item->getSlotPosition() & SLOTP_LEFT) {
				if (!otx::config::getBoolean(otx::config::CLASSIC_EQUIPMENT_SLOTS)) {
					if (!item->isWeapon() || item->getWeaponType() == WEAPON_SHIELD) {
						ret = RET_CANNOTBEDRESSED;
					} else if (m_inventory[SLOT_RIGHT] && (item->getSlotPosition() & SLOTP_TWO_HAND)) {
						ret = RET_BOTHHANDSNEEDTOBEFREE;
					} else {
						ret = RET_NOERROR;
					}
				} else if (item->getSlotPosition() & SLOTP_TWO_HAND) {
					if (m_inventory[SLOT_RIGHT] && m_inventory[SLOT_RIGHT] != item) {
						ret = RET_BOTHHANDSNEEDTOBEFREE;
					} else {
						ret = RET_NOERROR;
					}
				} else if (m_inventory[SLOT_RIGHT]) {
					const Item* rightItem = m_inventory[SLOT_RIGHT];
					WeaponType_t type = item->getWeaponType(), rightType = rightItem->getWeaponType();
					if (rightItem->getSlotPosition() & SLOTP_TWO_HAND) {
						ret = RET_DROPTWOHANDEDITEM;
					} else if (item == rightItem && count == item->getItemCount()) {
						ret = RET_NOERROR;
					} else if (rightType == WEAPON_SHIELD && type == WEAPON_SHIELD) {
						ret = RET_CANONLYUSEONESHIELD;
					} else if (!rightItem->isWeapon() || !item->isWeapon() || rightType == WEAPON_SHIELD || rightType == WEAPON_AMMO || type == WEAPON_SHIELD || type == WEAPON_AMMO || (rightItem->isDualWield() && item->isDualWield())) {
						ret = RET_NOERROR;
					} else {
						ret = RET_CANONLYUSEONEWEAPON;
					}
				} else {
					ret = RET_NOERROR;
				}
			}
			break;
		case SLOT_LEGS:
			if (item->getSlotPosition() & SLOTP_LEGS) {
				ret = RET_NOERROR;
			}
			break;
		case SLOT_FEET:
			if (item->getSlotPosition() & SLOTP_FEET) {
				ret = RET_NOERROR;
			}
			break;
		case SLOT_RING:
			if (item->getSlotPosition() & SLOTP_RING) {
				ret = RET_NOERROR;
			}
			break;
		case SLOT_AMMO:
			if ((item->getSlotPosition() & SLOTP_AMMO) || otx::config::getBoolean(otx::config::CLASSIC_EQUIPMENT_SLOTS)) {
				ret = RET_NOERROR;
			}
			break;
		case SLOT_WHEREEVER:
		case -1:
			ret = RET_NOTENOUGHROOM;
			break;
		default:
			ret = RET_NOTPOSSIBLE;
			break;
	}

	Player* self = const_cast<Player*>(this);
	if (ret == RET_NOERROR || ret == RET_NOTENOUGHROOM) {
		// need an exchange with source?
		Item* tmpItem = nullptr;
		if ((tmpItem = getInventoryItem(static_cast<Slots_t>(index))) && (!tmpItem->isStackable() || tmpItem->getID() != item->getID())) {
			return RET_NEEDEXCHANGE;
		}

		if (!hasCapacity(item, count)) { // check if enough m_capacity
			return RET_NOTENOUGHCAPACITY;
		}

		if (!g_moveEvents.onPlayerEquip(self, const_cast<Item*>(item), static_cast<Slots_t>(index), true)) {
			return RET_CANNOTBEDRESSED;
		}
	}

	if (index == SLOT_LEFT || index == SLOT_RIGHT) {
		if (ret == RET_NOERROR && item->getWeaponType() != WEAPON_NONE) {
			self->setLastAttack(otx::util::mstime());
		}

		Item* tmpItem = m_inventory[index];
		if (ret == RET_NOTENOUGHROOM && g_game.internalAddItem(nullptr, self, tmpItem, INDEX_WHEREEVER) == RET_NOERROR) {
			self->sendRemoveInventoryItem(index);
			self->onRemoveInventoryItem(tmpItem);

			self->m_inventory[index] = nullptr;
			self->m_inventoryWeight -= tmpItem->getWeight();
			self->sendStats();
		}
	}
	return ret;
}

ReturnValue Player::__queryMaxCount(int32_t index, const Thing* thing, uint32_t count, uint32_t& maxQueryCount,
	uint32_t flags) const
{
	const Item* item = thing->getItem();
	if (!item) {
		maxQueryCount = 0;
		return RET_NOTPOSSIBLE;
	}

	if (index == INDEX_WHEREEVER) {
		uint32_t n = 0;
		for (int32_t i = SLOT_FIRST; i <= SLOT_LAST; ++i) {
			if (Item* inventoryItem = m_inventory[i]) {
				if (Container* subContainer = inventoryItem->getContainer()) {
					uint32_t queryCount = 0;
					subContainer->__queryMaxCount(INDEX_WHEREEVER, item, item->getItemCount(), queryCount, flags);

					// iterate through all items, including sub-containers (deep search)
					n += queryCount;
					for (ContainerIterator cit = subContainer->iterator(); cit.hasNext(); cit.advance()) {
						if (Container* tmpContainer = (*cit)->getContainer()) {
							queryCount = 0;
							tmpContainer->__queryMaxCount(INDEX_WHEREEVER, item, item->getItemCount(), queryCount, flags);
							n += queryCount;
						}
					}
				} else if (inventoryItem->isStackable() && item->getID() == inventoryItem->getID() && inventoryItem->getItemCount() < 100) {
					uint32_t remainder = (100 - inventoryItem->getItemCount());
					if (__queryAdd(i, item, remainder, flags) == RET_NOERROR) {
						n += remainder;
					}
				}
			} else if (__queryAdd(i, item, item->getItemCount(), flags) == RET_NOERROR) {
				if (item->isStackable()) {
					n += 100;
				} else {
					n += 1;
				}
			}
		}

		maxQueryCount = n;
	} else {
		const Thing* destThing = __getThing(index);
		const Item* destItem = nullptr;
		if (destThing) {
			destItem = destThing->getItem();
		}

		if (destItem) {
			if (destItem->isStackable() && item->getID() == destItem->getID() && destItem->getItemCount() < 100) {
				maxQueryCount = 100 - destItem->getItemCount();
			} else {
				maxQueryCount = 0;
			}
		} else if (__queryAdd(index, item, count, flags) == RET_NOERROR) {
			if (item->isStackable()) {
				maxQueryCount = 100;
			} else {
				maxQueryCount = 1;
			}

			return RET_NOERROR;
		}
	}

	if (maxQueryCount < count) {
		return RET_NOTENOUGHROOM;
	}

	return RET_NOERROR;
}

ReturnValue Player::__queryRemove(const Thing* thing, uint32_t count, uint32_t flags, Creature*) const
{
	int32_t index = __getIndexOfThing(thing);
	if (index == -1) {
		return RET_NOTPOSSIBLE;
	}

	const Item* item = thing->getItem();
	if (!item) {
		return RET_NOTPOSSIBLE;
	}

	if (!count || (item->isStackable() && count > item->getItemCount())) {
		return RET_NOTPOSSIBLE;
	}

	if (!item->isMovable() && !hasBitSet(FLAG_IGNORENOTMOVABLE, flags)) {
		return RET_NOTMOVABLE;
	}

	return RET_NOERROR;
}

Cylinder* Player::__queryDestination(int32_t& index, const Thing* thing, Item** destItem,
	uint32_t& flags)
{
	if (!index /*drop to m_capacity window*/ || index == INDEX_WHEREEVER) {
		*destItem = nullptr;
		const Item* item = thing->getItem();
		if (!item) {
			return this;
		}

		const bool autoStack = !(flags & FLAG_IGNOREAUTOSTACK);
		std::list<std::pair<Container*, int32_t>> containers;
		std::list<std::pair<Cylinder*, int32_t>> freeSlots;
		for (int32_t i = SLOT_FIRST; i <= SLOT_LAST; ++i) {
			if (Item* invItem = m_inventory[i]) {
				if (invItem == item || invItem == m_tradeItem) {
					continue;
				}

				if (autoStack && item->isStackable() && __queryAdd(i, item, item->getItemCount(), 0) == RET_NOERROR && invItem->getID() == item->getID() && invItem->getItemCount() < 100) {
					*destItem = invItem;
					index = i;
					return this;
				}

				if (Container* container = invItem->getContainer()) {
					if (!autoStack && container->__queryAdd(INDEX_WHEREEVER, item, item->getItemCount(), flags) == RET_NOERROR) {
						index = INDEX_WHEREEVER;
						return container;
					}

					containers.push_back(std::make_pair(container, 0));
				}
			} else if (!autoStack) {
				if (__queryAdd(i, item, item->getItemCount(), 0) == RET_NOERROR) {
					index = i;
					return this;
				}
			} else {
				freeSlots.push_back(std::make_pair(this, i));
			}
		}

		int32_t deepness = otx::config::getInteger(otx::config::PLAYER_DEEPNESS);
		while (!containers.empty()) {
			Container* tmpContainer = containers.front().first;
			int32_t level = containers.front().second;

			containers.pop_front();
			if (!tmpContainer) {
				continue;
			}

			for (uint32_t n = 0; n < tmpContainer->capacity(); ++n) {
				if (Item* tmpItem = tmpContainer->getItemByIndex(n)) {
					if (tmpItem == item || tmpItem == m_tradeItem) {
						continue;
					}

					if (autoStack && item->isStackable() && tmpContainer->__queryAdd(n, item, item->getItemCount(), 0) == RET_NOERROR && tmpItem->getID() == item->getID() && tmpItem->getItemCount() < 100) {
						index = n;
						*destItem = tmpItem;
						return tmpContainer;
					}

					if (Container* container = tmpItem->getContainer()) {
						if (!autoStack && container->__queryAdd(INDEX_WHEREEVER, item, item->getItemCount(), flags) == RET_NOERROR) {
							index = INDEX_WHEREEVER;
							return container;
						}

						if (deepness < 0 || level < deepness) {
							containers.push_back(std::make_pair(container, level + 1));
						}
					}
				} else {
					if (!autoStack) {
						if (tmpContainer->__queryAdd(n, item, item->getItemCount(), 0) == RET_NOERROR) {
							index = n;
							return tmpContainer;
						}
					} else {
						freeSlots.push_back(std::make_pair(tmpContainer, n));
					}

					break; // one slot to check is definitely enough.
				}
			}
		}

		if (autoStack) {
			while (!freeSlots.empty()) {
				Cylinder* tmpCylinder = freeSlots.front().first;
				int32_t i = freeSlots.front().second;

				freeSlots.pop_front();
				if (!tmpCylinder) {
					continue;
				}

				if (tmpCylinder->__queryAdd(i, item, item->getItemCount(), flags) == RET_NOERROR) {
					index = i;
					return tmpCylinder;
				}
			}
		}

		return this;
	}

	Thing* destThing = __getThing(index);
	if (destThing) {
		*destItem = destThing->getItem();
	}

	if (Cylinder* subCylinder = dynamic_cast<Cylinder*>(destThing)) {
		index = INDEX_WHEREEVER;
		*destItem = nullptr;
		return subCylinder;
	}

	return this;
}

void Player::__addThing(Creature* actor, Thing* thing)
{
	__addThing(actor, 0, thing);
}

void Player::__addThing(Creature*, int32_t index, Thing* thing)
{
	if (index < 0 || index > 11) {
		return /*RET_NOTPOSSIBLE*/;
	}

	if (!index) {
		return /*RET_NOTENOUGHROOM*/;
	}

	Item* item = thing->getItem();
	if (!item) {
		return /*RET_NOTPOSSIBLE*/;
	}

	item->setParent(this);
	m_inventory[index] = item;

	// send to client
	sendAddInventoryItem(index, item);
}

void Player::__updateThing(Thing* thing, uint16_t itemId, uint32_t count)
{
	int32_t index = __getIndexOfThing(thing);
	if (index == -1) {
		return /*RET_NOTPOSSIBLE*/;
	}

	Item* item = thing->getItem();
	if (!item) {
		return /*RET_NOTPOSSIBLE*/;
	}

	item->setID(itemId);
	item->setSubType(count);

	// send to client
	sendUpdateInventoryItem(index, item);

	// event methods
	onUpdateInventoryItem(item, item);
}

void Player::__replaceThing(uint32_t index, Thing* thing)
{
	if (index > 11) {
		return /*RET_NOTPOSSIBLE*/;
	}

	Item* oldItem = getInventoryItem(static_cast<Slots_t>(index));
	if (!oldItem) {
		return /*RET_NOTPOSSIBLE*/;
	}

	Item* item = thing->getItem();
	if (!item) {
		return /*RET_NOTPOSSIBLE*/;
	}

	// send to client
	sendUpdateInventoryItem(index, item);

	// event methods
	onUpdateInventoryItem(oldItem, item);

	item->setParent(this);

	m_inventory[index] = item;
}

void Player::__removeThing(Thing* thing, uint32_t count)
{
	Item* item = thing->getItem();
	if (!item) {
		return /*RET_NOTPOSSIBLE*/;
	}

	int32_t index = __getIndexOfThing(thing);
	if (index == -1) {
		return /*RET_NOTPOSSIBLE*/;
	}

	if (item->isStackable()) {
		if (count == item->getItemCount()) {
			// send change to client
			sendRemoveInventoryItem(index);

			// event methods
			onRemoveInventoryItem(item);

			item->setParent(nullptr);
			m_inventory[index] = nullptr;
		} else {
			item->setItemCount(std::max<int32_t>(0, item->getItemCount() - count));

			// send change to client
			sendUpdateInventoryItem(index, item);

			// event methods
			onUpdateInventoryItem(item, item);
		}
	} else {
		// send change to client
		sendRemoveInventoryItem(index);

		// event methods
		onRemoveInventoryItem(item);

		item->setParent(nullptr);
		m_inventory[index] = nullptr;
	}
}

Thing* Player::__getThing(uint32_t index) const
{
	if (index >= SLOT_FIRST && index <= SLOT_LAST) {
		return m_inventory[index];
	}

	return nullptr;
}

int32_t Player::__getIndexOfThing(const Thing* thing) const
{
	for (int32_t i = SLOT_FIRST; i <= SLOT_LAST; ++i) {
		if (m_inventory[i] == thing) {
			return i;
		}
	}

	return -1;
}

int32_t Player::__getFirstIndex() const
{
	return SLOT_FIRST;
}

int32_t Player::__getLastIndex() const
{
	return SLOT_LAST + 1;
}

uint32_t Player::__getItemTypeCount(uint16_t itemId, int32_t subType /*= -1*/) const
{
	Item* item = nullptr;
	Container* container = nullptr;

	uint32_t count = 0;
	for (int32_t i = SLOT_FIRST; i <= SLOT_LAST; ++i) {
		if (!(item = m_inventory[i])) {
			continue;
		}

		if (item->getID() == itemId) {
			count += Item::countByType(item, subType);
		}

		if (!(container = item->getContainer())) {
			continue;
		}

		for (ContainerIterator it = container->iterator(); it.hasNext(); it.advance()) {
			if ((*it)->getID() == itemId) {
				count += Item::countByType(*it, subType);
			}
		}
	}

	return count;
}

std::map<uint32_t, uint32_t>& Player::__getAllItemTypeCount(std::map<uint32_t, uint32_t>& countMap) const
{
	Item* item = nullptr;
	Container* container = nullptr;
	for (int32_t i = SLOT_FIRST; i <= SLOT_LAST; ++i) {
		if (!(item = m_inventory[i])) {
			continue;
		}

		countMap[item->getID()] += Item::countByType(item, -1);
		if (!(container = item->getContainer())) {
			continue;
		}

		for (ContainerIterator it = container->iterator(); it.hasNext(); it.advance()) {
			countMap[(*it)->getID()] += Item::countByType(*it, -1);
		}
	}

	return countMap;
}

void Player::postAddNotification(Creature*, Thing* thing, const Cylinder* oldParent,
	int32_t index, CylinderLink_t link /*= LINK_OWNER*/)
{
	if (link == LINK_OWNER) { // calling movement scripts
		g_moveEvents.onPlayerEquip(this, thing->getItem(), static_cast<Slots_t>(index), false);
	}

	bool requireListUpdate = true;
	if (link == LINK_OWNER || link == LINK_TOPPARENT) {
		if (const Item* item = (oldParent ? oldParent->getItem() : nullptr)) {
			assert(item->getContainer() != nullptr);
			requireListUpdate = item->getContainer()->getHoldingPlayer() != this;
		} else {
			requireListUpdate = oldParent != this;
		}

		updateInventoryWeight();
		updateItemsLight();
		sendStats();
	}

	if (const Item* item = thing->getItem()) {
		if (const Container* container = item->getContainer()) {
			onSendContainer(container);
		}

		if (m_shopOwner && requireListUpdate) {
			updateInventoryGoods(item->getID());
		}
	} else if (const Creature* creature = thing->getCreature()) {
		if (creature != this) {
			return;
		}

		std::vector<Container*> containers;
		for (ContainerVector::iterator it = m_containerVec.begin(); it != m_containerVec.end(); ++it) {
			if (!Position::areInRange<1, 1, 0>(it->second->getPosition(), getPosition())) {
				containers.push_back(it->second);
			}
		}

		for (std::vector<Container*>::const_iterator it = containers.begin(); it != containers.end(); ++it) {
			autoCloseContainers(*it);
		}
	}
}

void Player::postRemoveNotification(Creature*, Thing* thing, const Cylinder* newParent,
	int32_t index, bool isCompleteRemoval, CylinderLink_t link /* = LINK_OWNER*/)
{
	if (link == LINK_OWNER) { // calling movement scripts
		g_moveEvents.onPlayerDeEquip(this, thing->getItem(), static_cast<Slots_t>(index), isCompleteRemoval);
	}

	bool requireListUpdate = true;
	if (link == LINK_OWNER || link == LINK_TOPPARENT) {
		if (const Item* item = (newParent ? newParent->getItem() : nullptr)) {
			assert(item->getContainer() != nullptr);
			requireListUpdate = item->getContainer()->getHoldingPlayer() == this;
		} else {
			requireListUpdate = newParent == this;
		}

		updateInventoryWeight();
		updateItemsLight();
		sendStats();
	}

	if (const Item* item = thing->getItem()) {
		if (const Container* container = item->getContainer()) {
			if (container->isRemoved() || !Position::areInRange<1, 1, 0>(getPosition(), container->getPosition())) {
				autoCloseContainers(container);
			} else if (container->getTopParent() == this) {
				onSendContainer(container);
			} else if (const Container* topContainer = dynamic_cast<const Container*>(container->getTopParent())) {
				if (const Depot* depot = dynamic_cast<const Depot*>(topContainer)) {
					bool isOwner = false;
					for (DepotMap::iterator it = m_depots.begin(); it != m_depots.end(); ++it) {
						if (it->second.first != depot) {
							continue;
						}

						isOwner = true;
						onSendContainer(container);
					}

					if (!isOwner) {
						autoCloseContainers(container);
					}
				} else {
					onSendContainer(container);
				}
			} else {
				autoCloseContainers(container);
			}
		}

		if (m_shopOwner && requireListUpdate) {
			updateInventoryGoods(item->getID());
		}
	}
}

void Player::__internalAddThing(Thing* thing)
{
	__internalAddThing(0, thing);
}

void Player::__internalAddThing(uint32_t index, Thing* thing)
{
	if (!index || index > 11) {
		return;
	}

	if (m_inventory[index]) {
		return;
	}

	Item* item = thing->getItem();
	if (!item) {
		return;
	}

	m_inventory[index] = item;
	item->setParent(this);
}

bool Player::setFollowCreature(Creature* creature, bool fullPathSearch /*= false*/)
{
	bool deny = false;
	if (hasEventRegistered(CREATURE_EVENT_FOLLOW)) {
		for (CreatureEvent* it : getCreatureEvents(CREATURE_EVENT_FOLLOW)) {
			if (!it->executeAction(this, creature)) {
				deny = true;
			}
		}
	}

	if (deny || Creature::setFollowCreature(creature, fullPathSearch)) {
		return true;
	}

	setFollowCreature(nullptr);
	setAttackedCreature(nullptr);
	if (!deny) {
		sendCancelMessage(RET_THEREISNOWAY);
	}

	sendCancelTarget();
	m_cancelNextWalk = true;
	return false;
}

bool Player::setAttackedCreature(Creature* creature)
{
	if (!Creature::setAttackedCreature(creature)) {
		sendCancelTarget();
		return false;
	}

	if (m_chaseMode && creature && !getNoMove()) {
		if (m_followCreature != creature) { // chase opponent
			setFollowCreature(creature);
		}
	} else {
		setFollowCreature(nullptr);
	}

	if (creature) {
		addDispatcherTask([playerID = getID()]() { g_game.checkCreatureAttack(playerID); });
	}

	return true;
}

void Player::goToFollowCreature()
{
	if (!m_walkTask) {
		Creature::goToFollowCreature();
	}
}

void Player::getPathSearchParams(const Creature* creature, FindPathParams& fpp) const
{
	Creature::getPathSearchParams(creature, fpp);
	fpp.fullPathSearch = true;
}

void Player::doAttacking(uint32_t)
{
	const int64_t timeNow = otx::util::mstime();
	const uint32_t attackSpeed = getAttackSpeed();
	if (attackSpeed == 0 || (hasCondition(CONDITION_PACIFIED) && !hasCustomFlag(PlayerCustomFlag_IgnorePacification))) {
		m_lastAttack = timeNow;
		return;
	}

	if (m_lastAttack == 0) {
		m_lastAttack = timeNow - attackSpeed - 1;
	} else if ((timeNow - m_lastAttack) < attackSpeed) {
		return;
	}

	Item* weaponItem = getWeapon();
	if (const Weapon* weapon = g_weapons.getWeapon(weaponItem)) {
		if (weapon->interruptSwing() && !canDoAction()) {
			setNextActionTask(createSchedulerTask(
				getNextActionTime(),
				[playerID = getID()]() { g_game.checkCreatureAttack(playerID); }));
		} else {
			if ((!weapon->hasExhaustion() || !hasCondition(CONDITION_EXHAUST)) && weapon->useWeapon(this, weaponItem, m_attackedCreature)) {
				m_lastAttack = timeNow;
			}
		}
		return;
	}

	if (Weapon::useFist(this, m_attackedCreature)) {
		m_lastAttack = timeNow;
	}
}

bool Player::hasExtraSwing()
{
	return m_lastAttack > 0 && ((otx::util::mstime() - m_lastAttack) >= getAttackSpeed());
}

double Player::getGainedExperience(Creature* attacker) const
{
	if (!m_skillLoss) {
		return 0;
	}

	double rate = otx::config::getDouble(otx::config::RATE_PVP_EXPERIENCE);
	if (rate <= 0) {
		return 0;
	}

	Player* attackerPlayer = attacker->getPlayer();
	if (!attackerPlayer || attackerPlayer == this) {
		return 0;
	}

	double attackerLevel = attackerPlayer->getLevel();
	double min = otx::config::getDouble(otx::config::EFP_MIN_THRESHOLD);
	double max = otx::config::getDouble(otx::config::EFP_MAX_THRESHOLD);
	if ((min > 0.0 && m_level < static_cast<uint32_t>(std::floor(attackerLevel * min))) || (max > 0.0 && m_level > static_cast<uint32_t>(std::floor(attackerLevel * max)))) {
		return 0;
	}

	/*
		Formula
		a = attackers level * 0.9
		b = victims level
		c = victims experience

		result = (1 - (a / b)) * 0.05 * c
		Not affected by special multipliers(!)
	*/
	double a = std::floor(attackerLevel * 0.9);
	uint32_t b = m_level;
	uint64_t c = getExperience();
	return std::max<double>(0.0, std::floor(getDamageRatio(attacker) * std::max<double>(0.0, 1.0 - (a / b)) * 0.05 * c)) * rate;
}

void Player::onFollowCreature(const Creature* creature)
{
	if (!creature) {
		m_cancelNextWalk = true;
	}
}

void Player::setChaseMode(bool mode)
{
	if (m_chaseMode == mode) {
		return;
	}

	m_chaseMode = mode;
	if (m_chaseMode) {
		// chase opponent
		if (!m_followCreature && m_attackedCreature && !getNoMove()) {
			setFollowCreature(m_attackedCreature);
		}
	} else if (m_attackedCreature) {
		setFollowCreature(nullptr);
		m_cancelNextWalk = true;
	}
}

void Player::onWalkAborted()
{
	setNextWalkActionTask(nullptr);
	sendCancelWalk();
}

void Player::onWalkComplete()
{
	if (!m_walkTask) {
		return;
	}

	m_walkTaskEvent = g_scheduler.addEvent(std::move(m_walkTask));
}

void Player::getCreatureLight(LightInfo& light) const
{
	if (hasCustomFlag(PlayerCustomFlag_HasFullLight)) {
		light.level = 0xFF;
		light.color = 215;
	} else if (m_internalLight.level > m_itemsLight.level) {
		light = m_internalLight;
	} else {
		light = m_itemsLight;
	}
}

void Player::updateItemsLight(bool internal /* = false*/)
{
	LightInfo maxLight, curLight;
	Item* item = nullptr;
	for (int32_t i = SLOT_FIRST; i <= SLOT_LAST; ++i) {
		if (!(item = getInventoryItem(static_cast<Slots_t>(i)))) {
			continue;
		}

		item->getLight(curLight);
		if (curLight.level > maxLight.level) {
			maxLight = curLight;
		}
	}

	if (maxLight.level != m_itemsLight.level || maxLight.color != m_itemsLight.color) {
		m_itemsLight = maxLight;
		if (!internal) {
			g_game.changeLight(this);
		}
	}
}

void Player::onAddCondition(ConditionType_t type, bool hadCondition)
{
	Creature::onAddCondition(type, hadCondition);
	if (type == CONDITION_GAMEMASTER) {
		return;
	}

	if (getLastPosition().x) { // don't send if player have just logged in (its already done in protocolgame), or condition have no icons
		sendIcons();
	}
}

void Player::onAddCombatCondition(ConditionType_t type, bool)
{
	std::string tmp;
	switch (type) {
		// client hardcoded
		case CONDITION_FIRE:
			tmp = "burning";
			break;
		case CONDITION_POISON:
			tmp = "poisoned";
			break;
		case CONDITION_ENERGY:
			tmp = "electrified";
			break;
		case CONDITION_FREEZING:
			tmp = "freezing";
			break;
		case CONDITION_DAZZLED:
			tmp = "dazzled";
			break;
		case CONDITION_CURSED:
			tmp = "cursed";
			break;
		case CONDITION_DROWN:
			tmp = "drowning";
			break;
		case CONDITION_DRUNK:
			tmp = "drunk";
			break;
		case CONDITION_PARALYZE:
			tmp = "paralyzed";
			break;
		case CONDITION_BLEEDING:
			tmp = "bleeding";
			break;
		default:
			break;
	}

	if (!tmp.empty()) {
		sendTextMessage(MSG_STATUS_DEFAULT, "You are " + tmp + ".");
	}
}

void Player::onEndCondition(ConditionType_t type, ConditionId_t id)
{
	Creature::onEndCondition(type, id);
	switch (type) {
		case CONDITION_INFIGHT: {
			onIdleStatus();
			clearAttacked();

			m_pzLocked = false;
			if (m_skull < SKULL_RED) {
				setSkull(SKULL_NONE);
			}

			g_game.updateCreatureSkull(this);
			break;
		}

		default:
			break;
	}

	sendIcons();
}

void Player::onCombatRemoveCondition(const Creature*, Condition* condition)
{
	// Creature::onCombatRemoveCondition(attacker, condition);
	bool remove = true;
	if (condition->getId() > 0) {
		remove = false;
		// Means the condition is from an item, id == slot
		if (g_game.getWorldType() == WORLDTYPE_HARDCORE) {
			if (Item* item = getInventoryItem(static_cast<Slots_t>(condition->getId()))) {
				// 25% chance to destroy the item
				if (random_range(1, 100) < 26) {
					g_game.internalRemoveItem(nullptr, item);
				}
			}
		}
	}

	if (remove) {
		if (!canDoAction()) {
			int32_t delay = getNextActionTime(false);
			delay -= (delay % EVENT_CREATURE_THINK_INTERVAL);
			if (delay < 0) {
				removeCondition(condition);
			} else {
				condition->setTicks(delay);
			}
		} else {
			removeCondition(condition);
		}
	}
}

void Player::onTickCondition(ConditionType_t type, ConditionId_t id, int32_t interval, bool& _remove)
{
	Creature::onTickCondition(type, id, interval, _remove);
	switch (type) {
		case CONDITION_HUNTING:
			useStamina(-(interval * otx::config::getInteger(otx::config::RATE_STAMINA_LOSS)));
			break;

		default:
			break;
	}
}

void Player::onTarget(Creature* target)
{
	Creature::onTarget(target);

	if (target == this) {
		addInFightTicks(false);
		return;
	}

	if (hasFlag(PlayerFlag_NotGainInFight)) {
		return;
	}

	Player* targetPlayer = target->getPlayer();
	if (targetPlayer && !isPartner(targetPlayer) && !isAlly(targetPlayer)) {
		if (!m_pzLocked && g_game.getWorldType() == WORLDTYPE_HARDCORE && !targetPlayer->getTile()->hasFlag(TILESTATE_HARDCOREZONE)) {
			m_pzLocked = true;
			sendIcons();
		}

		if (otx::config::getBoolean(otx::config::PZLOCK_ON_ATTACK_SKULLED_PLAYERS)) {
			if (!m_pzLocked && targetPlayer->getSkull() >= SKULL_WHITE) {
				m_pzLocked = true;
				sendIcons();
				addInFightTicks(m_pzLocked, 60000);
			}
		}

		if (getSkull() == SKULL_NONE && getSkullType(targetPlayer) == SKULL_YELLOW) {
			addAttacked(targetPlayer);
			targetPlayer->sendCreatureSkull(this);
		} else if (!targetPlayer->hasAttacked(this)) {
			if (!m_pzLocked && !targetPlayer->getTile()->hasFlag(TILESTATE_HARDCOREZONE)) {
				m_pzLocked = true;
				sendIcons();
			}

			if (!Combat::isInPvpZone(this, targetPlayer) && !isEnemy(targetPlayer, false)) {
				addAttacked(targetPlayer);

				if (targetPlayer->getSkull() == SKULL_NONE && getSkull() == SKULL_NONE) {
					setSkull(SKULL_WHITE);
					g_game.updateCreatureSkull(this);
				}

				if (getSkull() == SKULL_NONE) {
					targetPlayer->sendCreatureSkull(this);
				}
			}
		}
	}
	if (!m_pzLocked) {
		addInFightTicks(false);
	}
}

void Player::onSummonTarget(Creature* summon, Creature* target)
{
	Creature::onSummonTarget(summon, target);
	onTarget(target);
}

void Player::onAttacked()
{
	Creature::onAttacked();
	addInFightTicks(false);
}

bool Player::checkLoginDelay() const
{
	return (!hasCustomFlag(PlayerCustomFlag_IgnoreLoginDelay) && otx::util::mstime() <= (m_lastLoad + otx::config::getInteger(otx::config::LOGIN_PROTECTION)));
}

void Player::onIdleStatus()
{
	Creature::onIdleStatus();
	if (m_party) {
		m_party->clearPlayerPoints(this);
	}
}

void Player::onPlacedCreature()
{
	// scripting event - onLogin
	if (!g_creatureEvents.playerLogin(this)) {
		kick(true, true);
	}
}

void Player::onTargetDrain(Creature* target, int32_t points)
{
	if (points < 0) {
		return;
	}

	Creature::onTargetDrain(target, points);
	if (m_party && target && (!target->getMaster() || !target->getMaster()->getPlayer())
		&& target->getMonster() && target->getMonster()->isHostile()) { // we have fulfilled a requirement for shared experience
		m_party->addPlayerDamageMonster(this, points);
	}
}

void Player::onSummonTargetDrain(Creature* summon, Creature* target, int32_t points)
{
	if (points < 0) {
		return;
	}

	Creature::onSummonTargetDrain(summon, target, points);
	if (m_party && target && (!target->getMaster() || !target->getMaster()->getPlayer())
		&& target->getMonster() && target->getMonster()->isHostile()) { // we have fulfilled a requirement for shared experience
		m_party->addPlayerDamageMonster(this, points);
	}
}

void Player::onTargetGain(Creature* target, int32_t points)
{
	Creature::onTargetGain(target, points);
	if (!target || !m_party) {
		return;
	}

	Player* tmpPlayer = nullptr;
	if (target->getPlayer()) {
		tmpPlayer = target->getPlayer();
	} else if (target->getMaster() && target->getMaster()->getPlayer()) {
		tmpPlayer = target->getMaster()->getPlayer();
	}

	if (isPartner(tmpPlayer)) {
		m_party->addPlayerHealedMember(this, points);
	}
}

void Player::onUpdateQuest()
{
	sendTextMessage(MSG_EVENT_ADVANCE, "Your quest log has been updated.");
}

GuildEmblems_t Player::getGuildEmblem(const Creature* creature) const
{
	const Player* player = creature->getPlayer();
	if (!player || !player->hasEnemy()) {
		return Creature::getGuildEmblem(creature);
	}

	if (player->isEnemy(this, false)) {
		return GUILDEMBLEM_ENEMY;
	}

	return player->getGuildId() == m_guildId ? GUILDEMBLEM_ALLY : GUILDEMBLEM_NEUTRAL;
}

bool Player::getEnemy(const Player* player, War_t& data) const
{
	if (!m_guildId || !player || player->isRemoved()) {
		return false;
	}

	uint32_t guild = player->getGuildId();
	if (!guild) {
		return false;
	}

	WarMap::const_iterator it = m_warMap.find(guild);
	if (it == m_warMap.end()) {
		return false;
	}

	data = it->second;
	return true;
}

bool Player::isEnemy(const Player* player, bool allies) const
{
	if (!m_guildId || !player || player->isRemoved()) {
		return false;
	}

	uint32_t guild = player->getGuildId();
	if (!guild) {
		return false;
	}

	return !m_warMap.empty() && (((g_game.getWorldType() != WORLDTYPE_OPTIONAL || otx::config::getBoolean(otx::config::OPTIONAL_WAR_ATTACK_ALLY)) && allies && m_guildId == guild) || m_warMap.find(guild) != m_warMap.end());
}

bool Player::isAlly(const Player* player) const
{
	return !m_warMap.empty() && player && player->getGuildId() == m_guildId;
}

bool Player::onKilledCreature(Creature* target, DeathEntry& entry)
{
	if (!Creature::onKilledCreature(target, entry)) {
		return false;
	}

	if (hasFlag(PlayerFlag_NotGenerateLoot)) {
		target->setDropLoot(LOOT_DROP_NONE);
	}

	Condition* condition = nullptr;
	if (target->getMonster() && !target->isPlayerSummon() && !hasFlag(PlayerFlag_HasInfiniteStamina)
		&& (condition = Condition::createCondition(CONDITIONID_DEFAULT, CONDITION_HUNTING,
				otx::config::getInteger(otx::config::HUNTING_DURATION)))) {
		addCondition(condition);
	}

	if (hasFlag(PlayerFlag_NotGainInFight) || getZone() != target->getZone()) {
		return true;
	}

	Player* targetPlayer = target->getPlayer();
	if (!targetPlayer || Combat::isInPvpZone(this, targetPlayer)
		|| isPartner(targetPlayer) || isAlly(targetPlayer)) {
		return true;
	}

	War_t enemy;
	if (targetPlayer->getEnemy(this, enemy)) {
		if (entry.isLast()) {
			IOGuild::getInstance()->updateWar(enemy);
		}

		entry.setWar(enemy);
	}

	if (targetPlayer) {
		if (hasEventRegistered(CREATURE_EVENT_NOCOUNTFRAG)) {
			for (CreatureEvent* it : getCreatureEvents(CREATURE_EVENT_NOCOUNTFRAG)) {
				if (!it->executeNoCountFragArea(this, target)) {
					return true;
				}
			}
		}

		if (!otx::config::getBoolean(otx::config::ADD_FRAG_SAMEIP)) {
			if (this->getIP() == targetPlayer->getIP()) {
				return true;
			}
		}
	}

	if (!entry.isJustify() || !hasCondition(CONDITION_INFIGHT)) {
		return true;
	}

	if (!targetPlayer->hasAttacked(this) && target->getSkull() == SKULL_NONE
		&& targetPlayer != this && (addUnjustifiedKill(targetPlayer, !enemy.war) || entry.isLast())) {
		entry.setUnjustified();
	}

	if (entry.isLast()) {
		addInFightTicks(true, otx::config::getInteger(otx::config::WHITE_SKULL_TIME));
	}
	return true;
}

bool Player::gainExperience(double& gainExp, Creature* target)
{
	if (!rateExperience(gainExp, target)) {
		return false;
	}

	// use cast exp, make by feetads
	if (otx::config::getBoolean(otx::config::CAST_EXP_ENABLED)) {
		if (m_client->isBroadcasting() && m_client->getPassword().empty()) {
			uint32_t extraExpCast = otx::config::getInteger(otx::config::CAST_EXP_PERCENT);
			gainExp *= 1 + ((extraExpCast && extraExpCast > 0) ? (extraExpCast / 100.0) : 0);
		}
	}

	// soul regeneration
	if (gainExp >= m_level) {
		if (Condition* condition = Condition::createCondition(
				CONDITIONID_DEFAULT, CONDITION_SOUL, 4 * 60 * 1000)) {
			condition->setParam(CONDITIONPARAM_SOULGAIN,
				m_vocation->gainAmount[GAIN_SOUL]);
			condition->setParam(CONDITIONPARAM_SOULTICKS,
				(m_vocation->gainTicks[GAIN_SOUL] * 1000));
			addCondition(condition);
		}
	}

	addExperience(gainExp);
	return true;
}

bool Player::rateExperience(double& gainExp, Creature* target)
{
	if (hasFlag(PlayerFlag_NotGainExperience) || gainExp <= 0) {
		return false;
	}

	if (target->getPlayer()) {
		return true;
	}

	gainExp *= m_rates[SKILL__LEVEL] * g_game.getExperienceStage(m_level, m_vocation->getExperienceMultiplier());
	if (!hasFlag(PlayerFlag_HasInfiniteStamina)) {
		int32_t minutes = getStaminaMinutes();
		if (minutes >= otx::config::getInteger(otx::config::STAMINA_LIMIT_TOP)) {
			if (isPremium() || !otx::config::getBoolean(otx::config::STAMINA_BONUS_PREMIUM)) {
				gainExp *= otx::config::getDouble(otx::config::RATE_STAMINA_ABOVE);
			}
		} else if (minutes < (otx::config::getInteger(otx::config::STAMINA_LIMIT_BOTTOM)) && minutes > 0) {
			gainExp *= otx::config::getDouble(otx::config::RATE_STAMINA_UNDER);
		} else if (minutes <= 0) {
			gainExp = 0;
		}
	} else if (isPremium() || !otx::config::getBoolean(otx::config::STAMINA_BONUS_PREMIUM)) {
		gainExp *= otx::config::getDouble(otx::config::RATE_STAMINA_ABOVE);
	}

	return true;
}

void Player::onGainExperience(double& gainExp, Creature* target, bool multiplied)
{
	uint64_t tmp = m_experience;
	if (m_party && m_party->isSharedExperienceEnabled() && m_party->isSharedExperienceActive()) {
		m_party->shareExperience(gainExp, target, multiplied);
		rateExperience(gainExp, target);
		return; // we will get a share of the experience through the sharing mechanism
	}

	if (gainExperience(gainExp, target)) {
		Creature::onGainExperience(gainExp, target, true);
	}

	if (hasEventRegistered(CREATURE_EVENT_ADVANCE)) {
		for (CreatureEvent* it : getCreatureEvents(CREATURE_EVENT_ADVANCE)) {
			it->executeAdvance(this, SKILL__EXPERIENCE, tmp, m_experience);
		}
	}
}

void Player::onGainSharedExperience(double& gainExp, Creature* target, bool)
{
	if (gainExperience(gainExp, target)) {
		Creature::onGainSharedExperience(gainExp, target, true);
	}
}

bool Player::isImmune(CombatType_t type) const
{
	return hasCustomFlag(PlayerCustomFlag_IsImmune) || Creature::isImmune(type);
}

bool Player::isImmune(ConditionType_t type) const
{
	return hasCustomFlag(PlayerCustomFlag_IsImmune) || Creature::isImmune(type);
}

bool Player::isProtected() const
{
	return (m_vocation && !m_vocation->attackable) || hasCustomFlag(PlayerCustomFlag_IsProtected) || m_level < otx::config::getInteger(otx::config::PROTECTION_LEVEL);
}

bool Player::isAttackable() const
{
	return !hasFlag(PlayerFlag_CannotBeAttacked) && !isAccountManager();
}

void Player::changeHealth(int32_t healthChange)
{
	Creature::changeHealth(healthChange);
	sendStats();
}

void Player::changeMana(int32_t manaChange)
{
	if (!hasFlag(PlayerFlag_HasInfiniteMana)) {
		if (manaChange > 0) {
			m_mana += std::min<int32_t>(manaChange, getMaxMana() - m_mana);
		} else {
			m_mana = std::max<int32_t>(0, m_mana + manaChange);
		}
	}

	sendStats();
}

void Player::changeMaxMana(int32_t manaChange)
{
	m_manaMax = manaChange;
	if (m_mana > m_manaMax) {
		m_mana = m_manaMax;
	}
}

void Player::changeSoul(int32_t soulChange)
{
	if (!hasFlag(PlayerFlag_HasInfiniteSoul)) {
		m_soul = std::min<int32_t>(m_soulMax, m_soul + soulChange);
	}
	sendStats();
}

bool Player::changeOutfit(Outfit_t outfit, bool checkList)
{
	uint32_t outfitId = Outfits::getInstance()->getOutfitId(outfit.lookType);
	if (checkList && (!canWearOutfit(outfitId, outfit.lookAddons) || !m_requestedOutfit)) {
		return false;
	}

	m_requestedOutfit = false;
	if (m_outfitAttributes) {
		uint32_t oldId = Outfits::getInstance()->getOutfitId(m_defaultOutfit.lookType);
		m_outfitAttributes = !Outfits::getInstance()->removeAttributes(getID(), oldId, m_sex);
	}

	m_defaultOutfit = outfit;
	m_outfitAttributes = Outfits::getInstance()->addAttributes(getID(), outfitId, m_sex, m_defaultOutfit.lookAddons);
	return true;
}

bool Player::canWearOutfit(uint32_t outfitId, uint32_t addons)
{
	OutfitMap::iterator it = m_outfits.find(outfitId);
	if (it == m_outfits.end() || (it->second.isPremium && !isPremium()) || getAccess() < it->second.accessLevel
		|| (!it->second.groups.empty() && std::find(it->second.groups.begin(), it->second.groups.end(), m_groupId) == it->second.groups.end()) || ((it->second.addons & addons) != addons && !hasCustomFlag(PlayerCustomFlag_CanWearAllAddons))) {
		return false;
	}

	if (it->second.storageId.empty()) {
		return true;
	}

	std::string value;
	getStorage(it->second.storageId, value);
	if (value == it->second.storageValue) {
		return true;
	}

	int32_t intValue = atoi(value.c_str());
	if (!intValue && value != "0") {
		return false;
	}

	int32_t tmp = atoi(it->second.storageValue.c_str());
	if (!tmp && it->second.storageValue != "0") {
		return false;
	}

	return intValue >= tmp;
}

bool Player::addOutfit(uint32_t outfitId, uint32_t addons)
{
	Outfit outfit;
	if (!Outfits::getInstance()->getOutfit(outfitId, m_sex, outfit)) {
		return false;
	}

	OutfitMap::iterator it = m_outfits.find(outfitId);
	if (it != m_outfits.end()) {
		outfit.addons |= it->second.addons;
	}

	outfit.addons |= addons;
	m_outfits[outfitId] = outfit;
	return true;
}

bool Player::removeOutfit(uint32_t outfitId, uint32_t addons)
{
	OutfitMap::iterator it = m_outfits.find(outfitId);
	if (it == m_outfits.end()) {
		return false;
	}

	bool update = false;
	if (addons == 0xFF) // remove outfit
	{
		if (it->second.lookType == m_defaultOutfit.lookType) {
			m_outfits.erase(it);
			if ((it = m_outfits.begin()) != m_outfits.end()) {
				m_defaultOutfit.lookType = it->second.lookType;
			}

			update = true;
		} else {
			m_outfits.erase(it);
		}
	} else // remove addons
	{
		update = it->second.lookType == m_defaultOutfit.lookType;
		it->second.addons &= ~addons;
	}

	if (update) {
		g_game.internalCreatureChangeOutfit(this, m_defaultOutfit, true);
	}

	return true;
}

void Player::generateReservedStorage()
{
	uint32_t key = PSTRG_OUTFITSID_RANGE_START + 1;
	const OutfitMap& defaultOutfits = Outfits::getInstance()->getOutfits(m_sex);
	for (OutfitMap::const_iterator it = m_outfits.begin(); it != m_outfits.end(); ++it) {
		OutfitMap::const_iterator dit = defaultOutfits.find(it->first);
		if (dit == defaultOutfits.end() || (dit->second.isDefault && (dit->second.addons & it->second.addons) == it->second.addons)) {
			continue;
		}

		std::ostringstream k, v;
		k << key++; // this may not work as intended, revalidate it
		v << ((it->first << 16) | (it->second.addons & 0xFF));

		m_storageMap[k.str()] = v.str();
		if (key <= PSTRG_OUTFITSID_RANGE_START + PSTRG_OUTFITSID_RANGE_SIZE) {
			continue;
		}

		std::clog << "[Warning - Player::genReservedStorageRange] Player " << getName() << " with more than 500 outfits!" << std::endl;
		break;
	}
}

void Player::setSex(uint16_t newSex)
{
	m_sex = newSex;
	const OutfitMap& defaultOutfits = Outfits::getInstance()->getOutfits(m_sex);
	for (OutfitMap::const_iterator it = defaultOutfits.begin(); it != defaultOutfits.end(); ++it) {
		if (it->second.isDefault) {
			addOutfit(it->first, it->second.addons);
		}
	}
}

Skulls_t Player::getSkull() const
{
	if (hasFlag(PlayerFlag_NotGainInFight) || hasCustomFlag(PlayerCustomFlag_NotGainSkull)) {
		return SKULL_NONE;
	}
	return m_skull;
}

Skulls_t Player::getSkullType(const Creature* creature) const
{
	if (const Player* player = creature->getPlayer()) {
		if (g_game.getWorldType() != WORLDTYPE_OPEN) {
			return SKULL_NONE;
		}

		if ((player == this || (m_skull != SKULL_NONE && player->getSkull() < SKULL_RED)) && player->hasAttacked(this) && !player->isEnemy(this, false)) {
			return SKULL_YELLOW;
		}

		if (player->getSkull() == SKULL_NONE && (isPartner(player) || isAlly(player)) && g_game.getWorldType() != WORLDTYPE_OPTIONAL) {
			return SKULL_GREEN;
		}
	}

	return Creature::getSkullType(creature);
}

bool Player::hasAttacked(const Player* attacked) const
{
	if (hasFlag(PlayerFlag_NotGainInFight) || !attacked) {
		return false;
	}

	const uint32_t attackedId = attacked->getID();

	auto it = m_attackedSet.find(attackedId);
	if (it != m_attackedSet.end()) {
		return true;
	}
	return false;
}

void Player::addAttacked(const Player* attacked)
{
	if (hasFlag(PlayerFlag_NotGainInFight) || !attacked || attacked == this) {
		return;
	}

	const uint32_t attackedId = attacked->getID();

	auto it = m_attackedSet.find(attackedId);
	if (it == m_attackedSet.end()) {
		m_attackedSet.insert(attackedId);
	}
}

void Player::setSkullEnd(time_t _time, bool login, Skulls_t _skull)
{
	if (g_game.getWorldType() != WORLDTYPE_OPEN
		|| hasFlag(PlayerFlag_NotGainInFight) || hasCustomFlag(PlayerCustomFlag_NotGainSkull)) {
		return;
	}

	bool requireUpdate = false;
	if (_time > time(nullptr)) {
		requireUpdate = true;
		setSkull(_skull);
	} else if (m_skull == _skull) {
		requireUpdate = true;
		setSkull(SKULL_NONE);
		_time = 0;
	}

	if (requireUpdate) {
		m_skullEnd = _time;
		if (!login) {
			g_game.updateCreatureSkull(this);
		}
	}
}

bool Player::addUnjustifiedKill(const Player* attacked, bool countNow)
{
	if (!otx::config::getBoolean(otx::config::USE_FRAG_HANDLER) || hasFlag(PlayerFlag_NotGainInFight) || g_game.getWorldType() != WORLDTYPE_OPEN
		|| hasCustomFlag(PlayerCustomFlag_NotGainUnjustified) || hasCustomFlag(PlayerCustomFlag_NotGainSkull)) {
		return false;
	}

	if (countNow) {
		char buffer[90];
		sprintf(buffer, "Warning! The murder of %s was not justified.", attacked->getName().c_str());
		sendTextMessage(MSG_STATUS_WARNING, buffer);
	}

	time_t now = time(nullptr), first = (now - otx::config::getInteger(otx::config::FRAG_LIMIT)),
		   second = (now - otx::config::getInteger(otx::config::FRAG_SECOND_LIMIT));
	std::vector<time_t> dateList;

	IOLoginData::getInstance()->getUnjustifiedDates(m_guid, dateList, now);
	if (countNow) {
		dateList.push_back(now);
	}

	uint32_t fc = 0, sc = 0, tc = dateList.size();
	for (std::vector<time_t>::iterator it = dateList.begin(); it != dateList.end(); ++it) {
		if (second > 0 && (*it) > second) {
			sc++;
		}

		if (first > 0 && (*it) > first) {
			fc++;
		}
	}

	uint32_t f = otx::config::getInteger(otx::config::RED_LIMIT), s = otx::config::getInteger(otx::config::RED_SECOND_LIMIT), t = otx::config::getInteger(otx::config::RED_THIRD_LIMIT);
	if (m_skull < SKULL_RED && ((f > 0 && fc >= f) || (s > 0 && sc >= s) || (t > 0 && tc >= t))) {
		setSkullEnd(now + otx::config::getInteger(otx::config::RED_SKULL_LENGTH), false, SKULL_RED);
	}

	if (!otx::config::getBoolean(otx::config::USE_BLACK_SKULL)) {
		f += otx::config::getInteger(otx::config::BAN_LIMIT);
		s += otx::config::getInteger(otx::config::BAN_SECOND_LIMIT);
		t += otx::config::getInteger(otx::config::BAN_THIRD_LIMIT);
		if ((f <= 0 || fc < f) && (s <= 0 || sc < s) && (t <= 0 || tc < t)) {
			return true;
		}

		if (!IOBan::getInstance()->addAccountBanishment(m_accountId, (now + otx::config::getInteger(otx::config::KILLS_BAN_LENGTH)), 28, ACTION_BANISHMENT, "Player Killing (automatic)", 0, m_guid)) {
			return true;
		}

		sendTextMessage(MSG_INFO_DESCR, "You have been banished.");
		g_game.addMagicEffect(getPosition(), MAGIC_EFFECT_WRAPS_GREEN);
		addSchedulerTask(1000, [playerID = getID()]() { g_game.kickPlayer(playerID, false); });
	} else {
		f += otx::config::getInteger(otx::config::BLACK_LIMIT);
		s += otx::config::getInteger(otx::config::BLACK_SECOND_LIMIT);
		t += otx::config::getInteger(otx::config::BLACK_THIRD_LIMIT);
		if (m_skull < SKULL_BLACK && ((f > 0 && fc >= f) || (s > 0 && sc >= s) || (t > 0 && tc >= t))) {
			setSkullEnd(now + otx::config::getInteger(otx::config::BLACK_SKULL_LENGTH), false, SKULL_BLACK);
			setAttackedCreature(nullptr);
			destroySummons();
		}
	}

	return true;
}

void Player::setPromotionLevel(uint32_t pLevel)
{
	if (pLevel > m_promotionLevel) {
		int32_t tmpLevel = 0, currentVoc = m_vocationId;
		for (uint32_t i = m_promotionLevel; i < pLevel; ++i) {
			currentVoc = g_vocations.getPromotedVocation(currentVoc);
			if (currentVoc == -1) {
				break;
			}

			tmpLevel++;
			Vocation* voc = g_vocations.getVocation(currentVoc);
			if (!voc || (voc->needPremium && !isPremium() && otx::config::getBoolean(otx::config::PREMIUM_FOR_PROMOTION))) {
				continue;
			}

			m_vocationId = currentVoc;
		}

		m_promotionLevel += tmpLevel;
	} else if (pLevel < m_promotionLevel) {
		uint32_t tmpLevel = 0, currentVoc = m_vocationId;
		for (uint32_t i = pLevel; i < m_promotionLevel; ++i) {
			Vocation* voc = g_vocations.getVocation(currentVoc);
			if (!voc || voc->fromVocationId == currentVoc) {
				break;
			}

			tmpLevel++;
			currentVoc = voc->fromVocationId;
			if (voc->needPremium && !isPremium() && otx::config::getBoolean(otx::config::PREMIUM_FOR_PROMOTION)) {
				continue;
			}

			m_vocationId = currentVoc;
		}

		m_promotionLevel -= tmpLevel;
	}

	setVocation(m_vocationId);
}

uint16_t Player::getBlessings() const
{
	if (!otx::config::getBoolean(otx::config::BLESSINGS) || (!isPremium() && otx::config::getBoolean(otx::config::BLESSING_ONLY_PREMIUM))) {
		return 0;
	}

	uint16_t count = 0;
	for (int16_t i = 0; i < 16; ++i) {
		if (hasBlessing(i)) {
			count++;
		}
	}

	return count;
}

uint64_t Player::getLostExperience() const
{
	int32_t blessingCount = getBlessings();
	int32_t deathLosePercentCFG = m_lossPercent[LOSS_EXPERIENCE];
	int32_t deathLossPercent = 100;
	int64_t lossTotalExp = ((m_level + 50.0) / 100.0) * 50.0 * ((m_level * m_level) - (5.0 * m_level) + 8.0);

	deathLossPercent = deathLossPercent - (blessingCount * 8);
	if (isPromoted()) {
		deathLossPercent = deathLossPercent - 30;
	}

	if (m_level <= 23) {
		return ((m_experience * 0.10) * (deathLossPercent / 100.0)) * (deathLosePercentCFG / 100.0);
	}
	return (lossTotalExp * (deathLossPercent / 100.0)) * (deathLosePercentCFG / 100.0);
}

uint32_t Player::getAttackSpeed() const
{
	int32_t modifiers = 0;
	if (m_outfitAttributes) {
		Outfit outfit;
		if (Outfits::getInstance()->getOutfit(m_defaultOutfit.lookType, outfit)) {
			if (outfit.attackSpeed == -1) {
				return 0;
			}

			modifiers += outfit.attackSpeed;
		}
	}

	if (Item* weapon = getWeapon(true)) {
		const int32_t weaponAttackSpeed = weapon->getAttackSpeed();
		if (weaponAttackSpeed > 0) {
			return weaponAttackSpeed + modifiers;
		}
	}
	return m_vocation->attackSpeed + modifiers;
}

void Player::learnInstantSpell(const std::string& name)
{
	if (!hasLearnedInstantSpell(name)) {
		m_learnedInstantSpellList.push_back(name);
	}
}

void Player::unlearnInstantSpell(const std::string& name)
{
	if (!hasLearnedInstantSpell(name)) {
		return;
	}

	LearnedInstantSpellList::iterator it = std::find(m_learnedInstantSpellList.begin(), m_learnedInstantSpellList.end(), name);
	if (it != m_learnedInstantSpellList.end()) {
		m_learnedInstantSpellList.erase(it);
	}
}

bool Player::hasLearnedInstantSpell(const std::string& name) const
{
	if (hasFlag(PlayerFlag_CannotUseSpells)) {
		return false;
	}

	if (hasFlag(PlayerFlag_IgnoreSpellCheck)) {
		return true;
	}

	for (LearnedInstantSpellList::const_iterator it = m_learnedInstantSpellList.begin(); it != m_learnedInstantSpellList.end(); ++it) {
		if (caseInsensitiveEqual(*it, name)) {
			return true;
		}
	}

	return false;
}

void Player::manageAccount(const std::string& text)
{
	std::ostringstream msg;
	bool noSwap = true;
	switch (m_accountManager) {
		case MANAGER_NAMELOCK: {
			if (!m_talkState[1]) {
				m_managerString = text;
				otx::util::trim_string(m_managerString);
				if (m_managerString.length() < 3) {
					msg << "The name is too short, please select a longer one.";
				} else if (m_managerString.length() > 20) {
					msg << "The name is too long, please select a shorter one.";
				} else if (!isValidName(m_managerString)) {
					msg << "Your name seems to contain invalid symbols, please choose another one.";
				} else if (IOLoginData::getInstance()->playerExists(m_managerString, true)) {
					msg << "Player with that name already exists, please choose another one.";
				} else {
					std::string tmp = otx::util::as_lower_string(m_managerString);
					if (tmp.substr(0, 4) != "god " && tmp.substr(0, 3) != "cm " && tmp.substr(0, 3) != "gm ") {
						m_talkState[1] = m_talkState[2] = true;
						msg << "{" << m_managerString << "}, are you sure? {yes} or {no}?";
					} else {
						msg << "Your character is not a staff member, please choose another name.";
					}
				}
			} else if (checkText(text, "no") && m_talkState[2]) {
				m_talkState[1] = m_talkState[2] = false;
				msg << "What new name would you like have then?";
			} else if (checkText(text, "yes") && m_talkState[2]) {
				if (!IOLoginData::getInstance()->playerExists(m_managerString, true)) {
					uint32_t tmp;
					if (IOLoginData::getInstance()->getGuidByName(tmp, m_managerString2) && IOLoginData::getInstance()->changeName(tmp, m_managerString, m_managerString2) && IOBan::getInstance()->removePlayerBanishment(tmp, PLAYERBAN_LOCK)) {
						msg << "Your character {" << m_managerString << "} has been successfully renamed to {" << m_managerString2 << "}, you should be able to login now.";
						if (House* house = Houses::getInstance()->getHouseByPlayerId(tmp)) {
							house->updateDoorDescription(m_managerString);
						}

						m_talkState[1] = true;
						m_talkState[2] = false;
					} else {
						m_talkState[1] = m_talkState[2] = false;
						msg << "Failed to change your name, please contact with staff.";
					}
				} else {
					m_talkState[1] = m_talkState[2] = false;
					msg << "Player with that name already exists, please choose another one.";
				}
			} else {
				msg << "Sorry, but I can't understand you, please try to repeat.";
			}

			break;
		}
		case MANAGER_ACCOUNT: {
			Account account = IOLoginData::getInstance()->loadAccount(m_managerNumber);
			if (checkText(text, "cancel") || (checkText(text, "account") && !m_talkState[1])) {
				m_talkState[1] = true;
				for (int8_t i = 2; i <= 12; ++i) {
					m_talkState[i] = false;
				}

				msg << "Do you want to change your {password}, generate a {recovery key}, create a {character}, or {delete} an existing character?";
			} else if (checkText(text, "delete") && m_talkState[1]) {
				m_talkState[1] = false;
				m_talkState[2] = true;
				msg << "Which character would you like to delete?";
			} else if (m_talkState[2]) {
				std::string tmp = text;
				otx::util::trim_string(tmp);
				if (!isValidName(tmp, false)) {
					msg << "That name to contain invalid symbols, please try again.";
				} else {
					m_talkState[2] = false;
					m_talkState[3] = true;
					m_managerString = tmp;
					msg << "Do you really want to delete the character {" << m_managerString << "}? {yes} or {no}";
				}
			} else if (checkText(text, "yes") && m_talkState[3]) {
				switch (IOLoginData::getInstance()->deleteCharacter(m_managerNumber, m_managerString)) {
					case DELETE_INTERNAL:
						msg << "An error occured while deleting your character. Either the character does not belong to you or it doesn't exist.";
						break;

					case DELETE_SUCCESS:
						msg << "Your character has been deleted.";
						break;

					case DELETE_HOUSE:
						msg << "Your character owns a house. You have to login and leave the house or pass it to someone else to complete.";
						break;

					case DELETE_LEADER:
						msg << "Your character is leader of a guild. You have to disband the guild or pass the leadership to someone else to complete.";
						break;

					case DELETE_ONLINE:
						msg << "Character with that name is currently online, to delete a character it has to be offline.";
						break;
				}

				m_talkState[1] = true;
				for (int8_t i = 2; i <= 12; ++i) {
					m_talkState[i] = false;
				}
			} else if (checkText(text, "no") && m_talkState[3]) {
				m_talkState[1] = true;
				m_talkState[3] = false;
				msg << "Which character would you like to delete then?";
			} else if (checkText(text, "password") && m_talkState[1]) {
				m_talkState[1] = false;
				m_talkState[4] = true;
				msg << "What would you like your password to be?";
			} else if (m_talkState[4]) {
				std::string tmp = text;
				otx::util::trim_string(tmp);
				if (tmp.length() < 6) {
					msg << "That password is too short, please select a longer one.";
				} else if (!isValidPassword(tmp)) {
					msg << "Your password seems to contain invalid symbols, please choose another one.";
				} else {
					m_talkState[4] = false;
					m_talkState[5] = true;
					m_managerString = tmp;
					msg << "{" << m_managerString << "} is it? {yes} or {no}?";
				}
			} else if (checkText(text, "yes") && m_talkState[5]) {
				m_talkState[1] = true;
				for (int8_t i = 2; i <= 12; ++i) {
					m_talkState[i] = false;
				}

				IOLoginData::getInstance()->setPassword(m_managerNumber, m_managerString);
				msg << "Your password has been changed.";
			} else if (checkText(text, "no") && m_talkState[5]) {
				m_talkState[1] = true;
				for (int8_t i = 2; i <= 12; ++i) {
					m_talkState[i] = false;
				}

				msg << "Ok, then not.";
			} else if (checkText(text, "character") && m_talkState[1]) {
				if (account.charList.size() <= 15) {
					m_talkState[1] = false;
					m_talkState[6] = true;
					msg << "What would you like as your character name?";
				} else {
					m_talkState[1] = true;
					for (int8_t i = 2; i <= 12; ++i) {
						m_talkState[i] = false;
					}

					msg << "Your account has reached the limit of 15 characters, you should {delete} a character if you want to create a new one.";
				}
			} else if (m_talkState[6]) {
				m_managerString = text;
				otx::util::trim_string(m_managerString);
				if (m_managerString.length() < 3) {
					msg << "That name is too short, please select a longer one.";
				} else if (m_managerString.length() > 20) {
					msg << "That name is too long, please select a shorter one.";
				} else if (!isValidName(m_managerString)) {
					msg << "Your name seems to contain invalid symbols, please choose another one.";
				} else if (IOLoginData::getInstance()->playerExists(m_managerString, true)) {
					msg << "Player with that name already exists, please choose another one.";
				} else {
					std::string tmp = otx::util::as_lower_string(m_managerString);
					if (tmp.substr(0, 4) != "god " && tmp.substr(0, 3) != "cm " && tmp.substr(0, 3) != "gm ") {
						m_talkState[6] = false;
						m_talkState[7] = true;
						msg << "{" << m_managerString << "}, are you sure? {yes} or {no}";
					} else {
						msg << "Your character is not a staff member, please choose another name.";
					}
				}
			} else if (checkText(text, "no") && m_talkState[7]) {
				m_talkState[6] = true;
				m_talkState[7] = false;
				msg << "What would you like your character name to be then?";
			} else if (checkText(text, "yes") && m_talkState[7]) {
				m_talkState[7] = false;
				m_talkState[8] = true;
				msg << "Would you like to be a {male} or a {female}.";
			} else if (m_talkState[8] && (checkText(text, "female") || checkText(text, "male"))) {
				m_talkState[8] = false;
				m_talkState[9] = true;
				if (checkText(text, "female")) {
					msg << "A female, are you sure? {yes} or {no}";
					m_managerSex = PLAYERSEX_FEMALE;
				} else {
					msg << "A male, are you sure? {yes} or {no}";
					m_managerSex = PLAYERSEX_MALE;
				}
			} else if (checkText(text, "no") && m_talkState[9]) {
				m_talkState[8] = true;
				m_talkState[9] = false;
				msg << "Tell me then, would you like to be a {male} or a {female}?";
			} else if (checkText(text, "yes") && m_talkState[9]) {
				if (otx::config::getBoolean(otx::config::START_CHOOSEVOC)) {
					m_talkState[9] = false;
					m_talkState[11] = true;

					std::vector<std::string> vocations;
					for (const auto& [id, vocation] : g_vocations.getVocations()) {
						if (id == vocation.fromVocationId && id != 0) {
							vocations.push_back(otx::util::as_lower_string(vocation.name));
						}
					}

					msg << "What would you like to be... ";
					for (std::vector<std::string>::const_iterator it = vocations.begin(); it != vocations.end(); ++it) {
						if (it == vocations.begin()) {
							msg << "{" << *it << "}";
						} else if (*it == *(vocations.rbegin())) {
							msg << " or {" << *it << "}.";
						} else {
							msg << ", {" << *it << "}";
						}
					}
				} else if (!IOLoginData::getInstance()->playerExists(m_managerString, true)) {
					m_talkState[1] = true;
					for (int8_t i = 2; i <= 12; ++i) {
						m_talkState[i] = false;
					}

					if (IOLoginData::getInstance()->createCharacter(m_managerNumber, m_managerString, managerNumber2, m_managerSex)) {
						msg << "Your character {" << m_managerString << "} has been created.";
					} else {
						msg << "Your character couldn't be created, please contact with staff.";
					}
				} else {
					m_talkState[6] = true;
					m_talkState[9] = false;
					msg << "Player with that name already exists, please choose another one.";
				}
			} else if (m_talkState[11]) {
				for (const auto& [id, vocation] : g_vocations.getVocations()) {
					if (checkText(text, otx::util::as_lower_string(vocation.name)) && id == vocation.fromVocationId && id != 0) {
						msg << "So you would like to be " << vocation.description << ", {yes} or {no}?";
						managerNumber2 = id;
						m_talkState[11] = false;
						m_talkState[12] = true;
					}
				}
			} else if (checkText(text, "yes") && m_talkState[12]) {
				if (!IOLoginData::getInstance()->playerExists(m_managerString, true)) {
					m_talkState[1] = true;
					for (int8_t i = 2; i <= 12; ++i) {
						m_talkState[i] = false;
					}

					if (IOLoginData::getInstance()->createCharacter(m_managerNumber, m_managerString, managerNumber2, m_managerSex)) {
						msg << "Your character {" << m_managerString << "} has been created.";
					} else {
						msg << "Your character couldn't be created, please contact with staff.";
					}
				} else {
					m_talkState[6] = true;
					m_talkState[9] = false;
					msg << "Player with that name already exists, please choose another one.";
				}
			} else if (checkText(text, "no") && m_talkState[12]) {
				m_talkState[11] = true;
				m_talkState[12] = false;
				msg << "What would you like to be then?";
			} else if (checkText(text, "recovery key") && m_talkState[1]) {
				m_talkState[1] = false;
				m_talkState[10] = true;
				msg << "Would you like to generate a recovery key? {yes} or {no}";
			} else if (checkText(text, "yes") && m_talkState[10]) {
				if (account.recoveryKey != "0") {
					msg << "Sorry, but you already have a recovery key. For security reasons I may not generate for you you a new one.";
				} else {
					m_managerString = generateRecoveryKey(4, 4);
					IOLoginData::getInstance()->setRecoveryKey(m_managerNumber, m_managerString);
					msg << "Your recovery key is {" << m_managerString << "}.";
				}

				m_talkState[1] = true;
				for (int8_t i = 2; i <= 12; ++i) {
					m_talkState[i] = false;
				}
			} else if (checkText(text, "no") && m_talkState[10]) {
				msg << "Ok, then not.";
				m_talkState[1] = true;
				for (int8_t i = 2; i <= 12; ++i) {
					m_talkState[i] = false;
				}
			} else {
				msg << "Sorry, but I can't understand you, please try to repeat.";
			}

			break;
		}
		case MANAGER_NEW: {
			if (checkText(text, "account") && !m_talkState[1]) {
				msg << "What would you like your password to be?";
				m_talkState[1] = true;
				m_talkState[2] = true;
			} else if (m_talkState[2]) {
				std::string tmp = text;
				otx::util::trim_string(tmp);
				if (tmp.length() < 6) {
					msg << "That password is too short, please select a longer one.";
				} else if (!isValidPassword(tmp)) {
					msg << "Your password seems to contain invalid symbols, please choose another one.";
				} else {
					m_talkState[3] = true;
					m_talkState[2] = false;
					m_managerString = tmp;
					msg << "{" << m_managerString << "} is it? {yes} or {no}?";
				}
			} else if (checkText(text, "yes") && m_talkState[3]) {
				if (otx::config::getBoolean(otx::config::GENERATE_ACCOUNT_NUMBER)) {
					do {
						sprintf(m_managerChar, "%d%d%d%d%d%d%d", random_range(2, 9), random_range(2, 9), random_range(2, 9), random_range(2, 9), random_range(2, 9), random_range(2, 9), random_range(2, 9));
					} while (IOLoginData::getInstance()->accountNameExists(m_managerChar));

					uint32_t id = IOLoginData::getInstance()->createAccount(m_managerChar, m_managerString);
					if (id) {
						m_accountManager = MANAGER_ACCOUNT;
						m_managerNumber = id;

						noSwap = m_talkState[1] = false;
						msg << "Your account has been created, you may manage it now, but please remember your account name {"
							<< m_managerChar << "} and password {" << m_managerString << "}!";
					} else {
						msg << "Your account could not be created, please contact with staff.";
					}

					for (int8_t i = 2; i <= 5; ++i) {
						m_talkState[i] = false;
					}
				} else {
					msg << "What would you like your account name to be?";
					m_talkState[3] = false;
					m_talkState[4] = true;
				}
			} else if (checkText(text, "no") && m_talkState[3]) {
				m_talkState[2] = true;
				m_talkState[3] = false;
				msg << "What would you like your password to be then?";
			} else if (m_talkState[4]) {
				std::string tmp = text;
				otx::util::trim_string(tmp);
				if (tmp.length() < 3) {
					msg << "That account name is too short, please select a longer one.";
				} else if (tmp.length() > 32) {
					msg << "That account name is too long, please select a shorter one.";
				} else if (!isValidAccountName(tmp)) {
					msg << "Your account name seems to contain invalid symbols, please choose another one.";
				} else if (otx::util::as_lower_string(tmp) == otx::util::as_lower_string(m_managerString)) {
					msg << "Your account name cannot be same as password, please choose another one.";
				} else {
					sprintf(m_managerChar, "%s", tmp.c_str());
					msg << "{" << m_managerChar << "}, is it? {yes} or {no}?";
					m_talkState[4] = false;
					m_talkState[5] = true;
				}
			} else if (checkText(text, "yes") && m_talkState[5]) {
				if (!IOLoginData::getInstance()->accountNameExists(m_managerChar)) {
					uint32_t id = IOLoginData::getInstance()->createAccount(m_managerChar, m_managerString);
					if (id) {
						m_accountManager = MANAGER_ACCOUNT;
						m_managerNumber = id;

						noSwap = m_talkState[1] = false;
						msg << "Your account has been created, you may manage it now, but please remember your account name {"
							<< m_managerChar << "} and password {" << m_managerString << "}!";
					} else {
						msg << "Your account could not be created, please contact with staff.";
					}

					for (int8_t i = 2; i <= 5; ++i) {
						m_talkState[i] = false;
					}
				} else {
					msg << "Account with that name already exists, please choose another one.";
					m_talkState[4] = true;
					m_talkState[5] = false;
				}
			} else if (checkText(text, "no") && m_talkState[5]) {
				m_talkState[5] = false;
				m_talkState[4] = true;
				msg << "What would you like your account name to be then?";
			} else if (checkText(text, "recover") && !m_talkState[6]) {
				m_talkState[6] = true;
				m_talkState[7] = true;
				msg << "What was your account name?";
			} else if (m_talkState[7]) {
				m_managerString = text;
				if (IOLoginData::getInstance()->getAccountId(m_managerString, m_managerNumber)) {
					m_talkState[7] = false;
					m_talkState[8] = true;
					msg << "What was your recovery key?";
				} else {
					msg << "Sorry, but account with name {" << m_managerString << "} does not exists.";
					m_talkState[6] = m_talkState[7] = false;
				}
			} else if (m_talkState[8]) {
				m_managerString2 = text;
				if (IOLoginData::getInstance()->validRecoveryKey(m_managerNumber, m_managerString2) && m_managerString2 != "0") {
					sprintf(m_managerChar, "%s%d", otx::config::getString(otx::config::SERVER_NAME).c_str(), random_range(100, 999));
					IOLoginData::getInstance()->setPassword(m_managerNumber, m_managerChar);
					msg << "Correct! Your new password is {" << m_managerChar << "}.";
				} else {
					msg << "Sorry, but this key does not match to specified account.";
				}

				m_talkState[7] = m_talkState[8] = false;
			} else {
				msg << "Sorry, but I can't understand you, please try to repeat.";
			}

			break;
		}
		default:
			return;
			break;
	}

	sendCreatureSay(this, MSG_NPC_FROM, msg.str());
	if (!noSwap) {
		sendCreatureSay(this, MSG_NPC_FROM, "Hint: Type {account} to manage your account and if you want to start over then type {cancel}.");
	}
}

bool Player::isGuildInvited(uint32_t guildId) const
{
	for (InvitationsList::const_iterator it = m_invitationsList.begin(); it != m_invitationsList.end(); ++it) {
		if ((*it) == guildId) {
			return true;
		}
	}
	return false;
}

void Player::leaveGuild()
{
	m_warMap.clear();
	g_game.updateCreatureEmblem(this);
	sendClosePrivate(CHANNEL_GUILD);

	m_guildLevel = GUILDLEVEL_NONE;
	m_guildId = m_rankId = 0;
	m_guildName = m_rankName = m_guildNick = std::string();
}

bool Player::isPremium() const
{
	if (otx::config::getBoolean(otx::config::FREE_PREMIUM) || hasFlag(PlayerFlag_IsAlwaysPremium)) {
		return true;
	}

	return (m_premiumDays != 0);
}

void Player::addPremiumDays(int32_t days)
{
	if (m_premiumDays < 0xFFFF) {
		Account account = IOLoginData::getInstance()->loadAccount(m_accountId);
		if (days < 0) {
			account.premiumDays = std::max<int32_t>(0, account.premiumDays + days);
			m_premiumDays = std::max<int32_t>(0, m_premiumDays + days);
		} else {
			account.premiumDays = std::min<int32_t>(65534, account.premiumDays + days);
			m_premiumDays = std::min<int32_t>(65534, m_premiumDays + days);
		}

		IOLoginData::getInstance()->saveAccount(account);
	}
}

bool Player::setGuildLevel(GuildLevel_t newLevel, uint32_t rank /* = 0*/)
{
	std::string name;
	if (!IOGuild::getInstance()->getRankEx(rank, name, m_guildId, newLevel)) {
		return false;
	}

	m_guildLevel = newLevel;
	m_rankName = name;
	m_rankId = rank;
	return true;
}

bool Player::setGroupId(uint16_t id)
{
	if (Group* group = g_game.groups.getGroup(id)) {
		setGroup(group);
		return true;
	}
	return false;
}

void Player::setGroup(Group* group)
{
	m_group = group;
	m_groupId = group->id;
}

PartyShields_t Player::getPartyShield(const Creature* creature) const
{
	const Player* player = creature->getPlayer();
	if (!player) {
		return Creature::getPartyShield(creature);
	}

	if (m_party) {
		if (m_party->getLeader() == player) {
			if (!m_party->isSharedExperienceActive()) {
				return SHIELD_YELLOW;
			}

			if (m_party->isSharedExperienceEnabled()) {
				return SHIELD_YELLOW_SHAREDEXP;
			}

			if (m_party->canUseSharedExperience(player)) {
				return SHIELD_YELLOW_NOSHAREDEXP;
			}

			return SHIELD_YELLOW_NOSHAREDEXP_BLINK;
		}

		if (m_party->isPlayerMember(player)) {
			if (!m_party->isSharedExperienceActive()) {
				return SHIELD_BLUE;
			}

			if (m_party->isSharedExperienceEnabled()) {
				return SHIELD_BLUE_SHAREDEXP;
			}

			if (m_party->canUseSharedExperience(player)) {
				return SHIELD_BLUE_NOSHAREDEXP;
			}

			return SHIELD_BLUE_NOSHAREDEXP_BLINK;
		}

		if (isInviting(player)) {
			return SHIELD_WHITEBLUE;
		}
	}

	if (player->isInviting(this)) {
		return SHIELD_WHITEYELLOW;
	}
	return SHIELD_NONE;
}

bool Player::isInviting(const Player* player) const
{
	if (!player || player->isRemoved() || !m_party || m_party->getLeader() != this) {
		return false;
	}

	return m_party->isPlayerInvited(player);
}

bool Player::isPartner(const Player* player) const
{
	return player && player->getParty() && player->getParty() == m_party;
}

bool Player::getHideHealth() const
{
	if (hasFlag(PlayerFlag_HideHealth)) {
		return true;
	}

	return m_hideHealth;
}

void Player::sendPlayerIcons(Player* player)
{
	sendCreatureShield(player);
	sendCreatureSkull(player);
}

bool Player::addPartyInvitation(Party* party)
{
	if (!party) {
		return false;
	}

	PartyList::iterator it = std::find(m_invitePartyList.begin(), m_invitePartyList.end(), party);
	if (it != m_invitePartyList.end()) {
		return false;
	}

	m_invitePartyList.push_back(party);
	return true;
}

bool Player::removePartyInvitation(Party* party)
{
	if (!party) {
		return false;
	}

	PartyList::iterator it = std::find(m_invitePartyList.begin(), m_invitePartyList.end(), party);
	if (it != m_invitePartyList.end()) {
		m_invitePartyList.erase(it);
		return true;
	}
	return false;
}

void Player::clearPartyInvitations()
{
	if (m_invitePartyList.empty()) {
		return;
	}

	PartyList list;
	for (PartyList::iterator it = m_invitePartyList.begin(); it != m_invitePartyList.end(); ++it) {
		list.push_back(*it);
	}

	m_invitePartyList.clear();
	for (PartyList::iterator it = list.begin(); it != list.end(); ++it) {
		(*it)->removeInvite(this);
	}
}

void Player::increaseCombatValues(int32_t& min, int32_t& max, bool useCharges, bool countWeapon)
{
	if (min > 0) {
		min *= m_vocation->formulaMultipliers[MULTIPLIER_HEALING];
	} else {
		min *= m_vocation->formulaMultipliers[MULTIPLIER_MAGIC];
	}

	if (max > 0) {
		max *= m_vocation->formulaMultipliers[MULTIPLIER_HEALING];
	} else {
		max *= m_vocation->formulaMultipliers[MULTIPLIER_MAGIC];
	}

	Item* item = nullptr;
	int32_t minValue = 0, maxValue = 0, i = SLOT_FIRST;
	for (; i <= SLOT_LAST; ++i) {
		if (!(item = getInventoryItem(static_cast<Slots_t>(i))) || item->isRemoved() || (g_moveEvents.hasEquipEvent(item) && !isItemAbilityEnabled(i))) {
			continue;
		}

		const ItemType& it = Item::items[item->getID()];
		if (!it.hasAbilities()) {
			continue;
		}

		if (min > 0) {
			minValue += it.abilities->increment[HEALING_VALUE];
			if (it.abilities->increment[HEALING_PERCENT]) {
				min = std::ceil((min * it.abilities->increment[HEALING_PERCENT]) / 100.);
			}
		} else {
			minValue -= it.abilities->increment[MAGIC_VALUE];
			if (it.abilities->increment[MAGIC_PERCENT]) {
				min = std::ceil((min * it.abilities->increment[MAGIC_PERCENT]) / 100.);
			}
		}

		if (max > 0) {
			maxValue += it.abilities->increment[HEALING_VALUE];
			if (it.abilities->increment[HEALING_PERCENT]) {
				max = std::ceil((max * it.abilities->increment[HEALING_PERCENT]) / 100.);
			}
		} else {
			maxValue -= it.abilities->increment[MAGIC_VALUE];
			if (it.abilities->increment[MAGIC_PERCENT]) {
				max = std::ceil((max * it.abilities->increment[MAGIC_PERCENT]) / 100.);
			}
		}

		bool removeCharges = false;
		for (int32_t j = INCREMENT_FIRST; j <= INCREMENT_LAST; ++j) {
			if (!it.abilities->increment[j]) {
				continue;
			}

			removeCharges = true;
			break;
		}

		if (useCharges && removeCharges && (countWeapon || item != getWeapon()) && item->hasCharges()) {
			g_game.transformItem(item, item->getID(), std::max<int32_t>(0, item->getCharges() - 1));
		}
	}

	min += minValue;
	max += maxValue;
}

bool Player::transferMoneyTo(const std::string& name, uint64_t amount)
{
	if (!otx::config::getBoolean(otx::config::BANK_SYSTEM) || amount > m_balance) {
		return false;
	}

	Player* target = g_game.getPlayerByNameEx(name);
	if (!target) {
		return false;
	}

	m_balance -= amount;
	target->m_balance += amount;
	if (target->isVirtual()) {
		IOLoginData::getInstance()->savePlayer(target);
		delete target;
	}
	return true;
}

bool Player::addOfflineTrainingTries(Skills_t skill, int32_t tries)
{
	if (tries <= 0 || skill == SKILL__LEVEL || skill == SKILL__EXPERIENCE) {
		return false;
	}

	uint32_t oldSkillValue, newSkillValue;
	double oldPercentToNextLevel, newPercentToNextLevel;

	std::ostringstream ss;
	if (skill == SKILL__MAGLEVEL) {
		uint64_t currReqMana = m_vocation->getReqMana(m_magLevel), nextReqMana = m_vocation->getReqMana(m_magLevel + 1);
		if (currReqMana >= nextReqMana) {
			return false;
		}

		oldSkillValue = m_magLevel;
		oldPercentToNextLevel = (m_manaSpent * 100.0) / nextReqMana;

		tries *= otx::config::getDouble(otx::config::RATE_MAGIC_OFFLINE);
		while ((m_manaSpent + tries) >= nextReqMana) {
			tries -= nextReqMana - m_manaSpent;
			m_manaSpent = 0;
			m_magLevel++;

			if (hasEventRegistered(CREATURE_EVENT_ADVANCE)) {
				for (CreatureEvent* it : getCreatureEvents(CREATURE_EVENT_ADVANCE)) {
					it->executeAdvance(this, SKILL__MAGLEVEL, (m_magLevel - 1), m_magLevel);
				}
			}

			currReqMana = nextReqMana;
			nextReqMana = m_vocation->getReqMana(m_magLevel + 1);
			if (currReqMana >= nextReqMana) {
				tries = 0;
				break;
			}
		}

		if (tries) {
			m_manaSpent += tries;
		}

		uint32_t newPercent;
		if (nextReqMana > currReqMana) {
			newPercent = Player::getPercentLevel(m_manaSpent, nextReqMana);
			newPercentToNextLevel = (m_manaSpent * 100.0) / nextReqMana;
		} else {
			newPercent = newPercentToNextLevel = 0;
		}

		if (newPercent != m_magLevelPercent) {
			m_magLevelPercent = newPercent;
			sendStats();
		}

		newSkillValue = m_magLevel;
		ss << "You advanced to magic level " << m_magLevel << ".";
		sendTextMessage(MSG_EVENT_ADVANCE, ss.str().c_str());
		ss.str("");
	} else {
		uint64_t currReqTries = m_vocation->getReqSkillTries(skill, m_skills[skill].level);
		uint64_t nextReqTries = m_vocation->getReqSkillTries(skill, m_skills[skill].level + 1);
		if (currReqTries >= nextReqTries) {
			return false;
		}

		oldSkillValue = m_skills[skill].level;
		oldPercentToNextLevel = (m_skills[skill].tries * 100.0) / nextReqTries;

		tries *= otx::config::getDouble(otx::config::RATE_SKILL_OFFLINE);
		while ((m_skills[skill].tries + tries) >= nextReqTries) {
			tries -= nextReqTries - m_skills[skill].tries;
			++m_skills[skill].level;
			m_skills[skill].tries = m_skills[skill].percent = 0;

			if (hasEventRegistered(CREATURE_EVENT_ADVANCE)) {
				for (CreatureEvent* it : getCreatureEvents(CREATURE_EVENT_ADVANCE)) {
					it->executeAdvance(this, skill, (m_skills[skill].level - 1), m_skills[skill].level);
				}
			}

			currReqTries = nextReqTries;
			nextReqTries = m_vocation->getReqSkillTries(skill, m_skills[skill].level + 1);
			if (currReqTries >= nextReqTries) {
				tries = 0;
				break;
			}
		}

		if (tries) {
			m_skills[skill].tries += tries;
		}

		uint32_t newPercent;
		if (nextReqTries > currReqTries) {
			newPercent = Player::getPercentLevel(m_skills[skill].tries, nextReqTries);
			newPercentToNextLevel = (m_skills[skill].tries * 100.0) / nextReqTries;
		} else {
			newPercent = newPercentToNextLevel = 0;
		}

		if (m_skills[skill].percent != newPercent) {
			m_skills[skill].percent = newPercent;
			sendStats();
		}

		newSkillValue = m_skills[skill].level;
		ss << "You advanced to " << getSkillName(skill) << " level " << m_skills[skill].level << ".";
		sendTextMessage(MSG_EVENT_ADVANCE, ss.str().c_str());
		ss.str("");
	}

	ss << std::fixed << std::setprecision(2) << "Your " << ucwords(getSkillName(skill)) << " skill changed from level " << oldSkillValue << " (with " << oldPercentToNextLevel << "% progress towards level " << (oldSkillValue + 1) << ") to level " << newSkillValue << " (with " << newPercentToNextLevel << "% progress towards level " << (newSkillValue + 1) << ")";
	sendTextMessage(MSG_EVENT_ADVANCE, ss.str().c_str());
	return true;
}

void Player::setPlayerExtraAttackSpeed(uint32_t speed)
{
	m_extraAttackSpeed = speed;
}
