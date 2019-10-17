/***************************************************************************
*                                                                          *
*  Copyright (C) Golaem S.A.  All Rights Reserved.                         *
*                                                                          *
***************************************************************************/

#include "GU_PackedGolaemEntity.h"
#include "GT_PackedGolaemEntity.h"

HDK_INCLUDES_START

#include <GEO/GEO_PolyCounts.h>
#include <GEO/GEO_PrimPoly.h>
#include <GU/GU_PackedFactory.h>
#include <GU/GU_PrimPacked.h>
#include <UT/UT_MemoryCounter.h>
#include <FS/UT_DSO.h>
#include <UT/UT_VarEncode.h>

HDK_INCLUDES_END

#include <glmLog.h>
#include <glmSimulationData.h>
#include <glmFrameData.h>
#include <glmHierarchicalBone.h>
#include <glmGolaemCharacter.h>
#include <glmAssetManagementUtils.h>

#include <glmCrowdFBXCharacter.h>
#include <glmCrowdGcgStorage.h>
#include <glmCrowdGcgCharacter.h>
#include <glmCrowdGcgBaker.h>
#include <glmCrowdFBXStorage.h>
#include <glmCrowdFBXBaker.h>
#include <glmRenderGeometry.h>
#include <glmSimulationCacheFactorySimulation.h>

namespace glm
{
    GA_PrimitiveTypeId GU_PackedGolaemEntity::_typeId(-1);

    //-----------------------------------------------------------------------------
    const glm::GlmString& getEntityIdAttrName()
    {
        static glm::GlmString entityIdAttrName = "glmEntityId";
        return entityIdAttrName;
    }

    //-----------------------------------------------------------------------------
    const glm::GlmString& getMeshAttrName()
    {
        static glm::GlmString meshAttrName = "glmMeshName";
        return meshAttrName;
    }

    //-----------------------------------------------------------------------------
    glm::crowdio::CrowdFBXStorage& getFbxStorage()
    {
        static glm::crowdio::CrowdFBXStorage fbxStorage;
        return fbxStorage;
    }

    //-----------------------------------------------------------------------------
    glm::crowdio::CrowdFBXBaker& getFbxBaker()
    {
        glm::crowdio::CrowdFBXStorage& fbxStorage = getFbxStorage();
        static glm::crowdio::CrowdFBXBaker fbxBaker(fbxStorage.touchFbxSdkManager());
        return fbxBaker;
    }

    //-----------------------------------------------------------------------------
    class GU_PackedGolaemFactory : public GU_PackedFactory
    {
    public:
        GU_PackedGolaemFactory()
            : GU_PackedFactory("glmPackedEntity", "Golaem Entity")
        {
        }

        GU_PackedImpl* create() const override
        {
            return new GU_PackedGolaemEntity();
        }
    };

    static GU_PackedGolaemFactory* theGolaemFactory = NULL;

    //-----------------------------------------------------------------------------
    GU_PackedGolaemEntity::GU_PackedGolaemEntity()
        : _rootPos(0, 0, 0)
        , _halfExtents(0, 0, 0)
        , _inputData()
        , _character(NULL)
        , _sortedBonesInverse(NULL)
        , _shaderDataContainer(NULL)
        , _shadingGroupToSurfaceShader(NULL)
        , _displayMode(GolaemDisplayMode::END)
        , _materialAssignMode(GolaemMaterialAssignMode::END)
        , _materialPath()
        , _updateGeo(false)
    {
        GU_Detail* detailPtr = new GU_Detail();
        _detail.allocateAndSet(detailPtr, true);

        _inputData._enableLOD = false;
        _inputData._entityPos = NULL;              // not used when enableLOD is false
        _inputData._cameraWorldPosition = NULL;    // not used when enableLOD is false
        _inputData._maxVerticesPerFace = UINT_MAX; // for fbx assets
        _inputData._generateFur = false;           // do not generate fur for now
        _inputData._frameDatas.resize(1, NULL);
        _inputData._frames.resize(1);

        _inputData._fbxStorage = &getFbxStorage();
        _inputData._fbxBaker = &getFbxBaker();
    }

    //-----------------------------------------------------------------------------
    GU_PackedGolaemEntity::GU_PackedGolaemEntity(const GU_PackedGolaemEntity& src)
        : GU_PackedGolaemEntity()
    {
        *this = src;
    }

    //-----------------------------------------------------------------------------
    GU_PackedGolaemEntity& GU_PackedGolaemEntity::operator=(const GU_PackedGolaemEntity& src)
    {
        if (this != &src)
        {
            _rootPos = src._rootPos;
            _halfExtents = src._halfExtents;
            _inputData = src._inputData;
            _character = src._character;
            _sortedBonesInverse = src._sortedBonesInverse;
            _shaderDataContainer = src._shaderDataContainer;
            _shadingGroupToSurfaceShader = src._shadingGroupToSurfaceShader;
            _displayMode = src._displayMode;
            _materialAssignMode = src._materialAssignMode;
            _materialPath = src._materialPath;
        }
        return *this;
    }

    //-----------------------------------------------------------------------------
    GU_PackedGolaemEntity::~GU_PackedGolaemEntity()
    {
        clearGeo();
    }

    //-----------------------------------------------------------------------------
    void GU_PackedGolaemEntity::clearGeo()
    {
        _detail = GU_DetailHandle();
    }

    //-----------------------------------------------------------------------------
    GU_PackedGolaemEntity* GU_PackedGolaemEntity::build(GU_Detail* gdp)
    {
        GU_PrimPacked* packedPrim = GU_PrimPacked::build(*gdp, theGolaemFactory->typeDef().getId());
        GU_PackedGolaemEntity* packedEntity = static_cast<GU_PackedGolaemEntity*>(packedPrim->implementation());
        return packedEntity;
    }

    //-----------------------------------------------------------------------------
    const GA_PrimitiveTypeId& GU_PackedGolaemEntity::getTypeId()
    {
        return _typeId;
    }

    //-----------------------------------------------------------------------------
    void GU_PackedGolaemEntity::install(GA_PrimitiveFactory* factory)
    {
        GLM_DEBUG_ASSERT(theGolaemFactory == NULL);
        if (theGolaemFactory != NULL)
            return;

        theGolaemFactory = new GU_PackedGolaemFactory();
        GU_PrimPacked::registerPacked(factory, theGolaemFactory);
        if (theGolaemFactory->isRegistered())
        {
            _typeId = theGolaemFactory->typeDef().getId();
            GT_PackedGolaemEntity::registerPrimitive(_typeId);
        }
        else
        {
            GLM_CROWD_TRACE_ERROR("Unable to register packed Golaem factory from " << UT_DSO::getRunningFile());
        }
    }

    //-----------------------------------------------------------------------------
    GU_PackedFactory* GU_PackedGolaemEntity::getFactory() const
    {
        return theGolaemFactory;
    }

    //-----------------------------------------------------------------------------
    GU_PackedImpl* GU_PackedGolaemEntity::copy() const
    {
        return new GU_PackedGolaemEntity(*this);
    }

    //-----------------------------------------------------------------------------
    bool GU_PackedGolaemEntity::isValid() const
    {
        return _detail.isValid();
    }

    //-----------------------------------------------------------------------------
    void GU_PackedGolaemEntity::clearData()
    {
        // This method is called when primitives are "stashed" during the cooking
        // process.  However, primitives are typically immediately "unstashed" or
        // they are deleted if the primitives aren't recreated after the fact.
        // We can just leave our data.
    }

    //-----------------------------------------------------------------------------
    bool GU_PackedGolaemEntity::load(GU_PrimPacked* prim, const UT_Options& options, const GA_LoadMap& map)
    {
        updateFrom(prim, options);
        GLM_UNREFERENCED(map);
        return true;
    }

    //-----------------------------------------------------------------------------
    void GU_PackedGolaemEntity::updateFrom(GU_PrimPacked* prim, const UT_Options& options)
    {
        int64_t intValue;
        double doubleValue;
        UT_Vector3F vector3Value;
        UT_StringHolder stringValue;
        if (import(options, "entityIndex", intValue))
        {
            _inputData._entityIndex = (uint32_t)intValue;
        }
        if (import(options, "entityId", intValue))
        {
            _inputData._entityId = intValue;
        }
        if (import(options, "character", intValue))
        {
            _character = (const glm::GolaemCharacter*)intValue;
        }
        if (import(options, "geometryTag", intValue))
        {
            _inputData._geometryTag = (short)intValue;
        }
        if (import(options, "sortedBonesInverse", intValue))
        {
            _sortedBonesInverse = (glm::PODArray<size_t>*)intValue;
        }
        if (import(options, "shaderDataContainer", intValue))
        {
            _shaderDataContainer = (const glm::ShaderAssetDataContainer*)intValue;
        }
        if (import(options, "cachedSimulation", intValue))
        {
            _inputData._cachedSimulation = (crowdio::CachedSimulation*)intValue;
        }

        if (options.hasOption("halfExtents"))
        {
            vector3Value = options.getOptionV3("halfExtents");
            _halfExtents.setValues(vector3Value[0], vector3Value[1], vector3Value[2]);
        }
        if (import(options, "sortedBonesInverse", intValue))
        {
            _sortedBonesInverse = (glm::PODArray<size_t>*)intValue;
        }
        if (import(options, "displayMode", intValue))
        {
            _displayMode = (GolaemDisplayMode::Value)intValue;
        }
        if (import(options, "shadingGroupToSurfaceShader", intValue))
        {
            _shadingGroupToSurfaceShader = (glm::PODArray<int>*)intValue;
        }
        if (import(options, "materialAssignMode", intValue))
        {
            _materialAssignMode = (GolaemMaterialAssignMode::Value)intValue;
        }
        if (import(options, "materialPath", stringValue))
        {
            _materialPath = stringValue.c_str();
        }
        if (options.hasOption("rootPos"))
        {
            vector3Value = options.getOptionV3("rootPos");
            _rootPos.setValues(vector3Value[0], vector3Value[1], vector3Value[2]);
        }
        if (import(options, "shaderDataContainer", intValue))
        {
            _shaderDataContainer = (const glm::ShaderAssetDataContainer*)intValue;
        }
        if (import(options, "frameData", intValue))
        {
            _inputData._frameDatas[0] = (const glm::crowdio::GlmFrameData*)intValue;
        }
        if (import(options, "frame", doubleValue))
        {
            _inputData._frames[0] = doubleValue;
        }
        clearGeo();
        prim->topologyDirty();
    }

    //-----------------------------------------------------------------------------
    void GU_PackedGolaemEntity::update(GU_PrimPacked* prim, const UT_Options& options)
    {
        updateFrom(prim, options);
    }

    //-----------------------------------------------------------------------------
    bool GU_PackedGolaemEntity::save(UT_Options& options, const GA_SaveMap& map) const
    {
        options.setOptionI("entityIndex", _inputData._entityIndex);
        options.setOptionI("entityId", (int64)_inputData._entityId);
        options.setOptionI("character", (int64)_character);
        options.setOptionI("geometryTag", (int64)_inputData._geometryTag);
        options.setOptionI("sortedBonesInverse", (int64)_sortedBonesInverse);
        options.setOptionI("shaderDataContainer", (int64)_shaderDataContainer);
        options.setOptionI("cachedSimulation", (int64)_inputData._cachedSimulation);
        options.setOptionV3("halfExtents", UT_Vector3F(_halfExtents.getFloatValues()));
        options.setOptionI("sortedBonesInverse", (int64)_sortedBonesInverse);
        options.setOptionI("displayMode", (int64)_displayMode);
        options.setOptionI("shadingGroupToSurfaceShader", (int64)_shadingGroupToSurfaceShader);
        options.setOptionI("materialAssignMode", (int64)_materialAssignMode);
        options.setOptionS("materialPath", _materialPath.c_str());
        options.setOptionV3("rootPos", UT_Vector3F(_rootPos.getFloatValues()));
        options.setOptionI("shaderDataContainer", (int64)_shaderDataContainer);
        options.setOptionI("frameData", (int64)_inputData._frameDatas[0]);
        options.setOptionF("frame", _inputData._frames[0]);
        GLM_UNREFERENCED(map);
        return true;
    }

    //-----------------------------------------------------------------------------
    bool GU_PackedGolaemEntity::getBounds(UT_BoundingBox& box) const
    {
        if (_inputData._entityId != -1)
        {
            Vector3 vect = _rootPos;
            vect -= _halfExtents;
            box.initBounds(vect.getFloatValues());
            vect = _rootPos;
            vect += _halfExtents;
            box.enlargeBounds(vect.getFloatValues());
            return true;
        }
        return false;
    }

    //-----------------------------------------------------------------------------
    bool GU_PackedGolaemEntity::getRenderingBounds(UT_BoundingBox& box) const
    {
        return getBounds(box);
    }

    //-----------------------------------------------------------------------------
    void GU_PackedGolaemEntity::getVelocityRange(UT_Vector3& min, UT_Vector3& max) const
    {
        GLM_UNREFERENCED(min);
        GLM_UNREFERENCED(max);
    }

    //-----------------------------------------------------------------------------
    void GU_PackedGolaemEntity::getWidthRange(fpreal& wmin, fpreal& wmax) const
    {
        wmin = wmax = 0; // Width is only important for curves/points.
    }

    //-----------------------------------------------------------------------------
    bool GU_PackedGolaemEntity::unpack(GU_Detail& destgdp, const UT_Matrix4D* transform) const
    {
        // This may allocate geometry for the primitive
        GU_DetailHandleAutoReadLock rlock(getPackedDetail());
        if (!rlock.getGdp())
        {
            return false;
        }
        return unpackToDetail(destgdp, rlock.getGdp(), transform);
    }

    //-----------------------------------------------------------------------------
    GU_ConstDetailHandle GU_PackedGolaemEntity::getPackedDetail(GU_PackedContext* context) const
    {
        GLM_UNREFERENCED(context);
        GU_Detail* detailPtr = _detail.gdpNC();
        if (_inputData._entityId != -1)
        {
            bool firstCompute = detailPtr->isEmpty();
            glm::crowdio::OutputEntityGeoData outputData; // TODO: see if storage is better
            glm::crowdio::GlmGeometryGenerationStatus geoStatus = glm::crowdio::GIO_SUCCESS;
            if (firstCompute)
            {
                GEO_PolyCounts polyCounts;
                UT_IntArray polygonpointnumbers;

                const glm::crowdio::GlmSimulationData* simuData = _inputData._cachedSimulation->getFinalSimulationData();

                uint16_t entityType = simuData->_entityTypes[_inputData._entityIndex];
                uint16_t boneCount = simuData->_boneCount[entityType];
                _bonePositionOffset = simuData->_iBoneOffsetPerEntityType[entityType] + simuData->_indexInEntityType[_inputData._entityIndex] * boneCount;

                switch (_displayMode)
                {
                case glm::GolaemDisplayMode::BOUNDING_BOX:
                {
                    _pointStartOffsets.resize(1);
                    // cube = 6 faces
                    for (size_t iFace = 0; iFace < 6; ++iFace)
                    {
                        polyCounts.append(4);
                    }

                    // face 0
                    polygonpointnumbers.append(0);
                    polygonpointnumbers.append(1);
                    polygonpointnumbers.append(2);
                    polygonpointnumbers.append(3);

                    // face 1
                    polygonpointnumbers.append(1);
                    polygonpointnumbers.append(2);
                    polygonpointnumbers.append(6);
                    polygonpointnumbers.append(5);

                    // face 2
                    polygonpointnumbers.append(2);
                    polygonpointnumbers.append(3);
                    polygonpointnumbers.append(7);
                    polygonpointnumbers.append(6);

                    // face 3
                    polygonpointnumbers.append(3);
                    polygonpointnumbers.append(0);
                    polygonpointnumbers.append(4);
                    polygonpointnumbers.append(7);

                    // face 4
                    polygonpointnumbers.append(0);
                    polygonpointnumbers.append(1);
                    polygonpointnumbers.append(5);
                    polygonpointnumbers.append(4);

                    // face 5
                    polygonpointnumbers.append(4);
                    polygonpointnumbers.append(5);
                    polygonpointnumbers.append(6);
                    polygonpointnumbers.append(7);

                    GA_Offset& pointStartOffset = _pointStartOffsets[0];
                    pointStartOffset = detailPtr->appendPointBlock(8);
                    GEO_PrimPoly::buildBlock(detailPtr, pointStartOffset, 8, polyCounts, polygonpointnumbers.array(), false);
                }
                break;
                case glm::GolaemDisplayMode::SKELETON:
                {
                    _pointStartOffsets.resize(1);

                    const glm::PODArray<size_t>& sortedBonesInverse = *_sortedBonesInverse;

                    GA_Offset& pointStartOffset = _pointStartOffsets[0];
                    pointStartOffset = detailPtr->appendPointBlock(boneCount);

                    const glm::PODArray<glm::HierarchicalBone*>& hBones = _character->_converterMapping._skeletonDescription->getBones();
                    const glm::PODArray<size_t>& sortedBones = _character->_converterMapping._skeletonDescription->getSortedBones();

                    for (int iBone = 0, primCount = glm::min(sortedBones.sizeInt(), (int)boneCount); iBone < primCount; ++iBone)
                    {
                        const glm::HierarchicalBone* hBone = hBones[sortedBones[iBone]];
                        const glm::HierarchicalBone* hBoneParent = hBone->getFather();
                        if (hBoneParent == NULL)
                        {
                            continue;
                        }
                        int parentIdx = hBoneParent->getSpecificBoneIndex();
                        int parentIdxInCache = (int)sortedBonesInverse[parentIdx];

                        polyCounts.clear();
                        polygonpointnumbers.clear();
                        polyCounts.append(2); // polygon size = 2
                        polygonpointnumbers.append(iBone);
                        polygonpointnumbers.append(parentIdxInCache);

                        GEO_PrimPoly::buildBlock(detailPtr, pointStartOffset, 2, polyCounts, polygonpointnumbers.array(), false);
                    }
                }
                break;
                case glm::GolaemDisplayMode::SKINMESH:
                {
                    // compute shaders
                    const glm::Array<glm::GlmString>& shaderData = _shaderDataContainer->data[_inputData._entityIndex];

                    glm::GlmMap<size_t, size_t> globalToIntShaderAttrIdx;
                    glm::GlmMap<size_t, size_t> globalToFloatShaderAttrIdx;
                    glm::GlmMap<size_t, size_t> globalToStringShaderAttrIdx;
                    glm::GlmMap<size_t, size_t> globalToVectorShaderAttrIdx;

                    glm::PODArray<int> intAttrValues;
                    glm::PODArray<float> floatAttrValues;
                    glm::Array<glm::GlmString> stringAttrValues;
                    glm::Array<glm::Vector3> vectorAttrValues;

                    for (size_t iShaderAttr = 0, shaderAttrCount = _character->_shaderAttributes.size(); iShaderAttr < shaderAttrCount; iShaderAttr++)
                    {
                        const glm::GlmString& attrValueStr = shaderData[iShaderAttr];
                        const glm::ShaderAttribute& shaderAttr = _character->_shaderAttributes[iShaderAttr];
                        switch (shaderAttr._type)
                        {
                        case glm::ShaderAttributeType::INT:
                        {

                            globalToIntShaderAttrIdx[iShaderAttr] = intAttrValues.size();
                            intAttrValues.addOne();
                            glm::fromString<int>(attrValueStr, intAttrValues.back());
                        }
                        break;
                        case glm::ShaderAttributeType::FLOAT:
                        {

                            globalToFloatShaderAttrIdx[iShaderAttr] = floatAttrValues.size();
                            floatAttrValues.addOne();
                            glm::fromString<float>(attrValueStr, floatAttrValues.back());
                        }
                        break;
                        case glm::ShaderAttributeType::STRING:
                        {

                            globalToStringShaderAttrIdx[iShaderAttr] = stringAttrValues.size();
                            stringAttrValues.addOne();
                            stringAttrValues.back() = attrValueStr;
                        }
                        break;
                        case glm::ShaderAttributeType::VECTOR:
                        {
                            globalToVectorShaderAttrIdx[iShaderAttr] = vectorAttrValues.size();
                            vectorAttrValues.addOne();
                            glm::fromString(attrValueStr, vectorAttrValues.back());
                        }
                        break;
                        default:
                            break;
                        }
                    }

                    geoStatus = glm::crowdio::glmPrepareEntityGeometry(&_inputData, &outputData);
                    if (geoStatus == glm::crowdio::GIO_SUCCESS)
                    {
                        size_t meshCount = outputData._meshAssetNameIndices.size();
                        _pointStartOffsets.resize(meshCount);

                        glm::PODArray<int> meshShadingGroups(meshCount, -1);

                        for (size_t iMesh = 0; iMesh < meshCount; ++iMesh)
                        {
                            const glm::GlmString& meshName = outputData._meshAssetNames[outputData._meshAssetNameIndices[iMesh]];
                            int& shadingGroupIdx = meshShadingGroups[iMesh];

                            // find shader assets
                            unsigned int iMaterial = outputData._meshAssetMaterialIndices[iMesh];
                            int meshAssetIdx = _character->findMeshAssetIdx(meshName);
                            if (meshAssetIdx != -1)
                            {
                                const glm::MeshAsset& meshAsset = _character->_meshAssets[meshAssetIdx];
                                if (iMaterial < meshAsset._shadingGroups.size())
                                {
                                    shadingGroupIdx = meshAsset._shadingGroups[iMaterial];
                                }
                            }

                            if (shadingGroupIdx == -1)
                            {
                                GLM_CROWD_TRACE_WARNING_LIMIT("No Shading Group found for mesh " << meshName << ". Using default material shader instead");
                            }
                        }

                        glm::Array<glm::Array<glm::Vector3>>& frameDeformedVertices = outputData._deformedVertices[0];

                        _vertexOffsets.resize(meshCount);

                        for (size_t iMesh = 0; iMesh < meshCount; ++iMesh)
                        {
                            const glm::Array<glm::Vector3>& meshDeformedVertices = frameDeformedVertices[iMesh];
                            size_t vertexCount = meshDeformedVertices.size();
                            if (vertexCount == 0)
                            {
                                continue;
                            }

                            const glm::GlmString& meshName = outputData._meshAssetNames[outputData._meshAssetNameIndices[iMesh]];

                            polyCounts.clear();
                            polygonpointnumbers.clear();

                            GA_Offset primOffset;
                            GA_Offset& vertexOffset = _vertexOffsets[iMesh];

                            GA_Offset& pointStartOffset = _pointStartOffsets[iMesh];

                            pointStartOffset = detailPtr->appendPointBlock(vertexCount);

                            if (outputData._geoType == glm::crowdio::GeometryType::FBX)
                            {
                                // must use the same fbx mutex because of fbx's 'unthreadfullness'
                                glm::ScopedLock<glm::Mutex> lock(glm::crowdio::getCrowdFBXMutex());
                                // when fbxMesh == NULL, vertexCount == 0, so no need to check fbxMesh != NULL
                                FbxMesh* fbxMesh = outputData._fbxCharacter->getCharacterFBXMesh(iMesh);

                                FbxLayer* fbxLayer0 = fbxMesh->GetLayer(0);
                                bool hasNormals = false;
                                bool hasMaterials = false;
                                FbxLayerElementMaterial* materialElement = NULL;
                                if (fbxLayer0 != NULL)
                                {
                                    hasNormals = fbxLayer0->GetNormals() != NULL;
                                    materialElement = fbxLayer0->GetMaterials();
                                    hasMaterials = materialElement != NULL;
                                }

                                glm::PODArray<int> vertexMasks;
                                glm::PODArray<int> polygonMasks;

                                unsigned int fbxVertexCount = fbxMesh->GetControlPointsCount();
                                vertexMasks.assign(fbxVertexCount, -1);

                                unsigned int fbxPolyCount = fbxMesh->GetPolygonCount();
                                polygonMasks.assign(fbxPolyCount, 0);

                                unsigned int meshMtlIdx = outputData._meshAssetMaterialIndices[iMesh];

                                // check material id and reconstruct data
                                for (unsigned int iFbxPoly = 0; iFbxPoly < fbxPolyCount; ++iFbxPoly)
                                {
                                    unsigned int currentMtlIdx = 0;
                                    if (hasMaterials)
                                    {
                                        currentMtlIdx = materialElement->GetIndexArray().GetAt(iFbxPoly);
                                    }
                                    if (currentMtlIdx == meshMtlIdx)
                                    {
                                        polygonMasks[iFbxPoly] = 1;
                                        for (int iPolyVertex = 0, polyVertexCount = fbxMesh->GetPolygonSize(iFbxPoly); iPolyVertex < polyVertexCount; ++iPolyVertex)
                                        {
                                            int vertexId = fbxMesh->GetPolygonVertex(iFbxPoly, iPolyVertex);
                                            int& vertexMask = vertexMasks[vertexId];
                                            if (vertexMask >= 0)
                                            {
                                                continue;
                                            }
                                            vertexMask = 0;
                                        }
                                    }
                                }

                                unsigned int iActualVertex = 0;
                                for (unsigned int iFbxVertex = 0; iFbxVertex < fbxVertexCount; ++iFbxVertex)
                                {
                                    int& vertexMask = vertexMasks[iFbxVertex];
                                    if (vertexMask >= 0)
                                    {
                                        vertexMask = iActualVertex;
                                        ++iActualVertex;
                                    }
                                }

                                for (unsigned int iFbxPoly = 0; iFbxPoly < fbxPolyCount; ++iFbxPoly)
                                {
                                    if (polygonMasks[iFbxPoly])
                                    {
                                        int polySize = fbxMesh->GetPolygonSize(iFbxPoly);

                                        polyCounts.append(polySize);
                                        for (int iPolyVertex = 0; iPolyVertex < polySize; ++iPolyVertex)
                                        {
                                            // reverse polygon order
                                            polygonpointnumbers.append(vertexMasks[fbxMesh->GetPolygonVertex(iFbxPoly, polySize - 1 - iPolyVertex)]);
                                        } // iPolyVertex
                                    }
                                }

                                primOffset = GEO_PrimPoly::buildBlock(detailPtr, pointStartOffset, vertexCount, polyCounts, polygonpointnumbers.array());
                                GA_Primitive* prim = detailPtr->getPrimitive(primOffset);
                                vertexOffset = prim->getVertexOffset(0);

                                // find how many uv layers are available
                                int uvSetCount = fbxMesh->GetLayerCount(FbxLayerElement::eUV);
                                FbxLayerElementUV* uvElement = NULL;
                                for (int iUVSet = 0; iUVSet < uvSetCount; ++iUVSet)
                                {
                                    glm::GlmString attrName = detailPtr->getStdAttributeName(GEO_ATTRIBUTE_TEXTURE, iUVSet + 1).c_str();
                                    FbxLayer* layer = fbxMesh->GetLayer(fbxMesh->GetLayerTypedIndex((int)iUVSet, FbxLayerElement::eUV));
                                    uvElement = layer->GetUVs();
                                    bool uvsByControlPoint = uvElement->GetMappingMode() == FbxLayerElement::eByControlPoint;
                                    bool uvReferenceDirect = uvElement->GetReferenceMode() == FbxLayerElement::eDirect;

                                    GA_Attribute* uvAttr = detailPtr->addFloatTuple(GA_ATTRIB_VERTEX, attrName.c_str(), 3);
                                    uvAttr->setTypeInfo(GA_TypeInfo::GA_TYPE_TEXTURE_COORD);
                                    GA_RWHandleV3 uvAttrHandle(uvAttr);

                                    if (uvsByControlPoint)
                                    {
                                        // houdini doesn't mix attributes with the same name but different owners by default (point or vertex)
                                        // (the behavior can be overriden with GA_ReuseStrategy https://www.sidefx.com/docs/hdk/_h_d_k__geometry__intro.html#HDK_Geometry_Intro_Attribute)
                                        // to simplify things, we create a GA_ATTRIB_VERTEX attribute here instead of GA_ATTRIB_POINT

                                        int uvIndex;
                                        int actualIndexByPolyVertex = 0;
                                        for (unsigned int iFbxPoly = 0; iFbxPoly < fbxPolyCount; ++iFbxPoly)
                                        {
                                            int polySize = fbxMesh->GetPolygonSize(iFbxPoly);
                                            if (polygonMasks[iFbxPoly])
                                            {
                                                for (int iPolyVertex = 0; iPolyVertex < polySize; ++iPolyVertex)
                                                {
                                                    // reverse polygon order
                                                    uvIndex = vertexMasks[fbxMesh->GetPolygonVertex(iFbxPoly, polySize - 1 - iPolyVertex)];
                                                    if (!uvReferenceDirect)
                                                    {
                                                        uvIndex = uvElement->GetIndexArray().GetAt(uvIndex);
                                                    }
                                                    FbxVector2 tempUV(uvElement->GetDirectArray().GetAt(uvIndex));
                                                    uvAttrHandle.set(
                                                        vertexOffset + actualIndexByPolyVertex,
                                                        UT_Vector3F((float)tempUV[0], (float)tempUV[1], 0));

                                                    ++actualIndexByPolyVertex;
                                                }
                                            }
                                        }
                                    }
                                    else
                                    {
                                        int uvIndex;
                                        int actualIndexByPolyVertex = 0;
                                        int fbxIndexByPolyVertex = 0;
                                        for (unsigned int iFbxPoly = 0; iFbxPoly < fbxPolyCount; ++iFbxPoly)
                                        {
                                            int polySize = fbxMesh->GetPolygonSize(iFbxPoly);
                                            if (polygonMasks[iFbxPoly])
                                            {
                                                for (int iPolyVertex = 0; iPolyVertex < polySize; ++iPolyVertex, ++actualIndexByPolyVertex)
                                                {
                                                    // reverse polygon order
                                                    uvIndex = fbxIndexByPolyVertex + polySize - 1 - iPolyVertex;
                                                    if (!uvReferenceDirect)
                                                    {
                                                        uvIndex = uvElement->GetIndexArray().GetAt(uvIndex);
                                                    }

                                                    FbxVector2 tempUV(uvElement->GetDirectArray().GetAt(uvIndex));
                                                    uvAttrHandle.set(
                                                        vertexOffset + actualIndexByPolyVertex,
                                                        UT_Vector3F((float)tempUV[0], (float)tempUV[1], 0));

                                                } // iPolyVertex
                                            }
                                            fbxIndexByPolyVertex += polySize;
                                        } // iPoly
                                    }
                                }
                            }
                            else if (outputData._geoType == glm::crowdio::GeometryType::GCG)
                            {
                                glm::crowdio::GlmFileMeshTransform& assetFileMeshTransform = outputData._gcgCharacter->getGeometry()._transforms[outputData._transformIndicesInGcgFile[iMesh]];
                                glm::crowdio::GlmFileMesh& assetFileMesh = outputData._gcgCharacter->getGeometry()._meshes[assetFileMeshTransform._meshIndex];

                                for (uint32_t iPoly = 0, iVertex = 0; iPoly < assetFileMesh._polygonCount; ++iPoly)
                                {
                                    uint32_t polySize = assetFileMesh._polygonsVertexCount[iPoly];
                                    polyCounts.append(polySize);
                                    for (uint32_t iPolyVtx = 0; iPolyVtx < polySize; ++iPolyVtx)
                                    {
                                        // reverse polygon order
                                        polygonpointnumbers.append(assetFileMesh._polygonsVertexIndices[iVertex + polySize - 1 - iPolyVtx]);
                                    }
                                    iVertex += polySize;
                                }

                                primOffset = GEO_PrimPoly::buildBlock(detailPtr, pointStartOffset, vertexCount, polyCounts, polygonpointnumbers.array());
                                GA_Primitive* prim = detailPtr->getPrimitive(primOffset);
                                vertexOffset = prim->getVertexOffset(0);

                                if (assetFileMesh._uvSetCount > 0)
                                {
                                    for (size_t iUVSet = 0; iUVSet < assetFileMesh._uvSetCount; ++iUVSet)
                                    {
                                        glm::GlmString attrName = "uv";
                                        if (iUVSet > 0)
                                        {
                                            attrName += glm::toString(iUVSet + 1);
                                        }
                                        GA_Attribute* uvAttr = detailPtr->addFloatTuple(GA_ATTRIB_VERTEX, attrName.c_str(), 3);
                                        uvAttr->setTypeInfo(GA_TypeInfo::GA_TYPE_TEXTURE_COORD);
                                        GA_RWHandleV3 uvAttrHandle(uvAttr);
                                        if (assetFileMesh._uvMode == glm::crowdio::GLM_UV_PER_CONTROL_POINT)
                                        {
                                            // houdini doesn't mix attributes with the same name but different owners by default (point or vertex)
                                            // (the behavior can be overriden with GA_ReuseStrategy https://www.sidefx.com/docs/hdk/_h_d_k__geometry__intro.html#HDK_Geometry_Intro_Attribute)
                                            // to simplify things, we create a GA_ATTRIB_VERTEX attribute here instead of GA_ATTRIB_POINT

                                            for (uint32_t iPoly = 0, iVertex = 0; iPoly < assetFileMesh._polygonCount; ++iPoly)
                                            {
                                                uint32_t polySize = assetFileMesh._polygonsVertexCount[iPoly];
                                                for (uint32_t iPolyVtx = 0; iPolyVtx < polySize; ++iPolyVtx)
                                                {
                                                    // reverse polygon order
                                                    uint32_t uvIndex = assetFileMesh._polygonsVertexIndices[iVertex + polySize - 1 - iPolyVtx];
                                                    uvAttrHandle.set(
                                                        vertexOffset + iVertex + iPolyVtx,
                                                        UT_Vector3F(assetFileMesh._us[iUVSet][uvIndex], assetFileMesh._vs[iUVSet][uvIndex], 0));
                                                }
                                                iVertex += polySize;
                                            }
                                        }
                                        else
                                        {
                                            for (uint32_t iPoly = 0, iVertex = 0; iPoly < assetFileMesh._polygonCount; ++iPoly)
                                            {
                                                uint32_t polySize = assetFileMesh._polygonsVertexCount[iPoly];
                                                for (uint32_t iPolyVtx = 0; iPolyVtx < polySize; ++iPolyVtx)
                                                {
                                                    // reverse polygon order
                                                    uint32_t uvIndex = assetFileMesh._polygonsUVIndices[iVertex + polySize - 1 - iPolyVtx];
                                                    uvAttrHandle.set(
                                                        vertexOffset + iVertex + iPolyVtx,
                                                        UT_Vector3F(assetFileMesh._us[iUVSet][uvIndex], assetFileMesh._vs[iUVSet][uvIndex], 0));
                                                }
                                                iVertex += polySize;
                                            }
                                        }
                                    }
                                }
                            }
                            else
                            {
                                continue;
                            }

                            GA_Size actualPolyCount = polyCounts.getNumPolygons();

                            GA_Attribute* meshAttr = detailPtr->addStringTuple(GA_ATTRIB_PRIMITIVE, getMeshAttrName().c_str(), 1);
                            GA_RWHandleS meshAttrHandle(meshAttr);

                            GA_Attribute* materialAttr = detailPtr->addStringTuple(GA_ATTRIB_PRIMITIVE, GEO_STD_ATTRIB_MATERIAL, 1);
                            GA_RWHandleS materialAttrHandle(materialAttr);

                            int shadingGroupIdx = meshShadingGroups[iMesh];
                            glm::GlmString materialName = "";
                            if (shadingGroupIdx >= 0)
                            {
                                const glm::ShadingGroup& shGroup = _character->_shadingGroups[shadingGroupIdx];
                                materialName = _materialPath;
                                materialName.rtrim("/");
                                materialName += "/";
                                switch (_materialAssignMode)
                                {
                                case GolaemMaterialAssignMode::BY_SHADING_GROUP:
                                {
                                    materialName += shGroup._name;
                                }
                                break;
                                case GolaemMaterialAssignMode::BY_SURFACE_SHADER:
                                {
                                    // get the surface shader
                                    int shaderAssetIdx = (*_shadingGroupToSurfaceShader)[shadingGroupIdx];
                                    if (shaderAssetIdx >= 0)
                                    {
                                        const glm::ShaderAsset& shAsset = _character->_shaderAssets[shaderAssetIdx];
                                        materialName += shAsset._name;
                                    }
                                    else
                                    {
                                        materialName += "glmDefaultMat";
                                    }
                                }
                                break;
                                default:
                                    break;
                                }
                                materialName = glm::replaceString(materialName, ":", "_");

                                // add shading group attributes
                                for (size_t iShAttr = 0, shAttrCount = shGroup._shaderAttributes.size(); iShAttr < shAttrCount; ++iShAttr)
                                {
                                    int shAttrIdx = shGroup._shaderAttributes[iShAttr];
                                    const glm::ShaderAttribute& shAttr = _character->_shaderAttributes[shAttrIdx];
                                    UT_StringHolder attrName = UT_VarEncode::encode(shAttr._name.c_str());
                                    GA_Attribute* attr = NULL;
                                    switch (shAttr._type)
                                    {
                                    case glm::ShaderAttributeType::INT:
                                    {
                                        attr = detailPtr->addIntTuple(GA_ATTRIB_PRIMITIVE, attrName, 1);
                                        GA_RWHandleI attrHandle(attr);
                                        if (attrHandle.isValid())
                                        {
                                            size_t attrValueIdx = globalToIntShaderAttrIdx[iShAttr];
                                            int attrValue = intAttrValues[attrValueIdx];
                                            for (GA_Size iPoly = 0; iPoly < actualPolyCount; ++iPoly)
                                            {
                                                attrHandle.set(primOffset + iPoly, attrValue);
                                            }
                                        }
                                    }
                                    break;
                                    case glm::ShaderAttributeType::FLOAT:
                                    {
                                        attr = detailPtr->addFloatTuple(GA_ATTRIB_PRIMITIVE, attrName, 1);
                                        GA_RWHandleF attrHandle(attr);
                                        if (attrHandle.isValid())
                                        {
                                            size_t attrValueIdx = globalToFloatShaderAttrIdx[iShAttr];
                                            float attrValue = floatAttrValues[attrValueIdx];
                                            for (GA_Size iPoly = 0; iPoly < actualPolyCount; ++iPoly)
                                            {
                                                attrHandle.set(primOffset + iPoly, attrValue);
                                            }
                                        }
                                    }
                                    break;
                                    case glm::ShaderAttributeType::STRING:
                                    {
                                        attr = detailPtr->addStringTuple(GA_ATTRIB_PRIMITIVE, attrName, 1);
                                        GA_RWHandleS attrHandle(attr);
                                        if (attrHandle.isValid())
                                        {
                                            size_t attrValueIdx = globalToStringShaderAttrIdx[iShAttr];
                                            const glm::GlmString& attrValue = stringAttrValues[attrValueIdx];
                                            for (GA_Size iPoly = 0; iPoly < actualPolyCount; ++iPoly)
                                            {
                                                attrHandle.set(primOffset + iPoly, attrValue.c_str());
                                            }
                                        }
                                    }
                                    break;
                                    case glm::ShaderAttributeType::VECTOR:
                                    {
                                        attr = detailPtr->addFloatTuple(GA_ATTRIB_PRIMITIVE, attrName, 3);
                                        GA_RWHandleV3 attrHandle(attr);
                                        if (attrHandle.isValid())
                                        {
                                            size_t attrValueIdx = globalToVectorShaderAttrIdx[iShAttr];
                                            const glm::Vector3& attrValue = vectorAttrValues[attrValueIdx];
                                            for (GA_Size iPoly = 0; iPoly < actualPolyCount; ++iPoly)
                                            {
                                                attrHandle.set(
                                                    primOffset + iPoly,
                                                    UT_Vector3F(attrValue[0], attrValue[1], attrValue[2]));
                                            }
                                        }
                                    }
                                    break;
                                    default:
                                        break;
                                    }
                                    if (attr == NULL)
                                    {
                                        GLM_CROWD_TRACE_WARNING_LIMIT("Failed to add shader attribute '" << shAttr._name << "'");
                                    }
                                }
                            }

                            for (GA_Size iPoly = 0; iPoly < actualPolyCount; ++iPoly)
                            {
                                meshAttrHandle.set(primOffset + iPoly, meshName.c_str());
                                materialAttrHandle.set(primOffset + iPoly, materialName.c_str());
                            }
                        }
                    }
                }
                break;
                default:
                    break;
                }

                GA_Attribute* entityIdAttr = detailPtr->addTuple(GA_STORE_INT64, GA_ATTRIB_DETAIL, getEntityIdAttrName().c_str(), 1);
                GA_RWHandleID entityIdAttrHandle(entityIdAttr);
                // NOTE: The detail is *always* at GA_Offset(0) - otherwise you can get strange memory crashes...
                entityIdAttrHandle.set(GA_Offset(0), _inputData._entityId);
            }

            if (_updateGeo)
            {
                switch (_displayMode)
                {
                case glm::GolaemDisplayMode::BOUNDING_BOX:
                {
                    const GA_Offset& pointStartOffset = _pointStartOffsets[0];
                    detailPtr->setPos3(pointStartOffset,
                                       UT_Vector3(
                                           _rootPos[0] - _halfExtents[0],
                                           _rootPos[1] - _halfExtents[1],
                                           _rootPos[2] + _halfExtents[2]));

                    detailPtr->setPos3(pointStartOffset + 1,
                                       UT_Vector3(
                                           _rootPos[0] + _halfExtents[0],
                                           _rootPos[1] - _halfExtents[1],
                                           _rootPos[2] + _halfExtents[2]));

                    detailPtr->setPos3(pointStartOffset + 2,
                                       UT_Vector3(
                                           _rootPos[0] + _halfExtents[0],
                                           _rootPos[1] - _halfExtents[1],
                                           _rootPos[2] - _halfExtents[2]));

                    detailPtr->setPos3(pointStartOffset + 3,
                                       UT_Vector3(
                                           _rootPos[0] - _halfExtents[0],
                                           _rootPos[1] - _halfExtents[1],
                                           _rootPos[2] - _halfExtents[2]));

                    detailPtr->setPos3(pointStartOffset + 4,
                                       UT_Vector3(
                                           _rootPos[0] - _halfExtents[0],
                                           _rootPos[1] + _halfExtents[1],
                                           _rootPos[2] + _halfExtents[2]));

                    detailPtr->setPos3(pointStartOffset + 5,
                                       UT_Vector3(
                                           _rootPos[0] + _halfExtents[0],
                                           _rootPos[1] + _halfExtents[1],
                                           _rootPos[2] + _halfExtents[2]));

                    detailPtr->setPos3(pointStartOffset + 6,
                                       UT_Vector3(
                                           _rootPos[0] + _halfExtents[0],
                                           _rootPos[1] + _halfExtents[1],
                                           _rootPos[2] - _halfExtents[2]));

                    detailPtr->setPos3(pointStartOffset + 7,
                                       UT_Vector3(
                                           _rootPos[0] - _halfExtents[0],
                                           _rootPos[1] + _halfExtents[1],
                                           _rootPos[2] - _halfExtents[2]));
                }
                break;
                case glm::GolaemDisplayMode::SKELETON:
                {
                    const GA_Offset& pointStartOffset = _pointStartOffsets[0];
                    const glm::crowdio::GlmSimulationData* simuData = _inputData._cachedSimulation->getFinalSimulationData();
                    const glm::crowdio::GlmFrameData* frameData = _inputData._frameDatas[0];
                    // set the bone positions
                    uint16_t entityType = simuData->_entityTypes[_inputData._entityIndex];
                    uint16_t boneCount = simuData->_boneCount[entityType];
                    for (uint16_t iBone = 0; iBone < boneCount; ++iBone)
                    {
                        float* bonePos = frameData->_bonePositions[_bonePositionOffset + iBone];
                        detailPtr->setPos3(pointStartOffset + iBone,
                                           UT_Vector3(
                                               bonePos[0],
                                               bonePos[1],
                                               bonePos[2]));
                    }
                }
                break;
                case glm::GolaemDisplayMode::SKINMESH:
                {
                    if (!firstCompute)
                    {
                        geoStatus = glm::crowdio::glmPrepareEntityGeometry(&_inputData, &outputData);
                    }
                    if (geoStatus == glm::crowdio::GIO_SUCCESS)
                    {
                        size_t meshCount = outputData._meshAssetNameIndices.size();
                        glm::Array<glm::Array<glm::Vector3>>& frameDeformedVertices = outputData._deformedVertices[0];
                        glm::Array<glm::Array<glm::Vector3>>& frameDeformedNormals = outputData._deformedNormals[0];
                        for (size_t iMesh = 0; iMesh < meshCount; ++iMesh)
                        {
                            const glm::Array<glm::Vector3>& meshDeformedVertices = frameDeformedVertices[iMesh];
                            size_t vertexCount = meshDeformedVertices.size();
                            if (vertexCount == 0)
                            {
                                continue;
                            }

                            const GA_Offset& pointStartOffset = _pointStartOffsets[iMesh];
                            const glm::Array<glm::Vector3>& meshDeformedNormals = frameDeformedNormals[iMesh];
                            for (size_t iVertex = 0; iVertex < vertexCount; ++iVertex)
                            {
                                const glm::Vector3& meshVertex = meshDeformedVertices[iVertex];
                                detailPtr->setPos3(pointStartOffset + iVertex,
                                                   UT_Vector3(
                                                       meshVertex[0],
                                                       meshVertex[1],
                                                       meshVertex[2]));
                            }
                            const GA_Offset& vertexOffset = _vertexOffsets[iMesh];
                            if (outputData._geoType == glm::crowdio::GeometryType::FBX)
                            {
                                // must use the same fbx mutex because of fbx's 'unthreadfullness'
                                glm::ScopedLock<glm::Mutex> lock(glm::crowdio::getCrowdFBXMutex());
                                // when fbxMesh == NULL, vertexCount == 0, so no need to check fbxMesh != NULL
                                FbxMesh* fbxMesh = outputData._fbxCharacter->getCharacterFBXMesh(iMesh);

                                FbxLayer* fbxLayer0 = fbxMesh->GetLayer(0);
                                bool hasNormals = false;
                                bool hasMaterials = false;
                                FbxLayerElementMaterial* materialElement = NULL;
                                if (fbxLayer0 != NULL)
                                {
                                    hasNormals = fbxLayer0->GetNormals() != NULL;
                                    materialElement = fbxLayer0->GetMaterials();
                                    hasMaterials = materialElement != NULL;
                                }

                                glm::PODArray<int> polygonMasks;

                                unsigned int fbxPolyCount = fbxMesh->GetPolygonCount();
                                polygonMasks.assign(fbxPolyCount, 0);

                                unsigned int meshMtlIdx = outputData._meshAssetMaterialIndices[iMesh];

                                // check material id and reconstruct data
                                for (unsigned int iFbxPoly = 0; iFbxPoly < fbxPolyCount; ++iFbxPoly)
                                {
                                    unsigned int currentMtlIdx = 0;
                                    if (hasMaterials)
                                    {
                                        currentMtlIdx = materialElement->GetIndexArray().GetAt(iFbxPoly);
                                    }
                                    if (currentMtlIdx == meshMtlIdx)
                                    {
                                        polygonMasks[iFbxPoly] = 1;
                                    }
                                }
                                if (hasNormals)
                                {
                                    // add normals
                                    GA_Attribute* normalAttr = detailPtr->addNormalAttribute(GA_ATTRIB_VERTEX, GA_STORE_REAL32);
                                    GA_RWHandleV3 normalAttrHandle(normalAttr);

                                    // normals are always stored per polygon vertex
                                    int actualIndexByPolyVertex = 0;
                                    for (unsigned int iFbxPoly = 0; iFbxPoly < fbxPolyCount; ++iFbxPoly)
                                    {
                                        if (polygonMasks[iFbxPoly])
                                        {
                                            int polySize = fbxMesh->GetPolygonSize(iFbxPoly);
                                            for (int iPolyVertex = 0; iPolyVertex < polySize; ++iPolyVertex)
                                            {
                                                // reverse polygon order
                                                const glm::Vector3& deformedNormal = meshDeformedNormals[actualIndexByPolyVertex + polySize - 1 - iPolyVertex];
                                                normalAttrHandle.set(
                                                    vertexOffset + actualIndexByPolyVertex + iPolyVertex,
                                                    UT_Vector3F(deformedNormal[0], deformedNormal[1], deformedNormal[2]));
                                            }
                                            actualIndexByPolyVertex += polySize;
                                        }
                                    }
                                }
                            }
                            else if (outputData._geoType == glm::crowdio::GeometryType::GCG)
                            {
                                glm::crowdio::GlmFileMeshTransform& assetFileMeshTransform = outputData._gcgCharacter->getGeometry()._transforms[outputData._transformIndicesInGcgFile[iMesh]];
                                glm::crowdio::GlmFileMesh& assetFileMesh = outputData._gcgCharacter->getGeometry()._meshes[assetFileMeshTransform._meshIndex];

                                // add normals
                                GA_Attribute* normalAttr = detailPtr->addNormalAttribute(GA_ATTRIB_VERTEX, GA_STORE_REAL32);
                                GA_RWHandleV3 normalAttrHandle(normalAttr);

                                if (assetFileMesh._normalMode == glm::crowdio::GLM_NORMAL_PER_POLYGON_VERTEX)
                                {
                                    for (uint32_t iPoly = 0, iVertex = 0; iPoly < assetFileMesh._polygonCount; ++iPoly)
                                    {
                                        uint32_t polySize = assetFileMesh._polygonsVertexCount[iPoly];
                                        for (uint32_t iPolyVtx = 0; iPolyVtx < polySize; ++iPolyVtx)
                                        {
                                            // reverse polygon order
                                            const glm::Vector3& vtxNormal = meshDeformedNormals[iVertex + polySize - 1 - iPolyVtx];
                                            normalAttrHandle.set(
                                                vertexOffset + iVertex + iPolyVtx,
                                                UT_Vector3F(vtxNormal[0], vtxNormal[1], vtxNormal[2]));
                                        }
                                        iVertex += polySize;
                                    }
                                }
                                else
                                {
                                    uint32_t* polygonNormalIndices = assetFileMesh._normalMode == glm::crowdio::GLM_NORMAL_PER_CONTROL_POINT ? assetFileMesh._polygonsVertexIndices : assetFileMesh._polygonsNormalIndices;
                                    for (uint32_t iPoly = 0, iVertex = 0; iPoly < assetFileMesh._polygonCount; ++iPoly)
                                    {
                                        uint32_t polySize = assetFileMesh._polygonsVertexCount[iPoly];
                                        for (uint32_t iPolyVtx = 0; iPolyVtx < polySize; ++iPolyVtx)
                                        {
                                            // reverse polygon order
                                            uint32_t normalIdx = polygonNormalIndices[iVertex + polySize - 1 - iPolyVtx];
                                            const glm::Vector3& vtxNormal = meshDeformedNormals[normalIdx];
                                            normalAttrHandle.set(
                                                vertexOffset + iVertex + iPolyVtx,
                                                UT_Vector3F(vtxNormal[0], vtxNormal[1], vtxNormal[2]));
                                        }
                                        iVertex += polySize;
                                    }
                                }
                            }
                        }
                    }
                }
                break;
                default:
                    break;
                }

                _updateGeo = false;
            }
        }
        else if (!detailPtr->isEmpty())
        {
            // reset detail
            detailPtr = new GU_Detail();
            _detail.allocateAndSet(detailPtr, true);
        }
        return _detail;
    }

    //-----------------------------------------------------------------------------
    int64 GU_PackedGolaemEntity::getMemoryUsage(bool inclusive) const
    {
        int64 mem = inclusive ? sizeof(*this) : 0;
        mem += _detail.getMemoryUsage(false);
        return mem;
    }

    //-----------------------------------------------------------------------------
    void GU_PackedGolaemEntity::countMemory(UT_MemoryCounter& counter, bool inclusive) const
    {
        if (counter.mustCountUnshared())
        {
            size_t mem = getMemoryUsage(inclusive);
            //UT_MEMORY_DEBUG_LOG("GU_PackedSphere", int64(mem));
            counter.countUnshared(mem);
        }
    }
} // namespace glm