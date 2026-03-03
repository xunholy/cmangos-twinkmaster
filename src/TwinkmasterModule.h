#ifndef CMANGOS_MODULE_TWINKMASTER_H
#define CMANGOS_MODULE_TWINKMASTER_H

#include "Module.h"
#include "TwinkmasterModuleConfig.h"

#include <unordered_set>

namespace cmangos_module
{
    class TwinkmasterModule : public Module
    {
    public:
        TwinkmasterModule();
        const TwinkmasterModuleConfig* GetConfig() const override;

        // Module hooks
        void OnInitialize() override;

        // Player hooks
        void OnLoadFromDB(Player* player) override;
        void OnLogOut(Player* player) override;
        bool OnPreGiveXP(Player* player, uint32& xp, Creature* victim) override;
        bool OnPreGossipHello(Player* player, Creature* creature) override;
        bool OnGossipSelect(Player* player, Creature* creature, uint32 sender, uint32 action, const std::string& code, uint32 gossipListId) override;

        // Public helpers
        bool IsEnabled() const;
        uint32 GetTargetLevel() const;
        bool IsXpLocked(uint32 guid) const;

        void SetLevelAndLock(Player* player);
        void LockXP(Player* player);
        void UnlockXP(Player* player);

    private:
        std::unordered_set<uint32> m_xpLockedPlayers;
    };
}

#endif
