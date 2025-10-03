CREATE TABLE IF NOT EXISTS `accounts`
(
	`id` INT NOT NULL AUTO_INCREMENT,
	`name` VARCHAR(32) NOT NULL,
	`password` VARCHAR(40) NOT NULL,
	`salt` VARCHAR(40) NOT NULL DEFAULT '',
	`premdays` INT NOT NULL DEFAULT '0',
	`lastday` INT UNSIGNED NOT NULL DEFAULT '0',
	`email` VARCHAR(255) NOT NULL DEFAULT '',
	`key` VARCHAR(32) NOT NULL DEFAULT '',
	`warnings` INT NOT NULL DEFAULT '0',
	`group_id` INT NOT NULL DEFAULT '1',
	PRIMARY KEY (`id`),
	UNIQUE KEY `name` (`name`)
) ENGINE = InnoDB;

CREATE TABLE IF NOT EXISTS `players`
(
	`id` INT NOT NULL AUTO_INCREMENT,
	`name` VARCHAR(255) NOT NULL,
	`group_id` INT NOT NULL DEFAULT '1',
	`account_id` INT NOT NULL DEFAULT '0',
	`level` INT NOT NULL DEFAULT '1',
	`vocation` INT NOT NULL DEFAULT '0',
	`health` INT NOT NULL DEFAULT '150',
	`healthmax` INT NOT NULL DEFAULT '150',
	`experience` BIGINT UNSIGNED NOT NULL DEFAULT '0',
	`lookbody` INT NOT NULL DEFAULT '0',
	`lookfeet` INT NOT NULL DEFAULT '0',
	`lookhead` INT NOT NULL DEFAULT '0',
	`looklegs` INT NOT NULL DEFAULT '0',
	`looktype` INT NOT NULL DEFAULT '136',
	`lookaddons` INT NOT NULL DEFAULT '0',
	`lookmount` INT NOT NULL DEFAULT '0',
	`maglevel` INT NOT NULL DEFAULT '0',
	`mana` INT NOT NULL DEFAULT '0',
	`manamax` INT NOT NULL DEFAULT '0',
	`manaspent` BIGINT UNSIGNED NOT NULL DEFAULT '0',
	`soul` INT UNSIGNED NOT NULL DEFAULT '0',
	`town_id` INT NOT NULL DEFAULT '0',
	`posx` INT NOT NULL DEFAULT '0',
	`posy` INT NOT NULL DEFAULT '0',
	`posz` INT NOT NULL DEFAULT '0',
	`conditions` BLOB DEFAULT NULL,
	`cap` INT NOT NULL DEFAULT '0',
	`sex` INT NOT NULL DEFAULT '0',
	`lastlogin` BIGINT UNSIGNED NOT NULL DEFAULT '0',
	`lastip` INT UNSIGNED NOT NULL DEFAULT '0',
	`save` TINYINT NOT NULL DEFAULT '1',
	`skull` TINYINT NOT NULL DEFAULT '0',
	`skulltime` INT NOT NULL DEFAULT '0',
	`rank_id` INT NOT NULL DEFAULT '0',
	`guildnick` VARCHAR(255) NOT NULL DEFAULT '',
	`lastlogout` BIGINT UNSIGNED NOT NULL DEFAULT '0',
	`blessings` TINYINT NOT NULL DEFAULT '0',
	`pvp_blessing` TINYINT NOT NULL DEFAULT '0',
	`balance` BIGINT UNSIGNED NOT NULL DEFAULT '0',
	`stamina` BIGINT UNSIGNED NOT NULL DEFAULT '151200000' COMMENT 'stored in miliseconds',
	`direction` INT NOT NULL DEFAULT '2',
	`loss_experience` INT NOT NULL DEFAULT '100',
	`loss_mana` INT NOT NULL DEFAULT '100',
	`loss_skills` INT NOT NULL DEFAULT '100',
	`loss_containers` INT NOT NULL DEFAULT '100',
	`loss_items` INT NOT NULL DEFAULT '100',
	`online` TINYINT NOT NULL DEFAULT '0',
	`marriage` INT UNSIGNED NOT NULL DEFAULT '0',
	`promotion` INT NOT NULL DEFAULT '0',
	`deleted` INT NOT NULL DEFAULT '0',
	`description` VARCHAR(255) NOT NULL DEFAULT '',
	`broadcasting` TINYINT NOT NULL DEFAULT '0',
	PRIMARY KEY (`id`),
	UNIQUE KEY `name` (`name`),
	FOREIGN KEY (`account_id`) REFERENCES `accounts`(`id`) ON DELETE CASCADE
) ENGINE = InnoDB;

CREATE TABLE IF NOT EXISTS `account_viplist`
(
	`account_id` INT NOT NULL,
	`player_id` INT NOT NULL,
	UNIQUE KEY `account_player_index` (`account_id`, `player_id`),
	FOREIGN KEY (`account_id`) REFERENCES `accounts`(`id`) ON DELETE CASCADE,
	FOREIGN KEY (`player_id`) REFERENCES `players`(`id`) ON DELETE CASCADE
) ENGINE = InnoDB;

CREATE TABLE IF NOT EXISTS `player_deaths`
(
	`id` INT NOT NULL AUTO_INCREMENT,
	`player_id` INT NOT NULL,
	`date` BIGINT UNSIGNED NOT NULL,
	`level` INT UNSIGNED NOT NULL,
	PRIMARY KEY (`id`),
	INDEX `date` (`date`),
	FOREIGN KEY (`player_id`) REFERENCES `players`(`id`) ON DELETE CASCADE
) ENGINE = InnoDB;

CREATE TABLE IF NOT EXISTS `player_depotitems`
(
	`player_id` INT NOT NULL,
	`sid` INT NOT NULL COMMENT 'any given range, eg. 0-100 is reserved for depot lockers and all above 100 will be normal items inside depots',
	`pid` INT NOT NULL DEFAULT '0',
	`itemtype` INT NOT NULL,
	`count` INT NOT NULL DEFAULT '0',
	`attributes` BLOB NOT NULL,
	`serial` VARCHAR(40) NOT NULL DEFAULT '',
	UNIQUE KEY `player_sid_index` (`player_id`, `sid`),
	FOREIGN KEY (`player_id`) REFERENCES `players`(`id`) ON DELETE CASCADE
) ENGINE = InnoDB;

CREATE TABLE IF NOT EXISTS `player_items`
(
	`player_id` INT NOT NULL,
	`pid` INT NOT NULL DEFAULT '0',
	`sid` INT NOT NULL DEFAULT '0',
	`itemtype` INT NOT NULL DEFAULT '0',
	`count` INT NOT NULL DEFAULT '0',
	`attributes` BLOB NOT NULL,
	`serial` VARCHAR(40) NOT NULL DEFAULT '',
	UNIQUE KEY `player_sid_index` (`player_id`, `sid`),
	FOREIGN KEY (`player_id`) REFERENCES `players`(`id`) ON DELETE CASCADE
) ENGINE = InnoDB;

CREATE TABLE IF NOT EXISTS `player_namelocks`
(
	`player_id` INT NOT NULL,
	`name` VARCHAR(255) NOT NULL,
	`new_name` VARCHAR(255) NOT NULL,
	`date` BIGINT NOT NULL DEFAULT '0',
	FOREIGN KEY (`player_id`) REFERENCES `players`(`id`) ON DELETE CASCADE
) ENGINE = InnoDB;

CREATE TABLE IF NOT EXISTS `player_statements`
(
	`id` INT NOT NULL AUTO_INCREMENT,
	`player_id` INT NOT NULL,
	`channel_id` INT NOT NULL DEFAULT '0',
	`text` VARCHAR (255) NOT NULL,
	`date` BIGINT NOT NULL DEFAULT '0',
	PRIMARY KEY (`id`),
	KEY `player_id` (`player_id`),
	KEY `channel_id` (`channel_id`),
	FOREIGN KEY (`player_id`) REFERENCES `players`(`id`) ON DELETE CASCADE
) ENGINE = InnoDB;

CREATE TABLE IF NOT EXISTS `player_skills`
(
	`player_id` INT NOT NULL,
	`skillid` TINYINT NOT NULL DEFAULT '0',
	`value` INT UNSIGNED NOT NULL DEFAULT '0',
	`count` INT UNSIGNED NOT NULL DEFAULT '0',
	UNIQUE KEY `players_skills_index` (`player_id`, `skillid`),
	FOREIGN KEY (`player_id`) REFERENCES `players`(`id`) ON DELETE CASCADE
) ENGINE = InnoDB;

CREATE TABLE IF NOT EXISTS `player_spells`
(
	`player_id` INT NOT NULL,
	`name` VARCHAR(255) NOT NULL,
	UNIQUE KEY `players_spells_index` (`player_id`, `name`),
	FOREIGN KEY (`player_id`) REFERENCES `players`(`id`) ON DELETE CASCADE
) ENGINE = InnoDB;

CREATE TABLE IF NOT EXISTS `player_storage`
(
	`player_id` INT NOT NULL,
	`key` VARCHAR(255) NOT NULL DEFAULT '0',
	`value` TEXT NOT NULL,
	UNIQUE KEY `players_storage_index` (`player_id`, `key`),
	FOREIGN KEY (`player_id`) REFERENCES `players`(`id`) ON DELETE CASCADE
) ENGINE = InnoDB;

CREATE TABLE IF NOT EXISTS `player_viplist`
(
	`player_id` INT NOT NULL,
	`vip_id` INT NOT NULL,
	UNIQUE KEY `players_viplist_index` (`player_id`, `vip_id`),
	FOREIGN KEY (`player_id`) REFERENCES `players`(`id`) ON DELETE CASCADE,
	FOREIGN KEY (`vip_id`) REFERENCES `players`(`id`) ON DELETE CASCADE
) ENGINE = InnoDB;

CREATE TABLE IF NOT EXISTS `killers`
(
	`id` INT NOT NULL AUTO_INCREMENT,
	`death_id` INT NOT NULL,
	`final_hit` TINYINT NOT NULL DEFAULT '0',
	`unjustified` TINYINT NOT NULL DEFAULT '0',
	PRIMARY KEY (`id`),
	FOREIGN KEY (`death_id`) REFERENCES `player_deaths`(`id`) ON DELETE CASCADE
) ENGINE = InnoDB;

CREATE TABLE IF NOT EXISTS `player_killers`
(
	`kill_id` INT NOT NULL,
	`player_id` INT NOT NULL,
	FOREIGN KEY (`kill_id`) REFERENCES `killers`(`id`) ON DELETE CASCADE,
	FOREIGN KEY (`player_id`) REFERENCES `players`(`id`) ON DELETE CASCADE
) ENGINE = InnoDB;

CREATE TABLE IF NOT EXISTS `environment_killers`
(
	`kill_id` INT NOT NULL,
	`name` VARCHAR(255) NOT NULL,
	FOREIGN KEY (`kill_id`) REFERENCES `killers`(`id`) ON DELETE CASCADE
) ENGINE = InnoDB;

CREATE TABLE IF NOT EXISTS `houses`
(
	`id` INT UNSIGNED NOT NULL,
	`owner` INT NOT NULL,
	`paid` INT UNSIGNED NOT NULL DEFAULT '0',
	`warnings` INT NOT NULL DEFAULT '0',
	`lastwarning` INT UNSIGNED NOT NULL DEFAULT '0',
	`name` VARCHAR(255) NOT NULL,
	`town` INT UNSIGNED NOT NULL DEFAULT '0',
	`size` INT UNSIGNED NOT NULL DEFAULT '0',
	`price` INT UNSIGNED NOT NULL DEFAULT '0',
	`rent` INT UNSIGNED NOT NULL DEFAULT '0',
	`doors` INT UNSIGNED NOT NULL DEFAULT '0',
	`beds` INT UNSIGNED NOT NULL DEFAULT '0',
	`tiles` INT UNSIGNED NOT NULL DEFAULT '0',
	`guild` TINYINT NOT NULL DEFAULT '0',
	`clear` TINYINT NOT NULL DEFAULT '0',
	UNIQUE KEY `id` (`id`)
) ENGINE = InnoDB;

CREATE TABLE IF NOT EXISTS `tile_store`
(
	`house_id` INT UNSIGNED NOT NULL,
	`data` LONGBLOB NOT NULL,
	FOREIGN KEY (`house_id`) REFERENCES `houses` (`id`) ON DELETE CASCADE
) ENGINE = InnoDB;

CREATE TABLE IF NOT EXISTS `house_auctions`
(
	`house_id` INT UNSIGNED NOT NULL,
	`player_id` INT NOT NULL,
	`bid` INT UNSIGNED NOT NULL DEFAULT '0',
	`limit` INT UNSIGNED NOT NULL DEFAULT '0',
	`endtime` BIGINT UNSIGNED NOT NULL DEFAULT '0',
	UNIQUE KEY `house_id` (`house_id`),
	FOREIGN KEY (`house_id`) REFERENCES `houses`(`id`) ON DELETE CASCADE,
	FOREIGN KEY (`player_id`) REFERENCES `players` (`id`) ON DELETE CASCADE
) ENGINE = InnoDB;

CREATE TABLE IF NOT EXISTS `house_lists`
(
	`house_id` INT UNSIGNED NOT NULL,
	`listid` INT NOT NULL,
	`list` TEXT NOT NULL,
	UNIQUE KEY `houses_lists_index` (`house_id`, `listid`),
	FOREIGN KEY (`house_id`) REFERENCES `houses`(`id`) ON DELETE CASCADE
) ENGINE = InnoDB;

CREATE TABLE IF NOT EXISTS `house_data`
(
	`house_id` INT UNSIGNED NOT NULL,
	`data` LONGBLOB NOT NULL,
	UNIQUE KEY `house_id` (`house_id`),
	FOREIGN KEY (`house_id`) REFERENCES `houses`(`id`) ON DELETE CASCADE
) ENGINE = InnoDB;

CREATE TABLE IF NOT EXISTS `tiles`
(
	`id` INT UNSIGNED NOT NULL,
	`house_id` INT UNSIGNED NOT NULL,
	`x` INT UNSIGNED NOT NULL,
	`y` INT UNSIGNED NOT NULL,
	`z` TINYINT UNSIGNED NOT NULL,
	UNIQUE KEY `id` (`id`),
	FOREIGN KEY (`house_id`) REFERENCES `houses`(`id`) ON DELETE CASCADE
) ENGINE = InnoDB;

CREATE TABLE IF NOT EXISTS `tile_items`
(
	`tile_id` INT UNSIGNED NOT NULL,
	`sid` INT NOT NULL,
	`pid` INT NOT NULL DEFAULT '0',
	`itemtype` INT NOT NULL,
	`count` INT NOT NULL DEFAULT '0',
	`attributes` BLOB NOT NULL,
	`serial` VARCHAR(40) NOT NULL DEFAULT '',
	UNIQUE KEY `tile_items_index` (`tile_id`, `sid`),
	FOREIGN KEY (`tile_id`) REFERENCES `tiles`(`id`) ON DELETE CASCADE
) ENGINE = InnoDB;

CREATE TABLE IF NOT EXISTS `guilds`
(
	`id` INT NOT NULL AUTO_INCREMENT,
	`name` VARCHAR(255) NOT NULL,
	`ownerid` INT NOT NULL,
	`creationdata` INT NOT NULL,
	`checkdata` INT NOT NULL,
	`motd` VARCHAR(255) NOT NULL,
	PRIMARY KEY (`id`),
	UNIQUE KEY `name` (`name`)
) ENGINE = InnoDB;

CREATE TABLE IF NOT EXISTS `guild_invites`
(
	`player_id` INT NOT NULL DEFAULT '0',
	`guild_id` INT NOT NULL DEFAULT '0',
	UNIQUE KEY `guild_invites_index` (`player_id`, `guild_id`),
	FOREIGN KEY (`player_id`) REFERENCES `players`(`id`) ON DELETE CASCADE,
	FOREIGN KEY (`guild_id`) REFERENCES `guilds`(`id`) ON DELETE CASCADE
) ENGINE = InnoDB;

CREATE TABLE IF NOT EXISTS `guild_ranks`
(
	`id` INT NOT NULL AUTO_INCREMENT,
	`guild_id` INT NOT NULL,
	`name` VARCHAR(255) NOT NULL,
	`level` INT NOT NULL COMMENT '1 - leader, 2 - vice leader, 3 - member',
	PRIMARY KEY (`id`),
	FOREIGN KEY (`guild_id`) REFERENCES `guilds`(`id`) ON DELETE CASCADE
) ENGINE = InnoDB;

CREATE TABLE IF NOT EXISTS `bans`
(
	`id` INT UNSIGNED NOT NULL auto_increment,
	`type` TINYINT NOT NULL COMMENT '1 - ip, 2 - player, 3 - account, 4 - notation',
	`value` INT UNSIGNED NOT NULL COMMENT 'ip - ip address, player - player_id, account - account_id, notation - account_id',
	`param` INT UNSIGNED NOT NULL COMMENT 'ip - mask, player - type (1 - report, 2 - lock, 3 - ban), account - player, notation - player',
	`active` TINYINT NOT NULL DEFAULT '1',
	`expires` INT NOT NULL DEFAULT '-1',
	`added` INT UNSIGNED NOT NULL,
	`admin_id` INT UNSIGNED NOT NULL DEFAULT '0',
	`comment` TEXT NOT NULL,
	`reason` INT UNSIGNED NOT NULL DEFAULT '0',
	`action` INT UNSIGNED NOT NULL DEFAULT '0',
	`statement` VARCHAR(255) NOT NULL DEFAULT '',
	PRIMARY KEY (`id`)
) ENGINE = InnoDB;

CREATE TABLE IF NOT EXISTS `global_storage`
(
	`key` VARCHAR(255) NOT NULL,
	`value` TEXT NOT NULL,
	UNIQUE KEY `key` (`key`)
) ENGINE = InnoDB;

CREATE TABLE IF NOT EXISTS `server_config`
(
	`config` VARCHAR(255) NOT NULL DEFAULT '',
	`value` VARCHAR(255) NOT NULL DEFAULT '',
	UNIQUE KEY `config` (`config`)
) ENGINE = InnoDB;

CREATE TABLE IF NOT EXISTS `server_motd`
(
	`id` INT UNSIGNED NOT NULL,
	`text` TEXT NOT NULL,
	UNIQUE KEY `id` (`id`)
) ENGINE = InnoDB;

CREATE TABLE IF NOT EXISTS `server_record`
(
	`record` INT NOT NULL,
	`timestamp` BIGINT NOT NULL,
	UNIQUE KEY `server_records_index` (`record`, `timestamp`)
) ENGINE = InnoDB;

CREATE TABLE IF NOT EXISTS `server_reports`
(
	`id` INT NOT NULL AUTO_INCREMENT,
	`player_id` INT NOT NULL DEFAULT '1',
	`posx` INT NOT NULL DEFAULT '0',
	`posy` INT NOT NULL DEFAULT '0',
	`posz` INT NOT NULL DEFAULT '0',
	`timestamp` BIGINT UNSIGNED NOT NULL DEFAULT '0',
	`report` TEXT NOT NULL,
	`reads` INT NOT NULL DEFAULT '0',
	PRIMARY KEY (`id`),
	FOREIGN KEY (`player_id`) REFERENCES `players`(`id`) ON DELETE CASCADE
) ENGINE = InnoDB;

CREATE TABLE IF NOT EXISTS `guild_wars`
(
	`id` INT NOT NULL,
	`guild_id` INT NOT NULL,
	`enemy_id` INT NOT NULL,
	`begin` BIGINT UNSIGNED NOT NULL DEFAULT '0',
	`end` BIGINT UNSIGNED NOT NULL DEFAULT '0',
	`frags` INT NOT NULL DEFAULT '0',
	`payment` BIGINT UNSIGNED NOT NULL DEFAULT '0',
	`guild_kills` INT NOT NULL DEFAULT '0',
	`enemy_kills` INT NOT NULL DEFAULT '0',
	`status` TINYINT NOT NULL DEFAULT '0',
	PRIMARY KEY (`id`),
	FOREIGN KEY (`guild_id`) REFERENCES `guilds`(`id`),
	FOREIGN KEY (`enemy_id`) REFERENCES `guilds`(`id`)
) ENGINE = InnoDB;

DROP TRIGGER IF EXISTS `ondelete_accounts`;
DROP TRIGGER IF EXISTS `oncreate_guilds`;
DROP TRIGGER IF EXISTS `ondelete_guilds`;
DROP TRIGGER IF EXISTS `oncreate_players`;
DROP TRIGGER IF EXISTS `ondelete_players`;

DELIMITER |

CREATE TRIGGER `ondelete_accounts`
BEFORE DELETE
ON `accounts`
FOR EACH ROW
BEGIN
	DELETE FROM `bans` WHERE `type` IN (3, 4) AND `value` = OLD.`id`;
END|

CREATE TRIGGER `oncreate_guilds`
AFTER INSERT
ON `guilds`
FOR EACH ROW
BEGIN
	INSERT INTO `guild_ranks` (`name`, `level`, `guild_id`) VALUES ('Leader', 3, NEW.`id`);
	INSERT INTO `guild_ranks` (`name`, `level`, `guild_id`) VALUES ('Vice-Leader', 2, NEW.`id`);
	INSERT INTO `guild_ranks` (`name`, `level`, `guild_id`) VALUES ('Member', 1, NEW.`id`);
END|

CREATE TRIGGER `ondelete_guilds`
BEFORE DELETE
ON `guilds`
FOR EACH ROW
BEGIN
	UPDATE `players` SET `guildnick` = '', `rank_id` = 0 WHERE `rank_id` IN (SELECT `id` FROM `guild_ranks` WHERE `guild_id` = OLD.`id`);
END|

CREATE TRIGGER `oncreate_players`
AFTER INSERT
ON `players`
FOR EACH ROW
BEGIN
	INSERT INTO `player_skills` (`player_id`, `skillid`, `value`) VALUES (NEW.`id`, 0, 10);
	INSERT INTO `player_skills` (`player_id`, `skillid`, `value`) VALUES (NEW.`id`, 1, 10);
	INSERT INTO `player_skills` (`player_id`, `skillid`, `value`) VALUES (NEW.`id`, 2, 10);
	INSERT INTO `player_skills` (`player_id`, `skillid`, `value`) VALUES (NEW.`id`, 3, 10);
	INSERT INTO `player_skills` (`player_id`, `skillid`, `value`) VALUES (NEW.`id`, 4, 10);
	INSERT INTO `player_skills` (`player_id`, `skillid`, `value`) VALUES (NEW.`id`, 5, 10);
	INSERT INTO `player_skills` (`player_id`, `skillid`, `value`) VALUES (NEW.`id`, 6, 10);
END|

CREATE TRIGGER `ondelete_players`
BEFORE DELETE
ON `players`
FOR EACH ROW
BEGIN
	DELETE FROM `bans` WHERE `type` IN (2, 5) AND `value` = OLD.`id`;
	UPDATE `houses` SET `owner` = 0 WHERE `owner` = OLD.`id`;
END|

DELIMITER ;

-- Inserts
INSERT INTO `accounts` (`name`, `password`, `premdays`) VALUES ('1', '356a192b7913b04c54574d18c28d46e6395428ab', 65535);
INSERT INTO `players` (`name`, `account_id`) VALUES ('Account Manager', 1);
INSERT INTO `server_record` VALUES (0, 0);
