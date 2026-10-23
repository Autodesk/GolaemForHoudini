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

#pragma once
#include "glmHoudini.h"
#include "GU_PackedGolaemEntity.h"

HDK_INCLUDES_START
#include <UT/UT_HDKVersion.h>
#include <GUI/GUI_PrimitiveHook.h>
#if HDK_API_VERSION >= 22000000
#include <GR/GR_PrimPacked.h>
#else
#include <GR/GR_Primitive.h>
#endif

HDK_INCLUDES_END

#include <glmPODArray.h>
#include <glmVector3.h>
#include <glmRenderGeometry.h>

namespace glm
{
    class GR_PackedGolaemEntityHook : public GUI_PrimitiveHook
    {
    public:
        GR_PackedGolaemEntityHook();
        ~GR_PackedGolaemEntityHook();

        GR_Primitive* createPrimitive(
            const GT_PrimitiveHandle& gt_prim,
            const GEO_Primitive* geo_prim,
            const GR_RenderInfo* info,
            const char* cache_name,
            GR_PrimAcceptResult& processed) override;
    };

#if HDK_API_VERSION >= 22000000
    class GR_PackedGolaemEntity : public GR_PrimPacked
    {
    public:
        GR_PackedGolaemEntity(
            const GR_RenderInfo* info,
            const char* cache_name,
            const GT_PrimitiveHandle& packed_prim,
            const GT_PrimitiveHandle& geometry,
            const GU_ConstDetailHandle& detail,
            GA_Index prim_index);

        const char* className() const override;

        GR_PrimAcceptResult acceptPrimitive(
            GT_PrimitiveType t,
            int geo_type,
            const GT_PrimitiveHandle& ph,
            const GEO_Primitive* prim) override;

    protected:
        void update(
            RE_RenderContext rend,
            const GT_PrimitiveHandle& primh,
            const GR_UpdateParms& p) override;

    private:
        GA_Index _primitiveIndex;
    };
#else
    class GR_PackedGolaemEntity : public GR_Primitive
    {
    public:
        RE_Geometry* _viewportGeo;

        uint32_t _bonePositionOffset; // computed when needed
    private:
        const GU_PackedGolaemEntity* _packedEntity;

    public:
        GR_PackedGolaemEntity(
            const GR_RenderInfo* info,
            const char* cache_name,
            const GEO_Primitive* prim);

        ~GR_PackedGolaemEntity();

        const char* className() const override;

        GR_PrimAcceptResult acceptPrimitive(
            GT_PrimitiveType t,
            int geo_type,
            const GT_PrimitiveHandle& ph,
            const GEO_Primitive* prim) override;

        bool getBoundingBox(UT_BoundingBoxD& bbox) const override;

    protected:
        void assignEntity(const GU_PackedGolaemEntity* packedEntity);

        void update(
            RE_RenderContext rend,
            const GT_PrimitiveHandle& primh,
            const GR_UpdateParms& p) override;

        void render(
            RE_RenderContext rend,
            GR_RenderMode render_mode,
            GR_RenderFlags flags,
            GR_DrawParms drawparams) override;

        void renderDecoration(
            RE_RenderContext rend,
            GR_Decoration decor,
            const GR_DecorationParms& parms) override;

        int renderPick(
            RE_RenderContext rend,
            const GR_DisplayOption* opt,
            unsigned int pick_type,
            GR_PickStyle pick_style,
            bool has_pick_map) override;

        void clearGeo();
    };
#endif
} // namespace glm