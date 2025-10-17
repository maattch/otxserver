---@meta

--------------------------------------
-------------- Game ------------------
--------------------------------------

---@param name string
---@return boolean
---@nodiscard
function existMonsterByName(name) end

---@param name string
---@return SpellInfoTable
---@nodiscard
function getInstantSpellInfo(name) end

---@return string[]
---@nodiscard
function getStorageList() end

---@param key string
---@return integer | string
---@nodiscard
function getStorage(key) end

---@param key string
---@param value? string|integer
---@return boolean
function doSetStorage(key, value) end

---@param channelId integer
---@return integer[]?
---@nodiscard
function getChannelUsers(channelId) end

---@return integer[]
---@nodiscard
function getPlayersOnline() end

---@param position Position
---@return TileInfoTable?
---@nodiscard
function getTileInfo(position) end

--- stackpos = 255 (top creature or top item, in that order))
---
--- stackpos = 254 (magic field)
---
--- stackpos = 253 (top creature)
---
--- stackpos = -1 (things count)
---@param position Position
---@return ThingTable | integer | nil
---@nodiscard
function getThingFromPosition(position) end

---@param uid integer
---@param position Position
---@param flags? integer 0
---@param displayError? boolean true
---@return ReturnValue
---@nodiscard
function doTileQueryAdd(uid, position, flags, displayError) end

---@param position Position
---@param itemId integer
---@param subType? integer -1
---@return ThingTable
---@nodiscard
function getTileItemById(position, itemId, subType) end

---@param position Position
---@param itemType integer
---@return ThingTable
---@nodiscard
function getTileItemByType(position, itemType) end

--- stackpos = -1 (things count)
---@param position Position
---@return ThingTable
---@nodiscard
function getTileThingByPos(position) end

---@param position Position
---@return ThingTable
---@nodiscard
function getTopCreature(position) end

---@param fromPosition Position
---@param toPosition Position
---@param fromIsCreature boolean false
---@param toIsCreature boolean false
---@return string
---@nodiscard
function getSearchString(fromPosition, toPosition, fromIsCreature, toIsCreature) end

---@param position Position
---@param effectId integer
---@param playerID? integer 0
---@return boolean
function doSendMagicEffect(position, effectId, playerID) end

---@param fromPosition Position
---@param toPosition Position
---@param effectId integer
---@param playerID? integer 0
---@return boolean
function doSendDistanceShoot(fromPosition, toPosition, effectId, playerID) end

---@param position Position
---@param text string
---@param color integer
---@param playerID? integer 0
---@return boolean
function doSendAnimatedText(position, text, color, playerID) end

---@param name string
---@return integer?
---@nodiscard
function getCreatureByName(name) end

---@param itemId integer
---@param count integer
---@param position Position
---@return integer?
function doCreateItem(itemId, count, position) end

---@param itemId integer
---@param count integer
---@return integer
---@nodiscard
function doCreateItemEx(itemId, count) end

---@param position Position
---@param uid integer
---@return ReturnValue | boolean | nil
function doTileAddItemEx(position, uid) end

---@param fromPosition Position
---@param toPosition Position
---@param creatures? boolean true
---@param unmovable? boolean true
---@return boolean
function doRelocate(fromPosition, toPosition, creatures, unmovable) end

---@param position Position
---@param forceMapLoaded? boolean false
---@return boolean
function doCleanTile(position, forceMapLoaded) end

---@param itemId integer
---@param destination Position
---@param position Position
---@return integer?
function doCreateTeleport(itemId, destination, position) end

---@param name string
---@param position Position
---@param extended? boolean false
---@param force? boolean false
---@return integer?
function doCreateMonster(name, position, extended, force) end

---@param name string
---@param position Position
---@return integer?
function doCreateNpc(name, position) end

---@param name string
---@return MonsterInfoTable?
---@nodiscard
function getMonsterInfo(name) end

---@param lookType integer
---@return integer
---@nodiscard
function getOutfitIdByLooktype(lookType) end

---@param houseId integer
---@param full? boolean true
---@return HouseInfoTable?
---@nodiscard
function getHouseInfo(houseId, full) end

---@param houseId integer
---@param listId integer
---@return string?
---@nodiscard
function getHouseAccessList(houseId, listId) end

---@param guid integer
---@return integer?
---@nodiscard
function getHouseByPlayerGUID(guid) end

---@param position Position
---@return integer?
---@nodiscard
function getHouseFromPosition(position) end

---@param houseId integer
---@param listId integer
---@param listText string
---@return boolean
function setHouseAccessList(houseId, listId, listText) end

---@param houseId integer
---@param owner integer
---@param clean? boolean true
---@return boolean
function setHouseOwner(houseId, owner, clean) end

---@return integer
---@nodiscard
function getWorldType() end

---@param worldType integer
---@return boolean
function setWorldType(worldType) end

---@return integer
---@nodiscard
function getWorldTime() end

---@return integer level, integer color
---@nodiscard
function getWorldLight() end

---@param type integer
---@return integer
---@nodiscard
function getWorldCreatures(type) end

---@return integer
---@nodiscard
function getWorldUpTime() end

---@param guid integer
---@return integer?
---@nodiscard
function aaaaaaaaaaaaa(guid) end

---@param guildName string
---@return integer?
---@nodiscard
function getGuildId(guildName) end

---@param guildId integer
---@return string?
---@nodiscard
function getGuildMotd(guildId) end

---@param cid integer
---@param combatType integer
---@param position Position
---@param area integer
---@param min integer
---@param max integer
---@param effectId integer
---@param aggressive? boolean true
---@return boolean
function doCombatAreaHealth(cid, combatType, position, area, min, max, effectId, aggressive) end

---@param cid integer
---@param target integer
---@param combatType integer
---@param min integer
---@param max integer
---@param effectId integer
---@param aggressive? boolean true
---@return boolean
function doTargetCombatHealth(cid, target, combatType, min, max, effectId, aggressive) end

---@param cid integer
---@param position Position
---@param area integer
---@param min integer
---@param max integer
---@param effectId integer
---@param aggressive? boolean true
---@return boolean
function doCombatAreaMana(cid, position, area, min, max, effectId, aggressive) end

---@param cid integer
---@param target integer
---@param min integer
---@param max integer
---@param effectId integer
---@param aggressive? boolean true
---@return boolean
function doTargetCombatMana(cid, target, min, max, effectId, aggressive) end

---@param cid integer
---@param position Position
---@param areaId integer
---@param conditionObjId integer
---@param effectId integer
---@return boolean
function doCombatAreaCondition(cid, position, areaId, conditionObjId, effectId) end

---@param cid integer
---@param target integer
---@param conditionObjId integer
---@param effectId integer
---@return boolean
function doTargetCombatCondition(cid, target, conditionObjId, effectId) end

---@param cid integer
---@param position Position
---@param area integer
---@param dispelType integer
---@param effectId integer
---@return boolean
function doCombatAreaDispel(cid, position, area, dispelType, effectId) end

---@param cid integer
---@param target integer
---@param dispelType integer
---@param effectId integer
---@return boolean
function doTargetCombatDispel(cid, target, dispelType, effectId) end

---@param fromPosition Position
---@param toPosition Position
---@param floorCheck boolean
---@return boolean
---@nodiscard
function isSightClear(fromPosition, toPosition, floorCheck) end

---@param callback function
---@param delay integer
---@return integer
function addEvent(callback, delay, ...) end

---@param eventId integer
---@return boolean
function stopEvent(eventId) end

---@param name string
---@return integer
---@nodiscard
function getAccountIdByName(name) end

---@param name string
---@return string
---@nodiscard
function getAccountByName(name) end

---@param accountName string
---@return integer
---@nodiscard
function getAccountIdByAccount(accountName) end

---@param accountId integer
---@return string
---@nodiscard
function getAccountByAccountId(accountId) end

---@param nameOrId string | integer
---@param flag integer
---@return boolean
---@nodiscard
function getAccountFlagValue(nameOrId, flag) end

---@param nameOrId string | integer
---@param flag integer
---@return boolean
---@nodiscard
function getAccountCustomFlagValue(nameOrId, flag) end

---@param name string
---@return integer?
---@nodiscard
function getIpByName(name) end

---@param ip integer
---@param mask? integer 0xFFFFFFFF
---@return integer[]
---@nodiscard
function getPlayersByIp(ip, mask) end

---@param townName string
---@return integer?
---@nodiscard
function getTownId(townName) end

---@param townId integer
---@return string?
---@nodiscard
function getTownName(townId) end

---@param townId integer
---@return Position?
---@nodiscard
function getTownTemplePosition(townId) end

---@param townId? integer 0
---@return integer[]
---@nodiscard
function getTownHouses(townId) end

---@param centerPos Position
---@param rangeX integer
---@param rangeY integer
---@param multiFloor? boolean false
---@param onlyPlayers? boolean false
---@return integer[]?
---@nodiscard
function getSpectators(centerPos, rangeX, rangeY, multiFloor, onlyPlayers) end

---@param vocationId integer
---@return VocationInfoTable?
---@nodiscard
function getVocationInfo(vocationId) end

---@param groupId integer
---@param premium boolean false
---@return GroupInfoTable?
---@nodiscard
function getGroupInfo(groupId, premium) end

---@return { id: integer, name: string }[]
---@nodiscard
function getVocationList() end

---@return { id: integer, name: string }[]
---@nodiscard
function getGroupList() end

---@return { id: integer, name: string, flags: integer, level: integer, access: integer }[]
---@nodiscard
function getChannelList() end

---@return { id: integer, name: string }[]
---@nodiscard
function getTownList() end

---@return { name: string, pos: Position }[]
---@nodiscard
function getWaypointList() end

---@return { words: string, access: integer, log: boolean, access: integer }[]
---@nodiscard
function getTalkactionList() end

---@return { level: integer, multiplier: number }[]?
---@nodiscard
function getExperienceStageList() end

---@param name string
---@return integer?
---@nodiscard
function getItemIdByName(name) end

---@param itemID integer
---@return ItemInfoTable?
---@nodiscard
function getItemInfo(itemID) end

---@param ip integer
---@param mask? integer 0xFFFFFFFF
---@return boolean
---@nodiscard
function isIpBanished(ip, mask) end

---@param nameOrGUID integer | string
---@param banType integer
---@return boolean
---@nodiscard
function isPlayerBanished(nameOrGUID, banType) end

---@param accountId integer
---@param playerId? integer 0
---@return boolean
---@nodiscard
function isAccountBanished(accountId, playerId) end

---@param ip integer
---@param mask? integer 0xFFFFFFFF
---@param length? integer config.ipBanLength
---@param reason? integer 21
---@param comment? string
---@param admin? integer 0
---@param statement? string
---@return boolean
function doAddIpBanishment(ip, mask, length, reason, comment, admin, statement) end

---@param nameOrGUID integer | string
---@param banType? integer PLAYERBAN_LOCK
---@param length? integer -1
---@param reason? integer 21
---@param action? integer ACTION_NAMELOCK
---@param comment? string
---@param admin? integer 0
---@param statement? string
---@return boolean
function doAddPlayerBanishment(nameOrGUID, banType, length, reason, action, comment, admin, statement) end

---@param accountId integer
---@param playerId? integer 0
---@param length? integer config.banLength
---@param reason? integer 21
---@param action? integer ACTION_BANISHMENT
---@param comment? string
---@param admin? integer 0
---@param statement? string
---@return boolean
function doAddAccountBanishment(accountId, playerId, length, reason, action, comment, admin, statement) end

---@param accountId integer
---@param warnings? integer 1
---@return boolean
function doAddAccountWarnings(accountId, warnings) end

---@param accountId integer
---@return integer
---@nodiscard
function getAccountWarnings(accountId) end

---@param accountId integer
---@param playerId? integer 0
---@param reason? integer 21
---@param comment? string
---@param admin? integer 0
---@param statement? string
---@return boolean
function doAddNotation(accountId, playerId, reason, comment, admin, statement) end

---@param nameOrGUID integer | string
---@param channelId? integer -1
---@param reason? integer 21
---@param comment? string
---@param admin? integer 0
---@param statement? string
---@return boolean
function doAddStatement(nameOrGUID, channelId, reason, comment, admin, statement) end

---@param ip integer
---@param mask? integer 0xFFFFFFFF
---@return boolean
function doRemoveIpBanishment(ip, mask) end

---@param nameOrGUID integer | string
---@param banType integer
---@return boolean
function doRemovePlayerBanishment(nameOrGUID, banType) end

---@param accountId integer
---@param playerId? integer 0
---@return boolean
function doRemoveAccountBanishment(accountId, playerId) end

---@param accountId integer
---@param playerId? integer 0
---@return boolean
function doRemoveNotations(accountId, playerId) end

---@param accountId integer
---@param playerId? integer 0
---@return integer
---@nodiscard
function getNotationsCount(accountId, playerId) end

---@param value integer
---@param banType? integer BAN_NONE
---@param param? integer 0
---@return BanDataTable?
---@nodiscard
function getBanData(value, banType, param) end

---@param banType integer
---@param value? integer 0
---@param param? integer 0
---@return BanDataTable[]
---@nodiscard
function getBanList(banType, value, param) end

---@param level integer
---@param divider? number 1.0
---@return number
---@nodiscard
function getExperienceStage(level, divider) end

---@return string
---@nodiscard
function getDataDir() end

---@return string
---@nodiscard
function getLogsDir() end

---@return string
---@nodiscard
function getConfigFile() end

---@param key string
---@return any
---@nodiscard
function getConfigValue(key) end

---@param skillID integer
---@return string
---@nodiscard
function getHighscoreString(skillID) end

---@param name string
---@return Position?
---@nodiscard
function getWaypointPosition(name) end

---@param name string
---@param position Position
---@return boolean
function doWaypointAddTemporial(name, position) end

---@return integer
---@nodiscard
function getGameState() end

---@param id integer
---@return boolean
function doSetGameState(id) end

---@param name string
---@return boolean
function doExecuteRaid(name) end

---@param id integer
---@return boolean
function doReloadInfo(id) end

---@param flags? integer 13
function doSaveServer(flags) end

---@param idOrList integer | integer[]
---@return boolean
function doSaveHouse(idOrList) end

---@param houseId integer
---@return boolean
function doCleanHouse(houseId) end

---@return integer
function doCleanMap() end

function doRefreshMap() end

---@param guildId integer
---@param enemyId integer
---@param warId integer
---@param warType integer
---@return integer
function doGuildAddEnemy(guildId, enemyId, warId, warType) end

---@param guildId integer
---@param enemyId integer
---@return integer
function doGuildRemoveEnemy(guildId, enemyId) end

---@return boolean
function doUpdateHouseAuctions() end

---@param directory string
---@param recursively? boolean false
---@param loadSystems? boolean true
---@return boolean
function dodirectory(directory, recursively, loadSystems) end

---@param enabled boolean
---@return boolean
function errors(enabled) end

---@param name string
---@return integer?
---@nodiscard
function getPlayerByName(name) end

---@param guid integer
---@return integer?
---@nodiscard
function getPlayerByGUID(guid) end

---@param name string
---@param pushValue? boolean false
---@return integer?
---@nodiscard
function getPlayerByNameWildcard(name, pushValue) end

---@param name string
---@return integer?
---@nodiscard
function getPlayerGUIDByName(name) end

---@param guid integer
---@return string?
---@nodiscard
function getPlayerNameByGUID(guid) end

---@param accountId integer
---@return integer[]
---@nodiscard
function getPlayersByAccountId(accountId) end

--------------------------------------
------------ Creature ----------------
--------------------------------------

---@param cid integer
---@return integer
---@nodiscard
function getCreatureHealth(cid) end

---@param cid integer
---@param ignoreModifiers? boolean false
---@return integer
---@nodiscard
function getCreatureMaxHealth(cid, ignoreModifiers) end

---@param cid integer
---@return integer
---@nodiscard
function getCreatureMana(cid) end

---@param cid integer
---@param ignoreModifiers? boolean false
---@return integer
---@nodiscard
function getCreatureMaxMana(cid, ignoreModifiers) end

---@param cid integer
---@return boolean
---@nodiscard
function getCreatureHideHealth(cid) end

---@param cid integer
---@param hide boolean
---@return boolean
function doCreatureSetHideHealth(cid, hide) end

---@param cid integer
---@return integer
---@nodiscard
function getCreatureSpeakType(cid) end

---@param cid integer
---@param messageType integer
---@return boolean
function doCreatureSetSpeakType(cid, messageType) end

---@param cid integer
---@return integer
---@nodiscard
function getCreatureLookDirection(cid) end

---@param cid integer
---@param spellName string
---@return boolean
function doCreatureCastSpell(cid, spellName) end

---@param cid integer
---@return string[]
---@nodiscard
function getCreatureStorageList(cid) end

---@param cid integer
---@param key string
---@return integer | string
---@nodiscard
function getCreatureStorage(cid, key) end

---@param cid integer
---@param key string
---@param value? string | integer
---@return boolean
function doCreatureSetStorage(cid, key, value) end

---@param cid integer
---@param targetPos Position
---@param extended? boolean false
---@param ignoreHouse? boolean true
---@return Position?
---@nodiscard
function getClosestFreeTile(cid, targetPos, extended, ignoreHouse) end

---@param cid integer
---@param text string
---@param messageType? integer MSG_SPEAK_SAY
---@param ghost? boolean false
---@param creatureId? integer 0
---@param pos? Position
---@return boolean
function doCreatureSay(cid, text, messageType, ghost, creatureId, pos) end

---@param cid integer
---@param maxMana integer
---@return boolean
function setCreatureMaxMana(cid, maxMana) end

---@param cid integer
---@param speaker integer
---@param message string
---@param messageType integer
---@param channelId integer
---@return boolean
function doCreatureChannelSay(cid, speaker, message, messageType, channelId) end

---@param cid integer
---@param name string
---@return integer
function doSummonMonster(cid, name) end

---@param cid integer
---@param color integer
---@param playerId? integer
---@return boolean
function doSendCreatureSquare(cid, color, playerId) end

---@param cid integer
---@param health integer
---@param hitEffect? integer MAGIC_EFFECT_NONE
---@param hitColor? integer COLOR_NONE
---@param force? boolean false
---@return boolean
function doCreatureAddHealth(cid, health, hitEffect, hitColor, force) end

---@param cid integer
---@param mana integer
---@param aggressive? boolean true
---@return boolean
function doCreatureAddMana(cid, mana, aggressive) end

---@param cid integer
---@param maxHealth integer
---@return boolean
function setCreatureMaxHealth(cid, maxHealth) end

---@param cid integer
---@param target integer
---@return boolean
function doConvinceCreature(cid, target) end

---@param cid integer
---@param conditionObjId integer
---@return boolean
function doAddCondition(cid, conditionObjId) end

---@param cid integer
---@param conditionType integer
---@param subId? integer 0
---@param conditionId? integer CONDITIONID_COMBAT
---@return boolean
function doRemoveCondition(cid, conditionType, subId, conditionId) end

---@param cid integer
---@param onlyPersistent? boolean true
---@return boolean
function doRemoveConditions(cid, onlyPersistent) end

---@param cid integer
---@param forceLogout? boolean true
---@return boolean
function doRemoveCreature(cid, forceLogout) end

---@param cid integer
---@param direction integer
---@param flag? integer FLAG_NOLIMIT
---@return integer
function doMoveCreature(cid, direction, flag) end

---@param cid integer
---@param position Position
---@param maxNodes? integer 100
---@return boolean
function doSteerCreature(cid, position, maxNodes) end

---@param cid integer
---@param conditionType integer
---@param subId? integer 0
---@param conditionId? integer
---@return boolean
---@nodiscard
function hasCreatureCondition(cid, conditionType, subId, conditionId) end

---@param cid integer
---@param conditionType integer
---@param subId? integer 0
---@param conditionId? integer CONDITIONID_COMBAT
---@return ConditionInfoTable?
---@nodiscard
function getCreatureConditionInfo(cid, conditionType, subId, conditionId) end

---@param cid integer
---@param doDrop boolean
---@return boolean
function doCreatureSetDropLoot(cid, doDrop) end

---@param cid integer
---@return boolean
---@nodiscard
function isCreature(cid) end

---@param cid integer
---@param name string
---@return boolean
function registerCreatureEvent(cid, name) end

---@param cid integer
---@param name string
---@return boolean
function unregisterCreatureEvent(cid, name) end

---@param cid integer
---@param typeName string
---@return boolean
function unregisterCreatureEventType(cid, typeName) end

---@param cid integer
---@param target integer
---@return boolean
function doChallengeCreature(cid, target) end

---@param cid integer
---@param delta integer
---@return boolean
function doChangeSpeed(cid, delta) end

---@param cid integer
---@param outfit Outfit
---@return boolean
function doCreatureChangeOutfit(cid, outfit) end

---@param cid integer
---@param monsterName string
---@param time? integer -1
---@return boolean
function doSetMonsterOutfit(cid, monsterName, time) end

---@param cid integer
---@param itemId integer
---@param time? integer -1
---@return boolean
function doSetItemOutfit(cid, itemId, time) end

---@param cid integer
---@param outfit Outfit
---@param time? integer -1
---@return boolean
function doSetCreatureOutfit(cid, outfit, time) end

---@param cid integer
---@return Outfit
---@nodiscard
function getCreatureOutfit(cid) end

---@param cid integer
---@return Position
---@nodiscard
function getCreatureLastPosition(cid) end

---@param cid integer
---@return string
---@nodiscard
function getCreatureName(cid) end

---@param cid integer
---@return integer
---@nodiscard
function getCreatureSpeed(cid) end

---@param cid integer
---@param speed integer
---@return boolean
function setCreatureSpeed(cid, speed) end

---@param cid integer
---@return integer
---@nodiscard
function getCreatureBaseSpeed(cid) end

---@param cid integer
---@return integer
---@nodiscard
function getCreatureTarget(cid) end

---@param cid integer
---@param text string
---@param ignoreAccess? boolean false
---@param channelId? integer CHANNEL_DEFAULT
---@return boolean
function doCreatureExecuteTalkAction(cid, text, ignoreAccess, channelId) end

---@param cid integer
---@param direction integer
---@return boolean
function doCreatureSetLookDirection(cid, direction) end

---@param cid integer
---@param targetId? integer 0
---@return integer
---@nodiscard
function getCreatureGuildEmblem(cid, targetId) end

---@param cid integer
---@param emblem integer
---@return boolean
function doCreatureSetGuildEmblem(cid, emblem) end

---@param cid integer
---@param targetId? integer 0
---@return integer
---@nodiscard
function getCreaturePartyShield(cid, targetId) end

---@param cid integer
---@param shield integer
---@return boolean
function doCreatureSetPartyShield(cid, shield) end

---@param cid integer
---@param targetId? integer 0
---@return integer
---@nodiscard
function getCreatureSkullType(cid, targetId) end

---@param cid integer
---@param skull integer
---@return boolean
function doCreatureSetSkullType(cid, skull) end

---@param cid integer
---@return boolean
---@nodiscard
function getCreatureNoMove(cid) end

---@param cid integer
---@param block boolean
---@return boolean
function doCreatureSetNoMove(cid, block) end

---@param cid integer
---@return integer?
---@nodiscard
function getCreatureMaster(cid) end

---@param cid integer
---@return integer[]
---@nodiscard
function getCreatureSummons(cid) end

--------------------------------------
------------- Player -----------------
--------------------------------------

---@param cid integer
---@return integer
---@nodiscard
function getPlayerLevel(cid) end

---@param cid integer
---@return integer
---@nodiscard
function getPlayerExperience(cid) end

---@param cid integer
---@param ignoreModifiers? boolean false
---@return integer
---@nodiscard
function getPlayerMagLevel(cid, ignoreModifiers) end

---@param cid integer
---@return integer
---@nodiscard
function getPlayerSpentMana(cid) end

---@param cid integer
---@return integer
---@nodiscard
function getPlayerFood(cid) end

---@param cid integer
---@return integer
---@nodiscard
function getPlayerAccess(cid) end

---@param cid integer
---@return integer
---@nodiscard
function getPlayerGhostAccess(cid) end

---@param cid integer
---@param skill integer
---@param ignoreModifiers? boolean false
---@return integer
---@nodiscard
function getPlayerSkillLevel(cid, skill, ignoreModifiers) end

---@param cid integer
---@param skill integer
---@return integer
---@nodiscard
function getPlayerSkillTries(cid, skill) end

---@param cid integer
---@param skill integer
---@return boolean
function doPlayerSetOfflineTrainingSkill(cid, skill) end

---@param cid integer
---@return integer
---@nodiscard
function getPlayerTown(cid) end

---@param cid integer
---@return integer
---@nodiscard
function getPlayerVocation(cid) end

---@param cid integer
---@return integer
---@nodiscard
function getPlayerIp(cid) end

---@param cid integer
---@param magicLevel integer
---@return integer
---@nodiscard
function getPlayerRequiredMana(cid, magicLevel) end

---@param cid integer
---@param skill integer
---@param level integer
---@return integer
---@nodiscard
function getPlayerRequiredSkillTries(cid, skill, level) end

---@param cid integer
---@param itemId integer
---@param subType? integer -1
---@return integer
---@nodiscard
function getPlayerItemCount(cid, itemId, subType) end

---@param cid integer
---@return integer
---@nodiscard
function getPlayerMoney(cid) end

---@param cid integer
---@param ignoreModifiers? boolean false
---@return integer
---@nodiscard
function getPlayerSoul(cid, ignoreModifiers) end

---@param cid integer
---@return integer
---@nodiscard
function getPlayerFreeCap(cid) end

---@param cid integer
---@return integer level, integer color
---@nodiscard
function getPlayerLight(cid) end

---@param cid integer
---@param slot integer
---@return ThingTable
---@nodiscard
function getPlayerSlotItem(cid, slot) end

---@param cid integer
---@param ignoreAmmo? boolean false
---@return ThingTable
---@nodiscard
function getPlayerWeapon(cid, ignoreAmmo) end

---@param cid integer
---@param deepSearch boolean
---@param itemId integer
---@param subType? integer -1
---@return ThingTable
---@nodiscard
function getPlayerItemById(cid, deepSearch, itemId, subType) end

---@param cid integer
---@param depotId integer
---@return integer?
---@nodiscard
function getPlayerDepotItems(cid, depotId) end

---@param cid integer
---@return integer
---@nodiscard
function getPlayerGuildId(cid) end

---@param cid integer
---@return string
---@nodiscard
function getPlayerGuildName(cid) end

---@param cid integer
---@return integer
---@nodiscard
function getPlayerGuildRankId(cid) end

---@param cid integer
---@return string
---@nodiscard
function getPlayerGuildRank(cid) end

---@param cid integer
---@return string
---@nodiscard
function getPlayerGuildNick(cid) end

---@param cid integer
---@return integer
---@nodiscard
function getPlayerGuildLevel(cid) end

---@param cid integer
---@return integer
---@nodiscard
function getPlayerGUID(cid) end

---@param cid integer
---@return string
---@nodiscard
function getPlayerNameDescription(cid) end

---@param cid integer
---@param description string
---@return boolean
function doPlayerSetNameDescription(cid, description) end

---@param cid integer
---@return string
---@nodiscard
function getPlayerSpecialDescription(cid) end

---@param cid integer
---@param description string
---@return boolean
function doPlayerSetSpecialDescription(cid, description) end

---@param cid integer
---@return integer
---@nodiscard
function getPlayerAccountId(cid) end

---@param cid integer
---@return string
---@nodiscard
function getPlayerAccount(cid) end

---@param cid integer
---@param flag integer
---@return boolean
---@nodiscard
function getPlayerFlagValue(cid, flag) end

---@param cid integer
---@param flag integer
---@return boolean
---@nodiscard
function getPlayerCustomFlagValue(cid, flag) end

---@param cid integer
---@return integer
---@nodiscard
function getPlayerPromotionLevel(cid) end

---@param cid integer
---@param level integer
---@return boolean
function doPlayerSetPromotionLevel(cid, level) end

---@param cid integer
---@return integer
---@nodiscard
function getPlayerGroupId(cid) end

---@param cid integer
---@param groupId integer
---@return boolean
function doPlayerSetGroupId(cid, groupId) end

---@param cid integer
---@return boolean
function doPlayerSendOutfitWindow(cid) end

---@param cid integer
---@param spellName string
---@return boolean
function doPlayerLearnInstantSpell(cid, spellName) end

---@param cid integer
---@param spellName string
---@return boolean
function doPlayerUnlearnInstantSpell(cid, spellName) end

---@param cid integer
---@param spellName string
---@return boolean
---@nodiscard
function getPlayerLearnedInstantSpell(cid, spellName) end

---@param cid integer
---@return integer
---@nodiscard
function getPlayerInstantSpellCount(cid) end

---@param cid integer
---@param index integer
---@return InstantSpellInfo?
---@nodiscard
function getPlayerInstantSpellInfo(cid, index) end

---@param cid integer
---@return SpectatorsInfo
---@nodiscard
function getPlayerSpectators(cid) end

---@param cid integer
---@param data SpectatorsInfo
---@return boolean
function doPlayerSetSpectators(cid, data) end

---@param cid integer
---@param skill integer
---@param count integer
---@param useMultiplier? boolean true
---@return boolean
function doPlayerAddSkillTry(cid, skill, count, useMultiplier) end

---@param cid integer
---@param food integer
---@return boolean
function doPlayerFeed(cid, food) end

---@param cid integer
---@param text string
---@return boolean
function doPlayerSendCancel(cid, text) end

---@param cid integer
---@return boolean
function doPlayerSendCastList(cid) end

---@param cid integer
---@param ret integer
---@return boolean
function doPlayerSendDefaultCancel(cid, ret) end

---@param cid integer
---@param cap number
---@return boolean
function doPlayerSetMaxCapacity(cid, cap) end

---@param cid integer
---@param amount integer
---@param useMultiplier? boolean true
---@return boolean
function doPlayerAddSpentMana(cid, amount, useMultiplier) end

---@param cid integer
---@param amount integer
---@return boolean
function doPlayerAddSoul(cid, amount) end

---@param cid integer
---@param speed integer
---@return boolean
function doPlayerSetExtraAttackSpeed(cid, speed) end

---@param cid integer
---@param itemId integer
---@param count? integer 1
---@param canDropOnMap? boolean true
---@param subTypeOrSlot? integer 1 | 0
---@param slot? integer 0
---@return integer, ...
function doPlayerAddItem(cid, itemId, count, canDropOnMap, subTypeOrSlot, slot) end

---@param cid integer
---@param uid integer
---@param canDropOnMap? boolean false
---@param slot? integer SLOT_WHEREEVER
---@return integer | boolean
function doPlayerAddItemEx(cid, uid, canDropOnMap, slot) end

---@param cid integer
---@param messageType integer
---@param message string
---@param value? integer 0
---@param color? integer COLOR_WHITE
---@param position? Position
---@return boolean
function doPlayerSendTextMessage(cid, messageType, message, value, color, position) end

---@param cid integer
---@param author string
---@param message string
---@param messageType integer
---@param channelId integer
---@return boolean
function doPlayerSendChannelMessage(cid, author, message, messageType, channelId) end

---@param cid integer
---@param channelId integer
---@return boolean
function doPlayerOpenChannel(cid, channelId) end

---@param cid integer
---@return boolean
function doPlayerOpenPrivateChannel(cid) end

---@param cid integer
---@param list? table<integer, boolean>
---@return boolean
function doPlayerSendChannels(cid, list) end

---@param cid integer
---@param money integer
---@return boolean
function doPlayerAddMoney(cid, money) end

---@param cid integer
---@param money integer
---@param canDrop? boolean true
---@return boolean
function doPlayerRemoveMoney(cid, money, canDrop) end

---@param cid integer
---@param target string
---@param money integer
---@return boolean
function doPlayerTransferMoneyTo(cid, target, money) end

---@param cid integer
---@param itemId integer
---@param textOrCanWrite? string | boolean
---@param lengthOrCanWrite? integer | boolean
---@param length? integer
---@return boolean
function doShowTextDialog(cid, itemId, textOrCanWrite, lengthOrCanWrite, length) end

---@param cid integer
---@param locked boolean
---@return boolean
function doPlayerSetPzLocked(cid, locked) end

---@param cid integer
---@param townId integer
---@return boolean
function doPlayerSetTown(cid, townId) end

---@param cid integer
---@param vocationId integer
---@return boolean
function doPlayerSetVocation(cid, vocationId) end

---@param cid integer
---@param itemId integer
---@param count integer
---@param subType? integer -1
---@param ignoreEquipped? boolean false
---@return boolean
function doPlayerRemoveItem(cid, itemId, count, subType, ignoreEquipped) end

---@param cid integer
---@param amount integer
---@return boolean
function doPlayerAddExperience(cid, amount) end

---@param cid integer
---@param id integer
---@return boolean
function doPlayerSetGuildId(cid, id) end

---@param cid integer
---@param level integer
---@param rank? integer 0
---@return boolean
function doPlayerSetGuildLevel(cid, level, rank) end

---@param cid integer
---@param nick string
---@return boolean
function doPlayerSetGuildNick(cid, nick) end

---@param cid integer
---@param lookType integer
---@param addons integer
---@return boolean
function doPlayerAddOutfit(cid, lookType, addons) end

---@param cid integer
---@param lookType integer
---@param addons? integer 0xFF
---@return boolean
function doPlayerRemoveOutfit(cid, lookType, addons) end

---@param cid integer
---@param outfitId integer
---@param addons integer
---@return boolean
function doPlayerAddOutfitId(cid, outfitId, addons) end

---@param cid integer
---@param outfitId integer
---@param addons? integer 0
---@return boolean
function doPlayerRemoveOutfitId(cid, outfitId, addons) end

---@param cid integer
---@param lookType integer
---@param addons? integer 0
---@return boolean
---@nodiscard
function canPlayerWearOutfit(cid, lookType, addons) end

---@param cid integer
---@param outfitId integer
---@param addons? integer 0
---@return boolean
---@nodiscard
function canPlayerWearOutfitId(cid, outfitId, addons) end

---@param cid integer
---@param lossType integer
---@return integer
---@nodiscard
function getPlayerLossPercent(cid, lossType) end

---@param cid integer
---@param lossType integer
---@param percent integer
---@return boolean
function doPlayerSetLossPercent(cid, lossType, percent) end

---@param cid integer
---@param doLose boolean
---@return boolean
function doPlayerSetLossSkill(cid, doLose) end

---@param cid integer
---@return boolean
---@nodiscard
function getPlayerLossSkill(cid) end

---@param cid integer
---@return boolean
function doPlayerSwitchSaving(cid) end

---@param cid integer
---@param shallow? boolean false
---@return boolean
function doPlayerSave(cid, shallow) end

---@param cid integer
---@return boolean
function doPlayerSaveItems(cid) end

---@param cid integer
---@return boolean
---@nodiscard
function isPlayerPzLocked(cid) end

---@param cid integer
---@return boolean
---@nodiscard
function isPlayerSaving(cid) end

---@param cid integer
---@return boolean
---@nodiscard
function isPlayerProtected(cid) end

---@param guid integer
---@param oldName string
---@param newName string
---@return boolean
function doPlayerChangeName(guid, oldName, newName) end

---@param cid integer
---@param full? boolean false
---@return integer
---@nodiscard
function getPlayerSex(cid, full) end

---@param cid integer
---@param sex integer
---@return boolean
function doPlayerSetSex(cid, sex) end

---@param cid integer
---@param amount integer
---@return boolean
function doPlayerAddSoul(cid, amount) end

---@param cid integer
---@return integer
---@nodiscard
function getPlayerSkullEnd(cid) end

---@param cid integer
---@param time integer
---@param type integer
---@return boolean
function doPlayerSetSkullEnd(cid, time, type) end

---@param cid integer
---@param message string
---@return boolean
function doPlayerPopupFYI(cid, message) end

---@param cid integer
---@param id integer
---@return boolean
function doPlayerSendTutorial(cid, id) end

---@param name string
---@param item integer
---@param townId? integer 0
---@param actorId? integer 0
---@return boolean
function doPlayerSendMailByName(name, item, townId, actorId) end

---@param cid integer
---@param position Position
---@param markType integer
---@param description? string
---@return boolean
function doPlayerAddMapMark(cid, position, markType, description) end

---@param cid integer
---@param days integer
---@return boolean
function doPlayerAddPremiumDays(cid, days) end

---@param cid integer
---@return integer
---@nodiscard
function getPlayerPremiumDays(cid) end

---@param cid integer
---@param blessing integer
---@return boolean
---@nodiscard
function getPlayerBlessing(cid, blessing) end

---@param cid integer
---@param blessing integer
---@return boolean
function doPlayerAddBlessing(cid, blessing) end

---@param cid integer
---@return boolean
---@nodiscard
function getPlayerPVPBlessing(cid) end

---@param cid integer
---@param value? boolean true
---@return boolean
function doPlayerSetPVPBlessing(cid, value) end

---@param cid integer
---@return integer
---@nodiscard
function getPlayerStamina(cid) end

---@param cid integer
---@param minutes integer
---@return integer
---@nodiscard
function doPlayerSetStamina(cid, minutes) end

---@param cid integer
---@return integer
---@nodiscard
function getPlayerBalance(cid) end

---@param cid integer
---@param balance integer
---@return boolean
function doPlayerSetBalance(cid, balance) end

---@param cid integer
function doUpdatePlayerStats(cid) end

---@param cid integer
---@return number
---@nodiscard
function getPlayerDamageMultiplier(cid) end

---@param cid integer
---@param multiplier number
---@return boolean
function setPlayerDamageMultiplier(cid, multiplier) end

---@param cid integer
---@return integer
---@nodiscard
function getPlayerIdleTime(cid) end

---@param cid integer
---@param amount integer
---@return boolean
function doPlayerSetIdleTime(cid, amount) end

---@param cid integer
---@return integer
---@nodiscard
function getPlayerLastLoad(cid) end

---@param cid integer
---@return integer
---@nodiscard
function getPlayerLastLogin(cid) end

---@param cid integer
---@return integer
---@nodiscard
function getPlayerAccountManager(cid) end

---@param cid integer
---@return integer
---@nodiscard
function getPlayerTradeState(cid) end

---@param cid integer
---@return integer
---@nodiscard
function getPlayerOperatingSystem(cid) end

---@param cid integer
---@return integer
---@nodiscard
function getPlayerClientVersion(cid) end

---@param cid integer
---@return { chase: boolean, fight: integer, secure: boolean }
---@nodiscard
function getPlayerModes(cid) end

---@param cid integer
---@return table<integer, number>
---@nodiscard
function getPlayerRates(cid) end

---@param cid integer
---@param skill integer
---@param value number
---@return boolean
function doPlayerSetRate(cid, skill, value) end

---@param cid integer
---@return integer
---@nodiscard
function getPlayerPartner(cid) end

---@param cid integer
---@param guid integer
---@return boolean
function doPlayerSetPartner(cid, guid) end

---@param cid integer
---@param target integer
---@return boolean
function doPlayerFollowCreature(cid, target) end

--- Return the party leader id
---@param cid integer
---@return integer?
---@nodiscard
function getPlayerParty(cid) end

---@param cid integer
---@param leaderId integer
---@return boolean
function doPlayerJoinParty(cid, leaderId) end

---@param cid integer
---@param forced? boolean false
---@return boolean
function doPlayerLeaveParty(cid, forced) end

--- The first array value is always the party leader id
---@param cid integer
---@return integer[]
---@nodiscard
function getPartyMembers(cid) end

---@param cid integer
---@return boolean
---@nodiscard
function hasPlayerClient(cid) end

---@param cid integer
---@return boolean
---@nodiscard
function isPlayerUsingOtclient(cid) end

---@param cid integer
---@param opcode integer
---@param buffer string
---@return boolean
function doSendPlayerExtendedOpcode(cid, opcode, buffer) end

---@param cid integer
---@param uid integer
---@param walkthrough boolean
---@return boolean
function doPlayerSetWalkthrough(cid, uid, walkthrough) end

--------------------------------------
-------------- Monster ---------------
--------------------------------------

---@param cid integer
---@return integer[]
---@nodiscard
function getMonsterTargetList(cid) end

---@param cid integer
---@return integer[]
---@nodiscard
function getMonsterFriendList(cid) end

---@param cid integer
---@param target integer
---@return boolean
function doMonsterSetTarget(cid, target) end

---@param cid integer
---@return boolean
function doMonsterChangeTarget(cid) end

---@param cid integer
---@return boolean
function hasMonsterRaid(cid) end

--------------------------------------
---------------- Item ----------------
--------------------------------------

---@param uid integer
---@param count? integer -1
---@return boolean
function doRemoveItem(uid, count) end

---@param uid integer
---@return boolean
function doItemRaidUnref(uid) end

---@param uid integer
---@param destination Position
---@return boolean
function doItemSetDestination(uid, destination) end

---@param uid integer
---@param newId integer
---@param count? integer -1
---@return boolean
function doTransformItem(uid, newId, count) end

---@param uid integer
---@return boolean
function doDecayItem(uid) end

---@param uid integer
---@return integer
---@nodiscard
function getContainerSize(uid) end

---@param uid integer
---@return integer
---@nodiscard
function getContainerCap(uid) end

---@param uid integer
---@param slot integer
---@return ThingTable
---@nodiscard
function getContainerItem(uid, slot) end

---@param uid integer
---@param itemId integer
---@param count? integer 1
---@return integer, ...
function doAddContainerItem(uid, itemId, count) end

---@param itemId integer
---@return boolean
---@nodiscard
function isItemDoor(itemId) end

---@param uid integer
---@param key string
---@return number | string | boolean | nil
---@nodiscard
function getItemAttribute(uid, key) end

---@param uid integer
---@param key string
---@param value number | string | boolean
---@return boolean
function doItemSetAttribute(uid, key, value) end

---@param uid integer
---@param key string
---@return boolean
function doItemEraseAttribute(uid, key) end

---@param uid integer
---@param duration integer
---@return boolean
function doItemSetDuration(uid, duration) end

---@param uid integer
---@return integer
---@nodiscard
function getItemDurationTime(uid) end

---@param uid integer
---@param precise? boolean true
---@return number
---@nodiscard
function getItemWeight(uid, precise) end

---@param uid integer
---@return ThingTable
---@nodiscard
function getItemParent(uid) end

---@param uid integer
---@param prop integer
---@return boolean
---@nodiscard
function hasItemProperty(uid, prop) end

---@param uid integer
---@param item integer
---@return integer?
function doAddContainerItemEx(uid, item) end

--------------------------------------
--------------- Thing ----------------
--------------------------------------

---@param uid integer
---@return boolean
---@nodiscard
function isMovable(uid) end

---@param uid integer
---@param recursive? integer RECURSE_FIRST
---@return ThingTable
---@nodiscard
function getThing(uid, recursive) end

---@param uid integer
---@return Position
---@nodiscard
function getThingPosition(uid) end

---@param uid integer
---@param destination Position
---@param pushMove? boolean true
---@param fullTeleport? boolean true
---@return boolean
function doTeleportThing(uid, destination, pushMove, fullTeleport) end

--------------------------------------
--------------- Combat ---------------
--------------------------------------

---@return integer
---@nodiscard
function createCombatObject() end

---@param area integer[][]
---@param extraArea? integer[][]
---@return integer
---@nodiscard
function createCombatArea(area, extraArea) end

---@param combat integer
---@param area integer
---@return boolean
function setCombatArea(combat, area) end

---@param combat integer
---@param condition integer
---@return boolean
function setCombatCondition(combat, condition) end

---@param combat integer
---@param key integer
---@param value integer | boolean
---@return boolean
function setCombatParam(combat, key, value) end

---@param combat integer
---@param key integer
---@param functionName string
---@return boolean
function setCombatCallback(combat, key, functionName) end

---@param combat integer
---@param formulaType integer
---@param mina number
---@param minb number
---@param maxa number
---@param minl? number config.formulaLevel
---@param maxl? number minl
---@param minm? number config.formulaMagic
---@param maxm? number minm
---@param minc? integer 0
---@param maxc? integer 0
---@return boolean
function setCombatFormula(combat, formulaType, mina, minb, maxa, maxb, minl, maxl, minm, maxm, minc, maxc) end

---@param cid integer
---@param combat integer
---@param var Variant
---@return boolean
function doCombat(cid, combat, var) end

--------------------------------------
------------- Condition --------------
--------------------------------------

---@param type integer
---@param ticks? integer 0
---@param buff? boolean false
---@param subId? integer 0
---@param conditionId? integer CONDITIONID_COMBAT
---@return integer
---@nodiscard
function createConditionObject(type, ticks, buff, subId, conditionId) end

---@param condition integer
---@param key integer
---@param value integer
---@return boolean
function setConditionParam(condition, key, value) end

---@param condition integer
---@param rounds integer
---@param time integer
---@param value integer
---@return boolean
function addDamageCondition(condition, rounds, time, value) end

---@param condition integer
---@param outfit Outfit
---@return boolean
function addOutfitCondition(condition, outfit) end

---@param condition integer
---@param mina number
---@param minb number
---@param maxa number
---@param maxb number
---@return boolean
function setConditionFormula(condition, mina, minb, maxa, maxb) end

--------------------------------------
-------------- Variant ---------------
--------------------------------------

---@param num integer
---@return Variant
---@nodiscard
function numberToVariant(num) end

---@param str string
---@return Variant
---@nodiscard
function stringToVariant(str) end

---@param pos Position
---@return Variant
---@nodiscard
function positionToVariant(pos) end

---@param pos Position
---@return Variant
---@nodiscard
function targetPositionToVariant(pos) end

---@param var Variant
---@return integer
---@nodiscard
function variantToNumber(var) end

---@param var Variant
---@return string
---@nodiscard
function variantToString(var) end

---@param var Variant
---@return Position
---@nodiscard
function variantToPosition(var) end

--------------------------------------
----------------- os -----------------
--------------------------------------

---@return integer
---@nodiscard
function os.mtime() end

--------------------------------------
----------------- db -----------------
--------------------------------------

---@class db
db = {}

---@param query string
---@return boolean
function db.query(query) end

---@param query string
---@return integer
---@nodiscard
function db.storeQuery(query) end

---@param str string
---@return string
---@nodiscard
function db.escapeString(str) end

---@param str string
---@param length integer
---@return string
---@nodiscard
function db.escapeBlob(str, length) end

---@return integer
---@nodiscard
function db.lastInsertId() end

---@param name string
---@return boolean
---@nodiscard
function db.tableExists(name) end

---@return boolean
function db.transBegin() end

---@return boolean
function db.transRollback() end

---@return boolean
function db.transCommit() end

--------------------------------------
--------------- result ---------------
--------------------------------------

---@class result
result = {}

---@param ret integer
---@param str string
---@return integer
---@nodiscard
function result.getNumber(ret, str) end

---@param ret integer
---@param str string
---@return string
---@nodiscard
function result.getString(ret, str) end

---@param ret integer
---@param str string
---@return string, integer
---@nodiscard
function result.getStream(ret, str) end

---@param ret integer
---@return boolean
---@nodiscard
function result.next(ret) end

---@param ret integer
---@return boolean
function result.free(ret) end

-- backwards-compatibility
result.getDataLong = result.getNumber
result.getDataInt = result.getNumber
result.getDataString = result.getString
result.getDataStream = result.getStream

--------------------------------------
---------------- std -----------------
--------------------------------------

---@class std
std = {}

---@return integer
function std.cout(...) end

---@return integer
function std.clog(...) end

---@return integer
function std.cerr(...) end

---@param str string
---@return string
---@nodiscard
function std.sha1(str) end

---@param name string
---@param forceUppercaseOnFirstLetter? boolean true
---@return boolean
---@nodiscard
function std.checkName(name, forceUppercaseOnFirstLetter) end
