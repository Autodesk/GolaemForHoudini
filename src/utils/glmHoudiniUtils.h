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
    namespace crowdio
    {
        class CrowdFBXStorage;
        class CrowdFBXBaker;
    } // namespace crowdio

    class HoudiniLogger : public glm::ILogger
    {
    public:
        virtual ~HoudiniLogger();
        virtual void trace(glm::Log::Module module, glm::Log::Severity severity, const char* msg, const char* file, int line, const char* operation);
    };

    typedef glm::Singleton<HoudiniLogger> theGolaemLogger;

    class HoudiniFbxData
    {
    protected:
        crowdio::CrowdFBXStorage* _fbxStorage = NULL;
        crowdio::CrowdFBXBaker* _fbxBaker = NULL;

    public:
        HoudiniFbxData();
        ~HoudiniFbxData();

        crowdio::CrowdFBXStorage* getFbxStorage();
        crowdio::CrowdFBXBaker* getFbxBaker();
    };
} // namespace glm