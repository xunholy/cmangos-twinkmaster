#include "TwinkmasterModule.h"
#include "Database/DatabaseEnv.h"
#include "Entities/Player.h"
#include "Entities/GossipDef.h"
#include "AI/ScriptDevAI/include/sc_gossip.h"
#include "Server/WorldPacket.h"
#include "Server/Opcodes.h"
#include "Globals/ObjectMgr.h"

namespace cmangos_module
{
    enum TwinkmasterActions
    {
        ACTION_SET_LEVEL_AND_LOCK = 100,
        ACTION_LOCK_XP            = 101,
        ACTION_UNLOCK_XP          = 102,
        ACTION_BROWSE_MENU        = 103,
        ACTION_RESPEC             = 104,
        ACTION_BUFF               = 105,
        ACTION_CONFIRM_SET_LEVEL  = 110,
        ACTION_MAIN_MENU          = 200,
        ACTION_ENCHANT_MENU       = 106,
        ACTION_BROWSE_BIS         = 301,
        ACTION_BROWSE_CONSUMABLES = 302,
        ACTION_BROWSE_HONOR       = 303,
        ACTION_BROWSE_INSANE      = 304,
        ACTION_ENCHANT_SLOT_BASE  = 400,
        ACTION_ENCHANT_SLOT_END   = 419,
        ACTION_ENCHANT_BASE       = 500,
    };

    enum VendorCategory : uint8
    {
        CATEGORY_BIS          = 1,
        CATEGORY_CONSUMABLES  = 2,
        CATEGORY_HONOR        = 3,
        CATEGORY_INSANE       = 4,
    };

    static const uint32 NPC_ENTRY_ALLIANCE   = 190012;
    static const uint32 NPC_ENTRY_HORDE      = 190013;
    static const uint32 NPC_TEXT_GREETING     = 50920;
    static const uint32 NPC_TEXT_LEVEL        = 50921;
    static const uint32 NPC_TEXT_VENDOR       = 50922;

    // Profession data: skill ID, apprentice spell, journeyman spell
    struct ProfessionInfo
    {
        uint16 skillId;
        uint32 apprenticeSpell;
        uint32 journeymanSpell;
    };

    static const ProfessionInfo professions[] =
    {
        { 129, 3279, 3280 },  // First Aid
        { 356, 7733, 7734 },  // Fishing
        { 185, 2550, 3412 },  // Cooking
        { 202, 4039, 4040 },  // Engineering
    };

    // Weapon skill IDs per class (null-terminated)
    static const uint16 WEAPON_SKILLS_WARRIOR[]  = { 43, 44, 45, 46, 54, 55, 136, 160, 162, 172, 173, 176, 226, 229, 473, 0 };
    static const uint16 WEAPON_SKILLS_PALADIN[]  = { 43, 44, 54, 55, 160, 162, 229, 0 };
    static const uint16 WEAPON_SKILLS_HUNTER[]   = { 43, 44, 45, 46, 55, 136, 162, 172, 173, 176, 226, 229, 473, 0 };
    static const uint16 WEAPON_SKILLS_ROGUE[]    = { 43, 45, 46, 54, 162, 173, 176, 226, 473, 0 };
    static const uint16 WEAPON_SKILLS_PRIEST[]   = { 54, 136, 162, 173, 228, 0 };
    static const uint16 WEAPON_SKILLS_SHAMAN[]   = { 44, 54, 136, 160, 162, 172, 173, 473, 0 };
    static const uint16 WEAPON_SKILLS_MAGE[]     = { 43, 136, 162, 173, 228, 0 };
    static const uint16 WEAPON_SKILLS_WARLOCK[]  = { 43, 136, 162, 173, 228, 0 };
    static const uint16 WEAPON_SKILLS_DRUID[]    = { 54, 136, 160, 162, 173, 473, 0 };

    static const uint16 SKILL_DEFENSE = 95;

    static bool IsTwinkmasterNPC(Creature* creature)
    {
        if (!creature) return false;
        uint32 entry = creature->GetEntry();
        return entry == NPC_ENTRY_ALLIANCE || entry == NPC_ENTRY_HORDE;
    }

    struct EnchantOption
    {
        const char* name;
        uint32 enchantId;
        uint8 equipSlot;
        bool requires2H;
    };

    static const EnchantOption ENCHANT_OPTIONS[] =
    {
        // Chest
        { "Greater Stats (+4 all)",    1891, EQUIPMENT_SLOT_CHEST,    false },
        { "Greater Health (+100 HP)",  1892, EQUIPMENT_SLOT_CHEST,    false },

        // Cloak
        { "Dodge (+1%)",               2622, EQUIPMENT_SLOT_BACK,     false },
        { "Subtlety (-2% threat)",     2621, EQUIPMENT_SLOT_BACK,     false },
        { "Armor (+70)",               1889, EQUIPMENT_SLOT_BACK,     false },
        { "Agility (+3)",               849, EQUIPMENT_SLOT_BACK,     false },
        { "Greater Resistance (+5)",   1888, EQUIPMENT_SLOT_BACK,     false },

        // Bracers
        { "Stamina (+9)",              1886, EQUIPMENT_SLOT_WRISTS,   false },
        { "Strength (+9)",             1885, EQUIPMENT_SLOT_WRISTS,   false },
        { "Healing (+24)",             2566, EQUIPMENT_SLOT_WRISTS,   false },
        { "Intellect (+7)",            1883, EQUIPMENT_SLOT_WRISTS,   false },
        { "MP5 (+4)",                  2565, EQUIPMENT_SLOT_WRISTS,   false },

        // Gloves
        { "Agility (+7)",              2564, EQUIPMENT_SLOT_HANDS,    false },
        { "Fire Power (+20)",          2616, EQUIPMENT_SLOT_HANDS,    false },
        { "Frost Power (+20)",         2615, EQUIPMENT_SLOT_HANDS,    false },
        { "Shadow Power (+20)",        2614, EQUIPMENT_SLOT_HANDS,    false },
        { "Healing (+30)",             2617, EQUIPMENT_SLOT_HANDS,    false },

        // Boots
        { "Agility (+7)",               904, EQUIPMENT_SLOT_FEET,     false },
        { "Minor Speed",                911, EQUIPMENT_SLOT_FEET,     false },
        { "Stamina (+7)",               929, EQUIPMENT_SLOT_FEET,     false },

        // Weapon (mainhand) — 1H enchants
        { "Crusader",                  1900, EQUIPMENT_SLOT_MAINHAND, false },
        { "Agility (+15, 1H)",         2564, EQUIPMENT_SLOT_MAINHAND, false },
        { "Spellpower (+30)",          2504, EQUIPMENT_SLOT_MAINHAND, false },
        { "Healing (+55)",             2505, EQUIPMENT_SLOT_MAINHAND, false },
        { "Lifesteal",                 1898, EQUIPMENT_SLOT_MAINHAND, false },
        { "Fiery Weapon",               803, EQUIPMENT_SLOT_MAINHAND, false },
        { "Icy Chill",                 1894, EQUIPMENT_SLOT_MAINHAND, false },
        { "Demonslaying",               912, EQUIPMENT_SLOT_MAINHAND, false },
        { "Agility (+25, 2H only)",    2646, EQUIPMENT_SLOT_MAINHAND, true  },
        { "Intellect (+22, 2H only)",  2568, EQUIPMENT_SLOT_MAINHAND, true  },
        { "Spirit (+20, 2H only)",     2567, EQUIPMENT_SLOT_MAINHAND, true  },

        // Shield/Offhand
        { "Spirit (+7)",               1890, EQUIPMENT_SLOT_OFFHAND,  false },
        { "Stamina (+7)",               929, EQUIPMENT_SLOT_OFFHAND,  false },
        { "Frost Resistance (+8)",      926, EQUIPMENT_SLOT_OFFHAND,  false },
        { "Shield Spike (26-38 dmg)",  1704, EQUIPMENT_SLOT_OFFHAND,  false },
    };

    static const uint32 ENCHANT_OPTIONS_COUNT = sizeof(ENCHANT_OPTIONS) / sizeof(ENCHANT_OPTIONS[0]);

    static const uint16* GetWeaponSkillsForClass(uint8 classId)
    {
        switch (classId)
        {
            case 1:  return WEAPON_SKILLS_WARRIOR;
            case 2:  return WEAPON_SKILLS_PALADIN;
            case 3:  return WEAPON_SKILLS_HUNTER;
            case 4:  return WEAPON_SKILLS_ROGUE;
            case 5:  return WEAPON_SKILLS_PRIEST;
            case 7:  return WEAPON_SKILLS_SHAMAN;
            case 8:  return WEAPON_SKILLS_MAGE;
            case 9:  return WEAPON_SKILLS_WARLOCK;
            case 11: return WEAPON_SKILLS_DRUID;
            default: return nullptr;
        }
    }

    TwinkmasterModule::TwinkmasterModule()
    : Module("Twinkmaster", new TwinkmasterModuleConfig())
    {
    }

    const TwinkmasterModuleConfig* TwinkmasterModule::GetConfig() const
    {
        return (const TwinkmasterModuleConfig*)Module::GetConfig();
    }

    bool TwinkmasterModule::IsEnabled() const
    {
        return GetConfig()->enabled;
    }

    uint32 TwinkmasterModule::GetTargetLevel() const
    {
        return GetConfig()->targetLevel;
    }

    bool TwinkmasterModule::IsXpLocked(uint32 guid) const
    {
        return m_xpLockedPlayers.count(guid) > 0;
    }

    void TwinkmasterModule::OnInitialize()
    {
        LoadVendorCategories();
    }

    void TwinkmasterModule::LoadVendorCategories()
    {
        m_categoryItems.clear();

        auto result = WorldDatabase.PQuery("SELECT item, categories FROM custom_twinkmaster_vendor_categories");
        if (!result)
            return;

        uint32 count = 0;
        do
        {
            Field* fields = result->Fetch();
            uint32 itemId = fields[0].GetUInt32();
            uint8 category = fields[1].GetUInt8();
            m_categoryItems[category].push_back(itemId);
            ++count;
        } while (result->NextRow());
    }

    void TwinkmasterModule::OnLoadFromDB(Player* player)
    {
        if (!IsEnabled() || !player)
            return;

        uint32 guid = player->GetGUIDLow();

        auto result = CharacterDatabase.PQuery(
            "SELECT `xp_locked` FROM `custom_twinkmaster_player_config` WHERE `guid` = %u", guid);

        if (result)
        {
            Field* fields = result->Fetch();
            if (fields[0].GetBool())
                m_xpLockedPlayers.insert(guid);
            else
                m_xpLockedPlayers.erase(guid);
        }
    }

    void TwinkmasterModule::OnLogOut(Player* player)
    {
        if (player)
            m_xpLockedPlayers.erase(player->GetGUIDLow());
    }

    bool TwinkmasterModule::OnPreGiveXP(Player* player, uint32& xp, Creature* victim)
    {
        if (!IsEnabled() || !player)
            return false;

        if (IsXpLocked(player->GetGUIDLow()))
        {
            xp = 0;
            return true;
        }

        return false;
    }

    void TwinkmasterModule::LearnProfessions(Player* player)
    {
        for (const auto& prof : professions)
        {
            player->learnSpell(prof.apprenticeSpell, false);
            player->learnSpell(prof.journeymanSpell, false);
            player->SetSkill(prof.skillId, 150, 150);
        }
    }

    void TwinkmasterModule::LearnWeaponSkills(Player* player)
    {
        uint16 maxSkill = GetTargetLevel() * 5;  // 95 at level 19

        const uint16* skills = GetWeaponSkillsForClass(player->getClass());
        if (skills)
        {
            for (const uint16* s = skills; *s; ++s)
                player->SetSkill(*s, maxSkill, maxSkill);
        }

        player->SetSkill(SKILL_DEFENSE, maxSkill, maxSkill);
    }

    void TwinkmasterModule::LearnClassSpells(Player* player)
    {
        uint32 classId = player->getClass();
        uint32 targetLevel = GetTargetLevel();

        auto result = WorldDatabase.PQuery(
            "SELECT DISTINCT ntt.spell FROM npc_trainer_template ntt "
            "JOIN creature_template ct ON ct.trainertemplateid = ntt.entry "
            "WHERE ct.trainer_class = %u AND ntt.reqlevel <= %u AND ntt.reqlevel > 0",
            classId, targetLevel);

        if (!result)
            return;

        do
        {
            Field* fields = result->Fetch();
            uint32 spellId = fields[0].GetUInt32();
            if (!player->HasSpell(spellId))
                player->learnSpell(spellId, false);
        } while (result->NextRow());
    }

    void TwinkmasterModule::SetLevelAndLock(Player* player)
    {
        if (!player)
            return;

        uint32 targetLevel = GetTargetLevel();

        if (player->GetLevel() != targetLevel)
        {
            player->GiveLevel(targetLevel);
            player->InitTalentForLevel();
            player->SetUInt32Value(PLAYER_XP, 0);
        }

        LearnProfessions(player);
        LearnWeaponSkills(player);
        LearnClassSpells(player);

        // Riding Turtle mount (no level requirement)
        player->learnSpell(30174, false);

        uint32 guid = player->GetGUIDLow();
        m_xpLockedPlayers.insert(guid);

        CharacterDatabase.PExecute(
            "REPLACE INTO `custom_twinkmaster_player_config` (`guid`, `xp_locked`, `level_set`) VALUES (%u, 1, 1)",
            guid);

        player->GetSession()->SendNotification(
            "Level set to %u, XP locked. Mount, professions, weapon skills, and class spells learned!",
            targetLevel);
    }

    void TwinkmasterModule::LockXP(Player* player)
    {
        if (!player)
            return;

        uint32 guid = player->GetGUIDLow();

        if (m_xpLockedPlayers.count(guid))
        {
            player->GetSession()->SendNotification("Your XP is already locked.");
            return;
        }

        m_xpLockedPlayers.insert(guid);

        CharacterDatabase.PExecute(
            "REPLACE INTO `custom_twinkmaster_player_config` (`guid`, `xp_locked`, `level_set`) VALUES (%u, 1, 0)",
            guid);

        player->GetSession()->SendNotification("XP gain is now locked.");
    }

    void TwinkmasterModule::UnlockXP(Player* player)
    {
        if (!player)
            return;

        uint32 guid = player->GetGUIDLow();

        if (!m_xpLockedPlayers.count(guid))
        {
            player->GetSession()->SendNotification("Your XP is not locked.");
            return;
        }

        m_xpLockedPlayers.erase(guid);

        CharacterDatabase.PExecute(
            "UPDATE `custom_twinkmaster_player_config` SET `xp_locked` = 0 WHERE `guid` = %u",
            guid);

        player->GetSession()->SendNotification("XP gain is now unlocked.");
    }

    void TwinkmasterModule::ApplyBuffPackage(Player* player)
    {
        if (!player)
            return;

        // Clear all auras (debuffs + old buffs — we reapply fresh)
        player->RemoveAllAuras();

        // Free full gear repair
        player->DurabilityRepairAll(false, 0.0f);

        // Apply world buffs and class buffs
        static const uint32 BUFF_SPELLS[] =
        {
            // World buffs
            22888,  // Rallying Cry of the Dragonslayer
            24425,  // Spirit of Zandalar
            16609,  // Warchief's Blessing
            15366,  // Songflower Serenade
            22817,  // Fengus' Ferocity (DM Tribute)
            22818,  // Mol'dar's Moxie (DM Tribute)
            22819,  // Slip'kik's Savvy (DM Tribute)
            // Class buffs (max vanilla rank)
            9887,   // Mark of the Wild Rank 7
            10940,  // Power Word: Fortitude Rank 6
            10158,  // Arcane Intellect Rank 5
            0       // sentinel
        };

        for (const uint32* spell = BUFF_SPELLS; *spell; ++spell)
            player->CastSpell(player, *spell, true);

        player->GetSession()->SendNotification("Gear repaired, debuffs removed, all buffs applied!");
    }

    void TwinkmasterModule::SendFilteredVendorInventory(Player* player, Creature* creature, uint8 category)
    {
        auto it = m_categoryItems.find(category);
        if (it == m_categoryItems.end() || it->second.empty())
        {
            player->GetSession()->SendNotification("No items available in this category.");
            return;
        }

        const std::vector<uint32>& items = it->second;
        uint32 classMask = uint32(1) << (player->getClass() - 1);
        uint32 raceMask = uint32(1) << (player->getRace() - 1);

        WorldPacket data(SMSG_LIST_INVENTORY, 8 + 1 + items.size() * 7 * 4);
        data << creature->GetObjectGuid();
        size_t countPos = data.wpos();
        data << uint8(0);  // placeholder for item count

        uint8 count = 0;
        for (uint32 itemId : items)
        {
            const ItemPrototype* pProto = sObjectMgr.GetItemPrototype(itemId);
            if (!pProto)
                continue;

            // Skip BoP items not usable by this class
            if (pProto->Bonding == 1 && !(pProto->AllowableClass & classMask))
                continue;

            // Skip items not usable by this race
            if (!(pProto->AllowableRace & raceMask))
                continue;

            if (count >= 255)
                break;

            ++count;
            data << uint32(count);              // 1-based slot index
            data << uint32(itemId);             // item entry
            data << uint32(pProto->DisplayInfoID);
            data << uint32(0xFFFFFFFF);         // infinite stock
            data << uint32(0);                  // price (free)
            data << uint32(pProto->MaxDurability);
            data << uint32(pProto->BuyCount);
        }

        if (count == 0)
        {
            player->GetSession()->SendNotification("No items available for your class in this category.");
            return;
        }

        data.put<uint8>(countPos, count);
        player->GetSession()->SendPacket(&data);
    }

    void TwinkmasterModule::ShowBrowseMenu(Player* player, Creature* creature)
    {
        PlayerMenu* playerMenu = player->GetPlayerMenu();
        if (!playerMenu)
            return;

        playerMenu->ClearMenus();

        playerMenu->GetGossipMenu().AddMenuItem(GOSSIP_ICON_BATTLE,    "BiS Gear",     GOSSIP_SENDER_MAIN, ACTION_BROWSE_BIS, "", false);
        playerMenu->GetGossipMenu().AddMenuItem(GOSSIP_ICON_VENDOR,    "Consumables",  GOSSIP_SENDER_MAIN, ACTION_BROWSE_CONSUMABLES, "", false);
        playerMenu->GetGossipMenu().AddMenuItem(GOSSIP_ICON_BATTLE,    "Honor Gear",   GOSSIP_SENDER_MAIN, ACTION_BROWSE_HONOR, "", false);
        playerMenu->GetGossipMenu().AddMenuItem(GOSSIP_ICON_DOT,       "Insane",       GOSSIP_SENDER_MAIN, ACTION_BROWSE_INSANE, "", false);
        playerMenu->GetGossipMenu().AddMenuItem(GOSSIP_ICON_CHAT,      "Back",         GOSSIP_SENDER_MAIN, ACTION_MAIN_MENU, "", false);

        playerMenu->SendGossipMenu(NPC_TEXT_VENDOR, creature->GetObjectGuid());
    }

    void TwinkmasterModule::ApplyEnchant(Player* player, Item* item, uint32 enchantId)
    {
        if (!item)
        {
            player->GetSession()->SendNotification("No item equipped in that slot.");
            return;
        }
        player->ApplyEnchantment(item, PERM_ENCHANTMENT_SLOT, false);
        item->ClearEnchantment(PERM_ENCHANTMENT_SLOT);
        item->SetEnchantment(PERM_ENCHANTMENT_SLOT, enchantId, 0, 0);
        player->ApplyEnchantment(item, PERM_ENCHANTMENT_SLOT, true);
        player->GetSession()->SendNotification("%s enchanted!", item->GetProto()->Name1);
    }

    void TwinkmasterModule::ShowEnchantSlotMenu(Player* player, Creature* creature)
    {
        PlayerMenu* playerMenu = player->GetPlayerMenu();
        if (!playerMenu)
            return;

        playerMenu->ClearMenus();

        playerMenu->GetGossipMenu().AddMenuItem(GOSSIP_ICON_INTERACT_2, "Chest",          GOSSIP_SENDER_MAIN, ACTION_ENCHANT_SLOT_BASE + EQUIPMENT_SLOT_CHEST, "", false);
        playerMenu->GetGossipMenu().AddMenuItem(GOSSIP_ICON_INTERACT_2, "Cloak",           GOSSIP_SENDER_MAIN, ACTION_ENCHANT_SLOT_BASE + EQUIPMENT_SLOT_BACK, "", false);
        playerMenu->GetGossipMenu().AddMenuItem(GOSSIP_ICON_INTERACT_2, "Bracers",         GOSSIP_SENDER_MAIN, ACTION_ENCHANT_SLOT_BASE + EQUIPMENT_SLOT_WRISTS, "", false);
        playerMenu->GetGossipMenu().AddMenuItem(GOSSIP_ICON_INTERACT_2, "Gloves",          GOSSIP_SENDER_MAIN, ACTION_ENCHANT_SLOT_BASE + EQUIPMENT_SLOT_HANDS, "", false);
        playerMenu->GetGossipMenu().AddMenuItem(GOSSIP_ICON_INTERACT_2, "Boots",           GOSSIP_SENDER_MAIN, ACTION_ENCHANT_SLOT_BASE + EQUIPMENT_SLOT_FEET, "", false);
        playerMenu->GetGossipMenu().AddMenuItem(GOSSIP_ICON_INTERACT_2, "Weapon",          GOSSIP_SENDER_MAIN, ACTION_ENCHANT_SLOT_BASE + EQUIPMENT_SLOT_MAINHAND, "", false);
        playerMenu->GetGossipMenu().AddMenuItem(GOSSIP_ICON_INTERACT_2, "Shield/Offhand",  GOSSIP_SENDER_MAIN, ACTION_ENCHANT_SLOT_BASE + EQUIPMENT_SLOT_OFFHAND, "", false);
        playerMenu->GetGossipMenu().AddMenuItem(GOSSIP_ICON_CHAT,       "Back",            GOSSIP_SENDER_MAIN, ACTION_MAIN_MENU, "", false);

        playerMenu->SendGossipMenu(NPC_TEXT_GREETING, creature->GetObjectGuid());
    }

    void TwinkmasterModule::ShowEnchantOptions(Player* player, Creature* creature, uint8 equipSlot)
    {
        PlayerMenu* playerMenu = player->GetPlayerMenu();
        if (!playerMenu)
            return;

        playerMenu->ClearMenus();

        for (uint32 i = 0; i < ENCHANT_OPTIONS_COUNT; ++i)
        {
            if (ENCHANT_OPTIONS[i].equipSlot == equipSlot)
            {
                playerMenu->GetGossipMenu().AddMenuItem(GOSSIP_ICON_INTERACT_2, ENCHANT_OPTIONS[i].name,
                    GOSSIP_SENDER_MAIN, ACTION_ENCHANT_BASE + i, "", false);
            }
        }

        playerMenu->GetGossipMenu().AddMenuItem(GOSSIP_ICON_CHAT, "Back", GOSSIP_SENDER_MAIN, ACTION_ENCHANT_MENU, "", false);

        playerMenu->SendGossipMenu(NPC_TEXT_GREETING, creature->GetObjectGuid());
    }

    bool TwinkmasterModule::OnPreGossipHello(Player* player, Creature* creature)
    {
        if (!IsEnabled() || !IsTwinkmasterNPC(creature))
            return false;

        PlayerMenu* playerMenu = player->GetPlayerMenu();
        if (!playerMenu)
            return false;

        playerMenu->ClearMenus();

        uint32 targetLevel = GetTargetLevel();
        bool isLocked = IsXpLocked(player->GetGUIDLow());

        if (!isLocked || player->GetLevel() != targetLevel)
        {
            char buf[128];
            snprintf(buf, sizeof(buf), "Set my level to %u and lock XP", targetLevel);
            playerMenu->GetGossipMenu().AddMenuItem(GOSSIP_ICON_BATTLE, buf, GOSSIP_SENDER_MAIN, ACTION_SET_LEVEL_AND_LOCK, "", false);
        }

        if (isLocked)
            playerMenu->GetGossipMenu().AddMenuItem(GOSSIP_ICON_INTERACT_1, "Unlock my XP gain", GOSSIP_SENDER_MAIN, ACTION_UNLOCK_XP, "", false);
        else
            playerMenu->GetGossipMenu().AddMenuItem(GOSSIP_ICON_INTERACT_1, "Lock my XP gain", GOSSIP_SENDER_MAIN, ACTION_LOCK_XP, "", false);

        playerMenu->GetGossipMenu().AddMenuItem(GOSSIP_ICON_VENDOR, "Browse your wares", GOSSIP_SENDER_MAIN, ACTION_BROWSE_MENU, "", false);
        playerMenu->GetGossipMenu().AddMenuItem(GOSSIP_ICON_TRAINER, "Reset my talents", GOSSIP_SENDER_MAIN, ACTION_RESPEC, "", false);
        playerMenu->GetGossipMenu().AddMenuItem(GOSSIP_ICON_INTERACT_2, "Buff me up! (repairs gear, removes debuffs)", GOSSIP_SENDER_MAIN, ACTION_BUFF, "", false);
        playerMenu->GetGossipMenu().AddMenuItem(GOSSIP_ICON_INTERACT_2, "Enchant my gear", GOSSIP_SENDER_MAIN, ACTION_ENCHANT_MENU, "", false);

        playerMenu->SendGossipMenu(NPC_TEXT_GREETING, creature->GetObjectGuid());
        return true;
    }

    bool TwinkmasterModule::OnGossipSelect(Player* player, Creature* creature, uint32 sender, uint32 action, const std::string& code, uint32 gossipListId)
    {
        if (!IsEnabled() || !IsTwinkmasterNPC(creature))
            return false;

        PlayerMenu* playerMenu = player->GetPlayerMenu();
        if (!playerMenu)
            return false;

        playerMenu->ClearMenus();

        switch (action)
        {
            case ACTION_SET_LEVEL_AND_LOCK:
            {
                uint32 targetLevel = GetTargetLevel();
                char buf[256];
                snprintf(buf, sizeof(buf), "Yes, set me to level %u and lock my XP!", targetLevel);

                playerMenu->GetGossipMenu().AddMenuItem(GOSSIP_ICON_BATTLE, buf, GOSSIP_SENDER_MAIN, ACTION_CONFIRM_SET_LEVEL, "", false);
                playerMenu->GetGossipMenu().AddMenuItem(GOSSIP_ICON_CHAT, "Maybe later...", GOSSIP_SENDER_MAIN, ACTION_MAIN_MENU, "", false);

                playerMenu->SendGossipMenu(NPC_TEXT_LEVEL, creature->GetObjectGuid());
                break;
            }
            case ACTION_CONFIRM_SET_LEVEL:
            {
                SetLevelAndLock(player);
                playerMenu->CloseGossip();
                break;
            }
            case ACTION_LOCK_XP:
            {
                LockXP(player);
                playerMenu->CloseGossip();
                break;
            }
            case ACTION_UNLOCK_XP:
            {
                UnlockXP(player);
                playerMenu->CloseGossip();
                break;
            }
            case ACTION_BROWSE_MENU:
            {
                ShowBrowseMenu(player, creature);
                break;
            }
            case ACTION_BROWSE_BIS:
            {
                SendFilteredVendorInventory(player, creature, CATEGORY_BIS);
                break;
            }
            case ACTION_BROWSE_CONSUMABLES:
            {
                SendFilteredVendorInventory(player, creature, CATEGORY_CONSUMABLES);
                break;
            }
            case ACTION_BROWSE_HONOR:
            {
                SendFilteredVendorInventory(player, creature, CATEGORY_HONOR);
                break;
            }
            case ACTION_BROWSE_INSANE:
            {
                SendFilteredVendorInventory(player, creature, CATEGORY_INSANE);
                break;
            }
            case ACTION_RESPEC:
            {
                player->ResetTalents(true);
                player->GetSession()->SendNotification("Talents have been reset.");
                playerMenu->CloseGossip();
                break;
            }
            case ACTION_BUFF:
            {
                ApplyBuffPackage(player);
                playerMenu->CloseGossip();
                break;
            }
            case ACTION_ENCHANT_MENU:
            {
                ShowEnchantSlotMenu(player, creature);
                break;
            }
            case ACTION_MAIN_MENU:
            {
                OnPreGossipHello(player, creature);
                break;
            }
            default:
            {
                if (action >= ACTION_ENCHANT_BASE)
                {
                    uint32 idx = action - ACTION_ENCHANT_BASE;
                    if (idx < ENCHANT_OPTIONS_COUNT)
                    {
                        const EnchantOption& opt = ENCHANT_OPTIONS[idx];
                        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, opt.equipSlot);

                        if (opt.requires2H)
                        {
                            if (!item || !item->GetProto())
                            {
                                player->GetSession()->SendNotification("No item equipped in that slot.");
                                playerMenu->CloseGossip();
                                break;
                            }
                            uint32 subClass = item->GetProto()->SubClass;
                            if (subClass != ITEM_SUBCLASS_WEAPON_AXE2 &&
                                subClass != ITEM_SUBCLASS_WEAPON_MACE2 &&
                                subClass != ITEM_SUBCLASS_WEAPON_SWORD2 &&
                                subClass != ITEM_SUBCLASS_WEAPON_POLEARM &&
                                subClass != ITEM_SUBCLASS_WEAPON_STAFF)
                            {
                                player->GetSession()->SendNotification("Requires a two-handed weapon.");
                                playerMenu->CloseGossip();
                                break;
                            }
                        }

                        ApplyEnchant(player, item, opt.enchantId);
                    }
                    playerMenu->CloseGossip();
                }
                else if (action >= ACTION_ENCHANT_SLOT_BASE && action <= ACTION_ENCHANT_SLOT_END)
                {
                    uint8 slot = action - ACTION_ENCHANT_SLOT_BASE;
                    ShowEnchantOptions(player, creature, slot);
                }
                else
                {
                    playerMenu->CloseGossip();
                }
                break;
            }
        }

        return true;
    }
}
