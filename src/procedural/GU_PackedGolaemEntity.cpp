/***************************************************************************
 *                                                                          *
 *  Copyright (C) Golaem S.A.  All Rights Reserved.                         *
 *                                                                          *
 ***************************************************************************/

#include "GU_PackedGolaemEntity.h"
#include "GT_PackedGolaemEntity.h"
#include "GR_PackedGolaemEntity.h"
#include "glmHoudiniUtils.h"

HDK_INCLUDES_START

#include <GEO/GEO_PolyCounts.h>
#include <GEO/GEO_PrimPoly.h>
#include <GU/GU_PackedFactory.h>
#include <GU/GU_PrimPacked.h>
#include <UT/UT_MemoryCounter.h>
#include <UT/UT_HDKVersion.h>
#include <FS/UT_DSO.h>
#include <UT/UT_VarEncode.h>
#include <DM/DM_RenderTable.h>

HDK_INCLUDES_END

#include <glmLog.h>
#include <glmSimulationData.h>
#include <glmFrameData.h>
#include <glmHierarchicalBone.h>
#include <glmGolaemCharacter.h>
#include <glmAssetManagementUtils.h>

#include <glmCrowdFBXCharacter.h>
#include <glmCrowdGcgCharacter.h>
#include <glmCrowdGcgBaker.h>
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
    UT_StringHolder sanitizeName(const UT_StringHolder& name)
    {
        return UT_VarEncode::encodeVar(name);
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

        const UT_IntrusivePtr<GU_PackedImpl>& defaultImpl() const override
        {
            return _defaultImpl;
        }

        UT_IntrusivePtr<GU_PackedImpl> _defaultImpl = new GU_PackedGolaemEntity();
    };

    static GU_PackedGolaemFactory* theGolaemFactory = NULL;

    //-----------------------------------------------------------------------------
    GU_PackedGolaemEntity::GU_PackedGolaemEntity()
        : _rootPos(0, 0, 0)
        , _halfExtents(0, 0, 0)
        , _inputData()
        , _character(NULL)
        , _sortedBonesInverse(NULL)
        , _shadingGroupToSurfaceShader(NULL)
        , _displayMode(GolaemDisplayMode::END)
        , _materialAssignMode(GolaemMaterialAssignMode::END)
        , _materialPath()
        , _isNew(true)
        , _updateGeo(false)
    {
        GU_Detail* detailPtr = new GU_Detail();
        _detail.allocateAndSet(detailPtr, true);

        _inputData._frameDatas.resize(1, NULL);
        _inputData._frames.resize(1);
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
            _shadingGroupToSurfaceShader = src._shadingGroupToSurfaceShader;
            _displayMode = src._displayMode;
            _materialAssignMode = src._materialAssignMode;
            _materialPath = src._materialPath;
            _isNew = src._isNew;
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
            // GT_PackedGolaemEntity::registerPrimitive(_typeId);

            // Since we're only registering one hook, the priority does not matter.
            /*int hookPriority = 0;

            DM_RenderTable::getTable()->registerGEOHook(
                new GR_PackedGolaemEntityHook(),
                _typeId,
                hookPriority,
                GUI_HOOK_FLAG_NONE);*/
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
        GLM_UNREFERENCED(map);
        updateFrom(prim, options);
        // invalidate entity
        _inputData._entityId = -1;
        return true;
    }

    //-----------------------------------------------------------------------------
    void GU_PackedGolaemEntity::updateFrom(GU_PrimPacked* prim, const UT_Options& options)
    {
        GLM_UNREFERENCED(prim);
        GLM_UNREFERENCED(options);
        clearGeo();
    }

    //-----------------------------------------------------------------------------
    void GU_PackedGolaemEntity::update(GU_PrimPacked* prim, const UT_Options& options)
    {
        updateFrom(prim, options);
    }

    //-----------------------------------------------------------------------------
    bool GU_PackedGolaemEntity::save(UT_Options& options, const GA_SaveMap& map) const
    {
        GLM_UNREFERENCED(options);
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

                const glm::crowdio::GlmSimulationData* simuData = _inputData._simuData;

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
                    const ShaderAssetDataContainer* shaderDataContainer = _inputData._shaderDataContainer;

                    const glm::PODArray<int>& entityIntShaderData = shaderDataContainer->intData[_inputData._entityIndex];
                    const glm::PODArray<float>& entityFloatShaderData = shaderDataContainer->floatData[_inputData._entityIndex];
                    const glm::Array<glm::Vector3>& entityVectorShaderData = shaderDataContainer->vectorData[_inputData._entityIndex];
                    const glm::Array<glm::GlmString>& entityStringShaderData = shaderDataContainer->stringData[_inputData._entityIndex];

                    const PODArray<size_t>& globalToSpecificShaderAttrIdx = shaderDataContainer->globalToSpecificShaderAttrIdxPerChar[_inputData._characterIdx];

                    _inputData._fbxStorage = glm::Singleton<glm::HoudiniFbxData>::getInstance().getFbxStorage();
                    _inputData._fbxBaker = glm::Singleton<glm::HoudiniFbxData>::getInstance().getFbxBaker();

                    geoStatus = glm::crowdio::glmPrepareEntityGeometry(&_inputData, &outputData);
                    if (geoStatus == glm::crowdio::GIO_SUCCESS)
                    {
                        size_t meshCount = outputData._meshAssetNameIndices.size();
                        _pointStartOffsets.resize(meshCount);

                        glm::Array<glm::Array<glm::Vector3>>& frameDeformedVertices = outputData._deformedVertices[0];

                        _vertexOffsets.resize(meshCount);

                        for (size_t iRenderMesh = 0; iRenderMesh < meshCount; ++iRenderMesh)
                        {
                            size_t iGeoFileMesh = outputData._meshAssetNameIndices[iRenderMesh];
                            size_t meshIdxInVertexArray = outputData._geoType == glm::crowdio::GeometryType::FBX ? iGeoFileMesh : iRenderMesh;
                            const glm::Array<glm::Vector3>& meshDeformedVertices = frameDeformedVertices[meshIdxInVertexArray];
                            size_t vertexCount = meshDeformedVertices.size();
                            if (vertexCount == 0)
                            {
                                continue;
                            }

                            const glm::GlmString& meshName = outputData._meshAssetNames[iGeoFileMesh];

                            polyCounts.clear();
                            polygonpointnumbers.clear();

                            GA_Offset primOffset;
                            GA_Offset& vertexOffset = _vertexOffsets[iRenderMesh];

                            GA_Offset& pointStartOffset = _pointStartOffsets[iRenderMesh];

                            pointStartOffset = detailPtr->appendPointBlock(vertexCount);

                            if (outputData._geoType == glm::crowdio::GeometryType::FBX)
                            {
                                crowdio::CrowdFBXCharacter* fbxCharacter = outputData._fbxCharacters[0];
                                // when fbxMesh == NULL, vertexCount == 0, so no need to check fbxMesh != NULL
                                FbxMesh* fbxMesh = fbxCharacter->getCharacterFBXMesh(iGeoFileMesh);

                                FbxLayer* fbxLayer0 = fbxMesh->GetLayer(0);
                                bool hasMaterials = false;
                                FbxLayerElementMaterial* materialElement = NULL;
                                if (fbxLayer0 != NULL)
                                {
                                    materialElement = fbxLayer0->GetMaterials();
                                    hasMaterials = materialElement != NULL;
                                }

                                glm::PODArray<int> vertexMasks;
                                glm::PODArray<int> polygonMasks;

                                unsigned int fbxVertexCount = fbxMesh->GetControlPointsCount();
                                vertexMasks.assign(fbxVertexCount, -1);

                                unsigned int fbxPolyCount = fbxMesh->GetPolygonCount();
                                polygonMasks.assign(fbxPolyCount, 0);

                                unsigned int meshMtlIdx = outputData._meshAssetMaterialIndices[iRenderMesh];

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

                                for (unsigned int iFbxVertex = 0, iActualVertex = 0; iFbxVertex < fbxVertexCount; ++iFbxVertex)
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
                                        for (unsigned int iFbxPoly = 0, actualIndexByPolyVertex = 0; iFbxPoly < fbxPolyCount; ++iFbxPoly)
                                        {
                                            int polySize = fbxMesh->GetPolygonSize(iFbxPoly);
                                            if (polygonMasks[iFbxPoly])
                                            {
                                                for (int iPolyVertex = 0; iPolyVertex < polySize; ++iPolyVertex, ++actualIndexByPolyVertex)
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
                                                }
                                            }
                                        }
                                    }
                                    else
                                    {
                                        int uvIndex;
                                        for (unsigned int iFbxPoly = 0, actualIndexByPolyVertex = 0, fbxIndexByPolyVertex = 0; iFbxPoly < fbxPolyCount; ++iFbxPoly)
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
                                crowdio::CrowdGcgCharacter* gcgCharacter = outputData._gcgCharacters[0];
                                glm::crowdio::GlmFileMeshTransform& assetFileMeshTransform = gcgCharacter->getGeometry()._transforms[outputData._transformIndicesInGcgFile[iRenderMesh]];
                                glm::crowdio::GlmFileMesh& assetFileMesh = gcgCharacter->getGeometry()._meshes[assetFileMeshTransform._meshIndex];

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

                            int shadingGroupIdx = outputData._meshShadingGroups[iRenderMesh];
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
                                    size_t specificAttrIdx = globalToSpecificShaderAttrIdx[shAttrIdx];
                                    UT_StringHolder attrName = sanitizeName(shAttr._name.c_str());
                                    GA_Attribute* attr = NULL;
                                    switch (shAttr._type)
                                    {
                                    case glm::ShaderAttributeType::INT:
                                    {
                                        attr = detailPtr->addIntTuple(GA_ATTRIB_PRIMITIVE, attrName, 1);
                                        GA_RWHandleI attrHandle(attr);
                                        if (attrHandle.isValid())
                                        {
                                            int attrValue = entityIntShaderData[specificAttrIdx];
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
                                            float attrValue = entityFloatShaderData[specificAttrIdx];
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
                                            const glm::GlmString& attrValue = entityStringShaderData[specificAttrIdx];
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
                                            const glm::Vector3& attrValue = entityVectorShaderData[specificAttrIdx];
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
                    const glm::crowdio::GlmSimulationData* simuData = _inputData._simuData;
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
                        // ----- FBX specific data
                        FbxAMatrix nodeTransform;
                        FbxAMatrix geomTransform;
                        FbxAMatrix identityMatrix;
                        identityMatrix.SetIdentity();
                        FbxTime fbxTime;
                        FbxVector4 fbxVect;
                        // ----- end FBX specific data

                        if (outputData._geoType == glm::crowdio::GeometryType::FBX)
                        {
                            // Extract frame
                            if (outputData._geoBeInfo._idGeometryFileIdx != -1)
                            {
                                crowdio::CrowdFBXCharacter* fbxCharacter = outputData._fbxCharacters[0];
                                const glm::crowdio::GlmFrameData* frameData = _inputData._frameDatas[0];
                                float(&geometryFrameCacheData)[3] = frameData->_geoBehaviorAnimFrameInfo[outputData._geoBeInfo._geoDataIndex];
                                double frameRate(FbxTime::GetFrameRate(fbxCharacter->touchFBXScene()->GetGlobalSettings().GetTimeMode()));
                                fbxTime.SetGlobalTimeMode(FbxTime::eCustom, frameRate);
                                fbxTime.SetMilliSeconds(long((double)geometryFrameCacheData[0] / frameRate * 1000.0));
                            }
                            else
                            {
                                fbxTime = 0;
                            }
                        }

                        size_t meshCount = outputData._meshAssetNameIndices.size();
                        glm::Array<glm::Array<glm::Vector3>>& frameDeformedVertices = outputData._deformedVertices[0];
                        glm::Array<glm::Array<glm::Vector3>>& frameDeformedNormals = outputData._deformedNormals[0];
                        for (size_t iRenderMesh = 0; iRenderMesh < meshCount; ++iRenderMesh)
                        {
                            size_t iGeoFileMesh = outputData._meshAssetNameIndices[iRenderMesh];
                            size_t meshIdxInVertexArray = outputData._geoType == glm::crowdio::GeometryType::FBX ? iGeoFileMesh : iRenderMesh;
                            const glm::Array<glm::Vector3>& meshDeformedVertices = frameDeformedVertices[meshIdxInVertexArray];
                            size_t vertexCount = meshDeformedVertices.size();
                            if (vertexCount == 0)
                            {
                                continue;
                            }

                            const GA_Offset& pointStartOffset = _pointStartOffsets[iRenderMesh];
                            const glm::Array<glm::Vector3>& meshDeformedNormals = frameDeformedNormals[meshIdxInVertexArray];
                            const GA_Offset& vertexOffset = _vertexOffsets[iRenderMesh];
                            if (outputData._geoType == glm::crowdio::GeometryType::FBX)
                            {
                                crowdio::CrowdFBXCharacter* fbxCharacter = outputData._fbxCharacters[0];
                                // when fbxMesh == NULL, vertexCount == 0, so no need to check fbxMesh != NULL
                                FbxNode* fbxNode = fbxCharacter->getCharacterFBXMeshes()[iGeoFileMesh];
                                FbxMesh* fbxMesh = fbxCharacter->getCharacterFBXMesh(iGeoFileMesh);

                                // for each mesh, get the transform in case of its position in not relative to the center of the world
                                fbxCharacter->getMeshGlobalTransform(nodeTransform, fbxNode, fbxTime);
                                glm::crowdio::CrowdFBXBaker::getGeomTransform(geomTransform, fbxNode);
                                nodeTransform *= geomTransform;

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

                                bool hasTransform = !(nodeTransform == identityMatrix);

                                glm::PODArray<int> vertexMasks;
                                glm::PODArray<int> polygonMasks;

                                unsigned int fbxVertexCount = fbxMesh->GetControlPointsCount();
                                vertexMasks.assign(fbxVertexCount, -1);

                                unsigned int fbxPolyCount = fbxMesh->GetPolygonCount();
                                polygonMasks.assign(fbxPolyCount, 0);

                                unsigned int meshMtlIdx = outputData._meshAssetMaterialIndices[iRenderMesh];

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

                                for (unsigned int iFbxVertex = 0, iActualVertex = 0; iFbxVertex < fbxVertexCount; ++iFbxVertex)
                                {
                                    int& vertexMask = vertexMasks[iFbxVertex];
                                    if (vertexMask >= 0)
                                    {
                                        vertexMask = iActualVertex;
                                        ++iActualVertex;
                                    }
                                }

                                for (unsigned int iFbxVertex = 0; iFbxVertex < fbxVertexCount; ++iFbxVertex)
                                {
                                    int vertexMask = vertexMasks[iFbxVertex];
                                    if (vertexMask >= 0)
                                    {
                                        // meshDeformedVertices contains all fbx points, not just the ones that were filtered by vertexMasks

                                        // vertices
                                        if (hasTransform)
                                        {
                                            const Vector3& glmVect = meshDeformedVertices[iFbxVertex];
                                            fbxVect.Set(glmVect.x, glmVect.y, glmVect.z);
                                            // transform vertex in case of local transformation
                                            fbxVect = nodeTransform.MultT(fbxVect);
                                            detailPtr->setPos3(pointStartOffset + vertexMask,
                                                               UT_Vector3(
                                                                   (float)fbxVect[0],
                                                                   (float)fbxVect[1],
                                                                   (float)fbxVect[2]));
                                        }
                                        else
                                        {
                                            const Vector3& meshVertex = meshDeformedVertices[iFbxVertex];
                                            detailPtr->setPos3(pointStartOffset + vertexMask,
                                                               UT_Vector3(
                                                                   meshVertex[0],
                                                                   meshVertex[1],
                                                                   meshVertex[2]));
                                        }
                                    }
                                }

                                if (hasNormals)
                                {
                                    FbxAMatrix globalRotate(identityMatrix);
                                    globalRotate.SetR(nodeTransform.GetR());
                                    bool hasRotate = globalRotate != identityMatrix;

                                    // add normals
                                    GA_Attribute* normalAttr = detailPtr->addNormalAttribute(GA_ATTRIB_VERTEX, GA_STORE_REAL32);
                                    GA_RWHandleV3 normalAttrHandle(normalAttr);

                                    // normals are always stored per polygon vertex
                                    for (unsigned int iFbxPoly = 0, iFbxNormal = 0, actualIndexByPolyVertex = 0; iFbxPoly < fbxPolyCount; ++iFbxPoly)
                                    {
                                        int polySize = fbxMesh->GetPolygonSize(iFbxPoly);
                                        if (polygonMasks[iFbxPoly])
                                        {
                                            for (int iPolyVertex = 0; iPolyVertex < polySize; ++iPolyVertex, ++actualIndexByPolyVertex)
                                            {
                                                // meshDeformedNormals contains all fbx normals, not just the ones that were filtered by polygonMasks
                                                // reverse polygon order
                                                if (hasRotate)
                                                {
                                                    const Vector3& glmVect = meshDeformedNormals[iFbxNormal + polySize - 1 - iPolyVertex];
                                                    fbxVect.Set(glmVect.x, glmVect.y, glmVect.z);
                                                    fbxVect = globalRotate.MultT(fbxVect);
                                                    normalAttrHandle.set(
                                                        vertexOffset + actualIndexByPolyVertex,
                                                        UT_Vector3F((float)fbxVect[0], (float)fbxVect[1], (float)fbxVect[2]));
                                                }
                                                else
                                                {
                                                    const glm::Vector3& deformedNormal = meshDeformedNormals[iFbxNormal + polySize - 1 - iPolyVertex];
                                                    normalAttrHandle.set(
                                                        vertexOffset + actualIndexByPolyVertex,
                                                        UT_Vector3F(deformedNormal[0], deformedNormal[1], deformedNormal[2]));
                                                }
                                            }
                                        }
                                        iFbxNormal += polySize;
                                    }
                                }
                            }
                            else if (outputData._geoType == glm::crowdio::GeometryType::GCG)
                            {
                                for (size_t iVertex = 0; iVertex < vertexCount; ++iVertex)
                                {
                                    const glm::Vector3& meshVertex = meshDeformedVertices[iVertex];
                                    detailPtr->setPos3(pointStartOffset + iVertex,
                                                       UT_Vector3(
                                                           meshVertex[0],
                                                           meshVertex[1],
                                                           meshVertex[2]));
                                }
                                crowdio::CrowdGcgCharacter* gcgCharacter = outputData._gcgCharacters[0];
                                glm::crowdio::GlmFileMeshTransform& assetFileMeshTransform = gcgCharacter->getGeometry()._transforms[outputData._transformIndicesInGcgFile[iRenderMesh]];
                                glm::crowdio::GlmFileMesh& assetFileMesh = gcgCharacter->getGeometry()._meshes[assetFileMeshTransform._meshIndex];

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
            // release fbx locks here, called by outputData destructor, but add the explicit call just in case
            outputData.finish();
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
            // UT_MEMORY_DEBUG_LOG("GU_PackedSphere", int64(mem));
            counter.countUnshared(mem);
        }
    }
} // namespace glm