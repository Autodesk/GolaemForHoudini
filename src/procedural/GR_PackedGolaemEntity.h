/***************************************************************************
*                                                                          *
*  Copyright (C) Golaem S.A.  All Rights Reserved.                         *
*                                                                          *
***************************************************************************/

#pragma once
#include "glmHoudini.h"
#include "GU_PackedGolaemEntity.h"

HDK_INCLUDES_START
#include <UT/UT_HDKVersion.h>
#include <GUI/GUI_PrimitiveHook.h>
#include <GR/GR_Primitive.h>

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

#if HDK_API_VERSION >= 20000000
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
#else
        void update(RE_Render* rend,
            const GT_PrimitiveHandle& primh,
            const GR_UpdateParms& params) override;

        void render(
            RE_Render* rend,
            GR_RenderMode render_mode,
            GR_RenderFlags flags,
            GR_DrawParms drawparams) override;

        void renderDecoration(
            RE_Render* rend,
            GR_Decoration decor,
            const GR_DecorationParms& parms) override;

        int renderPick(
            RE_Render* rend,
            const GR_DisplayOption* opt,
            unsigned int pick_type,
            GR_PickStyle pick_style,
            bool has_pick_map) override;
#endif

        void clearGeo();
    };
} // namespace glm