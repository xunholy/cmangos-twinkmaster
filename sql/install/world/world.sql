-- ============================================================
-- Twink Master NPC - Level 19 BiS Vendor
-- Entry: 190012, ScriptName: npc_twinkmaster
-- ============================================================

SET @Entry := 190012;

-- NPC Template (gossip + vendor)
DELETE FROM `creature_template` WHERE `entry` = @Entry;
INSERT INTO `creature_template` (`entry`, `DisplayId1`, `DisplayIdProbability1`, `name`, `subname`, `GossipMenuId`, `minlevel`, `maxlevel`, `faction`, `NpcFlags`, `scale`, `rank`, `DamageSchool`, `MeleeBaseAttackTime`, `RangedBaseAttackTime`, `unitClass`, `unitFlags`, `CreatureType`, `CreatureTypeFlags`, `ScriptName`, `lootid`, `PickpocketLootId`, `SkinningLootId`, `AIName`, `MovementType`, `RacialLeader`, `RegenerateStats`, `MechanicImmuneMask`, `ExtraFlags`) VALUES
(@Entry, 3455, 100, 'Twink Master', 'Level 19 Specialist', 0, 19, 19, 35, 5, 1, 0, 0, 2000, 0, 1, 0, 7, 138936390, 'npc_twinkmaster', 0, 0, 0, '', 0, 0, 1, 0, 0);

-- Spawns: Stormwind Trade District + Orgrimmar Valley of Honor
DELETE FROM `creature` WHERE `id` = @Entry;
INSERT INTO `creature` (`id`, `map`, `spawnMask`, `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecsmin`, `spawntimesecsmax`, `spawndist`, `MovementType`) VALUES
(@Entry, 0, 1, -8830.0, 665.0, 97.9, 0.65, 25, 25, 0, 0),
(@Entry, 1, 1, 1917.5, -4184.8, 43.5, 3.14, 25, 25, 0, 0);

-- NPC Text
SET @TEXT_ID := 50920;
DELETE FROM `npc_text` WHERE `ID` BETWEEN @TEXT_ID AND @TEXT_ID+2;
INSERT INTO `npc_text` (`ID`, `text0_0`) VALUES
(@TEXT_ID,   'Welcome, $N! Looking to gear up for the battleground? I have everything a level 19 champion needs. Browse my wares - everything is on the house!'),
(@TEXT_ID+1, 'Ready to lock in at level 19? I can set your level and lock your experience so you stay in peak form for Warsong Gulch.'),
(@TEXT_ID+2, 'Only the finest equipment for aspiring twinks. May your battles be glorious, $N!');

-- ============================================================
-- Vendor Items: Make free via BuyPrice = 0
-- ============================================================

-- Backup original prices for rollback
DROP TABLE IF EXISTS `custom_twinkmaster_original_prices`;
CREATE TABLE `custom_twinkmaster_original_prices` (
  `entry` int(11) unsigned NOT NULL,
  `original_buy_price` int(11) unsigned NOT NULL DEFAULT 0,
  `original_sell_price` int(11) unsigned NOT NULL DEFAULT 0,
  PRIMARY KEY (`entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_bin;

INSERT IGNORE INTO `custom_twinkmaster_original_prices` (`entry`, `original_buy_price`, `original_sell_price`)
SELECT `entry`, `BuyPrice`, `SellPrice` FROM `item_template`
WHERE `entry` IN (
  1482, 5191, 6448, 6469, 7230, 7429, 6472, 5196, 6505,
  2059, 10399, 5404, 5195, 6320, 2585, 4385, 6087, 5194,
  4381,
  5634, 3387, 2459, 3388, 2454, 2455, 929,
  2515, 3030, 2519, 3033,
  2581, 3531
);

UPDATE `item_template` SET `BuyPrice` = 0, `SellPrice` = 0
WHERE `entry` IN (
  1482, 5191, 6448, 6469, 7230, 7429, 6472, 5196, 6505,
  2059, 10399, 5404, 5195, 6320, 2585, 4385, 6087, 5194,
  4381,
  5634, 3387, 2459, 3388, 2454, 2455, 929,
  2515, 3030, 2519, 3033,
  2581, 3531
);

-- ============================================================
-- NPC Vendor Inventory
-- ============================================================
DELETE FROM `npc_vendor` WHERE `entry` = @Entry;
INSERT INTO `npc_vendor` (`entry`, `item`, `maxcount`, `incrtime`) VALUES
-- Weapons
(@Entry, 1482, 0, 0),   -- Shadowfang (1H Sword, BoE)
(@Entry, 5191, 0, 0),   -- Cruel Barb (1H Sword)
(@Entry, 6448, 0, 0),   -- Tail Spike (Dagger)
(@Entry, 6469, 0, 0),   -- Venomstrike (Bow)
(@Entry, 7230, 0, 0),   -- Smite's Mighty Hammer (2H Mace)
(@Entry, 7429, 0, 0),   -- Twisted Chanter's Staff (Staff)
(@Entry, 6472, 0, 0),   -- Stinging Viper (1H Mace)
(@Entry, 5196, 0, 0),   -- Wingblade (1H Sword)
(@Entry, 6505, 0, 0),   -- Crescent Staff (Staff)
-- Armor
(@Entry, 2059, 0, 0),   -- Sentry Cloak (Back)
(@Entry, 10399, 0, 0),  -- Blackened Defias Armor (Leather Chest)
(@Entry, 5404, 0, 0),   -- Serpent's Shoulders (Leather Shoulders)
(@Entry, 5195, 0, 0),   -- Gold-plated Buckler (Shield)
(@Entry, 6320, 0, 0),   -- Commander's Crest (Shield)
(@Entry, 2585, 0, 0),   -- Feet of the Lynx (Leather Feet)
(@Entry, 4385, 0, 0),   -- Green Tinted Goggles (Leather Head)
(@Entry, 6087, 0, 0),   -- Chausses of Westfall (Mail Legs)
(@Entry, 5194, 0, 0),   -- Deviate Scale Gloves (Leather Gloves)
-- Accessories
(@Entry, 4381, 0, 0),   -- Minor Recombobulator (Trinket)
-- Consumables
(@Entry, 5634, 0, 0),   -- Free Action Potion
(@Entry, 3387, 0, 0),   -- Limited Invulnerability Potion
(@Entry, 2459, 0, 0),   -- Swiftness Potion
(@Entry, 3388, 0, 0),   -- Strong Troll's Blood Potion
(@Entry, 2454, 0, 0),   -- Elixir of Lion's Strength
(@Entry, 2455, 0, 0),   -- Minor Mana Potion
(@Entry, 929,  0, 0),   -- Healing Potion
-- Ammo
(@Entry, 2515, 0, 0),   -- Sharp Arrow
(@Entry, 3030, 0, 0),   -- Razor Arrow
(@Entry, 2519, 0, 0),   -- Heavy Shot
(@Entry, 3033, 0, 0),   -- Solid Shot
-- Bandages
(@Entry, 2581, 0, 0),   -- Heavy Linen Bandage
(@Entry, 3531, 0, 0);   -- Heavy Wool Bandage
