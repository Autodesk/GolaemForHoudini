/***************************************************************************
*                                                                          *
*  Copyright (C) Golaem S.A.  All Rights Reserved.                         *
*                                                                          *
***************************************************************************/

#pragma once

#include <glmLog.h>
#include <glmSingleton.h>

class SOP_Node;

namespace glm
{
    class HoudiniLogger : public glm::ILogger
    {
    public:
        virtual ~HoudiniLogger();
        virtual void trace(glm::Log::Module module, glm::Log::Severity severity, const char* msg, const char* file, int line, const char* operation);

        SOP_Node* _node;
    };
} // namespace glm