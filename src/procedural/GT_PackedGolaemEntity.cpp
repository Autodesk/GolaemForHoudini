/***************************************************************************
*                                                                          *
*  Copyright (C) Golaem S.A.  All Rights Reserved.                         *
*                                                                          *
***************************************************************************/

#include "GT_PackedGolaemEntity.h"
#include "GU_PackedGolaemEntity.h"

HDK_INCLUDES_START

#include <GU/GU_PrimPacked.h>
#include <GT/GT_TransformArray.h>
#include <GT/GT_GEOAttributeFilter.h>
#include <GT/GT_PrimInstance.h>
#include <UT/UT_HDKVersion.h>
#include <GT/GT_GEOPrimPacked.h>
#include <GT/GT_PrimCollect.h>

HDK_INCLUDES_END

#include <glmCoreDefinitions.h>
#include <glmRenderGeometry.h>

namespace glm
{
    //-----------------------------------------------------------------------------
    void GT_PackedGolaemEntity::registerPrimitive(const GA_PrimitiveTypeId& id)
    {
        new GT_PackedGolaemEntity(id);
    }

    //-----------------------------------------------------------------------------
    GT_PackedGolaemEntity::GT_PackedGolaemEntity(const GA_PrimitiveTypeId& id)
    {
        bind(id);
    }

    //-----------------------------------------------------------------------------
    GT_PackedGolaemEntity::~GT_PackedGolaemEntity()
    {
    }

    //-----------------------------------------------------------------------------
    GT_GEOPrimCollectData* GT_PackedGolaemEntity::beginCollecting(const GT_GEODetailListHandle& geometry, const GT_RefineParms* parms) const
    {
        GLM_UNREFERENCED(geometry);
        GLM_UNREFERENCED(parms);
        return NULL;
    }

    //-----------------------------------------------------------------------------
    GT_PrimitiveHandle GT_PackedGolaemEntity::collect(const GT_GEODetailListHandle& geometry, const GEO_Primitive* const* prim_list, int nsegments, GT_GEOPrimCollectData* data) const
    {
        GLM_UNREFERENCED(nsegments);
        GLM_UNREFERENCED(data);

        GU_ConstDetailHandle geoHandle = geometry->getGeometry(0);

        const GEO_Primitive* prim = prim_list[0];
        if (prim != NULL && prim->getTypeId() == GU_PackedGolaemEntity::getTypeId())
        {
			const GU_PrimPacked* packedPrim = static_cast<const GU_PrimPacked*>(prim);

            const GU_PackedGolaemEntity* packedEntity = static_cast<const glm::GU_PackedGolaemEntity*>(packedPrim->sharedImplementation());

            if (packedEntity->_inputData._entityId != -1)
            {
                if (packedEntity->_viewportGeo.get() == NULL)
                {
                    packedEntity->_viewportGeo = new GT_GEOPrimPacked(geoHandle, packedPrim);
                }
                return packedEntity->_viewportGeo;
            }
            else
            {
                packedEntity->_viewportGeo = GT_PrimitiveHandle();
            }
        }

        return GT_PrimitiveHandle();
    }

    //-----------------------------------------------------------------------------
    GT_PrimitiveHandle GT_PackedGolaemEntity::endCollecting(const GT_GEODetailListHandle& geometry, GT_GEOPrimCollectData* data) const
    {
        GLM_UNREFERENCED(geometry);
        GLM_UNREFERENCED(data);

        return GT_PrimitiveHandle();
    }

} // namespace glm