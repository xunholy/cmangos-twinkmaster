#ifndef TWINKMASTER_MODULE_H
#define TWINKMASTER_MODULE_H

#include "TwinkmasterModuleConfig.h"
#include <unordered_set>

class Player;

class TwinkmasterModule
{
public:
    static TwinkmasterModule& Instance();

    // Lifecycle
    void LoadConfig();
    const TwinkmasterConfig& GetConfig() const { return m_config; }

    // Queries
    bool IsEnabled() const { return m_config.enabled; }
    uint32_t GetTargetLevel() const { return m_config.targetLevel; }
    bool IsXpLocked(uint32_t guid) const;

    // Actions
    void SetLevelAndLock(Player* player);
    void LockXP(Player* player);
    void UnlockXP(Player* player);

    // Hooks
    void OnPlayerLogin(Player* player);
    bool OnPreGiveXP(Player* player);

private:
    TwinkmasterModule() = default;
    TwinkmasterConfig m_config;
    std::unordered_set<uint32_t> m_xpLockedPlayers; // in-memory cache
};

#define sTwinkmasterModule TwinkmasterModule::Instance()

// Script registration
void AddSC_npc_twinkmaster();
void InitTwinkmasterModule();

#endif // TWINKMASTER_MODULE_H
