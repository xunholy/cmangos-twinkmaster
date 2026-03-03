#include "TwinkmasterModule.h"
#include "Database/DatabaseEnv.h"
#include "Entities/Player.h"
#include "Entities/GossipDef.h"

namespace cmangos_module
{
    enum TwinkmasterActions
    {
        ACTION_SET_LEVEL_AND_LOCK = 100,
        ACTION_LOCK_XP            = 101,
        ACTION_UNLOCK_XP          = 102,
        ACTION_BROWSE_WARES       = 103,
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

        player->PlayerTalkClass->ClearMenus();

        uint32 targetLevel = GetTargetLevel();
        bool isLocked = IsXpLocked(player->GetGUIDLow());

        if (!isLocked || player->GetLevel() != targetLevel)
        {
            char buf[128];
            snprintf(buf, sizeof(buf), "Set my level to %u and lock XP", targetLevel);
            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_BATTLE, buf, GOSSIP_SENDER_MAIN, ACTION_SET_LEVEL_AND_LOCK);
        }

        if (isLocked)
            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_INTERACT_1, "Unlock my XP gain", GOSSIP_SENDER_MAIN, ACTION_UNLOCK_XP);
        else
            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_INTERACT_1, "Lock my XP gain", GOSSIP_SENDER_MAIN, ACTION_LOCK_XP);

        player->ADD_GOSSIP_ITEM(GOSSIP_ICON_VENDOR, "Browse your wares", GOSSIP_SENDER_MAIN, ACTION_BROWSE_WARES);

        player->PlayerTalkClass->SendGossipMenu(NPC_TEXT_GREETING, creature->GetObjectGuid());
        return true;
    }

    bool TwinkmasterModule::OnGossipSelect(Player* player, Creature* creature, uint32 sender, uint32 action, const std::string& code, uint32 gossipListId)
    {
        if (!IsEnabled() || !IsTwinkmasterNPC(creature))
            return false;

        player->PlayerTalkClass->ClearMenus();

        switch (action)
        {
            case ACTION_SET_LEVEL_AND_LOCK:
            {
                uint32 targetLevel = GetTargetLevel();
                char buf[256];
                snprintf(buf, sizeof(buf), "Yes, set me to level %u and lock my XP!", targetLevel);

                player->ADD_GOSSIP_ITEM(GOSSIP_ICON_BATTLE, buf, GOSSIP_SENDER_MAIN, ACTION_CONFIRM_SET_LEVEL);
                player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "Maybe later...", GOSSIP_SENDER_MAIN, ACTION_MAIN_MENU);

                player->PlayerTalkClass->SendGossipMenu(NPC_TEXT_LEVEL, creature->GetObjectGuid());
                break;
            }
            case ACTION_CONFIRM_SET_LEVEL:
            {
                SetLevelAndLock(player);
                player->PlayerTalkClass->SendCloseGossip();
                break;
            }
            case ACTION_LOCK_XP:
            {
                LockXP(player);
                player->PlayerTalkClass->SendCloseGossip();
                break;
            }
            case ACTION_UNLOCK_XP:
            {
                UnlockXP(player);
                player->PlayerTalkClass->SendCloseGossip();
                break;
            }
            case ACTION_BROWSE_WARES:
            {
                player->GetSession()->SendListInventory(creature->GetObjectGuid());
                break;
            }
            case ACTION_MAIN_MENU:
            {
                OnPreGossipHello(player, creature);
                break;
            }
            default:
                player->PlayerTalkClass->SendCloseGossip();
                break;
        }

        return true;
    }
}
