/***************************************************************************
*                                                                          *
*  Copyright (C) Golaem S.A.  All Rights Reserved.                         *
*                                                                          *
***************************************************************************/

#include "GR_PackedGolaemEntity.h"
#include "GU_PackedGolaemEntity.h"
#include "glmHoudiniUtils.h"

HDK_INCLUDES_START

#include <GU/GU_PrimPacked.h>
#include <RE/RE_Geometry.h>
#include <RE/RE_VertexArray.h>
#include <RE/RE_ShaderHandle.h>
#include <RE/RE_Render.h>
#include <RE/RE_LightList.h>
#include <GR/GR_Utils.h>
#include <RE/RE_ElementArray.h>
#if HDK_API_VERSION >= 20000000
#include <GR/GR_Light.h>
#endif

HDK_INCLUDES_END

#include <glmSimulationData.h>
#include <glmFrameData.h>
#include <glmHierarchicalBone.h>
#include <glmGolaemCharacter.h>
#include <glmSimulationCacheFactorySimulation.h>

#include <glmCrowdFBXCharacter.h>
#include <glmCrowdGcgStorage.h>
#include <glmCrowdGcgCharacter.h>
#include <glmCrowdGcgBaker.h>
#include <glmCrowdFBXStorage.h>
#include <glmCrowdFBXBaker.h>
#include <glmRenderGeometry.h>

namespace glm
{
    //-----------------------------------------------------------------------------
    GR_PackedGolaemEntityHook::GR_PackedGolaemEntityHook()
        : GUI_PrimitiveHook("GolaemEntity")
    {
    }

    //-----------------------------------------------------------------------------
    GR_PackedGolaemEntityHook::~GR_PackedGolaemEntityHook()
    {
    }

    //-----------------------------------------------------------------------------
    GR_Primitive* GR_PackedGolaemEntityHook::createPrimitive(
        const GT_PrimitiveHandle& gt_prim,
        const GEO_Primitive* geo_prim,
        const GR_RenderInfo* info,
        const char* cache_name,
        GR_PrimAcceptResult& processed)
    {
        GLM_UNREFERENCED(gt_prim);
        processed = GR_PROCESSED;
        return new GR_PackedGolaemEntity(info, cache_name, geo_prim);
    }

    //-----------------------------------------------------------------------------
    GR_PackedGolaemEntity::GR_PackedGolaemEntity(
        const GR_RenderInfo* info,
        const char* cache_name,
        const GEO_Primitive* prim)
        : GR_Primitive(info, cache_name, GA_PrimCompat::TypeMask(0))
        , _viewportGeo(NULL)
        , _packedEntity(NULL)
    {
        GLM_UNREFERENCED(prim);
        //const GU_PrimPacked* packedPrim = static_cast<const GU_PrimPacked*>(prim);
        //_packedEntity = static_cast<const glm::GU_PackedGolaemEntity*>(packedPrim->implementation());
    }

    //-----------------------------------------------------------------------------
    void GR_PackedGolaemEntity::clearGeo()
    {
		GLM_SAFE_DELETE_NOPROFILING(_viewportGeo);
    }

    //-----------------------------------------------------------------------------
    GR_PackedGolaemEntity::~GR_PackedGolaemEntity()
    {
        clearGeo();
    }

    //-----------------------------------------------------------------------------
    const char* GR_PackedGolaemEntity::className() const
    {
        return "GR_PackedGolaemEntity";
    }

    //-----------------------------------------------------------------------------
    GR_PrimAcceptResult GR_PackedGolaemEntity::acceptPrimitive(
        GT_PrimitiveType t,
        int geo_type,
        const GT_PrimitiveHandle& ph,
        const GEO_Primitive* prim)
    {
        GLM_UNREFERENCED(t);
        GLM_UNREFERENCED(ph);
        GLM_UNREFERENCED(prim);
        if (geo_type == GU_PackedGolaemEntity::getTypeId().get())
        {
            return GR_PROCESSED;
        }
        return GR_NOT_PROCESSED;
    }

    //-----------------------------------------------------------------------------
    bool GR_PackedGolaemEntity::getBoundingBox(UT_BoundingBoxD& bbox) const
    {
        if (_packedEntity->_inputData._entityId != -1)
        {
            Vector3 vect = _packedEntity->_rootPos;
            vect -= _packedEntity->_halfExtents;
            bbox.initBounds(vect.getFloatValues());
            vect = _packedEntity->_rootPos;
            vect += _packedEntity->_halfExtents;
            bbox.enlargeBounds(vect.getFloatValues());
            return true;
        }
        return false;
    }

    //-----------------------------------------------------------------------------
    void GR_PackedGolaemEntity::assignEntity(const GU_PackedGolaemEntity* packedEntity)
    {
        bool entityIsNew = _packedEntity != packedEntity || _packedEntity->_inputData._entityId != packedEntity->_inputData._entityId;
        _packedEntity = packedEntity;
        if (_packedEntity->_inputData._entityId != -1)
        {
            entityIsNew = entityIsNew || _packedEntity->_isNew;
            if (entityIsNew)
            {
                clearGeo();
            }
        }
    }

    //-----------------------------------------------------------------------------
    void GR_PackedGolaemEntity::update(
#if HDK_API_VERSION >= 20000000
        RE_RenderContext rend,
#else
        RE_Render* rend,
#endif
        const GT_PrimitiveHandle& primh,
        const GR_UpdateParms& params)
    {   
        //const GT_GEOPrimitive* geoPrim = static_cast<const GT_GEOPrimitive*>(primh.get());
        //const GU_PrimPacked* packedPrim = static_cast<const GU_PrimPacked*>(geoPrim->getPrimitive(0));

        const GU_PrimPacked* packedPrim = NULL;
        getGEOPrimFromGT<GU_PrimPacked>(primh, packedPrim);
        const int num_tets = packedPrim ? 1 : 0;
        if (num_tets == 0)
            return;

        const GU_PackedGolaemEntity* packedEntity = static_cast<const glm::GU_PackedGolaemEntity*>(packedPrim->sharedImplementation());

        assignEntity(packedEntity);

        if (_packedEntity->_inputData._entityId == -1)
        {
            clearGeo();
            return;
        }

        // Initialize the geometry with the proper name for the GL cache
        bool firstCompute = _viewportGeo == NULL;
        if (firstCompute)
        {
            _viewportGeo = new RE_Geometry();
        }
        _viewportGeo->cacheBuffers(getCacheName());

        glm::crowdio::OutputEntityGeoData outputData; // TODO: see if storage is better
        glm::crowdio::GlmGeometryGenerationStatus geoStatus = glm::crowdio::GIO_SUCCESS;

        GR_UpdateParms decorationParams(params);
        const GR_Decoration pdecs[] = {
            GR_POINT_MARKER,
            GR_POINT_NUMBER,
            GR_POINT_NORMAL,
            GR_POINT_UV,
            GR_POINT_POSITION,
            GR_POINT_VELOCITY,
            GR_PRIM_NORMAL,
            GR_PRIM_NUMBER,
            GR_PRIM_HULL,
            GR_PRIM_BREAKPOINT,
            GR_VERTEX_MARKER,
            GR_VERTEX_NORMAL,
            GR_VERTEX_NUMBER,
            GR_VERTEX_UV,
            GR_NO_DECORATION};

        myDecorRender->setupFromDisplayOptions(params.dopts, params.required_dec, decorationParams, pdecs, GR_ALL_ATTRIBS);

        if (firstCompute)
        {
            switch (_packedEntity->_displayMode)
            {
            case GolaemDisplayMode::BOUNDING_BOX:
            {
                // debug print all materials
                for (auto itMat = params.material_atlas.cbegin(); itMat != params.material_atlas.cend(); ++itMat)
                {
                    RE_MaterialPtr currentMat = itMat->second;
                    currentMat->getMaterialName();
                }

                // Initialize the number of points in the geometry.
                _viewportGeo->setNumPoints(8);
                // Initialize the number of primitives in the geometry. 6 faces
                _viewportGeo->setNumPrimitives(6);
                _viewportGeo->setNumVertices(24);

                PODArray<unsigned int> lineConnect;
                lineConnect.push_back(0);
                lineConnect.push_back(1);

                lineConnect.push_back(1);
                lineConnect.push_back(2);

                lineConnect.push_back(2);
                lineConnect.push_back(3);

                lineConnect.push_back(3);
                lineConnect.push_back(0);

                lineConnect.push_back(0);
                lineConnect.push_back(4);

                lineConnect.push_back(4);
                lineConnect.push_back(5);

                lineConnect.push_back(5);
                lineConnect.push_back(1);

                lineConnect.push_back(5);
                lineConnect.push_back(6);

                lineConnect.push_back(6);
                lineConnect.push_back(2);

                lineConnect.push_back(6);
                lineConnect.push_back(7);

                lineConnect.push_back(7);
                lineConnect.push_back(3);

                lineConnect.push_back(7);
                lineConnect.push_back(4);

                PODArray<int> polyIds;
                PODArray<int> polySizes;
                for (size_t iFace = 0; iFace < 6; ++iFace)
                {
                    polySizes.push_back(4);
                }

                // face 0
                polyIds.push_back(0);
                polyIds.push_back(1);
                polyIds.push_back(2);
                polyIds.push_back(3);

                // face 1
                polyIds.push_back(1);
                polyIds.push_back(5);
                polyIds.push_back(6);
                polyIds.push_back(2);

                // face 2
                polyIds.push_back(2);
                polyIds.push_back(6);
                polyIds.push_back(7);
                polyIds.push_back(3);

                // face 3
                polyIds.push_back(3);
                polyIds.push_back(7);
                polyIds.push_back(4);
                polyIds.push_back(0);

                // face 4
                polyIds.push_back(0);
                polyIds.push_back(4);
                polyIds.push_back(5);
                polyIds.push_back(1);

                // face 5
                polyIds.push_back(4);
                polyIds.push_back(7);
                polyIds.push_back(6);
                polyIds.push_back(5);

                RE_ElementArray* polyArray = new RE_ElementArray();
                polyArray->requirePrimInfo(true);
                polyArray->requireVertexInfo(true);
                polyArray->setCacheName(getCacheName());
                polyArray->setPrimitiveType(RE_PRIM_POLYGONS);

                polyArray->beginPrims(rend);
                for (int iPoly = 0, polyCount = polySizes.sizeInt(), vertexOrigin = 0; iPoly < polyCount; ++iPoly)
                {
                    int polySize = polySizes[iPoly];
                    polyArray->addPrim(rend, polySize, &polyIds[vertexOrigin], NULL, NULL, iPoly, vertexOrigin);
                    vertexOrigin += polySize;
                }

                polyArray->endPrims(rend);

                polyArray->setCacheVersion(params.geo_version);

                _viewportGeo->connectIndexedPrims(rend, RE_GEO_WIRE_IDX, RE_PRIM_LINES, lineConnect.sizeInt(), lineConnect.begin(), NULL, true);
                _viewportGeo->connectIndexedPrims(rend, RE_GEO_SHADED_IDX, polyArray, NULL, true);
                {
                    RE_VertexArray* vertexNormals = _viewportGeo->findCachedAttrib(rend, GEO_STD_ATTRIB_NORMAL, RE_GPU_FLOAT32, 3, RE_ARRAY_VERTEX, true);
                    if (vertexNormals->getCacheVersion() != params.geo_version)
                    {
                        // map() returns a pointer to the GL buffer
                        UT_Vector3F* normalData = static_cast<UT_Vector3F*>(vertexNormals->map(rend));

                        int vertexIdx = 0;

                        // face 0
                        for (int iVtx = 0; iVtx < 4; ++iVtx, ++vertexIdx)
                        {
                            normalData[vertexIdx].assign(0, -1, 0);
                        }

                        // face 1
                        for (int iVtx = 0; iVtx < 4; ++iVtx, ++vertexIdx)
                        {
                            normalData[vertexIdx].assign(1, 0, 0);
                        }

                        // face 2
                        for (int iVtx = 0; iVtx < 4; ++iVtx, ++vertexIdx)
                        {
                            normalData[vertexIdx].assign(0, 0, -1);
                        }

                        // face 3
                        for (int iVtx = 0; iVtx < 4; ++iVtx, ++vertexIdx)
                        {
                            normalData[vertexIdx].assign(-1, 0, 0);
                        }

                        // face 4
                        for (int iVtx = 0; iVtx < 4; ++iVtx, ++vertexIdx)
                        {
                            normalData[vertexIdx].assign(0, 0, 1);
                        }

                        // face 5
                        for (int iVtx = 0; iVtx < 4; ++iVtx, ++vertexIdx)
                        {
                            normalData[vertexIdx].assign(0, 1, 0);
                        }

                        // unmap the buffer so it can be used by GL
                        vertexNormals->unmap(rend);

                        // Always set the cache version after assigning data.
                        vertexNormals->setCacheVersion(params.geo_version);
                    }
                }
                {
                    RE_VertexArray* vertexUVs = _viewportGeo->findCachedAttrib(rend, GEO_STD_ATTRIB_TEXTURE, RE_GPU_FLOAT32, 2, RE_ARRAY_VERTEX, true);
                    if (vertexUVs->getCacheVersion() != params.geo_version)
                    {
                        // map() returns a pointer to the GL buffer
                        UT_Vector2F* uvData = static_cast<UT_Vector2F*>(vertexUVs->map(rend));

                        int vertexIdx = 0;

                        uvData[vertexIdx + 0].assign(0, 0);
                        uvData[vertexIdx + 1].assign(1, 0);
                        uvData[vertexIdx + 2].assign(1, 1);
                        uvData[vertexIdx + 3].assign(0, 1);
                        vertexIdx += 4;

                        uvData[vertexIdx + 0].assign(0, 0);
                        uvData[vertexIdx + 1].assign(1, 0);
                        uvData[vertexIdx + 2].assign(1, 1);
                        uvData[vertexIdx + 3].assign(0, 1);
                        vertexIdx += 4;

                        uvData[vertexIdx + 0].assign(0, 0);
                        uvData[vertexIdx + 1].assign(1, 0);
                        uvData[vertexIdx + 2].assign(1, 1);
                        uvData[vertexIdx + 3].assign(0, 1);
                        vertexIdx += 4;

                        uvData[vertexIdx + 0].assign(0, 0);
                        uvData[vertexIdx + 1].assign(1, 0);
                        uvData[vertexIdx + 2].assign(1, 1);
                        uvData[vertexIdx + 3].assign(0, 1);
                        vertexIdx += 4;

                        uvData[vertexIdx + 0].assign(0, 0);
                        uvData[vertexIdx + 1].assign(1, 0);
                        uvData[vertexIdx + 2].assign(1, 1);
                        uvData[vertexIdx + 3].assign(0, 1);
                        vertexIdx += 4;

                        uvData[vertexIdx + 0].assign(0, 0);
                        uvData[vertexIdx + 1].assign(1, 0);
                        uvData[vertexIdx + 2].assign(1, 1);
                        uvData[vertexIdx + 3].assign(0, 1);

                        // unmap the buffer so it can be used by GL
                        vertexUVs->unmap(rend);

                        // Always set the cache version after assigning data.
                        vertexUVs->setCacheVersion(params.geo_version);
                    }
                }
            }
            break;
            case GolaemDisplayMode::SKELETON:
            {
                const glm::crowdio::GlmSimulationData* simuData = _packedEntity->_inputData._simuData;

                uint16_t entityType = simuData->_entityTypes[_packedEntity->_inputData._entityIndex];
                uint16_t boneCount = simuData->_boneCount[entityType];
                _bonePositionOffset = simuData->_iBoneOffsetPerEntityType[entityType] + simuData->_indexInEntityType[_packedEntity->_inputData._entityIndex] * boneCount;

                // Initialize the number of points in the geometry.
                _viewportGeo->setNumPoints(boneCount);

                PODArray<unsigned int> lineConnect;

                const glm::PODArray<size_t>& sortedBonesInverse = *_packedEntity->_sortedBonesInverse;

                const glm::PODArray<glm::HierarchicalBone*>& hBones = _packedEntity->_character->_converterMapping._skeletonDescription->getBones();
                const glm::PODArray<size_t>& sortedBones = _packedEntity->_character->_converterMapping._skeletonDescription->getSortedBones();

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

                    lineConnect.push_back(iBone);
                    lineConnect.push_back(parentIdxInCache);
                }

                if (!lineConnect.empty())
                {
                    _viewportGeo->connectIndexedPrims(rend, RE_GEO_WIRE_IDX, RE_PRIM_LINES, lineConnect.sizeInt(), lineConnect.begin(), NULL, true);
                    _viewportGeo->connectIndexedPrims(rend, RE_GEO_SHADED_IDX, RE_PRIM_LINES, lineConnect.sizeInt(), lineConnect.begin(), NULL, true);
                }
            }
            break;
            case GolaemDisplayMode::SKINMESH:
            {
                // TODO
                //// compute shaders
                //const glm::Array<glm::GlmString>& shaderData = _shaderDataContainer->data[_inputData._entityIndex];

                //glm::GlmMap<size_t, size_t> globalToIntShaderAttrIdx;
                //glm::GlmMap<size_t, size_t> globalToFloatShaderAttrIdx;
                //glm::GlmMap<size_t, size_t> globalToStringShaderAttrIdx;
                //glm::GlmMap<size_t, size_t> globalToVectorShaderAttrIdx;

                //glm::PODArray<int> intAttrValues;
                //glm::PODArray<float> floatAttrValues;
                //glm::Array<glm::GlmString> stringAttrValues;
                //glm::Array<glm::Vector3> vectorAttrValues;

                //for (size_t iShaderAttr = 0, shaderAttrCount = _character->_shaderAttributes.size(); iShaderAttr < shaderAttrCount; iShaderAttr++)
                //{
                //    const glm::GlmString& attrValueStr = shaderData[iShaderAttr];
                //    const glm::ShaderAttribute& shaderAttr = _character->_shaderAttributes[iShaderAttr];
                //    switch (shaderAttr._type)
                //    {
                //    case glm::ShaderAttributeType::INT:
                //    {

                //        globalToIntShaderAttrIdx[iShaderAttr] = intAttrValues.size();
                //        intAttrValues.addOne();
                //        glm::fromString<int>(attrValueStr, intAttrValues.back());
                //    }
                //    break;
                //    case glm::ShaderAttributeType::FLOAT:
                //    {

                //        globalToFloatShaderAttrIdx[iShaderAttr] = floatAttrValues.size();
                //        floatAttrValues.addOne();
                //        glm::fromString<float>(attrValueStr, floatAttrValues.back());
                //    }
                //    break;
                //    case glm::ShaderAttributeType::STRING:
                //    {

                //        globalToStringShaderAttrIdx[iShaderAttr] = stringAttrValues.size();
                //        stringAttrValues.addOne();
                //        stringAttrValues.back() = attrValueStr;
                //    }
                //    break;
                //    case glm::ShaderAttributeType::VECTOR:
                //    {
                //        globalToVectorShaderAttrIdx[iShaderAttr] = vectorAttrValues.size();
                //        vectorAttrValues.addOne();
                //        glm::fromString(attrValueStr, vectorAttrValues.back());
                //    }
                //    break;
                //    default:
                //        break;
                //    }
                //}

                _packedEntity->_inputData._fbxStorage = glm::Singleton<glm::HoudiniFbxData>::getInstance().getFbxStorage();
                _packedEntity->_inputData._fbxBaker = glm::Singleton<glm::HoudiniFbxData>::getInstance().getFbxBaker();

                geoStatus = glm::crowdio::glmPrepareEntityGeometry(&_packedEntity->_inputData, &outputData);
                if (geoStatus == glm::crowdio::GIO_SUCCESS)
                {
                    PODArray<int> polyIds;
                    PODArray<int> polySizes;
                    PODArray<unsigned int> lineConnect;

                    size_t meshCount = outputData._meshAssetNameIndices.size();

                    glm::Array<glm::Array<glm::Vector3>>& frameDeformedVertices = outputData._deformedVertices[0];
                    int globalVertexCount = 0;
                    int globalPolyCount = 0;
                    int globalPolyVertexCount = 0;

                    if (outputData._geoType == glm::crowdio::GeometryType::FBX)
                    {
                        crowdio::CrowdFBXCharacter* fbxCharacter = outputData._fbxCharacters[0];
                        glm::Array<glm::PODArray<int>> fbxVertexMasks(meshCount);
                        glm::Array<glm::PODArray<int>> fbxPolygonMasks(meshCount);
                        for (size_t iRenderMesh = 0; iRenderMesh < meshCount; ++iRenderMesh)
                        {
                            size_t iGeoFileMesh = outputData._meshAssetNameIndices[iRenderMesh];

                            // meshDeformedVertices contains all fbx points, not just the ones that were filtered by vertexMasks
                            const glm::Array<glm::Vector3>& meshDeformedVertices = frameDeformedVertices[iGeoFileMesh];
                            size_t vertexCount = meshDeformedVertices.size();
                            if (vertexCount == 0)
                            {
                                continue;
                            }

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

                            unsigned int fbxVertexCount = fbxMesh->GetControlPointsCount();
                            unsigned int fbxPolyCount = fbxMesh->GetPolygonCount();
                            glm::PODArray<int>& vertexMasks = fbxVertexMasks[iRenderMesh];
                            glm::PODArray<int>& polygonMasks = fbxPolygonMasks[iRenderMesh];

                            vertexMasks.assign(fbxVertexCount, -1);
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
                                    ++globalPolyCount;
                                    int polySize = fbxMesh->GetPolygonSize(iFbxPoly);
                                    globalPolyVertexCount += polySize;
                                    for (int iPolyVertex = 0; iPolyVertex < polySize; ++iPolyVertex)
                                    {
                                        int iFbxVertex = fbxMesh->GetPolygonVertex(iFbxPoly, iPolyVertex);
                                        int& vertexMask = vertexMasks[iFbxVertex];
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
                                    ++globalVertexCount;
                                }
                            }
                        }

                        if (globalVertexCount == 0)
                        {
                            // no points in geometry, no need to go any further
                            return;
                        }

                        // Initialize the number of points in the geometry.
                        _viewportGeo->setNumPoints(globalVertexCount);
                        _viewportGeo->setNumPrimitives(globalPolyCount);
                        _viewportGeo->setNumVertices(globalPolyVertexCount);

                        globalVertexCount = 0;
                        for (size_t iRenderMesh = 0; iRenderMesh < meshCount; ++iRenderMesh)
                        {
                            size_t iGeoFileMesh = outputData._meshAssetNameIndices[iRenderMesh];

                            // meshDeformedVertices contains all fbx points, not just the ones that were filtered by vertexMasks
                            const glm::Array<glm::Vector3>& meshDeformedVertices = frameDeformedVertices[iGeoFileMesh];
                            size_t vertexCount = meshDeformedVertices.size();
                            if (vertexCount == 0)
                            {
                                continue;
                            }

                            //const glm::GlmString& meshName = outputData._meshAssetNames[outputData._meshAssetNameIndices[iMesh]];

                            // when fbxMesh == NULL, vertexCount == 0, so no need to check fbxMesh != NULL
                            FbxMesh* fbxMesh = fbxCharacter->getCharacterFBXMesh(iGeoFileMesh);

                            const glm::PODArray<int>& vertexMasks = fbxVertexMasks[iRenderMesh];
                            const glm::PODArray<int>& polygonMasks = fbxPolygonMasks[iRenderMesh];
                            unsigned int fbxVertexCount = (unsigned int)vertexMasks.size();
                            unsigned int fbxPolyCount = (unsigned int)polygonMasks.size();

                            for (unsigned int iFbxPoly = 0; iFbxPoly < fbxPolyCount; ++iFbxPoly)
                            {
                                if (polygonMasks[iFbxPoly])
                                {
                                    int polySize = fbxMesh->GetPolygonSize(iFbxPoly);
                                    polySizes.push_back(polySize);
                                    size_t originIdx = polyIds.size();
                                    for (int iPolyVertex = 0; iPolyVertex < polySize; ++iPolyVertex)
                                    {
                                        // reverse polygon order
                                        int iFbxVertex = fbxMesh->GetPolygonVertex(iFbxPoly, polySize - 1 - iPolyVertex);
                                        int vertexId = globalVertexCount + vertexMasks[iFbxVertex];
                                        polyIds.push_back(vertexId);
                                        lineConnect.push_back(vertexId);
                                        if (iPolyVertex > 0)
                                        {
                                            lineConnect.push_back(vertexId);
                                        }
                                    } // iPolyVertex
                                    lineConnect.push_back(polyIds[originIdx]);
                                }
                            }

                            //

                            //// find how many uv layers are available
                            //int uvSetCount = fbxMesh->GetLayerCount(FbxLayerElement::eUV);
                            //FbxLayerElementUV* uvElement = NULL;
                            //for (int iUVSet = 0; iUVSet < uvSetCount; ++iUVSet)
                            //{
                            //    glm::GlmString attrName = detailPtr->getStdAttributeName(GEO_ATTRIBUTE_TEXTURE, iUVSet + 1).c_str();
                            //    FbxLayer* layer = fbxMesh->GetLayer(fbxMesh->GetLayerTypedIndex((int)iUVSet, FbxLayerElement::eUV));
                            //    uvElement = layer->GetUVs();
                            //    bool uvsByControlPoint = uvElement->GetMappingMode() == FbxLayerElement::eByControlPoint;
                            //    bool uvReferenceDirect = uvElement->GetReferenceMode() == FbxLayerElement::eDirect;

                            //    GA_Attribute* uvAttr = detailPtr->addFloatTuple(GA_ATTRIB_VERTEX, attrName.c_str(), 3);
                            //    uvAttr->setTypeInfo(GA_TypeInfo::GA_TYPE_TEXTURE_COORD);
                            //    GA_RWHandleV3 uvAttrHandle(uvAttr);

                            //    if (uvsByControlPoint)
                            //    {
                            //        // houdini doesn't mix attributes with the same name but different owners by default (point or vertex)
                            //        // (the behavior can be overriden with GA_ReuseStrategy https://www.sidefx.com/docs/hdk/_h_d_k__geometry__intro.html#HDK_Geometry_Intro_Attribute)
                            //        // to simplify things, we create a GA_ATTRIB_VERTEX attribute here instead of GA_ATTRIB_POINT

                            //        int uvIndex;
                            //        int actualIndexByPolyVertex = 0;
                            //        for (unsigned int iFbxPoly = 0; iFbxPoly < fbxPolyCount; ++iFbxPoly)
                            //        {
                            //            int polySize = fbxMesh->GetPolygonSize(iFbxPoly);
                            //            if (polygonMasks[iFbxPoly])
                            //            {
                            //                for (int iPolyVertex = 0; iPolyVertex < polySize; ++iPolyVertex)
                            //                {
                            //                    // reverse polygon order
                            //                    uvIndex = vertexMasks[fbxMesh->GetPolygonVertex(iFbxPoly, polySize - 1 - iPolyVertex)];
                            //                    if (!uvReferenceDirect)
                            //                    {
                            //                        uvIndex = uvElement->GetIndexArray().GetAt(uvIndex);
                            //                    }
                            //                    FbxVector2 tempUV(uvElement->GetDirectArray().GetAt(uvIndex));
                            //                    uvAttrHandle.set(
                            //                        vertexOffset + actualIndexByPolyVertex,
                            //                        UT_Vector3F((float)tempUV[0], (float)tempUV[1], 0));

                            //                    ++actualIndexByPolyVertex;
                            //                }
                            //            }
                            //        }
                            //    }
                            //    else
                            //    {
                            //        int uvIndex;
                            //        int actualIndexByPolyVertex = 0;
                            //        int fbxIndexByPolyVertex = 0;
                            //        for (unsigned int iFbxPoly = 0; iFbxPoly < fbxPolyCount; ++iFbxPoly)
                            //        {
                            //            int polySize = fbxMesh->GetPolygonSize(iFbxPoly);
                            //            if (polygonMasks[iFbxPoly])
                            //            {
                            //                for (int iPolyVertex = 0; iPolyVertex < polySize; ++iPolyVertex, ++actualIndexByPolyVertex)
                            //                {
                            //                    // reverse polygon order
                            //                    uvIndex = fbxIndexByPolyVertex + polySize - 1 - iPolyVertex;
                            //                    if (!uvReferenceDirect)
                            //                    {
                            //                        uvIndex = uvElement->GetIndexArray().GetAt(uvIndex);
                            //                    }

                            //                    FbxVector2 tempUV(uvElement->GetDirectArray().GetAt(uvIndex));
                            //                    uvAttrHandle.set(
                            //                        vertexOffset + actualIndexByPolyVertex,
                            //                        UT_Vector3F((float)tempUV[0], (float)tempUV[1], 0));

                            //                } // iPolyVertex
                            //            }
                            //            fbxIndexByPolyVertex += polySize;
                            //        } // iPoly
                            //    }
                            //}

                            for (unsigned int iFbxVertex = 0; iFbxVertex < fbxVertexCount; ++iFbxVertex)
                            {
                                int vertexMask = vertexMasks[iFbxVertex];
                                if (vertexMask >= 0)
                                {
                                    ++globalVertexCount;
                                }
                            }

                            //GA_Size actualPolyCount = polyCounts.getNumPolygons();

                            //GA_Attribute* meshAttr = detailPtr->addStringTuple(GA_ATTRIB_PRIMITIVE, getMeshAttrName().c_str(), 1);
                            //GA_RWHandleS meshAttrHandle(meshAttr);

                            //GA_Attribute* materialAttr = detailPtr->addStringTuple(GA_ATTRIB_PRIMITIVE, GEO_STD_ATTRIB_MATERIAL, 1);
                            //GA_RWHandleS materialAttrHandle(materialAttr);

                            //int shadingGroupIdx = meshShadingGroups[iMesh];
                            //glm::GlmString materialName = "";
                            //if (shadingGroupIdx >= 0)
                            //{
                            //    const glm::ShadingGroup& shGroup = _character->_shadingGroups[shadingGroupIdx];
                            //    materialName = _materialPath;
                            //    materialName.rtrim("/");
                            //    materialName += "/";
                            //    switch (_materialAssignMode)
                            //    {
                            //    case GolaemMaterialAssignMode::BY_SHADING_GROUP:
                            //    {
                            //        materialName += shGroup._name;
                            //    }
                            //    break;
                            //    case GolaemMaterialAssignMode::BY_SURFACE_SHADER:
                            //    {
                            //        // get the surface shader
                            //        int shaderAssetIdx = (*_shadingGroupToSurfaceShader)[shadingGroupIdx];
                            //        if (shaderAssetIdx >= 0)
                            //        {
                            //            const glm::ShaderAsset& shAsset = _character->_shaderAssets[shaderAssetIdx];
                            //            materialName += shAsset._name;
                            //        }
                            //        else
                            //        {
                            //            materialName += "glmDefaultMat";
                            //        }
                            //    }
                            //    break;
                            //    default:
                            //        break;
                            //    }
                            //    materialName = glm::replaceString(materialName, ":", "_");

                            //    // add shading group attributes
                            //    for (size_t iShAttr = 0, shAttrCount = shGroup._shaderAttributes.size(); iShAttr < shAttrCount; ++iShAttr)
                            //    {
                            //        int shAttrIdx = shGroup._shaderAttributes[iShAttr];
                            //        const glm::ShaderAttribute& shAttr = _character->_shaderAttributes[shAttrIdx];
                            //        UT_StringHolder attrName = UT_VarEncode::encode(shAttr._name.c_str());
                            //        GA_Attribute* attr = NULL;
                            //        switch (shAttr._type)
                            //        {
                            //        case glm::ShaderAttributeType::INT:
                            //        {
                            //            attr = detailPtr->addIntTuple(GA_ATTRIB_PRIMITIVE, attrName, 1);
                            //            GA_RWHandleI attrHandle(attr);
                            //            if (attrHandle.isValid())
                            //            {
                            //                size_t attrValueIdx = globalToIntShaderAttrIdx[iShAttr];
                            //                int attrValue = intAttrValues[attrValueIdx];
                            //                for (GA_Size iPoly = 0; iPoly < actualPolyCount; ++iPoly)
                            //                {
                            //                    attrHandle.set(primOffset + iPoly, attrValue);
                            //                }
                            //            }
                            //        }
                            //        break;
                            //        case glm::ShaderAttributeType::FLOAT:
                            //        {
                            //            attr = detailPtr->addFloatTuple(GA_ATTRIB_PRIMITIVE, attrName, 1);
                            //            GA_RWHandleF attrHandle(attr);
                            //            if (attrHandle.isValid())
                            //            {
                            //                size_t attrValueIdx = globalToFloatShaderAttrIdx[iShAttr];
                            //                float attrValue = floatAttrValues[attrValueIdx];
                            //                for (GA_Size iPoly = 0; iPoly < actualPolyCount; ++iPoly)
                            //                {
                            //                    attrHandle.set(primOffset + iPoly, attrValue);
                            //                }
                            //            }
                            //        }
                            //        break;
                            //        case glm::ShaderAttributeType::STRING:
                            //        {
                            //            attr = detailPtr->addStringTuple(GA_ATTRIB_PRIMITIVE, attrName, 1);
                            //            GA_RWHandleS attrHandle(attr);
                            //            if (attrHandle.isValid())
                            //            {
                            //                size_t attrValueIdx = globalToStringShaderAttrIdx[iShAttr];
                            //                const glm::GlmString& attrValue = stringAttrValues[attrValueIdx];
                            //                for (GA_Size iPoly = 0; iPoly < actualPolyCount; ++iPoly)
                            //                {
                            //                    attrHandle.set(primOffset + iPoly, attrValue.c_str());
                            //                }
                            //            }
                            //        }
                            //        break;
                            //        case glm::ShaderAttributeType::VECTOR:
                            //        {
                            //            attr = detailPtr->addFloatTuple(GA_ATTRIB_PRIMITIVE, attrName, 3);
                            //            GA_RWHandleV3 attrHandle(attr);
                            //            if (attrHandle.isValid())
                            //            {
                            //                size_t attrValueIdx = globalToVectorShaderAttrIdx[iShAttr];
                            //                const glm::Vector3& attrValue = vectorAttrValues[attrValueIdx];
                            //                for (GA_Size iPoly = 0; iPoly < actualPolyCount; ++iPoly)
                            //                {
                            //                    attrHandle.set(
                            //                        primOffset + iPoly,
                            //                        UT_Vector3F(attrValue[0], attrValue[1], attrValue[2]));
                            //                }
                            //            }
                            //        }
                            //        break;
                            //        default:
                            //            break;
                            //        }
                            //        if (attr == NULL)
                            //        {
                            //            GLM_CROWD_TRACE_WARNING_LIMIT("Failed to add shader attribute '" << shAttr._name << "'");
                            //        }
                            //    }
                            //}

                            //for (GA_Size iPoly = 0; iPoly < actualPolyCount; ++iPoly)
                            //{
                            //    meshAttrHandle.set(primOffset + iPoly, meshName.c_str());
                            //    materialAttrHandle.set(primOffset + iPoly, materialName.c_str());
                            //}
                        }
                        RE_ElementArray* polyArray = new RE_ElementArray();
                        polyArray->requirePrimInfo(true);
                        polyArray->requireVertexInfo(true);
                        polyArray->setCacheName(getCacheName());
                        polyArray->setPrimitiveType(RE_PRIM_POLYGONS);
                        polyArray->beginPrims(rend);
                        for (int iPoly = 0, polyCount = polySizes.sizeInt(), vertexOrigin = 0; iPoly < polyCount; ++iPoly)
                        {
                            int polySize = polySizes[iPoly];
                            polyArray->addPrim(rend, polySize, &polyIds[vertexOrigin], NULL, NULL, iPoly, vertexOrigin);
                            vertexOrigin += polySize;
                        }
                        polyArray->endPrims(rend);

                        polyArray->setCacheVersion(params.geo_version);

                        _viewportGeo->connectIndexedPrims(rend, RE_GEO_WIRE_IDX, RE_PRIM_LINES, lineConnect.sizeInt(), lineConnect.begin(), NULL, false);
                        _viewportGeo->connectIndexedPrims(rend, RE_GEO_SHADED_IDX, polyArray, NULL, false);
                    }
                    else if (outputData._geoType == glm::crowdio::GeometryType::GCG)
                    {
                        crowdio::CrowdGcgCharacter* gcgCharacter = outputData._gcgCharacters[0];
                        for (size_t iRenderMesh = 0; iRenderMesh < meshCount; ++iRenderMesh)
                        {
                            const glm::Array<glm::Vector3>& meshDeformedVertices = frameDeformedVertices[iRenderMesh];
                            globalVertexCount += meshDeformedVertices.sizeInt();

                            glm::crowdio::GlmFileMeshTransform& assetFileMeshTransform = gcgCharacter->getGeometry()._transforms[outputData._transformIndicesInGcgFile[iRenderMesh]];
                            glm::crowdio::GlmFileMesh& assetFileMesh = gcgCharacter->getGeometry()._meshes[assetFileMeshTransform._meshIndex];
                            globalPolyCount += assetFileMesh._polygonCount;
                            for (uint32_t iPoly = 0; iPoly < assetFileMesh._polygonCount; ++iPoly)
                            {
                                uint32_t polySize = assetFileMesh._polygonsVertexCount[iPoly];
                                globalPolyVertexCount += polySize;
                            }
                        }

                        if (globalVertexCount == 0)
                        {
                            // no points in geometry, no need to go any further
                            return;
                        }

                        // Initialize the number of points in the geometry.
                        _viewportGeo->setNumPoints(globalVertexCount);
                        _viewportGeo->setNumPrimitives(globalPolyCount);
                        _viewportGeo->setNumVertices(globalPolyVertexCount);

                        globalVertexCount = 0;
                        for (size_t iRenderMesh = 0; iRenderMesh < meshCount; ++iRenderMesh)
                        {
                            const glm::Array<glm::Vector3>& meshDeformedVertices = frameDeformedVertices[iRenderMesh];
                            size_t vertexCount = meshDeformedVertices.size();
                            if (vertexCount == 0)
                            {
                                continue;
                            }

                            //const glm::GlmString& meshName = outputData._meshAssetNames[outputData._meshAssetNameIndices[iMesh]];

                            glm::crowdio::GlmFileMeshTransform& assetFileMeshTransform = gcgCharacter->getGeometry()._transforms[outputData._transformIndicesInGcgFile[iRenderMesh]];
                            glm::crowdio::GlmFileMesh& assetFileMesh = gcgCharacter->getGeometry()._meshes[assetFileMeshTransform._meshIndex];

                            for (uint32_t iPoly = 0, iVertex = 0; iPoly < assetFileMesh._polygonCount; ++iPoly)
                            {
                                size_t originIdx = polyIds.size();
                                uint32_t polySize = assetFileMesh._polygonsVertexCount[iPoly];
                                polySizes.push_back(polySize);
                                for (uint32_t iPolyVtx = 0; iPolyVtx < polySize; ++iPolyVtx)
                                {
                                    // reverse polygon order
                                    int vertexId = globalVertexCount + assetFileMesh._polygonsVertexIndices[iVertex + polySize - 1 - iPolyVtx];
                                    polyIds.push_back(vertexId);
                                    lineConnect.push_back(vertexId);
                                    if (iPolyVtx > 0)
                                    {
                                        lineConnect.push_back(vertexId);
                                    }
                                }
                                iVertex += polySize;
                                lineConnect.push_back(polyIds[originIdx]);
                            }

                            //if (assetFileMesh._uvSetCount > 0)
                            //{
                            //    for (size_t iUVSet = 0; iUVSet < assetFileMesh._uvSetCount; ++iUVSet)
                            //    {
                            //        glm::GlmString attrName = "uv";
                            //        if (iUVSet > 0)
                            //        {
                            //            attrName += glm::toString(iUVSet + 1);
                            //        }
                            //        GA_Attribute* uvAttr = detailPtr->addFloatTuple(GA_ATTRIB_VERTEX, attrName.c_str(), 3);
                            //        uvAttr->setTypeInfo(GA_TypeInfo::GA_TYPE_TEXTURE_COORD);
                            //        GA_RWHandleV3 uvAttrHandle(uvAttr);
                            //        if (assetFileMesh._uvMode == glm::crowdio::GLM_UV_PER_CONTROL_POINT)
                            //        {
                            //            // houdini doesn't mix attributes with the same name but different owners by default (point or vertex)
                            //            // (the behavior can be overriden with GA_ReuseStrategy https://www.sidefx.com/docs/hdk/_h_d_k__geometry__intro.html#HDK_Geometry_Intro_Attribute)
                            //            // to simplify things, we create a GA_ATTRIB_VERTEX attribute here instead of GA_ATTRIB_POINT

                            //            for (uint32_t iPoly = 0, iVertex = 0; iPoly < assetFileMesh._polygonCount; ++iPoly)
                            //            {
                            //                uint32_t polySize = assetFileMesh._polygonsVertexCount[iPoly];
                            //                for (uint32_t iPolyVtx = 0; iPolyVtx < polySize; ++iPolyVtx)
                            //                {
                            //                    // reverse polygon order
                            //                    uint32_t uvIndex = assetFileMesh._polygonsVertexIndices[iVertex + polySize - 1 - iPolyVtx];
                            //                    uvAttrHandle.set(
                            //                        vertexOffset + iVertex + iPolyVtx,
                            //                        UT_Vector3F(assetFileMesh._us[iUVSet][uvIndex], assetFileMesh._vs[iUVSet][uvIndex], 0));
                            //                }
                            //                iVertex += polySize;
                            //            }
                            //        }
                            //        else
                            //        {
                            //            for (uint32_t iPoly = 0, iVertex = 0; iPoly < assetFileMesh._polygonCount; ++iPoly)
                            //            {
                            //                uint32_t polySize = assetFileMesh._polygonsVertexCount[iPoly];
                            //                for (uint32_t iPolyVtx = 0; iPolyVtx < polySize; ++iPolyVtx)
                            //                {
                            //                    // reverse polygon order
                            //                    uint32_t uvIndex = assetFileMesh._polygonsUVIndices[iVertex + polySize - 1 - iPolyVtx];
                            //                    uvAttrHandle.set(
                            //                        vertexOffset + iVertex + iPolyVtx,
                            //                        UT_Vector3F(assetFileMesh._us[iUVSet][uvIndex], assetFileMesh._vs[iUVSet][uvIndex], 0));
                            //                }
                            //                iVertex += polySize;
                            //            }
                            //        }
                            //    }
                            //}

                            globalVertexCount += (int)vertexCount;

                            //GA_Size actualPolyCount = polyCounts.getNumPolygons();

                            //GA_Attribute* meshAttr = detailPtr->addStringTuple(GA_ATTRIB_PRIMITIVE, getMeshAttrName().c_str(), 1);
                            //GA_RWHandleS meshAttrHandle(meshAttr);

                            //GA_Attribute* materialAttr = detailPtr->addStringTuple(GA_ATTRIB_PRIMITIVE, GEO_STD_ATTRIB_MATERIAL, 1);
                            //GA_RWHandleS materialAttrHandle(materialAttr);

                            //int shadingGroupIdx = meshShadingGroups[iMesh];
                            //glm::GlmString materialName = "";
                            //if (shadingGroupIdx >= 0)
                            //{
                            //    const glm::ShadingGroup& shGroup = _character->_shadingGroups[shadingGroupIdx];
                            //    materialName = _materialPath;
                            //    materialName.rtrim("/");
                            //    materialName += "/";
                            //    switch (_materialAssignMode)
                            //    {
                            //    case GolaemMaterialAssignMode::BY_SHADING_GROUP:
                            //    {
                            //        materialName += shGroup._name;
                            //    }
                            //    break;
                            //    case GolaemMaterialAssignMode::BY_SURFACE_SHADER:
                            //    {
                            //        // get the surface shader
                            //        int shaderAssetIdx = (*_shadingGroupToSurfaceShader)[shadingGroupIdx];
                            //        if (shaderAssetIdx >= 0)
                            //        {
                            //            const glm::ShaderAsset& shAsset = _character->_shaderAssets[shaderAssetIdx];
                            //            materialName += shAsset._name;
                            //        }
                            //        else
                            //        {
                            //            materialName += "glmDefaultMat";
                            //        }
                            //    }
                            //    break;
                            //    default:
                            //        break;
                            //    }
                            //    materialName = glm::replaceString(materialName, ":", "_");

                            //    // add shading group attributes
                            //    for (size_t iShAttr = 0, shAttrCount = shGroup._shaderAttributes.size(); iShAttr < shAttrCount; ++iShAttr)
                            //    {
                            //        int shAttrIdx = shGroup._shaderAttributes[iShAttr];
                            //        const glm::ShaderAttribute& shAttr = _character->_shaderAttributes[shAttrIdx];
                            //        UT_StringHolder attrName = UT_VarEncode::encode(shAttr._name.c_str());
                            //        GA_Attribute* attr = NULL;
                            //        switch (shAttr._type)
                            //        {
                            //        case glm::ShaderAttributeType::INT:
                            //        {
                            //            attr = detailPtr->addIntTuple(GA_ATTRIB_PRIMITIVE, attrName, 1);
                            //            GA_RWHandleI attrHandle(attr);
                            //            if (attrHandle.isValid())
                            //            {
                            //                size_t attrValueIdx = globalToIntShaderAttrIdx[iShAttr];
                            //                int attrValue = intAttrValues[attrValueIdx];
                            //                for (GA_Size iPoly = 0; iPoly < actualPolyCount; ++iPoly)
                            //                {
                            //                    attrHandle.set(primOffset + iPoly, attrValue);
                            //                }
                            //            }
                            //        }
                            //        break;
                            //        case glm::ShaderAttributeType::FLOAT:
                            //        {
                            //            attr = detailPtr->addFloatTuple(GA_ATTRIB_PRIMITIVE, attrName, 1);
                            //            GA_RWHandleF attrHandle(attr);
                            //            if (attrHandle.isValid())
                            //            {
                            //                size_t attrValueIdx = globalToFloatShaderAttrIdx[iShAttr];
                            //                float attrValue = floatAttrValues[attrValueIdx];
                            //                for (GA_Size iPoly = 0; iPoly < actualPolyCount; ++iPoly)
                            //                {
                            //                    attrHandle.set(primOffset + iPoly, attrValue);
                            //                }
                            //            }
                            //        }
                            //        break;
                            //        case glm::ShaderAttributeType::STRING:
                            //        {
                            //            attr = detailPtr->addStringTuple(GA_ATTRIB_PRIMITIVE, attrName, 1);
                            //            GA_RWHandleS attrHandle(attr);
                            //            if (attrHandle.isValid())
                            //            {
                            //                size_t attrValueIdx = globalToStringShaderAttrIdx[iShAttr];
                            //                const glm::GlmString& attrValue = stringAttrValues[attrValueIdx];
                            //                for (GA_Size iPoly = 0; iPoly < actualPolyCount; ++iPoly)
                            //                {
                            //                    attrHandle.set(primOffset + iPoly, attrValue.c_str());
                            //                }
                            //            }
                            //        }
                            //        break;
                            //        case glm::ShaderAttributeType::VECTOR:
                            //        {
                            //            attr = detailPtr->addFloatTuple(GA_ATTRIB_PRIMITIVE, attrName, 3);
                            //            GA_RWHandleV3 attrHandle(attr);
                            //            if (attrHandle.isValid())
                            //            {
                            //                size_t attrValueIdx = globalToVectorShaderAttrIdx[iShAttr];
                            //                const glm::Vector3& attrValue = vectorAttrValues[attrValueIdx];
                            //                for (GA_Size iPoly = 0; iPoly < actualPolyCount; ++iPoly)
                            //                {
                            //                    attrHandle.set(
                            //                        primOffset + iPoly,
                            //                        UT_Vector3F(attrValue[0], attrValue[1], attrValue[2]));
                            //                }
                            //            }
                            //        }
                            //        break;
                            //        default:
                            //            break;
                            //        }
                            //        if (attr == NULL)
                            //        {
                            //            GLM_CROWD_TRACE_WARNING_LIMIT("Failed to add shader attribute '" << shAttr._name << "'");
                            //        }
                            //    }
                            //}

                            //for (GA_Size iPoly = 0; iPoly < actualPolyCount; ++iPoly)
                            //{
                            //    meshAttrHandle.set(primOffset + iPoly, meshName.c_str());
                            //    materialAttrHandle.set(primOffset + iPoly, materialName.c_str());
                            //}
                        }

                        RE_ElementArray* polyArray = new RE_ElementArray();
                        polyArray->requirePrimInfo(true);
                        polyArray->requireVertexInfo(true);
                        polyArray->setCacheName(getCacheName());
                        polyArray->setPrimitiveType(RE_PRIM_POLYGONS);
                        polyArray->beginPrims(rend);
                        for (int iPoly = 0, polyCount = polySizes.sizeInt(), vertexOrigin = 0; iPoly < polyCount; ++iPoly)
                        {
                            int polySize = polySizes[iPoly];
                            polyArray->addPrim(rend, polySize, &polyIds[vertexOrigin], NULL, NULL, iPoly, vertexOrigin);
                            vertexOrigin += polySize;
                        }
                        polyArray->endPrims(rend);

                        polyArray->setCacheVersion(params.geo_version);

                        _viewportGeo->connectIndexedPrims(rend, RE_GEO_WIRE_IDX, RE_PRIM_LINES, lineConnect.sizeInt(), lineConnect.begin(), NULL, false);
                        _viewportGeo->connectIndexedPrims(rend, RE_GEO_SHADED_IDX, polyArray, NULL, false);
                    }
                    else
                    {
                        return;
                    }
                }
            }
            break;
            default:
                break;
            }

            float diffuseCol[3] = {1.f, 0.5f, 0.5f};
            _viewportGeo->createConstAttribute(rend, GEO_STD_ATTRIB_DIFFUSE, RE_GPU_FLOAT32, 3, diffuseCol);

            float alpha = 1.f;
            _viewportGeo->createConstAttribute(rend, GEO_STD_ATTRIB_ALPHA, RE_GPU_FLOAT32, 1, &alpha);

            float pointSelection = 0.f;
            _viewportGeo->createConstAttribute(rend, "pointSelection", RE_GPU_FLOAT32, 1, &pointSelection);

            {
                RE_VertexArray* pointIds = _viewportGeo->findCachedAttrib(rend, "pointID", RE_GPU_INT32, 1, RE_ARRAY_POINT, true);
                if (pointIds != NULL && pointIds->getCacheVersion() != params.geo_version)
                {
                    // map() returns a pointer to the GL buffer
                    int* pointIdsData = static_cast<int*>(pointIds->map(rend));

                    for (int iPoint = 0, pointCount = _viewportGeo->getNumPoints(); iPoint < pointCount; ++iPoint)
                    {
                        pointIdsData[iPoint] = iPoint;
                    }

                    // unmap the buffer so it can be used by GL
                    pointIds->unmap(rend);

                    // Always set the cache version after assigning data.
                    pointIds->setCacheVersion(params.geo_version);
                }
            }
        }

        if (_viewportGeo->getNumPoints() > 0)
        {
            switch (_packedEntity->_displayMode)
            {
            case GolaemDisplayMode::BOUNDING_BOX:
            {
                // Fetch P (point position). If its cache version matches, no upload is required.
                // if _viewportGeo->getNumPoints() is 0, the line below will crash
                RE_VertexArray* pos = _viewportGeo->findCachedAttrib(rend, GEO_STD_ATTRIB_POSITION, RE_GPU_FLOAT32, 3, RE_ARRAY_POINT, true);
                if (pos->getCacheVersion() != params.geo_version)
                {
                    // map() returns a pointer to the GL buffer
                    UT_Vector3F* pointData = static_cast<UT_Vector3F*>(pos->map(rend));

                    pointData[0].assign(
                        _packedEntity->_rootPos[0] - _packedEntity->_halfExtents[0],
                        _packedEntity->_rootPos[1] - _packedEntity->_halfExtents[1],
                        _packedEntity->_rootPos[2] + _packedEntity->_halfExtents[2]);

                    pointData[1].assign(
                        _packedEntity->_rootPos[0] + _packedEntity->_halfExtents[0],
                        _packedEntity->_rootPos[1] - _packedEntity->_halfExtents[1],
                        _packedEntity->_rootPos[2] + _packedEntity->_halfExtents[2]);

                    pointData[2].assign(
                        _packedEntity->_rootPos[0] + _packedEntity->_halfExtents[0],
                        _packedEntity->_rootPos[1] - _packedEntity->_halfExtents[1],
                        _packedEntity->_rootPos[2] - _packedEntity->_halfExtents[2]);

                    pointData[3].assign(
                        _packedEntity->_rootPos[0] - _packedEntity->_halfExtents[0],
                        _packedEntity->_rootPos[1] - _packedEntity->_halfExtents[1],
                        _packedEntity->_rootPos[2] - _packedEntity->_halfExtents[2]);

                    pointData[4].assign(
                        _packedEntity->_rootPos[0] - _packedEntity->_halfExtents[0],
                        _packedEntity->_rootPos[1] + _packedEntity->_halfExtents[1],
                        _packedEntity->_rootPos[2] + _packedEntity->_halfExtents[2]);

                    pointData[5].assign(
                        _packedEntity->_rootPos[0] + _packedEntity->_halfExtents[0],
                        _packedEntity->_rootPos[1] + _packedEntity->_halfExtents[1],
                        _packedEntity->_rootPos[2] + _packedEntity->_halfExtents[2]);

                    pointData[6].assign(
                        _packedEntity->_rootPos[0] + _packedEntity->_halfExtents[0],
                        _packedEntity->_rootPos[1] + _packedEntity->_halfExtents[1],
                        _packedEntity->_rootPos[2] - _packedEntity->_halfExtents[2]);

                    pointData[7].assign(
                        _packedEntity->_rootPos[0] - _packedEntity->_halfExtents[0],
                        _packedEntity->_rootPos[1] + _packedEntity->_halfExtents[1],
                        _packedEntity->_rootPos[2] - _packedEntity->_halfExtents[2]);

                    // unmap the buffer so it can be used by GL
                    pos->unmap(rend);

                    // Always set the cache version after assigning data.
                    pos->setCacheVersion(params.geo_version);
                }
            }
            break;
            case GolaemDisplayMode::SKELETON:
            {
                // Fetch P (point position). If its cache version matches, no upload is required.
                // if _viewportGeo->getNumPoints() is 0, the line below will crash
                RE_VertexArray* pos = _viewportGeo->findCachedAttrib(rend, GEO_STD_ATTRIB_POSITION, RE_GPU_FLOAT32, 3, RE_ARRAY_POINT, true);
                if (pos->getCacheVersion() != params.geo_version)
                {
                    // map() returns a pointer to the GL buffer
                    UT_Vector3F* pointData = static_cast<UT_Vector3F*>(pos->map(rend));

                    const glm::crowdio::GlmSimulationData* simuData = _packedEntity->_inputData._simuData;
                    const glm::crowdio::GlmFrameData* frameData = _packedEntity->_inputData._frameDatas[0];
                    // set the bone positions
                    uint16_t entityType = simuData->_entityTypes[_packedEntity->_inputData._entityIndex];
                    uint16_t boneCount = simuData->_boneCount[entityType];
                    for (uint16_t iBone = 0; iBone < boneCount; ++iBone)
                    {
                        float* bonePos = frameData->_bonePositions[_bonePositionOffset + iBone];
                        pointData[iBone].assign(bonePos[0], bonePos[1], bonePos[2]);
                    }

                    // unmap the buffer so it can be used by GL
                    pos->unmap(rend);

                    // Always set the cache version after assigning data.
                    pos->setCacheVersion(params.geo_version);
                }
            }
            break;
            case GolaemDisplayMode::SKINMESH:
            {
                if (!firstCompute)
                {
                    geoStatus = glm::crowdio::glmPrepareEntityGeometry(&_packedEntity->_inputData, &outputData);
                }
                if (geoStatus == glm::crowdio::GIO_SUCCESS)
                {
                    size_t meshCount = outputData._meshAssetNameIndices.size();

                    glm::Array<glm::Array<glm::Vector3>>& frameDeformedVertices = outputData._deformedVertices[0];
                    glm::Array<glm::Array<glm::Vector3>>& frameDeformedNormals = outputData._deformedNormals[0];

                    if (outputData._geoType == glm::crowdio::GeometryType::FBX)
                    {
                        crowdio::CrowdFBXCharacter* fbxCharacter = outputData._fbxCharacters[0];
                        // ----- FBX specific data
                        FbxAMatrix nodeTransform;
                        FbxAMatrix geomTransform;
                        FbxAMatrix identityMatrix;
                        identityMatrix.SetIdentity();
                        FbxTime fbxTime;
                        FbxVector4 fbxVect;
                        // ----- end FBX specific data

                        // Extract frame
                        if (outputData._geoBeInfo._idGeometryFileIdx != -1)
                        {
                            const glm::crowdio::GlmFrameData* frameData = _packedEntity->_inputData._frameDatas[0];
                            float(&geometryFrameCacheData)[3] = frameData->_geoBehaviorAnimFrameInfo[outputData._geoBeInfo._geoDataIndex];
                            double frameRate(FbxTime::GetFrameRate(fbxCharacter->touchFBXScene()->GetGlobalSettings().GetTimeMode()));
                            fbxTime.SetGlobalTimeMode(FbxTime::eCustom, frameRate);
                            fbxTime.SetMilliSeconds(long((double)geometryFrameCacheData[0] / frameRate * 1000.0));
                        }
                        else
                        {
                            fbxTime = 0;
                        }

                        // Fetch P (point position). If its cache version matches, no upload is required.
                        // if _viewportGeo->getNumPoints() is 0, the line below will crash
                        RE_VertexArray* pos = _viewportGeo->findCachedAttrib(rend, GEO_STD_ATTRIB_POSITION, RE_GPU_FLOAT32, 3, RE_ARRAY_POINT, true);

                        RE_VertexArray* normals = _viewportGeo->findCachedAttrib(rend, GEO_STD_ATTRIB_NORMAL, RE_GPU_FLOAT32, 3, RE_ARRAY_VERTEX, true);

                        if (pos->getCacheVersion() != params.geo_version || normals->getCacheVersion() != params.geo_version)
                        {
                            // map() returns a pointer to the GL buffer
                            UT_Vector3F* pointData = static_cast<UT_Vector3F*>(pos->map(rend));
                            UT_Vector3F* normalData = static_cast<UT_Vector3F*>(normals->map(rend));

                            for (size_t iRenderMesh = 0, iGlobalVertex = 0, iGlobalPolyVertex = 0; iRenderMesh < meshCount; ++iRenderMesh)
                            {
                                size_t iGeoFileMesh = outputData._meshAssetNameIndices[iRenderMesh];

                                // meshDeformedVertices contains all fbx points, not just the ones that were filtered by vertexMasks
                                const glm::Array<glm::Vector3>& meshDeformedVertices = frameDeformedVertices[iGeoFileMesh];
                                size_t vertexCount = meshDeformedVertices.size();
                                if (vertexCount == 0)
                                {
                                    continue;
                                }

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

                                unsigned int fbxVertexCount = fbxMesh->GetControlPointsCount();
                                unsigned int fbxPolyCount = fbxMesh->GetPolygonCount();

                                glm::PODArray<int> vertexMasks;
                                glm::PODArray<int> polygonMasks;

                                vertexMasks.assign(fbxVertexCount, -1);
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
                                            int iFbxVertex = fbxMesh->GetPolygonVertex(iFbxPoly, iPolyVertex);
                                            int& vertexMask = vertexMasks[iFbxVertex];
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
                                    int& vertexMask = vertexMasks[iFbxVertex];
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
                                            pointData[iGlobalVertex].assign((float)fbxVect[0], (float)fbxVect[1], (float)fbxVect[2]);
                                        }
                                        else
                                        {
                                            const Vector3& meshVertex = meshDeformedVertices[iFbxVertex];
                                            pointData[iGlobalVertex].assign(meshVertex.getFloatValues());
                                        }

                                        ++iGlobalVertex;
                                    }
                                }

                                if (hasNormals)
                                {
                                    FbxAMatrix globalRotate(identityMatrix);
                                    globalRotate.SetR(nodeTransform.GetR());
                                    bool hasRotate = globalRotate != identityMatrix;

                                    const glm::Array<glm::Vector3>& meshDeformedNormals = frameDeformedNormals[iGeoFileMesh];

                                    // normals are always stored per polygon vertex
                                    for (unsigned int iFbxPoly = 0, iFbxNormal = 0; iFbxPoly < fbxPolyCount; ++iFbxPoly)
                                    {
                                        int polySize = fbxMesh->GetPolygonSize(iFbxPoly);
                                        if (polygonMasks[iFbxPoly])
                                        {
                                            for (int iPolyVertex = 0; iPolyVertex < polySize; ++iPolyVertex, ++iGlobalPolyVertex)
                                            {
                                                // meshDeformedNormals contains all fbx normals, not just the ones that were filtered by polygonMasks
                                                // reverse polygon order
                                                if (hasRotate)
                                                {
                                                    const Vector3& glmVect = meshDeformedNormals[iFbxNormal + polySize - 1 - iPolyVertex];
                                                    fbxVect.Set(glmVect.x, glmVect.y, glmVect.z);
                                                    fbxVect = globalRotate.MultT(fbxVect);
                                                    normalData[iGlobalPolyVertex].assign(
                                                        (float)fbxVect[0], (float)fbxVect[1], (float)fbxVect[2]);
                                                }
                                                else
                                                {
                                                    const glm::Vector3& deformedNormal = meshDeformedNormals[iFbxNormal + polySize - 1 - iPolyVertex];
                                                    normalData[iGlobalPolyVertex].assign(
                                                        deformedNormal.getFloatValues());
                                                }
                                            }
                                        }
                                        iFbxNormal += polySize;
                                    }
                                }
                            }

                            // unmap the buffer so it can be used by GL
                            pos->unmap(rend);
                            normals->unmap(rend);

                            // Always set the cache version after assigning data.
                            pos->setCacheVersion(params.geo_version);
                            normals->setCacheVersion(params.geo_version);
                        }
                    }
                    else if (outputData._geoType == glm::crowdio::GeometryType::GCG)
                    {
                        crowdio::CrowdGcgCharacter* gcgCharacter = outputData._gcgCharacters[0];
                        // Fetch P (point position). If its cache version matches, no upload is required.
                        // if _viewportGeo->getNumPoints() is 0, the line below will crash
                        RE_VertexArray* pos = _viewportGeo->findCachedAttrib(rend, GEO_STD_ATTRIB_POSITION, RE_GPU_FLOAT32, 3, RE_ARRAY_POINT, true);
                        RE_VertexArray* normals = _viewportGeo->findCachedAttrib(rend, GEO_STD_ATTRIB_NORMAL, RE_GPU_FLOAT32, 3, RE_ARRAY_VERTEX, true);

                        if (pos->getCacheVersion() != params.geo_version || normals->getCacheVersion() != params.geo_version)
                        {
                            // map() returns a pointer to the GL buffer
                            UT_Vector3F* pointData = static_cast<UT_Vector3F*>(pos->map(rend));
                            UT_Vector3F* normalData = static_cast<UT_Vector3F*>(normals->map(rend));

                            for (size_t iRenderMesh = 0, iGlobalVertex = 0, iGlobalPolyVertex = 0; iRenderMesh < meshCount; ++iRenderMesh)
                            {
                                const glm::Array<glm::Vector3>& meshDeformedVertices = frameDeformedVertices[iRenderMesh];
                                size_t vertexCount = meshDeformedVertices.size();
                                if (vertexCount == 0)
                                {
                                    continue;
                                }

                                for (size_t iVertex = 0; iVertex < vertexCount; ++iVertex, ++iGlobalVertex)
                                {
                                    const glm::Vector3& meshVertex = meshDeformedVertices[iVertex];
                                    pointData[iGlobalVertex].assign(meshVertex.getFloatValues());
                                }

                                const glm::Array<glm::Vector3>& meshDeformedNormals = frameDeformedNormals[iRenderMesh];

                                glm::crowdio::GlmFileMeshTransform& assetFileMeshTransform = gcgCharacter->getGeometry()._transforms[outputData._transformIndicesInGcgFile[iRenderMesh]];
                                glm::crowdio::GlmFileMesh& assetFileMesh = gcgCharacter->getGeometry()._meshes[assetFileMeshTransform._meshIndex];

                                // add normals
                                if (assetFileMesh._normalMode == glm::crowdio::GLM_NORMAL_PER_POLYGON_VERTEX)
                                {
                                    for (uint32_t iPoly = 0, iVertex = 0; iPoly < assetFileMesh._polygonCount; ++iPoly)
                                    {
                                        uint32_t polySize = assetFileMesh._polygonsVertexCount[iPoly];
                                        for (uint32_t iPolyVtx = 0; iPolyVtx < polySize; ++iPolyVtx, ++iGlobalPolyVertex)
                                        {
                                            // reverse polygon order
                                            const glm::Vector3& vtxNormal = meshDeformedNormals[iVertex + polySize - 1 - iPolyVtx];
                                            normalData[iGlobalPolyVertex].assign(vtxNormal.getFloatValues());
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
                                        for (uint32_t iPolyVtx = 0; iPolyVtx < polySize; ++iPolyVtx, ++iGlobalPolyVertex)
                                        {
                                            // reverse polygon order
                                            uint32_t normalIdx = polygonNormalIndices[iVertex + polySize - 1 - iPolyVtx];
                                            const glm::Vector3& vtxNormal = meshDeformedNormals[normalIdx];
                                            normalData[iGlobalPolyVertex].assign(vtxNormal.getFloatValues());
                                        }
                                        iVertex += polySize;
                                    }
                                }
                            }

                            // unmap the buffer so it can be used by GL
                            pos->unmap(rend);
                            normals->unmap(rend);

                            // Always set the cache version after assigning data.
                            pos->setCacheVersion(params.geo_version);
                            normals->setCacheVersion(params.geo_version);
                        }
                    }
                }
            }
            break;
            default:
                break;
            }
            //_viewportGeo->print();
        }
    }

    // GL3 shaders. These use some of the builtin Houdini shaders, which are
    // described by the .prog file format - a simple container format for various
    // shader stages and other information.

    static RE_ShaderHandle theNQShader("material/GL32/beauty_primvertnorm_lit.prog");
    static RE_ShaderHandle theNQFlatShader("material/GL32/beauty_flat_primvert_lit.prog");
    static RE_ShaderHandle theNQUnlitShader("material/GL32/beauty_primvertnorm_unlit.prog");
    static RE_ShaderHandle theHQShader("material/GL32/beauty_material_primvertnorm.prog");
    static RE_ShaderHandle theLineShader("basic/GL32/wire_color.prog");
    static RE_ShaderHandle theConstShader("material/GL32/constant.prog");
    static RE_ShaderHandle theZCubeShader("basic/GL32/depth_cube.prog");
    static RE_ShaderHandle theZLinearShader("basic/GL32/depth_linear.prog");
    static RE_ShaderHandle theMatteShader("basic/GL32/matte.prog");

    //-----------------------------------------------------------------------------
    void GR_PackedGolaemEntity::render(
#if HDK_API_VERSION >= 20000000
        RE_RenderContext rend,
#else
        RE_Render* rend,
#endif
        GR_RenderMode render_mode,
        GR_RenderFlags flags,
        GR_DrawParms drawparams)
    {
        if (_viewportGeo == NULL)
        {
            return;
        }

        bool need_wire =
            (render_mode == GR_RENDER_WIREFRAME ||
             render_mode == GR_RENDER_HIDDEN_LINE ||
             render_mode == GR_RENDER_GHOST_LINE ||
             render_mode == GR_RENDER_MATERIAL_WIREFRAME ||
             (flags & GR_RENDER_FLAG_WIRE_OVER));

        // Shaded mode rendering
        if (render_mode == GR_RENDER_BEAUTY ||
            render_mode == GR_RENDER_MATERIAL ||
            render_mode == GR_RENDER_CONSTANT ||
            render_mode == GR_RENDER_HIDDEN_LINE ||
            render_mode == GR_RENDER_GHOST_LINE ||
            render_mode == GR_RENDER_DEPTH ||
            render_mode == GR_RENDER_DEPTH_LINEAR ||
            render_mode == GR_RENDER_DEPTH_CUBE ||
            render_mode == GR_RENDER_MATTE)
        {
            // enable polygon offset if doing a wireframe on top of shaded
            bool polyoff = rend->isPolygonOffset();
            if (need_wire)
            {
                rend->polygonOffset(true);
            }

            rend->pushShader();

            // GL3 requires the use of shaders. The fixed function pipeline
            // GL builtins (which are deprecated, like gl_ModelViewMatrix)
            // are not initialized in the GL3 renderer.

            if (render_mode == GR_RENDER_BEAUTY)
            {
                if (flags & GR_RENDER_FLAG_UNLIT)
                {
                    rend->bindShader(theNQUnlitShader);
                }
                else if (flags & GR_RENDER_FLAG_FLAT_SHADED)
                {
                    rend->bindShader(theNQFlatShader);
                }
                else
                {
                    rend->bindShader(theNQShader);
                }
            }
            else if (render_mode == GR_RENDER_MATERIAL)
            {
                rend->bindShader(theHQShader);
            }
            else if (
                render_mode == GR_RENDER_CONSTANT ||
                render_mode == GR_RENDER_DEPTH ||
                render_mode == GR_RENDER_HIDDEN_LINE ||
                render_mode == GR_RENDER_GHOST_LINE)
            {
                // Reuse constant for depth-only since it's so lightweight.
                rend->bindShader(theConstShader);
            }
            else if (render_mode == GR_RENDER_DEPTH_LINEAR)
            {
                // Depth written to world-space Z instead of non-linear depth
                // buffer Z ([0..1] near-far depth range)
                rend->bindShader(theZLinearShader);
            }
            else if (render_mode == GR_RENDER_DEPTH_CUBE)
            {
                // Linear depth written to
                rend->bindShader(theZCubeShader);
            }
            else if (render_mode == GR_RENDER_MATTE)
            {
                rend->bindShader(theMatteShader);
            }

            // setup materials and lighting
            if (drawparams.materials != NULL &&
                (drawparams.materials->getDefaultMaterial() != NULL ||
                 drawparams.materials->getFactoryMaterial() != NULL))
            {
                RE_Shader* shader = rend->getShader();
                RE_MaterialPtr mat = drawparams.materials->getDefaultMaterial();
                if (mat == NULL)
                {
                    mat = drawparams.materials->getFactoryMaterial();
                }

                // Set up lighting for any GL3 lighting blocks
                if (shader && drawparams.opts->getLightList())
                {
#if HDK_API_VERSION >= 20000000
                    drawparams.opts->getLightList()->glLights()->bindForShader(rend, shader);
#else
                    drawparams.opts->getLightList()->bindForShader(rend, shader);
#endif
                }

#if HDK_API_VERSION >= 19000000
                // set up the main material block for GL3
                mat->updateShaderForMaterial(
                    rend, 0, true,
                    RE_SHADER_TARGET_TRIANGLE, shader);
#else
                mat->updateShaderForMaterial(
                    rend, 0, true, true,
                    RE_SHADER_TARGET_TRIANGLE, shader);
#endif

            }

            // Draw call for the geometry
            if (drawparams.draw_instanced)
            {
                _viewportGeo->drawInstanceGroup(rend, RE_GEO_SHADED_IDX, drawparams.instance_group);
            }
            else
            {
                _viewportGeo->draw(rend, RE_GEO_SHADED_IDX);
            }

            if (rend->getShader())
            {
                rend->getShader()->removeOverrideBlocks();
            }
            rend->popShader();

            if (need_wire && !polyoff)
            {
                rend->polygonOffset(polyoff);
            }
        }

        // Wireframe rendering
        if (need_wire)
        {
            // GL3 requires a shader even for simple wireframe rendering.
            rend->pushShader(theLineShader);

            if (drawparams.draw_instanced)
            {
                _viewportGeo->drawInstanceGroup(rend, RE_GEO_WIRE_IDX, drawparams.instance_group);
            }
            else
            {
                _viewportGeo->draw(rend, RE_GEO_WIRE_IDX);
            }

            rend->popShader();
        }
    }

    //-----------------------------------------------------------------------------
    void GR_PackedGolaemEntity::renderDecoration(
#if HDK_API_VERSION >= 20000000
        RE_RenderContext rend,
#else
        RE_Render* rend,
#endif 
        GR_Decoration decor,
        const GR_DecorationParms& parms)
    {
        if (_viewportGeo == NULL)
        {
            return;
        }
        drawDecorationForGeo(
            rend, _viewportGeo, decor, parms.opts, parms.render_flags,
            parms.overlay, parms.override_vis, parms.instance_group,
            GR_SELECT_NONE);
    }

    //-----------------------------------------------------------------------------
    int GR_PackedGolaemEntity::renderPick(
#if HDK_API_VERSION >= 20000000
        RE_RenderContext rend,
#else
        RE_Render* rend,
#endif 
        const GR_DisplayOption* opt,
        unsigned int pick_type,
        GR_PickStyle pick_style,
        bool has_pick_map)
    {
        GLM_UNREFERENCED(rend);
        GLM_UNREFERENCED(opt);
        GLM_UNREFERENCED(pick_type);
        GLM_UNREFERENCED(pick_style);
        GLM_UNREFERENCED(has_pick_map);

        // TODO
        return 0;
    }

} // namespace glm