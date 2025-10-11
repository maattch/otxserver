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

#include "container.h"
#include "creature.h"
#include "cylinder.h"
#include "depot.h"
#include "enums.h"
#include "group.h"
#include "outfit.h"
#include "scheduler.h"
#include "spectators.h"
#include "vocation.h"

class House;
class Weapon;
class Npc;
class Party;
class Quest;
class ProtocolGame;

enum FightMode_t : uint8_t
{
	FIGHTMODE_ATTACK,
	FIGHTMODE_BALANCED,
	FIGHTMODE_DEFENSE
};

enum TradeState_t
{
	TRADE_NONE,
	TRADE_INITIATED,
	TRADE_ACCEPT,
	TRADE_ACKNOWLEDGE,
	TRADE_TRANSFER
};

enum AccountManager_t
{
	MANAGER_NONE,
	MANAGER_NEW,
	MANAGER_ACCOUNT,
	MANAGER_NAMELOCK
};

enum GamemasterCondition_t
{
	GAMEMASTER_INVISIBLE = 0,
	GAMEMASTER_IGNORE = 1,
	GAMEMASTER_TELEPORT = 2
};

struct Skill
{
	uint64_t tries = 0;
	uint16_t level = 10;
	uint16_t percent = 0;
};

typedef std::set<uint32_t> VIPSet;
typedef std::list<std::pair<uint16_t, std::string>> ChannelsList;
typedef std::vector<std::pair<uint32_t, Container*>> ContainerVector;
typedef std::map<uint32_t, std::pair<Depot*, bool>> DepotMap;
typedef std::map<uint32_t, uint32_t> MuteCountMap;
typedef std::list<std::string> LearnedInstantSpellList;
typedef std::list<uint32_t> InvitationsList;
typedef std::list<Party*> PartyList;
typedef std::map<uint32_t, War_t> WarMap;

static constexpr int32_t PLAYER_MAX_SPEED = 1500;
static constexpr int32_t PLAYER_MIN_SPEED = 10;
static constexpr uint64_t STAMINA_MAX = 42 * 60 * 60000;

class Player final : public Creature, public Cylinder
{
public:
#if ENABLE_SERVER_DIAGNOSTIC > 0
	static uint32_t playerCount;
#endif
	Player(const std::string& name, ProtocolGame_ptr p);
	~Player();

	Player* getPlayer() override { return this; }
	const Player* getPlayer() const override { return this; }

	CreatureType_t getType() const override { return CREATURE_TYPE_PLAYER; }

	void setID() override {
		if (m_id == 0) {
			m_id = playerAutoID++;
		}
	}

	static MuteCountMap muteCountMap;

	const std::string& getName() const override { return m_name; }
	const std::string& getNameDescription() const override { return m_nameDescription; }
	void setNameDescription(const std::string& description) { m_nameDescription = description; }
	std::string getDescription(int32_t lookDistance) const override;

	const std::string& getSpecialDescription() const { return m_specialDescription; }
	void setSpecialDescription(const std::string& desc) { m_specialDescription = desc; }

	void manageAccount(const std::string& text);
	bool isAccountManager() const { return (m_accountManager != MANAGER_NONE); }
	AccountManager_t getAccountManagerType() const { return m_accountManager; }

	void kick(bool displayEffect, bool forceLogout);

	void setGUID(uint32_t _guid) { m_guid = _guid; }
	uint32_t getGUID() const { return m_guid; }

	void addList() override;
	void removeList() override;

	static uint64_t getExpForLevel(const uint64_t lv) {
		return (((lv - 6ULL) * lv + 17ULL) * lv - 12ULL) / 6ULL * 100ULL;
	}

	bool addOfflineTrainingTries(skills_t skill, int32_t tries);

	void addOfflineTrainingTime(int32_t addTime) { m_offlineTrainingTime = std::min(12 * 3600 * 1000, m_offlineTrainingTime + addTime); }
	void removeOfflineTrainingTime(int32_t removeTime) { m_offlineTrainingTime = std::max(0, m_offlineTrainingTime - removeTime); }
	int32_t getOfflineTrainingTime() const { return m_offlineTrainingTime; }

	int32_t getOfflineTrainingSkill() const { return m_offlineTrainingSkill; }
	void setOfflineTrainingSkill(int32_t skill) { m_offlineTrainingSkill = skill; }

	uint32_t getPromotionLevel() const { return m_promotionLevel; }
	void setPromotionLevel(uint32_t pLevel);

	bool changeOutfit(Outfit_t outfit, bool checkList);
	void hasRequestedOutfit(bool v) { m_requestedOutfit = v; }

	const Vocation* getVocation() const { return m_vocation; }

	void setParty(Party* party) { m_party = party; }
	Party* getParty() const { return m_party; }

	bool isInviting(const Player* player) const;
	bool isPartner(const Player* player) const;

	bool getHideHealth() const;

	bool addPartyInvitation(Party* party);
	bool removePartyInvitation(Party* party);
	void clearPartyInvitations();

	uint32_t getGuildId() const { return m_guildId; }
	void setGuildId(uint32_t newId) { m_guildId = newId; }
	uint32_t getRankId() const { return m_rankId; }
	void setRankId(uint32_t newId) { m_rankId = newId; }

	GuildLevel_t getGuildLevel() const { return m_guildLevel; }
	bool setGuildLevel(GuildLevel_t newLevel, uint32_t rank = 0);

	const std::string& getGuildName() const { return m_guildName; }
	void setGuildName(const std::string& newName) { m_guildName = newName; }
	const std::string& getRankName() const { return m_rankName; }
	void setRankName(const std::string& newName) { m_rankName = newName; }

	const std::string& getGuildNick() const { return m_guildNick; }
	void setGuildNick(const std::string& newNick) { m_guildNick = newNick; }

	bool isGuildInvited(uint32_t guildId) const;
	void leaveGuild();

	void setFlags(uint64_t flags)
	{
		if (m_group) {
			m_group->setFlags(flags);
		}
	}
	bool hasFlag(PlayerFlags value) const { return m_group != nullptr && m_group->hasFlag(value); }
	void setCustomFlags(uint64_t flags)
	{
		if (m_group) {
			m_group->setCustomFlags(flags);
		}
	}
	bool hasCustomFlag(PlayerCustomFlags value) const { return m_group != nullptr && m_group->hasCustomFlag(value); }

	void addBlessing(int16_t blessing) { m_blessings += blessing; }
	bool hasBlessing(int16_t blessing) const { return ((m_blessings & ((int16_t)1 << blessing)) != 0); }
	void setPVPBlessing(bool value) { m_pvpBlessing = value; }
	bool hasPVPBlessing() const { return m_pvpBlessing; }
	uint16_t getBlessings() const;

	bool isUsingOtclient() const { return m_operatingSystem >= CLIENTOS_OTCLIENT_LINUX; }
	OperatingSystem_t getOperatingSystem() const { return m_operatingSystem; }
	void setOperatingSystem(OperatingSystem_t os) { m_operatingSystem = os; }
	uint32_t getClientVersion() const { return m_clientVersion; }
	void setClientVersion(uint32_t version) { m_clientVersion = version; }

	bool hasClient() const { return m_client->getOwner() != nullptr; }
	ProtocolGame_ptr getClient() const {
		if (m_client) {
			return m_client->getOwner();
		}
		return nullptr;
	}

	bool isVirtual() const { return getID() == 0; }
	uint32_t getIP() const;
	bool canOpenCorpse(uint32_t ownerId);

	Container* getContainer(uint32_t cid);
	int32_t getContainerID(const Container* container) const;

	void addContainer(uint32_t cid, Container* container);
	void closeContainer(uint32_t cid);

	bool setStorage(const std::string& key, const std::string& value) override;
	void eraseStorage(const std::string& key) override;

	void generateReservedStorage();
	bool transferMoneyTo(const std::string& name, uint64_t amount);
	void increaseCombatValues(int32_t& min, int32_t& max, bool useCharges, bool countWeapon);

	void setGroupId(int32_t newId);
	int32_t getGroupId() const { return m_groupId; }
	void setGroup(Group* newGroup);
	Group* getGroup() const { return m_group; }

	bool isGhost() const override { return hasCondition(CONDITION_GAMEMASTER, GAMEMASTER_INVISIBLE) || hasFlag(PlayerFlag_CannotBeSeen); }
	bool isWalkable() const override { return hasCustomFlag(PlayerCustomFlag_IsWalkable); }

	void switchSaving() { m_saving = !m_saving; }
	bool isSaving() const { return m_saving; }

	uint32_t getIdleTime() const { return m_idleTime; }
	void setIdleTime(uint32_t amount) { m_idleTime = amount; }

	bool checkLoginDelay() const;
	bool isTrading() const { return (m_tradePartner != nullptr); }

	uint32_t getAccount() const { return m_accountId; }
	std::string getAccountName() const { return m_account; }
	uint16_t getAccess() const { return m_group ? m_group->getAccess() : 0; }
	uint16_t getGhostAccess() const { return m_group ? m_group->getGhostAccess() : 0; }

	uint32_t getLevel() const { return m_level; }
	uint8_t getLevelPercent() const { return m_levelPercent; }
	uint64_t getExperience() const { return m_experience; }
	uint32_t getMagicLevel() const { return std::max<int32_t>(0, m_magLevel + m_varStats[STAT_MAGICLEVEL]); }
	uint8_t getMagicLevelPercent() const { return m_magLevelPercent; }
	uint32_t getBaseMagicLevel() const { return m_magLevel; }
	uint64_t getSpentMana() const { return m_manaSpent; }

	uint32_t getExtraAttackSpeed() const { return m_extraAttackSpeed; }
	void setPlayerExtraAttackSpeed(uint32_t speed);

	float getDamageMultiplier() const { return m_damageMultiplier; }
	void setDamageMultiplier(float multiplier) { m_damageMultiplier = multiplier; }

	bool isPremium() const;
	int32_t getPremiumDays() const { return m_premiumDays; }
	void addPremiumDays(int32_t days);

	bool hasEnemy() const { return !m_warMap.empty(); }
	bool getEnemy(const Player* player, War_t& data) const;

	bool isEnemy(const Player* player, bool allies) const;
	bool isAlly(const Player* player) const;

	void addEnemy(uint32_t guild, War_t war)
	{
		m_warMap[guild] = war;
	}
	void removeEnemy(uint32_t guild) { m_warMap.erase(guild); }

	uint32_t getVocationId() const { return m_vocationId; }
	bool setVocation(uint32_t id);
	uint16_t getSex(bool full) const { return full ? m_sex : m_sex % 2; }
	void setSex(uint16_t);

	void setDropLoot(lootDrop_t _lootDrop) override;
	void setLossSkill(bool _skillLoss) override;

	uint64_t getStamina() const { return hasFlag(PlayerFlag_HasInfiniteStamina) ? STAMINA_MAX : m_stamina; }
	void setStamina(uint64_t value) { m_stamina = std::min<uint64_t>(value, STAMINA_MAX); }
	uint32_t getStaminaMinutes() const { return getStamina() / 60000; }
	void setStaminaMinutes(uint64_t value) { setStamina(value * 60000); }
	void useStamina(int64_t value) { m_stamina = std::clamp<int64_t>(m_stamina + value, 0, STAMINA_MAX); }
	uint64_t getSpentStamina() const { return STAMINA_MAX - m_stamina; }

	int64_t getLastLoad() const { return m_lastLoad; }
	int64_t getLastLogin() const { return m_lastLogin; }
	int64_t getLastLogout() const { return m_lastLogout; }

	const Position& getLoginPosition() const { return m_loginPosition; }
	void setLoginPosition(const Position& position) { m_loginPosition = position; }

	uint32_t getTown() const { return m_town; }
	void setTown(uint32_t _town) { m_town = _town; }

	bool isPushable() const override;
	int32_t getThrowRange() const override { return 1; }
	double getGainedExperience(Creature* attacker) const override;

	bool isMuted(uint16_t channelId, MessageClasses type, int32_t& time);
	void addMessageBuffer();
	void removeMessageBuffer();

	double getCapacity() const { return m_capacity; }
	void setCapacity(double newCapacity) { m_capacity = newCapacity; }
	double getFreeCapacity() const;

	int32_t getSoul() const { return std::max<int32_t>(0, m_soul + m_varStats[STAT_SOUL]); }
	int32_t getBaseSoul() const { return m_soul; }
	int32_t getSoulMax() const { return m_soulMax; }

	int32_t getMaxHealth() const override { return std::max<int32_t>(1, m_healthMax + m_varStats[STAT_MAXHEALTH]); }

	int32_t getMana() const override { return m_mana; }
	int32_t getMaxMana() const override { return std::max<int32_t>(0, m_manaMax + m_varStats[STAT_MAXMANA]); }
	int32_t getBaseMaxMana() const override { return m_manaMax; }

	Item* getInventoryItem(slots_t slot) const;
	Item* getEquippedItem(slots_t slot) const;

	bool isItemAbilityEnabled(uint8_t slot) const { return m_inventoryAbilities[slot]; }
	void setItemAbility(uint8_t slot, bool enabled) { m_inventoryAbilities[slot] = enabled; }

	uint16_t getBaseSkillLevel(uint8_t skill) const { return m_skills[skill].level; }
	uint16_t getSkillLevel(uint8_t skill) const { return std::clamp<int32_t>(m_skills[skill].level + m_varSkills[skill], 0, 0xFFFF); }
	uint64_t getSkillTries(uint8_t skill) const { return m_skills[skill].tries; }
	uint64_t getSkillPercent(uint8_t skill) const { return m_skills[skill].percent; }

	int32_t getVarSkill(uint8_t skill) const { return m_varSkills[skill]; }
	void setVarSkill(uint8_t skill, int32_t modifier) { m_varSkills[skill] += modifier; }

	int32_t getVarStats(stats_t stat) const { return m_varStats[stat]; }
	void setVarStats(stats_t stat, int32_t modifier);
	int32_t getDefaultStats(stats_t stat);

	void setConditionSuppressions(uint32_t conditions, bool remove);

	uint32_t getLossPercent(lossTypes_t lossType) const { return m_lossPercent[lossType]; }
	void setLossPercent(lossTypes_t lossType, uint32_t newPercent) { m_lossPercent[lossType] = newPercent; }

	Depot* getDepot(uint32_t depotId, bool autoCreateDepot);
	bool addDepot(Depot* depot, uint32_t depotId);
	void useDepot(uint32_t depotId, bool value);

	bool canSee(const Position& pos) const override;
	bool canSeeCreature(const Creature* creature) const override;
	bool canWalkthrough(const Creature* creature) const override;
	void setWalkthrough(const Creature* creature, bool walkthrough);

	bool canSeeInvisibility() const override { return hasFlag(PlayerFlag_CanSenseInvisibility); }

	bool hasSentChat() const { return m_sentChat; }
	void setSentChat(bool sending) { m_sentChat = sending; }

	bool getLoot() const { return m_showLoot; }
	void setGetLoot(bool b) { m_showLoot = b; }

	RaceType_t getRace() const override { return RACE_BLOOD; }

	// safe-trade functions
	void setTradeState(TradeState_t state) { m_tradeState = state; }
	TradeState_t getTradeState() const { return m_tradeState; }
	Item* getTradeItem() const { return m_tradeItem; }

	// shop functions
	void setShopOwner(Npc* owner, int32_t onBuy, int32_t onSell, ShopInfoList offer)
	{
		m_shopOwner = owner;
		m_purchaseCallback = onBuy;
		m_saleCallback = onSell;
		m_shopOffer = offer;
	}

	Npc* getShopOwner() const { return m_shopOwner; }
	Npc* getShopOwner(int32_t& onBuy, int32_t& onSell)
	{
		onBuy = m_purchaseCallback;
		onSell = m_saleCallback;
		return m_shopOwner;
	}

	const Npc* getShopOwner(int32_t& onBuy, int32_t& onSell) const
	{
		onBuy = m_purchaseCallback;
		onSell = m_saleCallback;
		return m_shopOwner;
	}

	// Quest functions
	void onUpdateQuest();

	// V.I.P. functions
	void notifyLogIn(Player* loginPlayer);
	void notifyLogOut(Player* logoutPlayer);
	bool removeVIP(uint32_t guid);
	bool addVIP(uint32_t guid, const std::string& name, bool online, bool loading = false);

	// follow functions
	bool setFollowCreature(Creature* creature, bool fullPathSearch = false) override;
	void goToFollowCreature() override;

	// follow events
	void onFollowCreature(const Creature* creature) override;

	// walk events
	void onWalk(Direction& dir) override;
	void onWalkAborted() override;
	void onWalkComplete() override;

	void openShopWindow(Npc* npc);
	void closeShopWindow(bool send = true);

	bool canBuyItem(uint16_t itemId, uint8_t subType);
	bool canSellItem(uint16_t itemId);

	bool getChaseMode() const { return m_chaseMode; }
	void setChaseMode(bool mode);
	FightMode_t getFightMode() const { return m_fightMode; }
	void setFightMode(FightMode_t mode) { m_fightMode = mode; }
	bool getSecureMode() const { return m_secureMode; }
	void setSecureMode(bool mode) { m_secureMode = mode; }

	// combat functions
	bool setAttackedCreature(Creature* creature) override;
	bool hasShield() const;

	bool isImmune(CombatType_t type) const;
	bool isImmune(ConditionType_t type) const;
	bool isProtected() const;
	bool isAttackable() const override;

	void changeHealth(int32_t healthChange) override;
	void changeMana(int32_t manaChange) override;
	void changeMaxMana(int32_t manaChange) override;

	void changeSoul(int32_t soulChange);

	bool isPzLocked() const { return m_pzLocked; }
	void setPzLocked(bool v) { m_pzLocked = v; }

	BlockType_t blockHit(Creature* attacker, CombatType_t combatType, int32_t& damage,
		bool checkDefense = false, bool checkArmor = false, bool reflect = true, bool field = false, bool element = false) override;

	void doAttacking(uint32_t interval) override;
	bool hasExtraSwing() override;
	void setLastAttack(uint64_t time) { m_lastAttack = time; }

	bool getAddAttackSkill() const { return m_addAttackSkillPoint; }
	BlockType_t getLastAttackBlockType() const { return m_lastAttackBlockType; }

	Item* getWeapon(slots_t slot, bool ignoreAmmo) const;
	Item* getWeapon(bool ignoreAmmo = false) const;

	WeaponType_t getWeaponType() override;
	int32_t getWeaponSkill(const Item* item) const;

	void drainHealth(Creature* attacker, CombatType_t combatType, int32_t damage) override;
	void drainMana(Creature* attacker, CombatType_t combatType, int32_t damage) override;

	void addExperience(uint64_t exp);
	void removeExperience(uint64_t exp, bool updateStats = true);
	void addManaSpent(uint64_t amount, bool useMultiplier = true);
	void addSkillAdvance(skills_t skill, uint64_t count, bool useMultiplier = true);
	bool addUnjustifiedKill(const Player* attacked, bool countNow);

	int32_t getArmor() const override;
	int32_t getCriticalHitChance() const;
	int32_t getDefense() const override;
	float getAttackFactor() const override;
	float getDefenseFactor() const override;

	void addCooldown(uint32_t ticks, uint16_t spellId);
	void addExhaust(uint32_t ticks, Exhaust_t exhaust);
	void addInFightTicks(bool pzLock, int32_t ticks = 0);
	void addDefaultRegeneration(uint32_t addTicks);

	// combat event functions
	void onAddCondition(ConditionType_t type, bool hadCondition) override;
	void onAddCombatCondition(ConditionType_t type, bool hadCondition) override;
	void onEndCondition(ConditionType_t type, ConditionId_t id) override;
	void onCombatRemoveCondition(const Creature* attacker, Condition* condition) override;
	void onTickCondition(ConditionType_t type, ConditionId_t id, int32_t interval, bool& _remove) override;
	void onTarget(Creature* target) override;
	void onSummonTarget(Creature* summon, Creature* target) override;
	void onAttacked() override;
	void onTargetDrain(Creature* target, int32_t points) override;
	void onSummonTargetDrain(Creature* summon, Creature* target, int32_t points) override;
	void onTargetGain(Creature* target, int32_t points) override;
	bool onKilledCreature(Creature* target, DeathEntry& entry) override;
	void onGainExperience(double& gainExp, Creature* target, bool multiplied) override;
	void onGainSharedExperience(double& gainExp, Creature* taraget, bool multiplied) override;
	void onTargetBlockHit(Creature* target, BlockType_t blockType) override;
	void onBlockHit(BlockType_t blockType) override;
	void onChangeZone(ZoneType_t zone) override;
	void onTargetChangeZone(ZoneType_t zone) override;
	void onIdleStatus() override;
	void onPlacedCreature() override;

	void getCreatureLight(LightInfo& light) const override;
	Skulls_t getSkull() const;

	Skulls_t getSkullType(const Creature* creature) const;
	PartyShields_t getPartyShield(const Creature* creature) const;
	GuildEmblems_t getGuildEmblem(const Creature* creature) const;

	bool hasAttacked(const Player* attacked) const;
	void addAttacked(const Player* attacked);
	void clearAttacked() { m_attackedSet.clear(); }

	time_t getSkullEnd() const { return m_skullEnd; }
	void setSkullEnd(time_t _time, bool login, Skulls_t _skull);

	bool addOutfit(uint32_t outfitId, uint32_t addons);
	bool removeOutfit(uint32_t outfitId, uint32_t addons);

	bool canWearOutfit(uint32_t outfitId, uint32_t addons);

	// tile
	// send methods
	void sendAddTileItem(const Tile* tile, const Position& pos, const Item* item)
	{
		if (m_client) {
			m_client->sendAddTileItem(tile, pos, tile->getClientIndexOfThing(this, item), item);
		}
	}
	void sendUpdateTileItem(const Tile* tile, const Position& pos, const Item* oldItem, const Item* newItem)
	{
		if (m_client) {
			m_client->sendUpdateTileItem(tile, pos, tile->getClientIndexOfThing(this, oldItem), newItem);
		}
	}
	void sendRemoveTileItem(const Tile* tile, const Position& pos, uint32_t stackpos, const Item*)
	{
		if (m_client) {
			m_client->sendRemoveTileItem(tile, pos, stackpos);
		}
	}
	void sendUpdateTile(const Tile* tile, const Position& pos)
	{
		if (m_client) {
			m_client->sendUpdateTile(tile, pos);
		}
	}

	void sendChannelMessage(std::string author, std::string text, MessageClasses type, uint16_t channel, bool fakeChat = false, uint32_t ip = 0)
	{
		if (m_client) {
			m_client->sendChannelMessage(author, text, type, channel, fakeChat, ip);
		}
	}
	void sendCreatureAppear(const Creature* creature)
	{
		if (m_client) {
			m_client->sendAddCreature(creature, creature->getPosition(), creature->getTile()->getClientIndexOfThing(this, creature));
		}
	}
	void sendCreatureAppear(const Creature* creature, ProtocolGame* target) const
	{
		if (target) {
			target->sendAddCreature(creature, creature->getPosition(), creature->getTile()->getClientIndexOfThing(this, creature));
		}
	}
	void sendCreatureDisappear(const Creature* creature, uint32_t stackpos)
	{
		if (m_client) {
			m_client->sendRemoveCreature(creature, creature->getPosition(), stackpos);
		}
	}
	void sendCreatureMove(const Creature* creature, const Tile* newTile, const Position& newPos,
		const Tile* oldTile, const Position& oldPos, uint32_t oldStackpos, bool teleport)
	{
		if (m_client) {
			m_client->sendMoveCreature(creature, newTile, newPos, newTile->getClientIndexOfThing(this, creature), oldTile, oldPos, oldStackpos, teleport);
		}
	}

	void sendCreatureTurn(const Creature* creature)
	{
		if (m_client) {
			m_client->sendCreatureTurn(creature, creature->getTile()->getClientIndexOfThing(this, creature));
		}
	}
	void sendCreatureSay(const Creature* creature, MessageClasses type, const std::string& text, Position* pos = nullptr, uint32_t statementId = 0, bool fakeChat = false)
	{
		if (m_client) {
			m_client->sendCreatureSay(creature, type, text, pos, statementId, fakeChat);
		}
	}
	void sendCreatureChannelSay(Creature* creature, MessageClasses type, const std::string& text, uint16_t channelId, uint32_t statementId = 0, bool fakeChat = false) const
	{
		if (m_client) {
			m_client->sendCreatureChannelSay(creature, type, text, channelId, statementId, fakeChat);
		}
	}
	void sendCreatureSquare(const Creature* creature, uint8_t color)
	{
		if (m_client) {
			m_client->sendCreatureSquare(creature, color);
		}
	}
	void sendCreatureChangeOutfit(const Creature* creature, const Outfit_t& outfit)
	{
		if (m_client) {
			m_client->sendCreatureOutfit(creature, outfit);
		}
	}
	void sendCreatureChangeVisible(const Creature* creature, Visible_t visible);
	void sendCreatureLight(const Creature* creature)
	{
		if (m_client) {
			m_client->sendCreatureLight(creature);
		}
	}
	void sendCreatureShield(const Creature* creature)
	{
		if (m_client) {
			m_client->sendCreatureShield(creature);
		}
	}
	void sendCreatureEmblem(const Creature* creature)
	{
		if (m_client) {
			m_client->sendCreatureEmblem(creature);
		}
	}
	void sendCreatureWalkthrough(const Creature* creature, bool walkthrough)
	{
		if (m_client) {
			m_client->sendCreatureWalkthrough(creature, walkthrough);
		}
	}

	void sendExtendedOpcode(uint8_t opcode, const std::string& buffer)
	{
		if (m_client) {
			m_client->sendExtendedOpcode(opcode, buffer);
		}
	}

	// container
	void sendAddContainerItem(const Container* container, const Item* item);
	void sendUpdateContainerItem(const Container* container, uint8_t slot, const Item* oldItem, const Item* newItem);
	void sendRemoveContainerItem(const Container* container, uint8_t slot, const Item* item);
	void sendContainer(uint32_t cid, const Container* container, bool hasParent)
	{
		if (m_client) {
			m_client->sendContainer(cid, container, hasParent);
		}
	}
	void sendContainers(ProtocolGame* target);

	// inventory
	void sendAddInventoryItem(slots_t slot, const Item* item)
	{
		if (m_client) {
			m_client->sendAddInventoryItem(slot, item);
		}
	}
	void sendUpdateInventoryItem(slots_t slot, const Item*, const Item* newItem)
	{
		if (m_client) {
			m_client->sendUpdateInventoryItem(slot, newItem);
		}
	}
	void sendRemoveInventoryItem(slots_t slot, const Item*)
	{
		if (m_client) {
			m_client->sendRemoveInventoryItem(slot);
		}
	}

	// event methods
	void onUpdateTileItem(const Tile* tile, const Position& pos, const Item* oldItem,
		const ItemType& oldType, const Item* newItem, const ItemType& newType) override;
	void onRemoveTileItem(const Tile* tile, const Position& pos,
		const ItemType& iType, const Item* item) override;

	void onCreatureAppear(const Creature* creature) override;
	void onCreatureDisappear(const Creature* creature, bool isLogout) override;
	void onCreatureMove(const Creature* creature, const Tile* newTile, const Position& newPos,
		const Tile* oldTile, const Position& oldPos, bool teleport) override;

	void onTargetDisappear(bool isLogout) override;
	void onFollowCreatureDisappear(bool isLogout) override;

	// cylinder implementations
	virtual Cylinder* getParent() { return Creature::getParent(); }
	virtual const Cylinder* getParent() const { return Creature::getParent(); }
	virtual bool isRemoved() const { return Creature::isRemoved(); }
	virtual Position getPosition() const { return Creature::getPosition(); }
	virtual Tile* getTile() { return Creature::getTile(); }
	virtual const Tile* getTile() const { return Creature::getTile(); }
	virtual Item* getItem() { return nullptr; }
	virtual const Item* getItem() const { return nullptr; }
	virtual Creature* getCreature() { return this; }
	virtual const Creature* getCreature() const { return this; }

	// container
	void onAddContainerItem(const Container* container, const Item* item);
	void onUpdateContainerItem(const Container* container, uint8_t slot,
		const Item* oldItem, const ItemType& oldType, const Item* newItem, const ItemType& newType);
	void onRemoveContainerItem(const Container* container, uint8_t slot, const Item* item);

	void onCloseContainer(const Container* container);
	void onSendContainer(const Container* container);
	void autoCloseContainers(const Container* container);

	// inventory
	void onAddInventoryItem(slots_t, Item*) {}
	void onUpdateInventoryItem(slots_t slot, Item* oldItem, const ItemType& oldType,
		Item* newItem, const ItemType& newType);
	void onRemoveInventoryItem(slots_t slot, Item* item);

	void sendCancel(const std::string& msg) const
	{
		if (m_client) {
			m_client->sendCancel(msg);
		}
	}
	void sendCancelMessage(ReturnValue message) const;
	void sendCancelTarget() const
	{
		if (m_client) {
			m_client->sendCancelTarget();
		}
	}
	void sendCancelWalk() const
	{
		if (m_client) {
			m_client->sendCancelWalk();
		}
	}
	void sendChangeSpeed(const Creature* creature, uint32_t newSpeed) const
	{
		if (m_client) {
			m_client->sendChangeSpeed(creature, newSpeed);
		}
	}
	void sendCreatureHealth(const Creature* creature) const
	{
		if (m_client) {
			m_client->sendCreatureHealth(creature);
		}
	}

	void sendDistanceShoot(const Position& from, const Position& to, uint16_t type) const
	{
		if (m_client) {
			m_client->sendDistanceShoot(from, to, type);
		}
	}

	void sendHouseWindow(House* house, uint32_t listId) const;
	void sendOutfitWindow() const
	{
		if (m_client) {
			m_client->sendOutfitWindow();
		}
	}
	void sendQuests() const
	{
		if (m_client) {
			m_client->sendQuests();
		}
	}
	void sendQuestInfo(Quest* quest) const
	{
		if (m_client) {
			m_client->sendQuestInfo(quest);
		}
	}
	void sendCreatureSkull(const Creature* creature) const
	{
		if (m_client) {
			m_client->sendCreatureSkull(creature);
		}
	}
	void sendFYIBox(std::string message)
	{
		if (m_client) {
			m_client->sendFYIBox(message);
		}
	}
	void sendCreatePrivateChannel(uint16_t channelId, const std::string& channelName)
	{
		if (m_client) {
			m_client->sendCreatePrivateChannel(channelId, channelName);
		}
	}
	void sendClosePrivate(uint16_t channelId) const
	{
		if (m_client) {
			m_client->sendClosePrivate(channelId);
		}
	}
	void sendIcons() const;

	void sendMagicEffect(const Position& pos, uint16_t type) const
	{
		if (m_client) {
			m_client->sendMagicEffect(pos, type);
		}
	}

	void sendAnimatedText(const Position& pos, uint8_t color, const std::string& text) const
	{
		if (m_client) {
			m_client->sendAnimatedText(pos, color, text);
		}
	}
	void sendSkills() const
	{
		if (m_client) {
			m_client->sendSkills();
		}
	}
	void sendTextMessage(MessageClasses type, const std::string& message) const
	{
		if (m_client) {
			m_client->sendTextMessage(type, message);
		}
	}
	void sendStatsMessage(MessageClasses type, const std::string& message, Position pos, MessageDetails* details = nullptr) const
	{
		if (m_client) {
			m_client->sendStatsMessage(type, message, pos, details);
		}
	}
	void sendReLoginWindow() const
	{
		if (m_client) {
			m_client->sendReLoginWindow();
		}
	}
	void sendTextWindow(Item* item, uint16_t maxLen, bool canWrite) const
	{
		if (m_client) {
			m_client->sendTextWindow(m_windowTextId, item, maxLen, canWrite);
		}
	}
	void sendShop(Npc* npc) const
	{
		if (m_client) {
			m_client->sendShop(npc, m_shopOffer);
		}
	}
	void sendGoods() const
	{
		if (m_client) {
			m_client->sendGoods(m_shopOffer);
		}
	}
	void sendCloseShop() const
	{
		if (m_client) {
			m_client->sendCloseShop();
		}
	}
	void sendTradeItemRequest(const Player* player, const Item* item, bool ack) const
	{
		if (m_client) {
			m_client->sendTradeItemRequest(player, item, ack);
		}
	}
	void sendTradeClose() const
	{
		if (m_client) {
			m_client->sendCloseTrade();
		}
	}
	void sendWorldLight(LightInfo& lightInfo)
	{
		if (m_client) {
			m_client->sendWorldLight(lightInfo);
		}
	}
	void sendChannelsDialog(const ChannelsList& channels)
	{
		if (m_client) {
			m_client->sendChannelsDialog(channels);
		}
	}
	void sendOpenPrivateChannel(const std::string& receiver)
	{
		if (m_client) {
			m_client->sendOpenPrivateChannel(receiver);
		}
	}
	void sendOutfitWindow()
	{
		if (m_client) {
			m_client->sendOutfitWindow();
		}
	}
	void sendCloseContainer(uint32_t cid)
	{
		if (m_client) {
			m_client->sendCloseContainer(cid);
		}
	}
	void sendChannel(uint16_t channelId, const std::string& channelName)
	{
		if (m_client) {
			m_client->sendChannel(channelId, channelName);
		}
	}
	void sendTutorial(uint8_t tutorialId)
	{
		if (m_client) {
			m_client->sendTutorial(tutorialId);
		}
	}
	void sendAddMarker(const Position& pos, MapMarks_t markType, const std::string& desc)
	{
		if (m_client) {
			m_client->sendAddMarker(pos, markType, desc);
		}
	}
	void sendRuleViolationsChannel(uint16_t channelId)
	{
		if (m_client) {
			m_client->sendRuleViolationsChannel(channelId);
		}
	}
	void sendRemoveReport(const std::string& name)
	{
		if (m_client) {
			m_client->sendRemoveReport(name);
		}
	}
	void sendLockRuleViolation()
	{
		if (m_client) {
			m_client->sendLockRuleViolation();
		}
	}
	void sendRuleViolationCancel(const std::string& name)
	{
		if (m_client) {
			m_client->sendRuleViolationCancel(name);
		}
	}

	void sendCastList()
	{
		if (m_client) {
			m_client->sendCastList();
		}
	}

	// sendProgressbar OTCv8 features
	void sendProgressbar(const Creature* creature, uint32_t duration, bool ltr)
	{
		if (m_client) {
			m_client->sendProgressbar(creature, duration, ltr);
		}
	}

	void sendCritical() const;
	void sendPlayerIcons(Player* player);
	void sendStats();

	void receivePing();
	void onThink(uint32_t interval) override;
	uint32_t getAttackSpeed() const;

	void setLastMail(uint64_t v) { m_lastMail = v; }
	uint16_t getMailAttempts() const { return m_mailAttempts; }
	void addMailAttempt() { ++m_mailAttempts; }

	void postAddNotification(Creature* actor, Thing* thing, const Cylinder* oldParent,
		int32_t index, CylinderLink_t link = LINK_OWNER) override;
	void postRemoveNotification(Creature* actor, Thing* thing, const Cylinder* newParent,
		int32_t index, bool isCompleteRemoval, CylinderLink_t link = LINK_OWNER) override;

	void setNextAction(int64_t time) {
		if (time > m_nextAction) {
			m_nextAction = time;
		}
	}
	bool canDoAction() const;
	uint32_t getNextActionTime(bool scheduler = true) const;

	void setNextExAction(int64_t time) {
		if (time > m_nextExAction) {
			m_nextExAction = time;
		}
	}
	bool canDoExAction() const;

	Item* getWriteItem(uint32_t& _windowTextId, uint16_t& _maxWriteLen);
	void setWriteItem(Item* item, uint16_t _maxLen = 0);

	House* getEditHouse(uint32_t& _windowTextId, uint32_t& _listId);
	void setEditHouse(House* house, uint32_t listId = 0);

	void learnInstantSpell(const std::string& name);
	void unlearnInstantSpell(const std::string& name);
	bool hasLearnedInstantSpell(const std::string& name) const;

	Spectators* getSpectators() const { return m_client; }

	Thing* __getThing(uint32_t index) const override;
	uint32_t __getItemTypeCount(uint16_t itemId, int32_t subType = -1) const override;
	std::map<uint32_t, uint32_t>& __getAllItemTypeCount(std::map<uint32_t, uint32_t>& countMap) const override;

	VIPSet m_VIPList;
	ContainerVector m_containerVec;
	InvitationsList m_invitationsList;
	ConditionList m_storedConditionList;
	DepotMap m_depots;
	Container m_transferContainer;

	// TODO: make it private?
	uint32_t m_marriage = 0;
	uint64_t m_balance = 0;
	double m_rates[SKILL__LAST + 1] = { 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 };

private:
	void checkTradeState(const Item* item);
	void internalAddDepot(Depot* depot, uint32_t depotId);

	bool gainExperience(double& gainExp, Creature* target);
	bool rateExperience(double& gainExp, Creature* target);

	void updateInventoryWeight();
	void updateInventoryGoods(uint32_t itemId);
	void updateItemsLight(bool internal = false);
	void updateBaseSpeed() {
		if (!hasFlag(PlayerFlag_SetMaxSpeed)) {
			m_baseSpeed = m_vocation->baseSpeed + (2 * (m_level - 1));
		} else {
			m_baseSpeed = PLAYER_MAX_SPEED;
		}
	}

	void setNextWalkActionTask(SchedulerTaskPtr task);
	void setNextActionTask(SchedulerTaskPtr task);

	bool onDeath() override;
	Item* createCorpse(DeathList deathList) override;

	void dropCorpse(DeathList deathList) override;
	void dropLoot(Container* corpse) override;

	// cylinder implementations
	virtual ReturnValue __queryAdd(int32_t index, const Thing* thing, uint32_t count,
		uint32_t flags, Creature* actor = nullptr) const;
	virtual ReturnValue __queryMaxCount(int32_t index, const Thing* thing, uint32_t count, uint32_t& maxQueryCount,
		uint32_t flags) const;
	virtual ReturnValue __queryRemove(const Thing* thing, uint32_t count, uint32_t flags, Creature* actor = nullptr) const;
	virtual Cylinder* __queryDestination(int32_t& index, const Thing* thing, Item** destItem,
		uint32_t& flags);

	virtual void __addThing(Creature* actor, Thing* thing);
	virtual void __addThing(Creature* actor, int32_t index, Thing* thing);

	virtual void __updateThing(Thing* thing, uint16_t itemId, uint32_t count);
	virtual void __replaceThing(uint32_t index, Thing* thing);

	virtual void __removeThing(Thing* thing, uint32_t count);

	virtual int32_t __getIndexOfThing(const Thing* thing) const;
	virtual int32_t __getFirstIndex() const;
	virtual int32_t __getLastIndex() const;

	virtual void __internalAddThing(Thing* thing);
	virtual void __internalAddThing(uint32_t index, Thing* thing);

	uint32_t getVocAttackSpeed() const { return m_vocation->attackSpeed - getPlayer()->getExtraAttackSpeed(); }
	int32_t getStepSpeed() const override {
		return std::max<int32_t>(PLAYER_MIN_SPEED, std::min<int32_t>(PLAYER_MAX_SPEED, getSpeed()));
	}

	uint32_t getDamageImmunities() const override { return m_damageImmunities; }
	uint32_t getConditionImmunities() const override { return m_conditionImmunities; }
	uint32_t getConditionSuppressions() const override { return m_conditionSuppressions; }

	uint16_t getLookCorpse() const override;
	uint64_t getLostExperience() const override;

	void getPathSearchParams(const Creature* creature, FindPathParams& fpp) const override;
	static uint16_t getPercentLevel(uint64_t count, uint64_t nextLevelCount);

	bool isPromoted(uint32_t pLevel = 1) const { return m_promotionLevel >= pLevel; }
	bool hasCapacity(const Item* item, uint32_t count) const;

	static uint32_t playerAutoID;

	char m_managerChar[100];

	Skill m_skills[SKILL_LAST + 1];

	std::string m_managerString;
	std::string m_managerString2;
	std::string m_account;
	std::string m_password;
	std::string m_name;
	std::string m_nameDescription;
	std::string m_specialDescription;
	std::string m_guildName;
	std::string m_rankName;
	std::string m_guildNick;

	std::vector<uint32_t> m_forceWalkthrough;
	std::set<uint32_t> m_attackedSet;
	ShopInfoList m_shopOffer;
	LearnedInstantSpellList m_learnedInstantSpellList;
	PartyList m_invitePartyList;
	OutfitMap m_outfits;
	WarMap m_warMap;

	LightInfo m_itemsLight;

	SchedulerTaskPtr m_walkTask;
	const Vocation* m_vocation = nullptr;
	Spectators* m_client = nullptr;
	Party* m_party = nullptr;
	Group* m_group = nullptr;
	Player* m_tradePartner = nullptr;
	Item* m_tradeItem = nullptr;
	Item* m_writeItem = nullptr;
	House* m_editHouse = nullptr;
	Npc* m_shopOwner = nullptr;
	Item* m_inventory[SLOT_LAST] = {};

	double m_inventoryWeight = 0.0;
	double m_capacity = 400.0;

	uint64_t m_stamina = STAMINA_MAX;
	uint64_t m_experience = 0;
	uint64_t m_manaSpent = 0;

	int64_t m_lastLoad = 0;
	int64_t m_lastPong = 0;
	int64_t m_lastPing = 0;
	int64_t m_nextAction = 0;
	int64_t m_nextExAction = 0;
	int64_t m_lastAttack = 0;
	int64_t m_lastMail = 0;
	int64_t m_skullEnd = 0;
	int64_t m_lastLogin = 0;
	int64_t m_lastLogout = 0;

	Position m_loginPosition;

	uint32_t m_clientVersion = 0;
	uint32_t m_messageTicks = 0;
	uint32_t m_idleTime = 0;
	uint32_t m_extraAttackSpeed = 0;
	uint32_t m_accountId = 0;
	uint32_t m_lastIP = 0;
	uint32_t m_level = 1;
	uint32_t m_levelPercent = 0;
	uint32_t m_magLevel = 0;
	uint32_t m_magLevelPercent = 0;
	uint32_t m_damageImmunities = 0;
	uint32_t m_conditionImmunities = 0;
	uint32_t m_conditionSuppressions = 0;
	uint32_t m_actionTaskEvent = 0;
	uint32_t m_walkTaskEvent = 0;
	uint32_t m_guid = 0;
	uint32_t m_editListId = 0;
	uint32_t m_windowTextId = 0;
	uint32_t m_guildId = 0;
	uint32_t m_rankId = 0;
	uint32_t m_promotionLevel = 0;
	uint32_t m_town = 0;
	uint32_t m_lossPercent[LOSS_LAST + 1] = { 100, 100, 100, 100, 100 };

	int32_t m_mana = 0;
	int32_t m_manaMax = 0;
	int32_t m_premiumDays = 0;
	int32_t m_soul = 0;
	int32_t m_soulMax = 100;
	int32_t m_vocationId = 0;
	int32_t m_groupId = 0;
	int32_t m_managerNumber = 0;
	int32_t managerNumber2 = 0;
	int32_t m_purchaseCallback = -1;
	int32_t m_saleCallback = -1;
	int32_t m_messageBuffer = 0;
	int32_t m_bloodHitCount = 0;
	int32_t m_shieldBlockCount = 0;
	int32_t m_offlineTrainingSkill = -1;
	int32_t m_offlineTrainingTime = 0;
	int32_t m_varSkills[SKILL_LAST + 1] = {};
	int32_t m_varStats[STAT_LAST + 1] = {};

	float m_damageMultiplier = 1.f;

	uint16_t m_maxWriteLen = 0;
	uint16_t m_sex = 0;
	uint16_t m_mailAttempts = 0;
	uint16_t m_lastStatsTrainingTime = 0;

	int16_t m_blessings = 0;

	OperatingSystem_t m_operatingSystem = CLIENTOS_WINDOWS;
	AccountManager_t m_accountManager = MANAGER_NONE;
	PlayerSex_t m_managerSex = PLAYERSEX_MALE;
	BlockType_t m_lastAttackBlockType = BLOCK_NONE;
	FightMode_t m_fightMode = FIGHTMODE_ATTACK;
	TradeState_t m_tradeState = TRADE_NONE;
	GuildLevel_t m_guildLevel = GUILDLEVEL_NONE;

	bool m_pzLocked = false;
	bool m_saving = true;
	bool m_isConnecting = false;
	bool m_requestedOutfit = false;
	bool m_outfitAttributes = false;
	bool m_addAttackSkillPoint = false;
	bool m_pvpBlessing = false;
	bool m_sentChat = false;
	bool m_showLoot = false;
	bool m_chaseMode = false;
	bool m_secureMode = true;
	bool m_inventoryAbilities[SLOT_LAST] = {};
	bool m_talkState[13] = {};

	friend class Game;
	friend class LuaInterface;
	friend class Npc;
	friend class Actions;
	friend class IOLoginData;
	friend class ProtocolGame;
	friend class ProtocolLogin;
	friend class Spectators;
};
