/***************************************************************************
*                                                                          *
*  Copyright (C) Golaem S.A.  All Rights Reserved.                         *
*                                                                          *
***************************************************************************/

#include "glmHoudini.h"
#include "glmHoudiniLogger.h"

HDK_INCLUDES_START

#include <SOP/SOP_Node.h>
#include <OP/OP_AutoLockInputs.h>
#include <PRM/PRM_TemplateBuilder.h>
#include <UT/UT_StringHolder.h>
#include <OP/OP_OperatorTable.h>
#include <UT/UT_DSOVersion.h>
#include <UT/UT_HDKVersion.h>
#include <PRM/PRM_SpareData.h>
#include <PRM/PRM_ChoiceList.h>
#include <PRM/PRM_Range.h>
#include <GEO/GEO_PolyCounts.h>
#include <GEO/GEO_PrimPoly.h>
#include <CH/CH_Manager.h>
#include <UT/UT_Exit.h>
#include <UT/UT_DirUtil.h>
#include <PY/PY_Python.h>
#include <HOM/HOM_Module.h>
#include <HOM/HOM_shelves.h>

HDK_INCLUDES_END

#include <glmCore.h>
#include <glmCrowdIO.h>
#include <glmCrowdIOUtils.h>
#include <glmSimulationCacheLibrary.h>
#include <glmSimulationCacheInformation.h>
#include <glmFileDir.h>
#include <glmFileName.h>
#include <glmGolaemCharacter.h>
#include <glmCrowdFBXStorage.h>
#include <glmCrowdFBXBaker.h>
#include <glmCrowdFBXCharacter.h>
#include <glmCrowdGcgStorage.h>
#include <glmCrowdGcgCharacter.h>
#include <glmCrowdGcgBaker.h>

#ifndef GLM_GEO_ENGINE_VERSION
#define GLM_GEO_ENGINE_VERSION 1
#endif

#include "glmCrowdHoudiniPluginAPI.h"

glm::Mutex _glmCrowdGeoMutex;

//-----------------------------------------------------------------------------

struct GolaemParams
{
    enum Value
    {
        CACHELIB_FILE,
        CACHELIB_ITEM,
        FORCE_CACHELIB_EVAL,
        CROWDFIELD_NAMES,
        CACHE_NAME,
        CACHE_DIR,
        CHARACTER_FILES,
        SOURCE_TERRAIN,
        DEST_TERRAIN,
        ENABLE_LAYOUT,
        LAYOUT_FILE,
        OPEN_LAYOUT,
        CURRENT_FRAME,
        START_FRAME,
        END_FRAME,
        ENTITY_COUNT,
        DRAW_PERCENT,
        DISPLAY_MODE,
        GEO_TAG,
        END
    };
};

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
static inline const char* getParamName(GolaemParams::Value value)
{
    switch (value)
    {
    case GolaemParams::CACHELIB_FILE:
        return "glmCacheLibFile";
        break;
    case GolaemParams::CACHELIB_ITEM:
        return "glmCacheLibItem";
        break;
    case GolaemParams::FORCE_CACHELIB_EVAL:
        return "glmForceCacheLibEval";
        break;
    case GolaemParams::CROWDFIELD_NAMES:
        return "glmCrowdFieldNames";
        break;
    case GolaemParams::CACHE_NAME:
        return "glmCacheName";
        break;
    case GolaemParams::CACHE_DIR:
        return "glmCacheDir";
        break;
    case GolaemParams::CHARACTER_FILES:
        return "glmCharacterFiles";
        break;
    case GolaemParams::SOURCE_TERRAIN:
        return "glmSourceTerrain";
        break;
    case GolaemParams::DEST_TERRAIN:
        return "glmDestTerrain";
        break;
    case GolaemParams::ENABLE_LAYOUT:
        return "glmEnableLayout";
        break;
    case GolaemParams::LAYOUT_FILE:
        return "glmLayoutFile";
        break;
    case GolaemParams::OPEN_LAYOUT:
        return "glmOpenLayout";
        break;
    case GolaemParams::CURRENT_FRAME:
        return "glmCurrentFrame";
        break;
    case GolaemParams::START_FRAME:
        return "glmStartFrame";
        break;
    case GolaemParams::END_FRAME:
        return "glmEndFrame";
        break;
    case GolaemParams::ENTITY_COUNT:
        return "glmEntityCount";
        break;
    case GolaemParams::DRAW_PERCENT:
        return "glmDrawPercent";
        break;
    case GolaemParams::DISPLAY_MODE:
        return "glmDisplayMode";
        break;
    case GolaemParams::GEO_TAG:
        return "glmGeoTag";
        break;
    case GolaemParams::END:
        break;
    default:
        break;
    }
    return "";
}

//-----------------------------------------------------------------------------
class SOP_GolaemCacheProxy : public SOP_Node
{
private:
    bool _needsRefresh;
    bool _noUpdateLoop;

    glm::crowdio::SimulationCacheFactory _factory;

public:
    static PRM_Template* buildTemplates();
    static OP_Node* create(OP_Network* net, const char* name, OP_Operator* op);

    ~SOP_GolaemCacheProxy();

    virtual bool updateParmsFlags();

    static int onParamChanged(void* data, int index, fpreal t, const PRM_Template* tplate);
    static int onOpenLayoutEditor(void* data, int index, fpreal t, const PRM_Template* tplate);

    void opChanged(OP_EventType reason, void* data);

private:
    SOP_GolaemCacheProxy(OP_Network* net, const char* name, OP_Operator* op);

    OP_ERROR cookMySop(OP_Context& context) override;

    static void buildGolaemCacheChoice(void* thedata, PRM_Name* thechoicenames, int thelistsize, const PRM_SpareData* thespareptr, const PRM_Parm* theparm);

    void loadSimulationCacheLib(glm::crowdio::SimulationCacheLibrary& simuCacheLibrary, const glm::GlmString& cacheLibPath);

    void refreshParameters(
        fpreal time,
        bool updateCache = true,
        bool updateLayout = true,
        bool updateTerrain = true,
        bool updateCharacterFiles = true);

    void updateCacheLibParams(fpreal time);
};

//-----------------------------------------------------------------------------
void SOP_GolaemCacheProxy::loadSimulationCacheLib(glm::crowdio::SimulationCacheLibrary& simuCacheLibrary, const glm::GlmString& cacheLibPath)
{
    if (!cacheLibPath.empty() && glm::FileDir::exist(cacheLibPath.c_str()))
    {
        std::ifstream inFile(cacheLibPath.c_str());
        if (inFile.is_open())
        {
            inFile.seekg(0, std::ios::end);
            size_t fileSize = inFile.tellg();
            inFile.seekg(0);

            std::string fileContents(fileSize + 1, '\0');
            inFile.read(&fileContents[0], fileSize);
            simuCacheLibrary.loadLibrary(fileContents.c_str(), fileContents.size(), false);
        }
        else
        {
            GLM_CROWD_TRACE_ERROR("Failed to open Golaem simulation cache library file '" << cacheLibPath << "'");
        }
    }
}
//-----------------------------------------------------------------------------
void SOP_GolaemCacheProxy::buildGolaemCacheChoice(void* thedata, PRM_Name* thechoicenames, int thelistsize, const PRM_SpareData* /*thespareptr*/, const PRM_Parm* /*theparm*/)
{
    SOP_GolaemCacheProxy* currentNode = (SOP_GolaemCacheProxy*)thedata;
    fpreal time = CHgetEvalTime();
    UT_String cacheLibPath;
    currentNode->evalString(cacheLibPath, getParamName(GolaemParams::CACHELIB_FILE), 0, time);

    glm::crowdio::SimulationCacheLibrary simuCacheLibrary;
    currentNode->loadSimulationCacheLib(simuCacheLibrary, cacheLibPath.c_str());
    int listIdx = 0;
    for (size_t iCache = 0, cacheCount = simuCacheLibrary.getCacheInformationCount(); iCache < cacheCount && listIdx + 1 < thelistsize; ++iCache, ++listIdx)
    {
        const glm::crowdio::SimulationCacheInformation& cacheInfo = simuCacheLibrary.getCacheInformation(iCache);
        PRM_Name& choiceName = thechoicenames[listIdx];
        choiceName.setTokenAndLabel(cacheInfo._cacheName.c_str(), cacheInfo._cacheName.c_str());
    }
    thechoicenames[listIdx].setToken(0); // Need a null terminator
}

//-----------------------------------------------------------------------------
PRM_Template* SOP_GolaemCacheProxy::buildTemplates()
{
    static PRM_Default defaultParam(0, "");
    static PRM_Name cacheLibFilePrm(getParamName(GolaemParams::CACHELIB_FILE), "Cache Library File");
    static PRM_SpareData glmCacheFileOptions(
        PRM_SpareArgs() << PRM_SpareToken(PRM_SpareData::getFileChooserPatternToken(), "*.gscb"));

    static PRM_Name cacheItemNamePrm(getParamName(GolaemParams::CACHELIB_ITEM), "Cache Library Item");
    static PRM_ChoiceList cacheNameChoice((PRM_ChoiceListType)PRM_CHOICELIST_SINGLE, &SOP_GolaemCacheProxy::buildGolaemCacheChoice);

    static PRM_Name forceCacheLibEvalPrm(getParamName(GolaemParams::FORCE_CACHELIB_EVAL), "Force Cache Library Evaluation");

    static PRM_Name crowdFieldNamesPrm(getParamName(GolaemParams::CROWDFIELD_NAMES), "CrowdField Names");
    static PRM_Name cacheNamePrm(getParamName(GolaemParams::CACHE_NAME), "Cache Name");
    static PRM_Name cacheDirPrm(getParamName(GolaemParams::CACHE_DIR), "Cache Dir");
    static PRM_Name characterFilesPrm(getParamName(GolaemParams::CHARACTER_FILES), "Character Files");
    static PRM_Name sourceTerrainPrm(getParamName(GolaemParams::SOURCE_TERRAIN), "Source Terrain");
    static PRM_Name destTerrainPrm(getParamName(GolaemParams::DEST_TERRAIN), "Destination Terrain");
    static PRM_SpareData glmTerrainFileOptions(
        PRM_SpareArgs() << PRM_SpareToken(PRM_SpareData::getFileChooserPatternToken(), "*.gtg"));
    static PRM_Name enableLayoutPrm(getParamName(GolaemParams::ENABLE_LAYOUT), "Enable Layout");
    static PRM_Name layoutFilePrm(getParamName(GolaemParams::LAYOUT_FILE), "Layout File");
    static PRM_SpareData glmLayoutFileOptions(
        PRM_SpareArgs() << PRM_SpareToken(PRM_SpareData::getFileChooserPatternToken(), "*.gscl"));

    static PRM_Name openLayoutPrm(getParamName(GolaemParams::OPEN_LAYOUT), "Open");

    static PRM_Default currentFrameDefault(0, "@Frame"); // set expression to link to the current frame
    static PRM_Name currentFramePrm(getParamName(GolaemParams::CURRENT_FRAME), "Current Frame");
    static PRM_Range currentFrameRange(PRM_RANGE_UI, 0, PRM_RANGE_UI, 240);

    static PRM_Name startFramePrm(getParamName(GolaemParams::START_FRAME), "Start Frame");
    static PRM_Name endFramePrm(getParamName(GolaemParams::END_FRAME), "End Frame");
    static PRM_Name entityCountPrm(getParamName(GolaemParams::ENTITY_COUNT), "Entity Count");

    static PRM_Default defaultDrawPercentParam(100, "");
    static PRM_Name drawPercentPrm(getParamName(GolaemParams::DRAW_PERCENT), "Draw Entity Percent");
    static PRM_Range drawPercentRange(PRM_RANGE_RESTRICTED, 0, PRM_RANGE_RESTRICTED, 100);

    static PRM_Name displayModePrm(getParamName(GolaemParams::DISPLAY_MODE), "Display Mode");
    static PRM_Default defaultDisplayMode(2, ""); // skinmesh
    static PRM_Name diplayModeEnum[] =
        {
            PRM_Name("boundingBox", "Bounding Box"),
            PRM_Name("skeleton", "Skeleton"),
            PRM_Name("skinmesh", "Skinmesh"),
            PRM_Name(0) // Need a null terminator
        };
    static PRM_ChoiceList displayModeChoice((PRM_ChoiceListType)PRM_CHOICELIST_SINGLE, diplayModeEnum);

    static PRM_Name geoTagPrm(getParamName(GolaemParams::GEO_TAG), "Geometry Tag");
    static PRM_Name geoTagsEnum[] =
        {
            PRM_Name("None", "None"),
            PRM_Name("General", "General"),
            PRM_Name("Previz1", "Previz1"),
            PRM_Name("Previz2", "Previz2"),
            PRM_Name("Render1", "Render1"),
            PRM_Name("Render2", "Render2"),
            PRM_Name("Bake1", "Bake1"),
            PRM_Name("Bake2", "Bake2"),
            PRM_Name("User1", "User1"),
            PRM_Name("User2", "User2"),
            PRM_Name(0) // Need a null terminator
        };

    static PRM_ChoiceList geoTagsChoice((PRM_ChoiceListType)PRM_CHOICELIST_SINGLE, geoTagsEnum);

    static PRM_Template myTemplateList[] =
        {
            PRM_Template(
                PRM_FILE,
                1,
                &cacheLibFilePrm,
                &defaultParam,
                0,
                0,
                &SOP_GolaemCacheProxy::onParamChanged,
                &glmCacheFileOptions,
                1,
                "Simulation Cache Library file"),
            PRM_Template(
                PRM_STRING,
                1,
                &cacheItemNamePrm,
                &defaultParam,
                &cacheNameChoice,
                0,
                &SOP_GolaemCacheProxy::onParamChanged,
                0,
                1,
                "Simulation Cache Library item"),
            PRM_Template(
                PRM_TOGGLE,
                1,
                &forceCacheLibEvalPrm,
                &defaultParam,
                0,
                0,
                0,
                0,
                1,
                "Force Cache Library evaluation"),
            PRM_Template(
                PRM_STRING,
                1,
                &crowdFieldNamesPrm,
                &defaultParam,
                0,
                0,
                &SOP_GolaemCacheProxy::onParamChanged,
                0,
                1,
                "Simulation Cache CrowdField (simulation) names"),
            PRM_Template(
                PRM_STRING,
                1,
                &cacheNamePrm,
                &defaultParam,
                0,
                0,
                &SOP_GolaemCacheProxy::onParamChanged,
                0,
                1,
                "Simulation Cache name"),
            PRM_Template(
                PRM_DIRECTORY,
                1,
                &cacheDirPrm,
                &defaultParam,
                0,
                0,
                &SOP_GolaemCacheProxy::onParamChanged,
                0,
                1,
                "Simulation Cache directory"),
            PRM_Template(
                PRM_STRING,
                1,
                &characterFilesPrm,
                &defaultParam,
                0,
                0,
                &SOP_GolaemCacheProxy::onParamChanged,
                0,
                1,
                "Golaem Character files"),
            PRM_Template(
                PRM_FILE,
                1,
                &sourceTerrainPrm,
                &defaultParam,
                0,
                0,
                &SOP_GolaemCacheProxy::onParamChanged,
                &glmTerrainFileOptions,
                1,
                "Source Terrain"),
            PRM_Template(
                PRM_FILE,
                1,
                &destTerrainPrm,
                &defaultParam,
                0,
                0,
                &SOP_GolaemCacheProxy::onParamChanged,
                &glmTerrainFileOptions,
                1,
                "Destination Terrain"),
            PRM_Template(
                PRM_TOGGLE | PRM_TYPE_JOIN_NEXT | PRM_TYPE_LABEL_NONE,
                1,
                &enableLayoutPrm,
                &defaultParam,
                0,
                0,
                &SOP_GolaemCacheProxy::onParamChanged,
                0,
                1,
                "Enable Simulation Cache Layout"),
            PRM_Template(
                PRM_FILE | PRM_TYPE_JOIN_NEXT,
                1,
                &layoutFilePrm,
                &defaultParam,
                0,
                0,
                &SOP_GolaemCacheProxy::onParamChanged,
                &glmLayoutFileOptions,
                1,
                "Simulation Cache Layout file"),
            PRM_Template(
                PRM_CALLBACK_NOREFRESH,
                1,
                &openLayoutPrm,
                0,
                0,
                0,
                &SOP_GolaemCacheProxy::onOpenLayoutEditor,
                0,
                1,
                "Open in Layout Editor"),
            PRM_Template(
                PRM_FLT_J,
                1,
                &currentFramePrm,
                &currentFrameDefault,
                0,
                &currentFrameRange,
                &SOP_GolaemCacheProxy::onParamChanged,
                0,
                1,
                "Simulation Cache frame to display"),
            PRM_Template(
                PRM_INT,
                1,
                &startFramePrm,
                &defaultParam,
                0,
                0,
                &SOP_GolaemCacheProxy::onParamChanged,
                0,
                1,
                "Simulation Cache start frame"),
            PRM_Template(
                PRM_INT,
                1,
                &endFramePrm,
                &defaultParam,
                0,
                0,
                &SOP_GolaemCacheProxy::onParamChanged,
                0,
                1,
                "Simulation Cache end frame"),
            PRM_Template(
                PRM_INT,
                1,
                &entityCountPrm,
                &defaultParam,
                0,
                0,
                &SOP_GolaemCacheProxy::onParamChanged,
                0,
                1,
                "Simulation Cache entity count"),
            PRM_Template(
                PRM_FLT,
                1,
                &drawPercentPrm,
                &defaultDrawPercentParam,
                0,
                &drawPercentRange,
                &SOP_GolaemCacheProxy::onParamChanged,
                0,
                1,
                "Draw entity percent"),
            PRM_Template(
                PRM_ORD,
                1,
                &displayModePrm,
                &defaultDisplayMode,
                &displayModeChoice,
                0,
                &SOP_GolaemCacheProxy::onParamChanged,
                0,
                1,
                "Display Mode"),
            PRM_Template(
                PRM_ORD,
                1,
                &geoTagPrm,
                &defaultParam,
                &geoTagsChoice,
                0,
                &SOP_GolaemCacheProxy::onParamChanged,
                0,
                1,
                "Geometry Tag"),
            PRM_Template() // sentinel
        };
    return myTemplateList;
}

//-----------------------------------------------------------------------------
bool SOP_GolaemCacheProxy::updateParmsFlags()
{
    fpreal time = CHgetEvalTime();
    bool changed = SOP_Node::updateParmsFlags();
    UT_String cacheLibPath;
    evalString(cacheLibPath, getParamName(GolaemParams::CACHELIB_FILE), 0, time);
    changed |= enableParm(getParamName(GolaemParams::CACHELIB_ITEM), cacheLibPath.length() > 0);
    changed |= setVisibleState(getParamName(GolaemParams::FORCE_CACHELIB_EVAL), false);
    changed |= enableParm(getParamName(GolaemParams::CROWDFIELD_NAMES), 1);
    changed |= enableParm(getParamName(GolaemParams::CACHE_NAME), 1);
    changed |= enableParm(getParamName(GolaemParams::CACHE_DIR), 1);
    changed |= enableParm(getParamName(GolaemParams::CHARACTER_FILES), 1);
    changed |= enableParm(getParamName(GolaemParams::SOURCE_TERRAIN), 1);
    changed |= enableParm(getParamName(GolaemParams::DEST_TERRAIN), 1);
    changed |= enableParm(getParamName(GolaemParams::ENABLE_LAYOUT), 1);
    bool layoutEnabled = evalInt(getParamName(GolaemParams::ENABLE_LAYOUT), 0, time);
    changed |= enableParm(getParamName(GolaemParams::LAYOUT_FILE), layoutEnabled);
    changed |= enableParm(getParamName(GolaemParams::OPEN_LAYOUT), 1);
    changed |= enableParm(getParamName(GolaemParams::CURRENT_FRAME), 1);
    changed |= enableParm(getParamName(GolaemParams::START_FRAME), 0);
    changed |= enableParm(getParamName(GolaemParams::END_FRAME), 0);
    changed |= enableParm(getParamName(GolaemParams::ENTITY_COUNT), 0);
    changed |= enableParm(getParamName(GolaemParams::DRAW_PERCENT), 1);
    changed |= enableParm(getParamName(GolaemParams::DISPLAY_MODE), 1);
    GolaemDisplayMode::Value displayMode = (GolaemDisplayMode::Value)evalInt(getParamName(GolaemParams::DISPLAY_MODE), 0, time);
    changed |= enableParm(getParamName(GolaemParams::GEO_TAG), displayMode == GolaemDisplayMode::SKINMESH);
    //PRM_Parm* parm = getParmPtr("glmCacheIdx");
    return changed;
}

//-----------------------------------------------------------------------------
void SOP_GolaemCacheProxy::refreshParameters(
    fpreal time,
    bool updateCache,
    bool updateLayout,
    bool updateTerrain,
    bool updateCharacterFiles)
{
    if (_needsRefresh)
    {
        // force refresh after loading the node
        updateCache = true;
        updateLayout = true;
        updateTerrain = true;
        updateCharacterFiles = true;
        _needsRefresh = false;
    }

    UT_String cacheDir;
    evalString(cacheDir, getParamName(GolaemParams::CACHE_DIR), 0, time);
    UT_String cacheName;
    evalString(cacheName, getParamName(GolaemParams::CACHE_NAME), 0, time);
    UT_String cfNames;
    evalString(cfNames, getParamName(GolaemParams::CROWDFIELD_NAMES), 0, time);
    if (updateCache)
    {
        // completely clear the factory
        _factory.clear(glm::crowdio::FactoryClearMode::ALL);
    }
    if (updateCharacterFiles)
    {
        UT_String characterFiles;
        evalString(characterFiles, getParamName(GolaemParams::CHARACTER_FILES), 0, time);
        _factory.loadGolaemCharacters(characterFiles.c_str());
    }
    if (updateLayout)
    {
        // clear the factory history
        _factory.clear((glm::crowdio::FactoryClearMode::Value)(glm::crowdio::FactoryClearMode::ALL_HISTORY | glm::crowdio::FactoryClearMode::ALL_MODIFIED));

        bool enableLayout = evalInt(getParamName(GolaemParams::ENABLE_LAYOUT), 0, time) == 1;
        UT_String layoutFile;
        evalString(layoutFile, getParamName(GolaemParams::LAYOUT_FILE), 0, time);
        if (enableLayout && layoutFile.length() > 0)
        {
			glm::GlmString layoutFiles = layoutFile.buffer();
			glm::Array<glm::GlmString> layoutFilesParsed;
			glm::split(layoutFiles, ";", layoutFilesParsed);
			for (size_t iLayout = 0; iLayout < layoutFilesParsed.size(); iLayout++)
			{
				_factory.loadLayoutHistoryFile(iLayout, layoutFilesParsed[iLayout].c_str());
			}
        }
    }
    if (updateTerrain)
    {
        UT_String sourceTerrainFile;
        evalString(sourceTerrainFile, getParamName(GolaemParams::SOURCE_TERRAIN), 0, time);
        UT_String destTerrainFile;
        evalString(destTerrainFile, getParamName(GolaemParams::DEST_TERRAIN), 0, time);
        glm::crowdio::crowdTerrain::TerrainMesh* sourceTerrain = NULL;
        glm::crowdio::crowdTerrain::TerrainMesh* destTerrain = NULL;
        if (sourceTerrainFile.length() > 0)
        {
            sourceTerrain = glm::crowdio::crowdTerrain::loadTerrainAsset(sourceTerrainFile.c_str());
        }
        if (destTerrainFile.length() > 0)
        {
            destTerrain = glm::crowdio::crowdTerrain::loadTerrainAsset(destTerrainFile.c_str());
        }
        _factory.setTerrainMeshes(sourceTerrain, destTerrain);
    }

    float currentFrame = static_cast<float>(evalFloat(getParamName(GolaemParams::CURRENT_FRAME), 0, time));

    float renderPercent = static_cast<float>(evalFloat(getParamName(GolaemParams::DRAW_PERCENT), 0, time)) * 0.01f;

    int startFrame = 0;
    int endFrame = 0;
    bool framesFound = false;
    int64_t entityCount = 0;
    glm::Array<glm::GlmString> crowdFieldNames = glm::stringToStringArray(cfNames.c_str(), ";");
    for (size_t iCf = 0, cfCount = crowdFieldNames.size(); iCf < cfCount; ++iCf)
    {
        const glm::GlmString& cfName = crowdFieldNames[iCf];
        if (cfName.empty())
        {
            continue;
        }
        glm::crowdio::CachedSimulation& cachedSimulation = _factory.getCachedSimulation(cacheDir.c_str(), cacheName.c_str(), cfName.c_str());
        const glm::crowdio::GlmSimulationData* simuData = cachedSimulation.getFinalSimulationData();
        const glm::crowdio::GlmFrameData* frameData = cachedSimulation.getFinalFrameData(currentFrame, UINT32_MAX, true);

        if (simuData == NULL || frameData == NULL)
        {
            continue;
        }

        glm::PODArray<int64_t> excludedEntities;
		glm::Array<const glm::crowdio::glmHistoryRuntimeStructure*> historyStructures;
		cachedSimulation.getHistoryRuntimeStructures(historyStructures);
        glm::crowdio::createEntityExclusionList(excludedEntities, cachedSimulation.getSrcSimulationData(), _factory.getLayoutHistories(), historyStructures);
        size_t maxEntities = (size_t)floorf(simuData->_entityCount * renderPercent);
        for (uint32_t iEntity = 0; iEntity < simuData->_entityCount; ++iEntity)
        {
            int64_t entityId = simuData->_entityIds[iEntity];
            if (entityId < 0)
            {
                // entity was probably killed
                continue;
            }

            bool excludedEntity = frameData->_entityEnabled[iEntity] != 1;
            if (!excludedEntity)
            {
                excludedEntity = iEntity >= maxEntities;
                if (!excludedEntity)
                {
                    size_t excludedEntityIdx;
                    excludedEntity = glm::glmFindIndex(excludedEntities.begin(), excludedEntities.end(), entityId, excludedEntityIdx);
                }
            }

            if (excludedEntity)
            {
                continue;
            }
            ++entityCount;
        }

        if (!framesFound)
        {
            framesFound = cachedSimulation.getSrcFrameRangeAvailableOnDisk(startFrame, endFrame);
        }
        else
        {
            int cfStartFrame = 0;
            int cfEndFrame = 0;
            framesFound = cachedSimulation.getSrcFrameRangeAvailableOnDisk(cfStartFrame, cfEndFrame);
            if (framesFound)
            {
                startFrame = glm::max(cfStartFrame, startFrame);
                endFrame = glm::min(cfEndFrame, endFrame);
            }
        }
    }

    setInt(getParamName(GolaemParams::START_FRAME), 0, time, startFrame);
    setInt(getParamName(GolaemParams::END_FRAME), 0, time, endFrame);
    setInt(getParamName(GolaemParams::ENTITY_COUNT), 0, time, entityCount);
}

//-----------------------------------------------------------------------------
void SOP_GolaemCacheProxy::opChanged(OP_EventType reason, void* data)
{
    SOP_Node::opChanged(reason, data);
    if (_noUpdateLoop)
    {
        return;
    }
    if (reason == OP_PARM_CHANGED)
    {
        int64_t paramIdx = reinterpret_cast<int64_t>(data);
        GolaemParams::Value param = (GolaemParams::Value)(paramIdx);
        if (param == GolaemParams::FORCE_CACHELIB_EVAL)
        {
            // use this to force update when setting cache lib parameters in python (from cache library)
            _noUpdateLoop = true;
            fpreal time = CHgetEvalTime();
            // reset parameter
            setInt(getParamName(GolaemParams::FORCE_CACHELIB_EVAL), 0, time, 0);
            updateCacheLibParams(time);
            _noUpdateLoop = false;
        }
    }
}

//-----------------------------------------------------------------------------
void SOP_GolaemCacheProxy::updateCacheLibParams(fpreal time)
{
    // update cache library parameters
    UT_String cacheLibPath;
    evalString(cacheLibPath, getParamName(GolaemParams::CACHELIB_FILE), 0, time);
    UT_String cacheLibItem;
    evalString(cacheLibItem, getParamName(GolaemParams::CACHELIB_ITEM), 0, time);

    glm::crowdio::SimulationCacheLibrary simuCacheLibrary;
    loadSimulationCacheLib(simuCacheLibrary, cacheLibPath.c_str());

    glm::GlmString cfNames;
    glm::GlmString cacheName;
    glm::GlmString cacheDir;
    glm::GlmString characterFiles;
    glm::GlmString srcTerrain;
    glm::GlmString dstTerrain;
    bool enableLayout = false;
    glm::GlmString layoutFile;

    glm::crowdio::SimulationCacheInformation* cacheInfo = simuCacheLibrary.getCacheInformationByName(cacheLibItem.c_str());
    if (cacheInfo == NULL && simuCacheLibrary.getCacheInformationCount() > 0)
    {
        cacheInfo = &simuCacheLibrary.getCacheInformation(0);
    }
    if (cacheInfo != NULL)
    {
        cfNames = cacheInfo->_crowdFields;
        cacheName = cacheInfo->_cacheName;
        cacheDir = cacheInfo->_cacheDir;
        characterFiles = cacheInfo->_characterFiles;
        srcTerrain = cacheInfo->_srcTerrain;
        dstTerrain = cacheInfo->_destTerrain;
        enableLayout = cacheInfo->_enableLayout;
        layoutFile = cacheInfo->_layoutFile;
    }
    setString(cfNames.c_str(), CH_STRING_LITERAL, getParamName(GolaemParams::CROWDFIELD_NAMES), 0, time);
    setString(cacheName.c_str(), CH_STRING_LITERAL, getParamName(GolaemParams::CACHE_NAME), 0, time);
    setString(cacheDir.c_str(), CH_STRING_LITERAL, getParamName(GolaemParams::CACHE_DIR), 0, time);
    setString(characterFiles.c_str(), CH_STRING_LITERAL, getParamName(GolaemParams::CHARACTER_FILES), 0, time);
    setString(srcTerrain.c_str(), CH_STRING_LITERAL, getParamName(GolaemParams::SOURCE_TERRAIN), 0, time);
    setString(dstTerrain.c_str(), CH_STRING_LITERAL, getParamName(GolaemParams::DEST_TERRAIN), 0, time);
    setInt(getParamName(GolaemParams::ENABLE_LAYOUT), 0, time, enableLayout ? 1 : 0);
    setString(layoutFile.c_str(), CH_STRING_LITERAL, getParamName(GolaemParams::LAYOUT_FILE), 0, time);
}

//-----------------------------------------------------------------------------
int SOP_GolaemCacheProxy::onParamChanged(void* data, int /*index*/, fpreal time, const PRM_Template* tplate)
{
    SOP_GolaemCacheProxy* sop = static_cast<SOP_GolaemCacheProxy*>(data);
    if (!sop->getHardLock()) // only allow reloading if we're not locked
    {
        sop->forceRecook();
    }

    const UT_StringRef& paramToken = tplate->getNamePtr()->getTokenRef();
    bool updateCache = false;
    bool updateLayout = false;
    bool updateTerrain = false;
    bool updateCharacterFiles = false;
    if (paramToken == getParamName(GolaemParams::CACHELIB_FILE) || paramToken == getParamName(GolaemParams::CACHELIB_ITEM))
    {
        updateCache = true;
        updateLayout = true;
        updateTerrain = true;
        updateCharacterFiles = true;

        sop->updateCacheLibParams(time);
    }
    if (paramToken == getParamName(GolaemParams::CROWDFIELD_NAMES) || paramToken == getParamName(GolaemParams::CACHE_NAME) || paramToken == getParamName(GolaemParams::CACHE_DIR))
    {
        updateCache = true;
        updateLayout = true;
        updateTerrain = true;
        updateCharacterFiles = true;
    }
    if (paramToken == getParamName(GolaemParams::ENABLE_LAYOUT) || paramToken == getParamName(GolaemParams::LAYOUT_FILE))
    {
        updateLayout = true;
    }
    if (paramToken == getParamName(GolaemParams::SOURCE_TERRAIN) || paramToken == getParamName(GolaemParams::DEST_TERRAIN))
    {
        updateTerrain = true;
    }
    if (paramToken == getParamName(GolaemParams::CHARACTER_FILES))
    {
        updateCharacterFiles = true;
    }
    sop->refreshParameters(time, updateCache, updateLayout, updateTerrain, updateCharacterFiles);
    return 1;
}

//-----------------------------------------------------------------------------
int SOP_GolaemCacheProxy::onOpenLayoutEditor(void* data, int /*index*/, fpreal time, const PRM_Template* tplate)
{
    SOP_GolaemCacheProxy* sop = static_cast<SOP_GolaemCacheProxy*>(data);

    const UT_StringRef& paramToken = tplate->getNamePtr()->getTokenRef();
    if (paramToken == getParamName(GolaemParams::OPEN_LAYOUT))
    {
        UT_String layoutFile;
        sop->evalString(layoutFile, getParamName(GolaemParams::LAYOUT_FILE), 0, time);
        glm::GlmString pythonCommand = "import glm.ui.windowHoudiniLauncher as launcher\n";
        pythonCommand += glm::GlmString("launcher.LayoutEditorWindowMain(");
        if (layoutFile.length() > 0)
        {
            pythonCommand += glm::GlmString("layoutFile=\"") + layoutFile.c_str() + "\"";
        }
        pythonCommand += ")\n";
        PYrunPythonStatements(pythonCommand.c_str());
    }
    return 1;
}

//-----------------------------------------------------------------------------
SOP_GolaemCacheProxy::SOP_GolaemCacheProxy(OP_Network* net, const char* name, OP_Operator* op)
    : SOP_Node(net, name, op)
    , _needsRefresh(true)
    , _noUpdateLoop(false)
{
    //mySopFlags.setManagesDataIDs(true);
}

//-----------------------------------------------------------------------------
OP_Node* SOP_GolaemCacheProxy::create(OP_Network* net, const char* name, OP_Operator* op)
{
    SOP_GolaemCacheProxy* newSop = new SOP_GolaemCacheProxy(net, name, op);
    return newSop;
}

//-----------------------------------------------------------------------------
SOP_GolaemCacheProxy::~SOP_GolaemCacheProxy()
{
}

//-----------------------------------------------------------------------------
static void glmDsoExit(void* data)
{
    GLM_UNREFERENCED(data);

    glm::crowdio::finish();
    glm::theGolaemLogger::destroy();
    glm::finishCore();

    glm::setDefaultAllocator(NULL);
}

// register new sop
//-----------------------------------------------------------------------------
void GLM_CROWDHOUDINI_API newSopOperator(OP_OperatorTable* table)
{
    glm::useCoreDefaultAllocator();

    glm::initCore(); // inits logs
    glm::getLog()->_logSeverity[glm::Log::CROWD] = glm::Log::LOG_WARNING;
    glm::getLog()->_logSeverity[glm::Log::SDK] = glm::Log::LOG_ERROR;

    glm::theGolaemLogger::create();

    glm::crowdio::ProductDetails productDetails;
    productDetails._fullVersion = glm::crowdio::getGolaemVersion();
    productDetails._containerApplicationName = "Houdini";
    productDetails._containerApplicationVersion = SYS_Version::release();
    productDetails._notificationHandler = NULL; // todo: install viewport notification

    OP_Operator* op = new OP_Operator(
        "golaemCacheProxy",
        "Golaem Cache Proxy",
        SOP_GolaemCacheProxy::create,
        SOP_GolaemCacheProxy::buildTemplates(),
        0,
        0,
        nullptr,
        OP_FLAG_GENERATOR);

    op->setOpTabSubMenuPath("Golaem");
    op->setIconName("SimulationCacheProxy.png");

    UT_String defSource;
    op->getDefinitionSource(defSource);
    glm::FileName pluginPath(defSource.c_str());
    glm::GlmString pluginDir = pluginPath.pathname();

    bool allowCreatePLE = true;
    bool deferLicenseCheck = false; // check for licenses at crowdio::init
    glm::crowdio::setupGolaemProduct("GolaemForHoudini", pluginDir, productDetails, deferLicenseCheck, allowCreatePLE);
    glm::crowdio::init();

    UT_Exit::addExitCallback(glmDsoExit);

    table->addOperator(op);

    glm::GlmString licenseInfo = glm::crowdio::getLicenseRLMString();
    if (glm::crowdio::hasFullLicenseFeatures())
    {
        licenseInfo = "1;" + licenseInfo;
    }
    else
    {
        licenseInfo = "0;" + licenseInfo;
    }

    // init python session structure
    glm::GlmString pythonCommand = "import glm.ui.windowHoudiniLauncher as launcher\n";
    pythonCommand += glm::GlmString("launcher.glmSessionInfo._pluginDir=\"") + pluginDir + "\"\n";
    pythonCommand += glm::GlmString("launcher.glmSessionInfo._version=\"") + glm::crowdio::getGolaemVersion() + "\"\n";
    pythonCommand += glm::GlmString("launcher.glmSessionInfo._licenseInfo=\"") + licenseInfo + "\"\n";

    PYrunPythonStatements(pythonCommand.c_str());
}

//-----------------------------------------------------------------------------
OP_ERROR SOP_GolaemCacheProxy::cookMySop(OP_Context& context)
{
    OP_AutoLockInputs lockInputs(this);
    if (lockInputs.lock(context) >= UT_ERROR_ABORT)
    {
        return error();
    }

    int isRender = isCookingRender();

    // Erase our gdp but keep it around for reuse
    // This is the same as clearAndDestroy() in terms of the result
    // but keeps a list of all the primitives around so that
    // if you immediately recreate the same thing it can be done
    // very fast.
    gdp->stashAll();

    fpreal time = context.getTime();
    if (_needsRefresh)
    {
        refreshParameters(time);
    }

    UT_String cacheDir;
    evalString(cacheDir, getParamName(GolaemParams::CACHE_DIR), 0, time);
    UT_String cacheName;
    evalString(cacheName, getParamName(GolaemParams::CACHE_NAME), 0, time);
    UT_String cfNames;
    evalString(cfNames, getParamName(GolaemParams::CROWDFIELD_NAMES), 0, time);

    GolaemDisplayMode::Value displayMode = (GolaemDisplayMode::Value)evalInt(getParamName(GolaemParams::DISPLAY_MODE), 0, time);
    if (isRender)
    {
        // always render skinmeshes
        displayMode = GolaemDisplayMode::SKINMESH;
    }

    float currentFrame = static_cast<float>(evalFloat(getParamName(GolaemParams::CURRENT_FRAME), 0, time));
    float renderPercent = static_cast<float>(evalFloat(getParamName(GolaemParams::DRAW_PERCENT), 0, time)) * 0.01f;
    short geoTag = static_cast<short>(evalInt(getParamName(GolaemParams::GEO_TAG), 0, time));

    switch (displayMode)
    {
    case GolaemDisplayMode::BOUNDING_BOX:
    {
        glm::Array<glm::GlmString> crowdFieldNames = glm::stringToStringArray(cfNames.c_str(), ";");
        for (size_t iCf = 0, cfCount = crowdFieldNames.size(); iCf < cfCount; ++iCf)
        {
            const glm::GlmString& cfName = crowdFieldNames[iCf];
            if (cfName.empty())
            {
                continue;
            }
            glm::crowdio::CachedSimulation& cachedSimulation = _factory.getCachedSimulation(cacheDir.c_str(), cacheName.c_str(), cfName.c_str());
            const glm::crowdio::GlmSimulationData* simuData = cachedSimulation.getFinalSimulationData();
            const glm::crowdio::GlmFrameData* frameData = cachedSimulation.getFinalFrameData(currentFrame, UINT32_MAX, true);

            if (simuData == NULL || frameData == NULL)
            {
                continue;
            }

            glm::PODArray<int64_t> excludedEntities;
			glm::Array<const glm::crowdio::glmHistoryRuntimeStructure*> historyStructures;
			cachedSimulation.getHistoryRuntimeStructures(historyStructures);
            glm::crowdio::createEntityExclusionList(excludedEntities, cachedSimulation.getSrcSimulationData(), _factory.getLayoutHistories(), historyStructures);
            size_t maxEntities = (size_t)floorf(simuData->_entityCount * renderPercent);
            for (uint32_t iEntity = 0; iEntity < simuData->_entityCount; ++iEntity)
            {
                int64_t entityId = simuData->_entityIds[iEntity];
                if (entityId < 0)
                {
                    // entity was probably killed
                    continue;
                }

                bool excludedEntity = frameData->_entityEnabled[iEntity] != 1;
                if (!excludedEntity)
                {
                    excludedEntity = iEntity >= maxEntities;
                    if (!excludedEntity)
                    {
                        size_t excludedEntityIdx;
                        excludedEntity = glm::glmFindIndex(excludedEntities.begin(), excludedEntities.end(), entityId, excludedEntityIdx);
                    }
                }

                if (excludedEntity)
                {
                    continue;
                }

                int32_t characterIdx = simuData->_characterIdx[iEntity];
                const glm::GolaemCharacter* character = _factory.getGolaemCharacter(characterIdx);
                if (character == NULL)
                {
                    GLM_CROWD_TRACE_ERROR_LIMIT("The entity '" << entityId << "' has an invalid character index: '" << characterIdx << "'. Skipping it. Please assign a Rendering Type from the Rendering Attributes panel");
                    continue;
                }
                int32_t renderingTypeIdx = simuData->_renderingTypeIdx[iEntity];
                const glm::RenderingType* renderingType = NULL;
                if (renderingTypeIdx >= 0 && renderingTypeIdx < character->_renderingTypes.sizeInt())
                {
                    renderingType = &character->_renderingTypes[renderingTypeIdx];
                }

                if (renderingType == NULL)
                {
                    GLM_CROWD_TRACE_WARNING_LIMIT("The entity '" << entityId << "', character '" << character->_name << "' has an invalid rendering type: '" << renderingTypeIdx << "'. Using default rendering type.");
                }

                // compute the bounding box of the current entity
                glm::Vector3 halfExtents(1, 1, 1);
                const glm::GeometryAsset* geoAsset = character->getGeometryAsset(geoTag, 0); // any LOD should have same extents !
                if (geoAsset != NULL)
                {
                    halfExtents = geoAsset->_halfExtentsYUp;
                }

                uint16_t entityType = simuData->_entityTypes[iEntity];

                uint16_t boneCount = simuData->_boneCount[entityType];
                uint32_t positionOffset = simuData->_iBoneOffsetPerEntityType[entityType] + simuData->_indexInEntityType[iEntity] * boneCount;

                float* rootPos = frameData->_bonePositions[positionOffset];
                float characterScale = simuData->_scales[iEntity];
                halfExtents *= characterScale;
                gdp->cube(
                    rootPos[0] - halfExtents[0], rootPos[0] + halfExtents[0],
                    rootPos[1] - halfExtents[1], rootPos[1] + halfExtents[1],
                    rootPos[2] - halfExtents[2], rootPos[2] + halfExtents[2],
                    0, 0, 0, 0, 1);
            }
        }
    }
    break;
    case GolaemDisplayMode::SKELETON:
    {
    }
    break;
    case GolaemDisplayMode::SKINMESH:
    {
        glm::PODArray<glm::crowdio::FurIds> furIds;
        glm::Array<glm::crowdio::FurCache::SP> furCache;

        GEO_PolyCounts polyCounts;
        UT_IntArray polygonpointnumbers;

        glm::Array<glm::GlmString> crowdFieldNames = glm::stringToStringArray(cfNames.c_str(), ";");
        for (size_t iCf = 0, cfCount = crowdFieldNames.size(); iCf < cfCount; ++iCf)
        {
            const glm::GlmString& cfName = crowdFieldNames[iCf];
            if (cfName.empty())
            {
                continue;
            }
            glm::crowdio::CachedSimulation& cachedSimulation = _factory.getCachedSimulation(cacheDir.c_str(), cacheName.c_str(), cfName.c_str());
            const glm::crowdio::GlmSimulationData* simuData = cachedSimulation.getFinalSimulationData();
            const glm::crowdio::GlmFrameData* frameData = cachedSimulation.getFinalFrameData(currentFrame, UINT32_MAX, true);

            if (simuData == NULL || frameData == NULL)
            {
                continue;
            }

            glm::PODArray<int64_t> excludedEntities;
			glm::Array<const glm::crowdio::glmHistoryRuntimeStructure*> historyStructures;
			cachedSimulation.getHistoryRuntimeStructures(historyStructures);
            glm::crowdio::createEntityExclusionList(excludedEntities, cachedSimulation.getSrcSimulationData(), _factory.getLayoutHistories(), historyStructures);
            size_t maxEntities = (size_t)floorf(simuData->_entityCount * renderPercent);
            for (uint32_t iEntity = 0; iEntity < simuData->_entityCount; ++iEntity)
            {
                int64_t entityId = simuData->_entityIds[iEntity];
                if (entityId < 0)
                {
                    // entity was probably killed
                    continue;
                }

                bool excludedEntity = frameData->_entityEnabled[iEntity] != 1;
                if (!excludedEntity)
                {
                    excludedEntity = iEntity >= maxEntities;
                    if (!excludedEntity)
                    {
                        size_t excludedEntityIdx;
                        excludedEntity = glm::glmFindIndex(excludedEntities.begin(), excludedEntities.end(), entityId, excludedEntityIdx);
                    }
                }

                if (excludedEntity)
                {
                    continue;
                }

                int32_t characterIdx = simuData->_characterIdx[iEntity];
                const glm::GolaemCharacter* character = _factory.getGolaemCharacter(characterIdx);
                if (character == NULL)
                {
                    GLM_CROWD_TRACE_ERROR_LIMIT("The entity '" << entityId << "' has an invalid character index: '" << characterIdx << "'. Skipping it. Please assign a Rendering Type from the Rendering Attributes panel");
                    continue;
                }

                uint16_t entityType = simuData->_entityTypes[iEntity];
                // check bone count between simulation & character
                if (simuData->_boneCount[entityType] != character->_converterMapping._skeletonDescription->getBones().size())
                {
                    GLM_CROWD_TRACE_ERROR_LIMIT("Bone count mismatch between Golaem Character(" << character->_name.c_str() << ") and simulation data.");
                    continue;
                }

                int32_t renderingTypeIdx = simuData->_renderingTypeIdx[iEntity];
                const glm::RenderingType* renderingType = NULL;
                if (renderingTypeIdx >= 0 && renderingTypeIdx < character->_renderingTypes.sizeInt())
                {
                    renderingType = &character->_renderingTypes[renderingTypeIdx];
                }

                if (renderingType == NULL)
                {
                    GLM_CROWD_TRACE_WARNING_LIMIT("The entity '" << entityId << "', character '" << character->_name << "' has an invalid rendering type: '" << renderingTypeIdx << "'. Using default rendering type.");
                }

                // compute assets if needed
                const glm::Array<glm::PODArray<int>>& entityAssets = cachedSimulation.getModifiedEntityAssets(_factory.getLayoutHistoryCount()==0 ? 0: _factory.getLayoutHistoryCount() - 1);

                glm::Array<glm::GlmString> meshAssetNames;
                glm::PODArray<int> furAssetIds;
                glm::PODArray<size_t> meshAssetNameIndices;
                glm::PODArray<int> meshAssetMaterialIndices, gchaMeshIds;

                glm::PODArray<uint32_t> gcgMeshIndices;

                if (!glm::crowdio::computeMeshNames(character, entityId, entityAssets[iEntity], meshAssetNames, furAssetIds, meshAssetNameIndices, meshAssetMaterialIndices, &gchaMeshIds))
                {
                    continue;
                }

                int geoDataIndex = simuData->_iGeoBehaviorOffsetPerEntityType[entityType] + simuData->_indexInEntityType[iEntity];
                int idGeometryFileIdx = frameData->_geoBehaviorGeometryIds[geoDataIndex];
                if (idGeometryFileIdx == UINT16_MAX)
                {
                    idGeometryFileIdx = -1;
                }
                bool blendingModeIdx = true;

                bool hasGeoBehavior = simuData->_hasGeoBehavior[entityType] != 0;
                if (hasGeoBehavior)
                {
                    blendingModeIdx = frameData->_geoBehaviorBlendModes[geoDataIndex] != 0;
                }

                glm::GlmString assetFile;
                glm::GlmString assetFileExtension;

                // get character file
                const glm::GeometryAsset* geoAsset = character->getGeometryAsset(geoTag, glm::max(0, idGeometryFileIdx), -1.f /*, 0.5f*/);
                if (geoAsset != NULL)
                {
                    assetFile = geoAsset->_filename;
                }
                // geometry file exists?
                if (assetFile.empty())
                {
                    GLM_CROWD_TRACE_ERROR("The Character '" << character->_name << "' has no Geometry File for Geometry Tag '" << geoTag << "'! Please add a Geometry File to its Character Node.");
                    continue;
                }

                glm::FileName assetFileName;
                assetFileName.set(assetFile);
                assetFileExtension = assetFileName.extension();
                assetFileExtension.toLowerSelf();

                glm::crowdio::CrowdFBXCharacter* fbxCharacter = NULL;
                glm::crowdio::CrowdGcgCharacter* gcgCharacter = NULL;
                glm::Array<glm::GlmString> dirMapRules;

                glm::PODArray<size_t> notFoundMeshNameIndices;
                if (assetFileExtension == "fbx")
                {
                    glm::crowdio::CrowdFBXStorage& fbxStorage = getFbxStorage();

                    // lock because FBX is not thread safe!
                    _glmCrowdGeoMutex.lock(); // can't use scoped lock, we need to keep it until the geometry loading, beware of return from there

                    // preload character if not already loaded
                    fbxStorage.preLoadCharacter(assetFile, character->_converterMapping._skeletonDescription->getRootBone()->getName());

                    // get the created character
                    bool newCharacter = false;
                    fbxCharacter = fbxStorage.getCharacter(assetFile, dirMapRules, newCharacter);
                    if (fbxCharacter == NULL)
                    {
                        GLM_CROWD_TRACE_ERROR("Failed to load the Geometry file '" << assetFile << "' from Character " << character->_name);
                        _glmCrowdGeoMutex.unlock();
                        continue;
                    }

                    // define which character meshes will be rendered
                    fbxCharacter->updateMeshes(meshAssetNames);

                    // get the character meshes
                    const glm::PODArray<FbxNode*>& fbxCharacterMeshes = fbxCharacter->getCharacterFBXMeshes();
                    bool meshFound = false;
                    for (size_t iMesh = 0; iMesh < fbxCharacterMeshes.size(); ++iMesh)
                    {
                        if (fbxCharacterMeshes[iMesh] != NULL)
                        {
                            meshFound = true;
                        }
                        else
                        {
                            if (glm::glmFind(furAssetIds.begin(), furAssetIds.end(), (int)iMesh) == furAssetIds.end())
                            {
                                GLM_CROWD_TRACE_WARNING_LIMIT("Mesh '" << meshAssetNames[iMesh] << "' was not found in Character Geometry file '" << assetFile << "' for Character " << character->_name);
                            }

                            notFoundMeshNameIndices.push_back(iMesh);
                        }
                    }
                    if (!meshFound)
                    {
                        GLM_CROWD_TRACE_ERROR("No Mesh found in Character Geometry file '" << assetFile << "' for Character " << character->_name);
                        _glmCrowdGeoMutex.unlock();
                        continue;
                    }

                    // avoids moving newCharacter to main glmComputeCharacterRenderDataFbx call
                    if (newCharacter)
                    {
                        fbxCharacter->updateSnSValueIndices(*character);
                    }
                }
                else if (assetFileExtension == "gcg")
                {
                    glm::crowdio::CrowdGcgStorage& gcgStorage = glm::crowdio::getDefaultGcgStorage();

                    {
                        // scope lock only while accessing the map, which is not threadsafe
                        // lock because loading files from disk is not useful to do in multithread (limiting factor = hardware) and storage base is not multithread
                        glm::ScopedLock<glm::Mutex> geoLock(_glmCrowdGeoMutex);

                        // preload character if not already loaded
                        gcgStorage.preLoadCharacter(assetFile, character->_converterMapping._skeletonDescription->getRootBone()->getName());

                        // get the created character
                        bool newCharacter = false;
                        gcgCharacter = gcgStorage.getCharacter(assetFile, dirMapRules, newCharacter);
                        if (gcgCharacter == NULL)
                        {
                            GLM_CROWD_TRACE_ERROR("Failed to load the Character Geometry File '" << assetFile << "' from Character " << character->_name);
                            continue;
                        }

                        if (newCharacter)
                        {
                            // update SnS in lock part, it modifies the character
                            gcgCharacter->updateSnSValueIndices(*character);

                            // handle multimat for cloth
                            //gcgCharacter->computeMultiMappedMeshIndexConversion();
                        }
                    }

                    // get meshes indices in input file, according to asset repartition (array of int indices in input gcg file)
                    glm::PODArray<size_t> notFoundNameMaterialPairs; // index of meshAssetNameIndices & materialIdxPerRenderMesh
                    if (gcgCharacter->getAssetFileMeshIndicesFromMeshAssociation(meshAssetNames, meshAssetNameIndices, meshAssetMaterialIndices, gcgMeshIndices, &notFoundNameMaterialPairs) == 0)
                    {
                        GLM_CROWD_TRACE_ERROR("No Mesh found in Character Geometry File '" << assetFile << "' for Character " << character->_name);
                        continue;
                    }

                    for (size_t iMesh = 0; iMesh < notFoundNameMaterialPairs.size(); ++iMesh)
                    {
                        // we can still deal with a missing mesh, just notify
                        //status = GIO_GCG_FILE_MESH_NOT_FOUND;
                        if (glm::glmFind(furAssetIds.begin(), furAssetIds.end(), (int)iMesh) == furAssetIds.end())
                        {
                            GLM_CROWD_TRACE_WARNING_LIMIT("Mesh '" << meshAssetNames[meshAssetNameIndices[notFoundNameMaterialPairs[iMesh]]] << "' was not found in Character Geometry File '" << assetFile << "' for Character " << character->_name);
                        }
                        notFoundMeshNameIndices.push_back(notFoundNameMaterialPairs[iMesh]);
                    }
                }
                else
                {
                    GLM_CROWD_TRACE_WARNING("The Character '" << character->_name << "' has an unknow Geometry File extension for '" << assetFile << "'. It should be fbx or gcg.");
                    continue;
                }

                // update used meshName-material pairs, according to not found ones, as they are traced now
                for (size_t iMesh = notFoundMeshNameIndices.size(); iMesh > 0; iMesh--)
                {
                    meshAssetNameIndices.erase(meshAssetNameIndices.begin() + notFoundMeshNameIndices[iMesh - 1]);
                    meshAssetMaterialIndices.erase(meshAssetMaterialIndices.begin() + notFoundMeshNameIndices[iMesh - 1]);
                }

                // TODO: see if fur is needed
                size_t meshCount = meshAssetNameIndices.size();
                if (assetFileExtension == "fbx")
                {
                    // glmComputeCharacterRenderDataFbx

                    FbxAMatrix identityMatrix;
                    identityMatrix.SetIdentity();
                    FbxNode* node(NULL);
                    FbxAMatrix nodeTransform;
                    FbxAMatrix geomTransform;
                    bool hasTransform(false);
                    FbxMesh* mesh(NULL);
                    //FbxLayer* layer(NULL);
                    //FbxLayerElementUV* uvElement(NULL);
                    FbxLayerElementMaterial* materialElement(NULL);
                    FbxVector4* meshVertices(NULL);
                    FbxVector4 meshVertex;
                    //FbxVector4 tempNormal;
                    //FbxLayerElementTangent* tangentsElement = NULL;
                    //FbxLayerElementBinormal* binormalsElement = NULL;

                    glm::Array<glm::Array<glm::Vector3>> deformedFurVertices; // curve points per fur asset

                    fbxCharacter->updateBlendShapes(character->_converterMapping.getBlindDataInfo());

                    bool hasMaterials = false;

                    // Extract scale
                    float scale = simuData->_scales[iEntity];
                    const FbxVector4 characterScale(scale, scale, scale);

                    // Extract frame
                    FbxTime fbxTime;
                    // glm::Time glmTimeSec;
                    if (idGeometryFileIdx != -1)
                    {
                        float(&geometryFrameCacheData)[3] = frameData->_geoBehaviorAnimFrameInfo[geoDataIndex];
                        double frameRate(FbxTime::GetFrameRate(fbxCharacter->touchFBXScene()->GetGlobalSettings().GetTimeMode()));
                        fbxTime.SetGlobalTimeMode(FbxTime::eCustom, frameRate);
                        fbxTime.SetMilliSeconds(long((double)geometryFrameCacheData[0] / frameRate * 1000.0));

                        // glmTimeSec = (double)geometryFrameCacheData[0];
                    }
                    else
                    {
                        // glmTimeSec = 0;
                        fbxTime = 0;
                    }

                    ////////////////////////////////////////
                    // 1 - Process character
                    ////////////////////////////////////////

                    // apply current scale
                    glm::crowdio::CrowdFBXBaker::applyScale(*fbxCharacter, characterScale, false, idGeometryFileIdx != -1, blendingModeIdx, fbxTime);

                    glm::crowdio::CrowdFBXBaker& fbxBaker = getFbxBaker();

                    // apply the posture to the created character
                    fbxBaker.processCharacter(
                        iEntity,
                        *fbxCharacter,
                        scale,
                        *simuData,
                        *frameData,
                        glm::crowdio::CrowdFBXBaker::ProcessMode::DEFORM,
                        gchaMeshIds,
                        deformedFurVertices,
                        NULL,
                        furIds,
                        furCache,
                        idGeometryFileIdx != -1,
                        blendingModeIdx,
                        fbxTime,
                        fbxTime);

                    const glm::PODArray<FbxNode*>& fbxCharacterMeshes = fbxCharacter->getCharacterFBXMeshes();
                    for (size_t iMesh = 0; iMesh < meshCount; ++iMesh)
                    {
                        node = fbxCharacterMeshes[meshAssetNameIndices[iMesh]];
                        if (node == NULL)
                        {
                            continue;
                        }

                        fbxCharacter->getMeshGlobalTransform(nodeTransform, node, fbxTime);
                        glm::crowdio::CrowdFBXBaker::getGeomTransform(geomTransform, node);
                        nodeTransform *= geomTransform;

                        mesh = fbxCharacter->getCharacterFBXMesh(iMesh);

                        hasTransform = !(nodeTransform == identityMatrix);

                        glm::PODArray<int> vertexMasks;
                        glm::PODArray<int> polygonMasks;

                        meshVertices = mesh->GetControlPoints();

                        unsigned int fbxVertexCount = mesh->GetControlPointsCount();
                        vertexMasks.assign(fbxVertexCount, -1);

                        unsigned int fbxPolyCount = mesh->GetPolygonCount();
                        polygonMasks.assign(fbxPolyCount, 0);

                        unsigned int meshMtlIdx = meshAssetMaterialIndices[iMesh];

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
                                for (int iPolyVertex = 0, polyVertexCount = mesh->GetPolygonSize(iFbxPoly); iPolyVertex < polyVertexCount; ++iPolyVertex)
                                {
                                    int vertexId = mesh->GetPolygonVertex(iFbxPoly, iPolyVertex);
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
                            if (vertexMasks[iFbxVertex] >= 0)
                            {
                                ++iActualVertex;
                            }
                        }
                        size_t vertexCount = iActualVertex;

                        GA_Offset pointStartOffset = gdp->appendPointBlock(vertexCount);

                        iActualVertex = 0;
                        for (unsigned int iFbxVertex = 0; iFbxVertex < fbxVertexCount; ++iFbxVertex)
                        {
                            int& vertexMask = vertexMasks[iFbxVertex];
                            if (vertexMask >= 0)
                            {
                                // vertices
                                if (hasTransform)
                                {
                                    // transform vertex in case of local transformation
                                    meshVertex = nodeTransform.MultT(meshVertices[iFbxVertex]);
                                }
                                else
                                {
                                    meshVertex = meshVertices[iFbxVertex];
                                }
                                vertexMask = iActualVertex;

                                gdp->setPos3(pointStartOffset + iActualVertex,
                                             UT_Vector3(
                                                 (float)meshVertex[0],
                                                 (float)meshVertex[1],
                                                 (float)meshVertex[2]));

                                ++iActualVertex;
                            }
                        }

                        ////////////////////////////////////////
                        // 5 get faces
                        ////////////////////////////////////////

                        polyCounts.clear();
                        polygonpointnumbers.clear();

                        for (unsigned int iFbxPoly = 0; iFbxPoly < fbxPolyCount; ++iFbxPoly)
                        {
                            if (polygonMasks[iFbxPoly])
                            {
                                int polySize = mesh->GetPolygonSize(iFbxPoly);

                                polyCounts.append(polySize);
                                for (int iPolyVertex = 0; iPolyVertex < polySize; ++iPolyVertex)
                                {
                                    polygonpointnumbers.append(vertexMasks[mesh->GetPolygonVertex(iFbxPoly, iPolyVertex)]);
                                } // iPolyVertex
                            }
                        }

                        GEO_PrimPoly::buildBlock(gdp, pointStartOffset, vertexCount, polyCounts, polygonpointnumbers.array());

                        gdp->bumpDataIdsForAddOrRemove(true, true, true);
                    }

                    // reset character to default pose
                    glm::crowdio::CrowdFBXBaker::resetToBindPose(*fbxCharacter);

                    _glmCrowdGeoMutex.unlock();
                }
                else if (assetFileExtension == "gcg")
                {
                    glm::Array<glm::Array<glm::Vector3>> deformedVertices;
                    glm::Array<glm::Array<glm::Vector3>> deformedNormals;

                    glm::Array<glm::Array<glm::Vector3>> deformedFurVertices;

                    float(*geometryFrameCacheDataPtr)[3] = NULL;
                    if (idGeometryFileIdx != -1)
                    {
                        // geo info contains current frame, start frame and stop frame ("in timeline", not direct indices to vertex cache array).
                        float(&geometryFrameCacheData)[3] = frameData->_geoBehaviorAnimFrameInfo[geoDataIndex];
                        geometryFrameCacheDataPtr = &geometryFrameCacheData;

                        time = (double)geometryFrameCacheData[0]; // store rounded current frame in "time" value, still need to convert it to valid time according to mesh cache range.
                    }
                    else
                    {
                        time = 0;
                    }

                    float characterScale = simuData->_scales[iEntity];

                    glm::crowdio::CrowdGcgBaker& gcgBaker = glm::crowdio::getDefaultGcgBaker();
                    gcgBaker.processCharacter(
                        iEntity,
                        *character,
                        *gcgCharacter,
                        characterScale,
                        *simuData,
                        *frameData,
                        gchaMeshIds,
                        gcgMeshIndices,
                        deformedVertices,
                        deformedNormals,
                        deformedFurVertices,
                        furIds,
                        furCache,
                        idGeometryFileIdx != -1,
                        time,
                        geometryFrameCacheDataPtr);

                    for (size_t iMesh = 0; iMesh < meshCount; ++iMesh)
                    {

                        if (deformedVertices[iMesh].empty() || deformedNormals[iMesh].empty())
                        {
                            continue;
                        }

                        if (gcgMeshIndices[iMesh] == static_cast<uint32_t>(-1)) // skip mesh, not found in asset file
                        {
                            continue;
                        }

                        // need to duplicate from asset file and copy vertices position / normals :
                        // the meshes used by this entity assets are tagged in meshAssetNameIndices, and reference the names. there is as much GlmFileMesh as names, and as mush renderGeometry->_meshCount as "meshAssetNameIndices", get the proper GlmFileMesh :
                        glm::crowdio::GlmFileMesh& assetFileMesh = gcgCharacter->getGeometry()._meshes[gcgMeshIndices[iMesh]];

                        glm::Array<glm::Vector3>& deformedMeshVertices = deformedVertices[iMesh]; // should match vertexCount

                        size_t vtxCount = deformedMeshVertices.size();
                        GA_Offset pointStartOffset = gdp->appendPointBlock(vtxCount);

                        for (size_t iVertex = 0; iVertex < vtxCount; ++iVertex)
                        {
                            const glm::Vector3& meshVertex = deformedMeshVertices[iVertex];
                            gdp->setPos3(pointStartOffset + iVertex,
                                         UT_Vector3(
                                             meshVertex[0],
                                             meshVertex[1],
                                             meshVertex[2]));
                        }

                        polyCounts.clear();
                        polygonpointnumbers.clear();
                        for (uint32_t iPoly = 0, iVertex = 0; iPoly < assetFileMesh._polygonCount; ++iPoly)
                        {
                            uint32_t polySize = assetFileMesh._polygonsVertexCount[iPoly];
                            polyCounts.append(polySize);
                            for (uint32_t iPolyVtx = 0; iPolyVtx < polySize; ++iPolyVtx, ++iVertex)
                            {
                                polygonpointnumbers.append(assetFileMesh._polygonsVertexIndices[iVertex]);
                            }
                        }

                        GEO_PrimPoly::buildBlock(gdp, pointStartOffset, vtxCount, polyCounts, polygonpointnumbers.array());

                        gdp->bumpDataIdsForAddOrRemove(true, true, true);
                    }
                }
            }
        }
    }
    break;
    default:
        break;
    }

    // free anything not reused
    gdp->destroyStashed();

    return error();
}
