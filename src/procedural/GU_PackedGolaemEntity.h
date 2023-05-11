/***************************************************************************
*                                                                          *
*  Copyright (C) Golaem S.A.  All Rights Reserved.                         *
*                                                                          *
***************************************************************************/

#pragma once

#include "glmHoudini.h"

HDK_INCLUDES_START

#include <GU/GU_PackedImpl.h>
#include <GT/GT_Handles.h>

HDK_INCLUDES_END

#include <glmVector3.h>
#include <glmString.h>
#include <glmRenderGeometry.h>
#include <glmGolaemCharacter.h>	//predefining GolaemCharacter coudl be enough, but as it's a typedef on a versionned class, importing the file make sures the typedef is consistant

namespace glm
{
    struct ShaderAssetDataContainer;

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

    struct GolaemMaterialAssignMode
    {
        enum Value
        {
            BY_SURFACE_SHADER,
            BY_SHADING_GROUP,
            END
        };
    };

    class GU_PackedGolaemEntity : public GU_PackedImpl
    {
    public:
        glm::Vector3 _rootPos;
        glm::Vector3 _halfExtents;
        mutable glm::crowdio::InputEntityGeoData _inputData; // for geometry generation
        const glm::GolaemCharacter* _character;
        glm::PODArray<size_t>* _sortedBonesInverse;
        glm::PODArray<int>* _shadingGroupToSurfaceShader;

        mutable uint32_t _bonePositionOffset; // computed when needed

        GolaemDisplayMode::Value _displayMode;
        GolaemMaterialAssignMode::Value _materialAssignMode;
        glm::GlmString _materialPath;

        bool _isNew;

        mutable bool _updateGeo;
        mutable glm::PODArray<GA_Offset> _pointStartOffsets;
        mutable glm::PODArray<GA_Offset> _vertexOffsets; // for geometry generation

        mutable GU_DetailHandle _detail;

        mutable GT_PrimitiveHandle _viewportGeo;

    private:
        static GA_PrimitiveTypeId _typeId;

    public:
        GU_PackedGolaemEntity();
        GU_PackedGolaemEntity(const GU_PackedGolaemEntity& src);
        GU_PackedGolaemEntity& operator=(const GU_PackedGolaemEntity& src);

        virtual ~GU_PackedGolaemEntity();

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

        void updateFrom(GU_PrimPacked* prim, const UT_Options& options);
    };
} // namespace glm