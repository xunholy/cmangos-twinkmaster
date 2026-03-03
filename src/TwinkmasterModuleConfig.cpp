#include "TwinkmasterModuleConfig.h"
#include "TwinkmasterModule.h"
#include "Config/Config.h"

void TwinkmasterModule::LoadConfig()
{
    m_config.enabled = sConfig.GetBoolDefault("Twinkmaster.Enable", false);
    m_config.targetLevel = sConfig.GetIntDefault("Twinkmaster.TargetLevel", 19);

    if (m_config.enabled)
        sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "[Twinkmaster] Module enabled. Target level: %u", m_config.targetLevel);
    else
        sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "[Twinkmaster] Module disabled.");
}
