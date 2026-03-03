#include "TwinkmasterModule.h"
#include "Database/DatabaseEnv.h"
#include "Entities/Player.h"
#include "Log.h"

TwinkmasterModule& TwinkmasterModule::Instance()
{
    static TwinkmasterModule instance;
    return instance;
}

bool TwinkmasterModule::IsXpLocked(uint32_t guid) const
{
    return m_xpLockedPlayers.count(guid) > 0;
}

void TwinkmasterModule::SetLevelAndLock(Player* player)
{
    if (!player)
        return;

    uint32_t targetLevel = m_config.targetLevel;

    // Set level
    if (player->GetLevel() != targetLevel)
    {
        player->GiveLevel(targetLevel);
        player->InitTalentForLevel();
        player->SetUInt32Value(PLAYER_XP, 0);
    }

    // Lock XP
    LockXP(player);

    player->GetSession()->SendNotification("Your level has been set to %u and XP gain is now locked.", targetLevel);
    sLog.Out(LOG_BASIC, LOG_LVL_BASIC, "[Twinkmaster] Player %s (GUID: %u) set to level %u with XP locked.",
             player->GetName(), player->GetGUIDLow(), targetLevel);
}

void TwinkmasterModule::LockXP(Player* player)
{
    if (!player)
        return;

    uint32_t guid = player->GetGUIDLow();

    if (m_xpLockedPlayers.count(guid))
    {
        player->GetSession()->SendNotification("Your XP is already locked.");
        return;
    }

    m_xpLockedPlayers.insert(guid);

    // Persist to DB
    CharacterDatabase.PExecute(
        "REPLACE INTO `custom_twinkmaster_player_config` (`guid`, `xp_locked`, `level_set`) VALUES (%u, 1, 1)",
        guid);

    player->GetSession()->SendNotification("XP gain is now locked.");
}

void TwinkmasterModule::UnlockXP(Player* player)
{
    if (!player)
        return;

    uint32_t guid = player->GetGUIDLow();

    if (!m_xpLockedPlayers.count(guid))
    {
        player->GetSession()->SendNotification("Your XP is not locked.");
        return;
    }

    m_xpLockedPlayers.erase(guid);

    // Persist to DB
    CharacterDatabase.PExecute(
        "UPDATE `custom_twinkmaster_player_config` SET `xp_locked` = 0 WHERE `guid` = %u",
        guid);

    player->GetSession()->SendNotification("XP gain is now unlocked.");
}

void TwinkmasterModule::OnPlayerLogin(Player* player)
{
    if (!m_config.enabled || !player)
        return;

    uint32_t guid = player->GetGUIDLow();

    // Load XP lock state from DB
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

bool TwinkmasterModule::OnPreGiveXP(Player* player)
{
    if (!m_config.enabled || !player)
        return false;

    // Return true to block XP gain
    return IsXpLocked(player->GetGUIDLow());
}

void InitTwinkmasterModule()
{
    sTwinkmasterModule.LoadConfig();
}
