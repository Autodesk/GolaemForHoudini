/***************************************************************************
*                                                                          *
*  Copyright (C) Golaem S.A.  All Rights Reserved.                         *
*                                                                          *
***************************************************************************/

#pragma once

#include "glmHoudini.h"

HDK_INCLUDES_START

#include <GT/GT_GEOPrimCollect.h>

HDK_INCLUDES_END

namespace glm
{
    /// Collector for packed Golaem primitives.
    class GT_PackedGolaemEntity : public GT_GEOPrimCollect
    {
    public:
        /// Register the GT collector
        static void registerPrimitive(const GA_PrimitiveTypeId& id);
        
        GT_PackedGolaemEntity(const GA_PrimitiveTypeId& id);
        ~GT_PackedGolaemEntity();

        GT_GEOPrimCollectData* beginCollecting(const GT_GEODetailListHandle& geometry, const GT_RefineParms* parms) const;
        
        GT_PrimitiveHandle collect(const GT_GEODetailListHandle& geo, const GEO_Primitive* const* prim_list, int nsegments, GT_GEOPrimCollectData* data) const;
        
        GT_PrimitiveHandle endCollecting(const GT_GEODetailListHandle& geometry, GT_GEOPrimCollectData* data) const;
    };
} // namespace glm