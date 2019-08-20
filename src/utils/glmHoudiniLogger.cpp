/***************************************************************************
*                                                                          *
*  Copyright (C) Golaem S.A.  All Rights Reserved.                         *
*                                                                          *
***************************************************************************/

#include "glmHoudiniLogger.h"
#include "glmHoudini.h"

HDK_INCLUDES_START

#include <SOP/SOP_Node.h>

HDK_INCLUDES_END
namespace glm
{
    //-----------------------------------------------------------------------------
    HoudiniLogger::~HoudiniLogger()
    {
    }

    //-----------------------------------------------------------------------------
    void HoudiniLogger::trace(glm::Log::Module module, glm::Log::Severity severity, const char* msg, const char*, int, const char*)
    {
        glm::GlmString message = "[";
        if (module == Log::CROWD)
        {
            message = "[Golaem";
        }
        else if (module == Log::SDK)
        {
            message = "[GolaemSDK";
        }
        switch (severity)
        {
        case glm::Log::LOG_ERROR:
        {
            message += "::ERROR] ";
            message += msg;
            UTgetErrorManager()->addWarning("SOP", SOP_MESSAGE, message.c_str());
        }
        break;
        case glm::Log::LOG_WARNING:
        {
            message += "::WARNING] ";
            message += msg;
            UTgetErrorManager()->addWarning("SOP", SOP_MESSAGE, message.c_str());
        }
        break;
        case glm::Log::LOG_INFO:
        {
            message += "::INFO] ";
            message += msg;
            UTgetErrorManager()->addPrompt("SOP", SOP_MESSAGE, message.c_str());
        }
        break;
        case glm::Log::LOG_DEBUG:
        {
            message += "::DEBUG] ";
            message += msg;
            UTgetErrorManager()->addMessage("SOP", SOP_MESSAGE, message.c_str());
        }
        break;
        default:
            break;
        }
    }
} // namespace glm
