/***************************************************************************
*                                                                          *
*  Copyright (C) Golaem S.A.  All Rights Reserved.                         *
*                                                                          *
***************************************************************************/

#pragma once

#include "glmHoudini.h"

HDK_INCLUDES_START

#include <GU/GU_PackedImpl.h>

HDK_INCLUDES_END

#include <glmVector3.h>
#include <glmString.h>

namespace glm
{
    struct GolaemDisplayMode
    {
        enum Value
        {
            BOUNDING_BOX,
            SKELETON,
            SKINMESH,
            END
        };
    };

    class GU_PackedGolaemEntity : public GU_PackedImpl
    {
    public:
        mutable GU_DetailHandle _detail;

        glm::Vector3 _rootPos;
        glm::Vector3 _halfExtents;
        int64_t _entityId;

        mutable bool _updateGeo;
        mutable GA_Offset _pointStartOffset;
        mutable GA_Offset _primOffset;
        GolaemDisplayMode::Value _displayMode;

    private:
        static GA_PrimitiveTypeId _typeId;

    public:
        GU_PackedGolaemEntity();
        GU_PackedGolaemEntity(const GU_PackedGolaemEntity& src);
        virtual ~GU_PackedGolaemEntity();

        static GU_PackedGolaemEntity* build(GU_Detail* gdp);

        /// Get the type ID for the GU_PackedSphere primitive type.
        static const GA_PrimitiveTypeId& getTypeId();

        /// register the primitive
        static void install(GA_PrimitiveFactory* factory);

        GU_PackedFactory* getFactory() const override;
        GU_PackedImpl* copy() const override;
        bool isValid() const override;
        void clearData() override;

        bool load(GU_PrimPacked* prim, const UT_Options& options, const GA_LoadMap& map) override;
        void update(GU_PrimPacked* prim, const UT_Options& options) override;
        bool save(UT_Options& options, const GA_SaveMap& map) const override;
        bool getBounds(UT_BoundingBox& box) const override;
        bool getRenderingBounds(UT_BoundingBox& box) const override;
        void getVelocityRange(UT_Vector3& min, UT_Vector3& max) const override;
        void getWidthRange(fpreal& wmin, fpreal& wmax) const override;
        bool unpack(GU_Detail& destgdp, const UT_Matrix4D* transform) const override;
        GU_ConstDetailHandle getPackedDetail(GU_PackedContext* context = 0) const override;

        /// Report memory usage (includes all shared memory)
        int64 getMemoryUsage(bool inclusive) const override;

        /// Count memory usage using a UT_MemoryCounter in order to count
        /// shared memory correctly.
        void countMemory(UT_MemoryCounter& counter, bool inclusive) const override;

    private:
        void clearGeo();
    };
} // namespace glm