#ifndef CMANGOS_MODULE_TWINKMASTER_H
#define CMANGOS_MODULE_TWINKMASTER_H

#include "Module.h"
#include "TwinkmasterModuleConfig.h"

#include <unordered_map>
#include <unordered_set>

namespace cmangos_module
{
    enum VendorCategory : uint8
    {
        VENDOR_CAT_BIS   = 1,
        VENDOR_CAT_HONOR = 2,
        VENDOR_CAT_RARE  = 4,
    };

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
        void LoadVendorCategories();
        void SendFilteredVendorList(Player* player, Creature* creature, uint8 categoryMask);

        std::unordered_set<uint32> m_xpLockedPlayers;
        std::unordered_map<uint32, uint8> m_vendorCategories;
    };
}

#endif
