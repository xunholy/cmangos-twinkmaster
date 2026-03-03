#include "TwinkmasterModule.h"
#include "Entities/GossipDef.h"
#include "Entities/Player.h"
#include "Globals/ObjectMgr.h"
#include "World/World.h"

enum TwinkmasterActions
{
    ACTION_SET_LEVEL_AND_LOCK = 100,
    ACTION_LOCK_XP            = 101,
    ACTION_UNLOCK_XP          = 102,
    ACTION_BROWSE_WARES       = 103,
    ACTION_CONFIRM_SET_LEVEL  = 110,
    ACTION_MAIN_MENU          = 200,
};

static const uint32 NPC_TEXT_GREETING = 50920;
static const uint32 NPC_TEXT_LEVEL    = 50921;
static const uint32 NPC_TEXT_BROWSE   = 50922;

bool GossipHello_npc_twinkmaster(Player* player, Creature* creature)
{
    if (!sTwinkmasterModule.IsEnabled())
    {
        player->PlayerTalkClass->SendCloseGossip();
        return true;
    }

    player->PlayerTalkClass->ClearMenus();

    uint32_t targetLevel = sTwinkmasterModule.GetTargetLevel();
    bool isLocked = sTwinkmasterModule.IsXpLocked(player->GetGUIDLow());

    // Option: Set level + lock XP
    if (!isLocked || player->GetLevel() != targetLevel)
    {
        char buf[128];
        snprintf(buf, sizeof(buf), "Set my level to %u and lock XP", targetLevel);
        player->ADD_GOSSIP_ITEM(GOSSIP_ICON_BATTLE, buf, GOSSIP_SENDER_MAIN, ACTION_SET_LEVEL_AND_LOCK);
    }

    // Option: Lock/Unlock XP
    if (isLocked)
        player->ADD_GOSSIP_ITEM(GOSSIP_ICON_INTERACT_1, "Unlock my XP gain", GOSSIP_SENDER_MAIN, ACTION_UNLOCK_XP);
    else
        player->ADD_GOSSIP_ITEM(GOSSIP_ICON_INTERACT_1, "Lock my XP gain", GOSSIP_SENDER_MAIN, ACTION_LOCK_XP);

    // Option: Browse wares (vendor)
    player->ADD_GOSSIP_ITEM(GOSSIP_ICON_VENDOR, "Browse your wares", GOSSIP_SENDER_MAIN, ACTION_BROWSE_WARES);

    player->PlayerTalkClass->SendGossipMenu(NPC_TEXT_GREETING, creature->GetObjectGuid());
    return true;
}

bool GossipSelect_npc_twinkmaster(Player* player, Creature* creature, uint32 sender, uint32 action)
{
    player->PlayerTalkClass->ClearMenus();

    switch (action)
    {
        case ACTION_SET_LEVEL_AND_LOCK:
        {
            // Confirmation prompt
            uint32_t targetLevel = sTwinkmasterModule.GetTargetLevel();
            char buf[256];
            snprintf(buf, sizeof(buf),
                     "Yes, set me to level %u and lock my XP!", targetLevel);

            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_BATTLE, buf, GOSSIP_SENDER_MAIN, ACTION_CONFIRM_SET_LEVEL);
            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "Maybe later...", GOSSIP_SENDER_MAIN, ACTION_MAIN_MENU);

            player->PlayerTalkClass->SendGossipMenu(NPC_TEXT_LEVEL, creature->GetObjectGuid());
            break;
        }
        case ACTION_CONFIRM_SET_LEVEL:
        {
            sTwinkmasterModule.SetLevelAndLock(player);
            player->PlayerTalkClass->SendCloseGossip();
            break;
        }
        case ACTION_LOCK_XP:
        {
            sTwinkmasterModule.LockXP(player);
            player->PlayerTalkClass->SendCloseGossip();
            break;
        }
        case ACTION_UNLOCK_XP:
        {
            sTwinkmasterModule.UnlockXP(player);
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
            // Return to main menu
            GossipHello_npc_twinkmaster(player, creature);
            break;
        }
        default:
            player->PlayerTalkClass->SendCloseGossip();
            break;
    }

    return true;
}

void AddSC_npc_twinkmaster()
{
    Script* newscript = new Script;
    newscript->Name = "npc_twinkmaster";
    newscript->pGossipHello = &GossipHello_npc_twinkmaster;
    newscript->pGossipSelect = &GossipSelect_npc_twinkmaster;
    newscript->RegisterSelf();
}
