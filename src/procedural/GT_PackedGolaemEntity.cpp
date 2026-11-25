// Copyright 2026 Autodesk, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "GT_PackedGolaemEntity.h"
#include "GU_PackedGolaemEntity.h"

HDK_INCLUDES_START

#include <GU/GU_PrimPacked.h>
#include <GT/GT_TransformArray.h>
#include <GT/GT_GEOAttributeFilter.h>
#include <GT/GT_PrimInstance.h>
#include <UT/UT_HDKVersion.h>
#include <GT/GT_GEOPrimitive.h>
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
#if HDK_API_VERSION >= 22000000
                    // Keep the GEO primitive visible to the custom Vulkan render hook.
                    packedEntity->_viewportGeo = new GT_GEOPrimitive(&geoHandle, &prim, 1);
#else
                    packedEntity->_viewportGeo = new GT_GEOPrimPacked(geoHandle, packedPrim);
#endif
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