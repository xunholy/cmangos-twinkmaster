#include "TwinkmasterModule.h"
#include "Database/DatabaseEnv.h"
#include "Entities/Player.h"
#include "Entities/GossipDef.h"
#include "Globals/ObjectMgr.h"
#include "Server/WorldSession.h"

namespace cmangos_module
{
    enum TwinkmasterActions
    {
        ACTION_SET_LEVEL_AND_LOCK = 100,
        ACTION_LOCK_XP            = 101,
        ACTION_UNLOCK_XP          = 102,
        ACTION_BROWSE_WARES       = 103,
        ACTION_BROWSE_HONOR       = 104,
        ACTION_BROWSE_RARE        = 105,
        ACTION_CONFIRM_SET_LEVEL  = 110,
        ACTION_MAIN_MENU          = 200,
    };

    static const uint32 NPC_ENTRY_ALLIANCE   = 190012;
    static const uint32 NPC_ENTRY_HORDE      = 190013;
    static const uint32 NPC_TEXT_GREETING    = 50920;
    static const uint32 NPC_TEXT_LEVEL       = 50921;

    static bool IsTwinkmasterNPC(Creature* creature)
    {
        if (!creature) return false;
        uint32 entry = creature->GetEntry();
        return entry == NPC_ENTRY_ALLIANCE || entry == NPC_ENTRY_HORDE;
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
        if (!IsEnabled())
            return;

        LoadVendorCategories();
    }

    void TwinkmasterModule::LoadVendorCategories()
    {
        m_vendorCategories.clear();

        auto result = WorldDatabase.Query(
            "SELECT `item`, `categories` FROM `custom_twinkmaster_vendor_categories`");

        if (!result)
            return;

        uint32 count = 0;
        do
        {
            Field* fields = result->Fetch();
            uint32 itemId = fields[0].GetUInt32();
            uint8 categories = fields[1].GetUInt8();
            m_vendorCategories[itemId] = categories;
            ++count;
        } while (result->NextRow());

        sLog.outString("[Twinkmaster] Loaded %u vendor category entries.", count);
    }

    void TwinkmasterModule::SendFilteredVendorList(Player* player, Creature* creature, uint8 categoryMask)
    {
        if (!player || !creature)
            return;

        VendorItemData const* vItems = creature->GetVendorItems();
        if (!vItems || vItems->GetItemCount() == 0)
        {
            WorldPacket data(SMSG_LIST_INVENTORY, 8 + 1 + 1);
            data << creature->GetObjectGuid();
            data << uint8(0);
            data << uint8(0);
            player->GetSession()->SendPacket(data);
            return;
        }

        uint8 numitems = vItems->GetItemCount();
        uint8 count = 0;

        float discountMod = player->GetReputationPriceDiscount(creature);

        WorldPacket data(SMSG_LIST_INVENTORY, 8 + 1 + numitems * 7 * 4);
        data << creature->GetObjectGuid();

        size_t count_pos = data.wpos();
        data << uint8(count);

        for (uint8 i = 0; i < numitems; ++i)
        {
            VendorItem const* crItem = vItems->GetItem(i);
            if (!crItem)
                continue;

            // Filter by category
            auto it = m_vendorCategories.find(crItem->item);
            if (it == m_vendorCategories.end())
                continue;
            if (!(it->second & categoryMask))
                continue;

            ItemPrototype const* pProto = ObjectMgr::GetItemPrototype(crItem->item);
            if (!pProto)
                continue;

            if (!player->IsGameMaster())
            {
                if ((pProto->AllowableClass & player->getClassMask()) == 0 && pProto->Bonding == BIND_WHEN_PICKED_UP)
                    continue;

                if ((pProto->AllowableRace & player->getRaceMask()) == 0)
                    continue;
            }

            ++count;

            uint32 price = uint32(floor(pProto->BuyPrice * discountMod));

            data << uint32(count);
            data << uint32(crItem->item);
            data << uint32(pProto->DisplayInfoID);
            data << uint32(crItem->maxcount <= 0 ? 0xFFFFFFFF : creature->GetVendorItemCurrentCount(crItem));
            data << uint32(price);
            data << uint32(pProto->MaxDurability);
            data << uint32(pProto->BuyCount);

            if (count >= MAX_VENDOR_ITEMS)
                break;
        }

        if (count == 0)
        {
            WorldPacket emptyData(SMSG_LIST_INVENTORY, 8 + 1 + 1);
            emptyData << creature->GetObjectGuid();
            emptyData << uint8(0);
            emptyData << uint8(0);
            player->GetSession()->SendPacket(emptyData);
            return;
        }

        data.put<uint8>(count_pos, count);
        player->GetSession()->SendPacket(data);
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

        // Lock XP and mark level_set = 1
        uint32 guid = player->GetGUIDLow();
        m_xpLockedPlayers.insert(guid);

        CharacterDatabase.PExecute(
            "REPLACE INTO `custom_twinkmaster_player_config` (`guid`, `xp_locked`, `level_set`) VALUES (%u, 1, 1)",
            guid);

        player->GetSession()->SendNotification("Your level has been set to %u and XP gain is now locked.", targetLevel);
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

        playerMenu->GetGossipMenu().AddMenuItem(GOSSIP_ICON_VENDOR, "Browse Best-in-Slot gear", GOSSIP_SENDER_MAIN, ACTION_BROWSE_WARES, "", false);
        playerMenu->GetGossipMenu().AddMenuItem(GOSSIP_ICON_VENDOR, "Browse Honor rewards", GOSSIP_SENDER_MAIN, ACTION_BROWSE_HONOR, "", false);
        playerMenu->GetGossipMenu().AddMenuItem(GOSSIP_ICON_VENDOR, "Browse Rare acquisitions", GOSSIP_SENDER_MAIN, ACTION_BROWSE_RARE, "", false);

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
            case ACTION_BROWSE_WARES:
            {
                if (m_vendorCategories.empty())
                    player->GetSession()->SendListInventory(creature->GetObjectGuid());
                else
                    SendFilteredVendorList(player, creature, VENDOR_CAT_BIS);
                break;
            }
            case ACTION_BROWSE_HONOR:
            {
                SendFilteredVendorList(player, creature, VENDOR_CAT_HONOR);
                break;
            }
            case ACTION_BROWSE_RARE:
            {
                SendFilteredVendorList(player, creature, VENDOR_CAT_RARE);
                break;
            }
            case ACTION_MAIN_MENU:
            {
                OnPreGossipHello(player, creature);
                break;
            }
            default:
                playerMenu->CloseGossip();
                break;
        }

        return true;
    }
}
