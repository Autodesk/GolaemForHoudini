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
#include <CH/CH_Manager.h>
#include <UT/UT_Exit.h>
#include <UT/UT_DirUtil.h>
#include <PY/PY_Python.h>
#include <HOM/HOM_Module.h>
#include <HOM/HOM_shelves.h>
#include <UT/UT_VarEncode.h>
#include <GU/GU_PackedFactory.h>
#include <GU/GU_PrimPacked.h>

#include <GEO/GEO_PolyCounts.h> // TODO: cleanup
#include <GEO/GEO_PrimPoly.h>   // TODO: cleanup

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

#include "glmCrowdHoudiniPluginAPI.h"

#include "GU_PackedGolaemEntity.h"

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
        LAYOUT_FILES,
        LAYOUT_FILE,
        OPEN_LAYOUT,
        CURRENT_FRAME,
        START_FRAME,
        END_FRAME,
        ENTITY_COUNT,
        DRAW_PERCENT,
        DISPLAY_MODE,
        GEO_TAG,
        MATERIAL_PATH,
        MATERIAL_ASSIGN_MODE,
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
    case GolaemParams::LAYOUT_FILES:
        return "glmLayoutFiles";
        break;
    case GolaemParams::LAYOUT_FILE:
        return "glmLayoutFile#";
        break;
    case GolaemParams::OPEN_LAYOUT:
        return "glmOpenLayout#";
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
    case GolaemParams::MATERIAL_PATH:
        return "glmMaterialPath";
        break;
    case GolaemParams::MATERIAL_ASSIGN_MODE:
        return "glmMAterialAssignMode";
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
    bool _clearBakedGeo;

    glm::crowdio::SimulationCacheFactory _factory;

    // for geometry generation
    glm::crowdio::InputEntityGeoData _inputData;
    glm::crowdio::OutputEntityGeoData _outputData;

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
        bool updateCharacterFiles = true,
        bool updateDisplayMode = true);

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
    static PRM_Name layoutFilesPrm(getParamName(GolaemParams::LAYOUT_FILES), "Layout Files");
    static PRM_Name layoutFilePrm(getParamName(GolaemParams::LAYOUT_FILE), "Layout File #");
    static PRM_SpareData glmLayoutFileOptions(
        PRM_SpareArgs() << PRM_SpareToken(PRM_SpareData::getFileChooserPatternToken(), "*.gscl"));
    static PRM_Name openLayoutPrm(getParamName(GolaemParams::OPEN_LAYOUT), "Open");

    static PRM_Template layoutFilesTemplate[] =
        {
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
            PRM_Template() // sentinel
        };

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

    static PRM_Name materialPathPrm(getParamName(GolaemParams::MATERIAL_PATH), "Material Path");
    static PRM_Default materialPathDefault(0, "/mat");

    static PRM_Name materialAssignModePrm(getParamName(GolaemParams::MATERIAL_ASSIGN_MODE), "Material Assign Mode");
    static PRM_Name materialAssignModeEnum[] =
        {
            PRM_Name("surfSh", "By Surface Shader"),
            PRM_Name("shGroup", "By Shading Group"),
            PRM_Name(0) // Need a null terminator
        };

    static PRM_ChoiceList materialAssignModeChoice((PRM_ChoiceListType)PRM_CHOICELIST_SINGLE, materialAssignModeEnum);

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
                PRM_TOGGLE,
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
                PRM_MULTITYPE_LIST,
                layoutFilesTemplate,
                0,
                &layoutFilesPrm),
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
            PRM_Template(
                PRM_STRING,
                1,
                &materialPathPrm,
                &materialPathDefault,
                0,
                0,
                &SOP_GolaemCacheProxy::onParamChanged,
                0,
                1,
                "Material Path"),
            PRM_Template(
                PRM_ORD,
                1,
                &materialAssignModePrm,
                &defaultParam,
                &materialAssignModeChoice,
                0,
                &SOP_GolaemCacheProxy::onParamChanged,
                0,
                1,
                "Material Assignment Mode"),
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
    changed |= enableParm(getParamName(GolaemParams::LAYOUT_FILES), layoutEnabled);
    changed |= enableParm(getParamName(GolaemParams::CURRENT_FRAME), 1);
    changed |= enableParm(getParamName(GolaemParams::START_FRAME), 0);
    changed |= enableParm(getParamName(GolaemParams::END_FRAME), 0);
    changed |= enableParm(getParamName(GolaemParams::ENTITY_COUNT), 0);
    changed |= enableParm(getParamName(GolaemParams::DRAW_PERCENT), 1);
    changed |= enableParm(getParamName(GolaemParams::DISPLAY_MODE), 1);
    glm::GolaemDisplayMode::Value displayMode = (glm::GolaemDisplayMode::Value)evalInt(getParamName(GolaemParams::DISPLAY_MODE), 0, time);
    changed |= enableParm(getParamName(GolaemParams::GEO_TAG), displayMode == glm::GolaemDisplayMode::SKINMESH);
    changed |= enableParm(getParamName(GolaemParams::MATERIAL_PATH), displayMode == glm::GolaemDisplayMode::SKINMESH);
    changed |= enableParm(getParamName(GolaemParams::MATERIAL_ASSIGN_MODE), displayMode == glm::GolaemDisplayMode::SKINMESH);
    //PRM_Parm* parm = getParmPtr("glmCacheIdx");
    return changed;
}

//-----------------------------------------------------------------------------
void SOP_GolaemCacheProxy::refreshParameters(
    fpreal time,
    bool updateCache,
    bool updateLayout,
    bool updateTerrain,
    bool updateCharacterFiles,
    bool updateDisplayMode)
{
    if (_needsRefresh)
    {
        // force refresh after loading the node
        updateCache = true;
        updateLayout = true;
        updateTerrain = true;
        updateCharacterFiles = true;
        updateDisplayMode = true;
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
        int layoutCount = (int)evalInt(getParamName(GolaemParams::LAYOUT_FILES), 0, time);
        if (enableLayout && layoutCount > 0)
        {
            int layoutStartIdx = getParm(getParamName(GolaemParams::LAYOUT_FILES)).getMultiStartOffset();
            UT_String layoutFile;
            for (int iLayout = 0; iLayout < layoutCount; ++iLayout)
            {
                int layoutIdx = layoutStartIdx + iLayout;
                evalStringInst(getParamName(GolaemParams::LAYOUT_FILE), &layoutIdx, layoutFile, 0, time);
                if (layoutFile.length() > 0)
                {
                    _factory.loadLayoutHistoryFile(_factory.getLayoutHistoryCount(), layoutFile.c_str());
                }
            }
        }
    }
    if (updateCache || updateCharacterFiles || updateLayout || updateDisplayMode)
    {
        _clearBakedGeo = true;
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
        if (destTerrain == NULL)
        {
            destTerrain = sourceTerrain;
        }
        _factory.setTerrainMeshes(sourceTerrain, destTerrain);
    }

    float currentFrame = static_cast<float>(evalFloat(getParamName(GolaemParams::CURRENT_FRAME), 0, time));

    int startFrame = 0;
    int endFrame = 0;
    bool framesFound = false;
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
    glm::GlmString layoutFiles;

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
        layoutFiles = cacheInfo->_layoutFile;
        layoutFiles.trim(";");
    }
    setString(cfNames.c_str(), CH_STRING_LITERAL, getParamName(GolaemParams::CROWDFIELD_NAMES), 0, time);
    setString(cacheName.c_str(), CH_STRING_LITERAL, getParamName(GolaemParams::CACHE_NAME), 0, time);
    setString(cacheDir.c_str(), CH_STRING_LITERAL, getParamName(GolaemParams::CACHE_DIR), 0, time);
    setString(characterFiles.c_str(), CH_STRING_LITERAL, getParamName(GolaemParams::CHARACTER_FILES), 0, time);
    setString(srcTerrain.c_str(), CH_STRING_LITERAL, getParamName(GolaemParams::SOURCE_TERRAIN), 0, time);
    setString(dstTerrain.c_str(), CH_STRING_LITERAL, getParamName(GolaemParams::DEST_TERRAIN), 0, time);
    setInt(getParamName(GolaemParams::ENABLE_LAYOUT), 0, time, enableLayout ? 1 : 0);
    int layoutStartIdx = getParm(getParamName(GolaemParams::LAYOUT_FILES)).getMultiStartOffset();
    glm::Array<glm::GlmString> splitLayoutFiles;
    glm::split(layoutFiles, ";", splitLayoutFiles);
    // set the number of layout files
    setInt(getParamName(GolaemParams::LAYOUT_FILES), 0, time, splitLayoutFiles.size());
    for (size_t iLayout = 0, layoutCount = splitLayoutFiles.size(); iLayout < layoutCount; ++iLayout)
    {
        const glm::GlmString& layoutFile = splitLayoutFiles[iLayout];
        int layoutIdx = layoutStartIdx + (int)iLayout;
        setStringInst(layoutFile.c_str(), CH_STRING_LITERAL, getParamName(GolaemParams::LAYOUT_FILE), &layoutIdx, 0, time);
    }
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
    bool updateDisplayMode = false;
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
    glm::GlmString layoutFileTokenPrefix = getParamName(GolaemParams::LAYOUT_FILE);
    layoutFileTokenPrefix.rtrim("#"); // remove the # character
    if (paramToken == getParamName(GolaemParams::ENABLE_LAYOUT) || paramToken == getParamName(GolaemParams::LAYOUT_FILE) || paramToken.startsWith(layoutFileTokenPrefix.c_str()))
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
    if (paramToken == getParamName(GolaemParams::DISPLAY_MODE))
    {
        updateDisplayMode = true;
    }
    sop->refreshParameters(time, updateCache, updateLayout, updateTerrain, updateCharacterFiles, updateDisplayMode);
    return 1;
}

//-----------------------------------------------------------------------------
int SOP_GolaemCacheProxy::onOpenLayoutEditor(void* data, int /*index*/, fpreal time, const PRM_Template* tplate)
{
    SOP_GolaemCacheProxy* sop = static_cast<SOP_GolaemCacheProxy*>(data);

    const UT_StringRef& paramToken = tplate->getNamePtr()->getTokenRef();
    glm::GlmString openLayoutTokenPrefix = getParamName(GolaemParams::OPEN_LAYOUT);
    openLayoutTokenPrefix.rtrim("#"); // remove the # character
    if (paramToken.startsWith(openLayoutTokenPrefix.c_str()))
    {
        // get the index
        int layoutIdx = 0;
        glm::GlmString indexStr = glm::GlmString(paramToken.c_str()).replace(0, openLayoutTokenPrefix.size(), "");
        glm::fromString(indexStr, layoutIdx);

        UT_String layoutFile;
        sop->evalStringInst(getParamName(GolaemParams::LAYOUT_FILE), &layoutIdx, layoutFile, 0, time);
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
    , _clearBakedGeo(false)
{
    //mySopFlags.setManagesDataIDs(true);
    _inputData._enableLOD = false;
    _inputData._entityPos = NULL;                // not used when enableLOD is false
    _inputData._cameraWorldPosition = NULL;      // not used when enableLOD is false
    _inputData._maxVerticesPerFace = UINT32_MAX; // for fbx assets
    _inputData._generateFur = false;             // do not generate fur for now
    _inputData._frameDatas.resize(1, NULL);
    _inputData._frames.resize(1);
    _inputData._fbxStorage = &getFbxStorage();
    _inputData._fbxBaker = &getFbxBaker();
    //_inputData._dirMapRules // left empty for now
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

/// Register new geometry primitive
//-----------------------------------------------------------------------------
void GLM_CROWDHOUDINI_API newGeometryPrim(GA_PrimitiveFactory* factory)
{
    glm::GU_PackedGolaemEntity::install(factory);
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

    glm::GolaemDisplayMode::Value displayMode = (glm::GolaemDisplayMode::Value)evalInt(getParamName(GolaemParams::DISPLAY_MODE), 0, time);
    if (isRender)
    {
        if (displayMode != glm::GolaemDisplayMode::SKINMESH)
        {
            _clearBakedGeo = true;
        }
        // always render skinmeshes
        displayMode = glm::GolaemDisplayMode::SKINMESH;
    }

    if (_clearBakedGeo)
    {
        // clean all baked entities
        gdp->clearAndDestroy();
        _clearBakedGeo = false;
    }

    float currentFrame = static_cast<float>(evalFloat(getParamName(GolaemParams::CURRENT_FRAME), 0, time));
    float renderPercent = static_cast<float>(evalFloat(getParamName(GolaemParams::DRAW_PERCENT), 0, time)) * 0.01f;
    short geoTag = static_cast<short>(evalInt(getParamName(GolaemParams::GEO_TAG), 0, time));

    int64_t entityCount = 0;

    switch (displayMode)
    {
    case glm::GolaemDisplayMode::BOUNDING_BOX:
    {
        GA_Size primitiveIndex = 0;
        GA_Size primCount = gdp->getNumPrimitives();

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
            for (uint32_t iEntity = 0; iEntity < simuData->_entityCount; ++iEntity, ++primitiveIndex)
            {
                glm::GU_PackedGolaemEntity* packedEntity = NULL;

                GA_Primitive* prim = NULL;
                if (primitiveIndex < primCount)
                {
                    prim = gdp->getPrimitiveByIndex(primitiveIndex);
                }
                if (prim != NULL && prim->getTypeId() == glm::GU_PackedGolaemEntity::getTypeId())
                {
                    packedEntity = static_cast<glm::GU_PackedGolaemEntity*>((static_cast<GU_PrimPacked*>(prim))->implementation());
                }
                else
                {
                    packedEntity = glm::GU_PackedGolaemEntity::build(gdp);
                }

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

                ++entityCount;

                uint16_t entityType = simuData->_entityTypes[iEntity];

                uint16_t boneCount = simuData->_boneCount[entityType];
                uint32_t positionOffset = simuData->_iBoneOffsetPerEntityType[entityType] + simuData->_indexInEntityType[iEntity] * boneCount;

                float* rootPos = frameData->_bonePositions[positionOffset];
                float characterScale = simuData->_scales[iEntity];
                halfExtents *= characterScale;

                packedEntity->_rootPos.setValues(rootPos);
                packedEntity->_halfExtents = halfExtents;
                packedEntity->_entityId = entityId;
                packedEntity->_updateGeo = true;
                packedEntity->_displayMode = displayMode;
            }
        }
    }
    break;
    case glm::GolaemDisplayMode::SKELETON:
    {
        GEO_PolyCounts polyCounts;
        UT_IntArray polygonpointnumbers;

        glm::Array<glm::PODArray<size_t>> sortedBonesInversePerChar(_factory.getGolaemCharacters().size());
        for (int iChar = 0, charCount = _factory.getGolaemCharacters().sizeInt(); iChar < charCount; ++iChar)
        {
            glm::PODArray<size_t>& sortedBonesInverse = sortedBonesInversePerChar[iChar];
            const glm::GolaemCharacter* character = _factory.getGolaemCharacter(iChar);
            if (character == NULL)
            {
                continue;
            }
            const glm::PODArray<size_t>& sortedBones = character->_converterMapping._skeletonDescription->getSortedBones();
            sortedBonesInverse.resize(sortedBones.size());
            for (size_t iBone = 0, boneCount = sortedBones.size(); iBone < boneCount; ++iBone)
            {
                sortedBonesInverse[sortedBones[iBone]] = iBone;
            }
        }
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

                ++entityCount;

                glm::PODArray<size_t>& sortedBonesInverse = sortedBonesInversePerChar[characterIdx];

                uint16_t entityType = simuData->_entityTypes[iEntity];

                uint16_t boneCount = simuData->_boneCount[entityType];

                uint32_t positionOffset = simuData->_iBoneOffsetPerEntityType[entityType] + simuData->_indexInEntityType[iEntity] * boneCount;

                // set the bone positions
                GA_Offset pointStartOffset = gdp->appendPointBlock(boneCount);
                for (uint16_t iBone = 0; iBone < boneCount; ++iBone)
                {
                    float* bonePos = frameData->_bonePositions[positionOffset + iBone];
                    gdp->setPos3(pointStartOffset + iBone,
                                 UT_Vector3(
                                     bonePos[0],
                                     bonePos[1],
                                     bonePos[2]));
                }

                const glm::PODArray<glm::HierarchicalBone*>& hBones = character->_converterMapping._skeletonDescription->getBones();
                const glm::PODArray<size_t>& sortedBones = character->_converterMapping._skeletonDescription->getSortedBones();

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

                    GA_Offset primOffset = GEO_PrimPoly::buildBlock(gdp, pointStartOffset, 2, polyCounts, polygonpointnumbers.array(), false);

                    GA_Attribute* entityIdAttr = gdp->addTuple(GA_STORE_INT64, GA_ATTRIB_PRIMITIVE, "glmEntityId", 1);
                    GA_RWHandleID entityIdAttrHandle(entityIdAttr);
                    entityIdAttrHandle.set(primOffset, entityId);
                }
            }
        }
    }
    break;
    case glm::GolaemDisplayMode::SKINMESH:
    {
        UT_String materialPath;
        evalString(materialPath, getParamName(GolaemParams::MATERIAL_PATH), 0, time);

        GolaemMaterialAssignMode::Value materialAssignMode = (GolaemMaterialAssignMode::Value)evalInt(getParamName(GolaemParams::MATERIAL_ASSIGN_MODE), 0, time);

        glm::Array<const glm::crowdio::glmHistoryRuntimeStructure*> historyStructures;

        _inputData._frames[0] = (double)currentFrame;
        _inputData._geometryTag = geoTag;

        GEO_PolyCounts polyCounts;
        UT_IntArray polygonpointnumbers;

        glm::GlmString meshAttrName = "glmMeshName";

        // shading group to surface shader map
        glm::Array<glm::PODArray<int>> sgToSsPerChar(_factory.getGolaemCharacters().size());
        for (int iChar = 0, charCount = _factory.getGolaemCharacters().sizeInt(); iChar < charCount; ++iChar)
        {
            const glm::GolaemCharacter* character = _factory.getGolaemCharacter(iChar);
            if (character == NULL)
            {
                continue;
            }
            glm::PODArray<int>& shadingGroupToSurfaceShader = sgToSsPerChar[iChar];
            shadingGroupToSurfaceShader.resize(character->_shadingGroups.size(), -1);
            for (size_t iSg = 0, sgCount = character->_shadingGroups.size(); iSg < sgCount; ++iSg)
            {
                const glm::ShadingGroup& shadingGroup = character->_shadingGroups[iSg];
                for (size_t iSa = 0, saCount = shadingGroup._shaderAssets.size(); iSa < saCount; ++iSa)
                {
                    int shaderAssetIdx = shadingGroup._shaderAssets[iSa];
                    const glm::ShaderAsset& shaderAsset = character->_shaderAssets[shaderAssetIdx];
                    if (shaderAsset._category.find("surface") != glm::GlmString::npos)
                    {
                        shadingGroupToSurfaceShader[iSg] = shaderAssetIdx;
                        break;
                    }
                }
            }
        }

        glm::Array<glm::GlmString> crowdFieldNames = glm::stringToStringArray(cfNames.c_str(), ";");
        for (size_t iCf = 0, cfCount = crowdFieldNames.size(); iCf < cfCount; ++iCf)
        {
            const glm::GlmString& cfName = crowdFieldNames[iCf];
            if (cfName.empty())
            {
                continue;
            }
            _inputData._cachedSimulation = &_factory.getCachedSimulation(cacheDir.c_str(), cacheName.c_str(), cfName.c_str());
            const glm::crowdio::GlmSimulationData* simuData = _inputData._cachedSimulation->getFinalSimulationData();
            const glm::crowdio::GlmFrameData* frameData = _inputData._cachedSimulation->getFinalFrameData(currentFrame, UINT32_MAX, true);

            if (simuData == NULL || frameData == NULL)
            {
                continue;
            }

            _inputData._frameDatas[0] = frameData;

            glm::PODArray<int64_t> excludedEntities;
            _inputData._cachedSimulation->getHistoryRuntimeStructures(historyStructures);
            glm::crowdio::createEntityExclusionList(excludedEntities, _inputData._cachedSimulation->getSrcSimulationData(), _factory.getLayoutHistories(), historyStructures);

            const glm::ShaderAssetDataContainer* shaderDataContainer = _inputData._cachedSimulation->getFinalShaderData(currentFrame, UINT32_MAX, true);

            size_t maxEntities = (size_t)floorf(simuData->_entityCount * renderPercent);
            for (_inputData._entityIndex = 0; _inputData._entityIndex < simuData->_entityCount; ++_inputData._entityIndex)
            {
                _inputData._entityId = simuData->_entityIds[_inputData._entityIndex];
                if (_inputData._entityId < 0)
                {
                    // entity was probably killed
                    continue;
                }

                bool excludedEntity = frameData->_entityEnabled[_inputData._entityIndex] != 1;
                if (!excludedEntity)
                {
                    excludedEntity = _inputData._entityIndex >= maxEntities;
                    if (!excludedEntity)
                    {
                        size_t excludedEntityIdx;
                        excludedEntity = glm::glmFindIndex(excludedEntities.begin(), excludedEntities.end(), _inputData._entityId, excludedEntityIdx);
                    }
                }

                if (excludedEntity)
                {
                    continue;
                }

                int32_t characterIdx = simuData->_characterIdx[_inputData._entityIndex];
                const glm::GolaemCharacter* character = _factory.getGolaemCharacter(characterIdx);
                if (character == NULL)
                {
                    GLM_CROWD_TRACE_ERROR_LIMIT("The entity '" << _inputData._entityId << "' has an invalid character index: '" << characterIdx << "'. Skipping it. Please assign a Rendering Type from the Rendering Attributes panel");
                    continue;
                }

                uint16_t entityType = simuData->_entityTypes[_inputData._entityIndex];
                // check bone count between simulation & character
                if (simuData->_boneCount[entityType] != character->_converterMapping._skeletonDescription->getBones().size())
                {
                    GLM_CROWD_TRACE_ERROR_LIMIT("Bone count mismatch between Golaem Character(" << character->_name.c_str() << ") and simulation data.");
                    continue;
                }

                int32_t renderingTypeIdx = simuData->_renderingTypeIdx[_inputData._entityIndex];
                const glm::RenderingType* renderingType = NULL;
                if (renderingTypeIdx >= 0 && renderingTypeIdx < character->_renderingTypes.sizeInt())
                {
                    renderingType = &character->_renderingTypes[renderingTypeIdx];
                }

                if (renderingType == NULL)
                {
                    GLM_CROWD_TRACE_WARNING_LIMIT("The entity '" << _inputData._entityId << "', character '" << character->_name << "' has an invalid rendering type: '" << renderingTypeIdx << "'. Using default rendering type.");
                }

                glm::crowdio::GlmGeometryGenerationStatus status = glm::crowdio::glmPrepareEntityGeometry(&_inputData, &_outputData);
                if (status != glm::crowdio::GIO_SUCCESS)
                {
                    continue;
                }

                ++entityCount;

                // compute shaders
                const glm::Array<glm::GlmString>& shaderData = shaderDataContainer->data[_inputData._entityIndex];

                glm::GlmMap<size_t, size_t> globalToIntShaderAttrIdx;
                glm::GlmMap<size_t, size_t> globalToFloatShaderAttrIdx;
                glm::GlmMap<size_t, size_t> globalToStringShaderAttrIdx;
                glm::GlmMap<size_t, size_t> globalToVectorShaderAttrIdx;

                glm::PODArray<int> intAttrValues;
                glm::PODArray<float> floatAttrValues;
                glm::Array<glm::GlmString> stringAttrValues;
                glm::Array<glm::Vector3> vectorAttrValues;

                glm::PODArray<int>& shadingGroupToSurfaceShader = sgToSsPerChar[characterIdx];

                for (size_t iShaderAttr = 0, shaderAttrCount = character->_shaderAttributes.size(); iShaderAttr < shaderAttrCount; iShaderAttr++)
                {
                    const glm::GlmString& attrValueStr = shaderData[iShaderAttr];
                    const glm::ShaderAttribute& shaderAttr = character->_shaderAttributes[iShaderAttr];
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

                size_t meshCount = _outputData._meshAssetNameIndices.size();

                glm::PODArray<int> meshShadingGroups(meshCount, -1);

                for (size_t iMesh = 0; iMesh < meshCount; ++iMesh)
                {
                    const glm::GlmString& meshName = _outputData._meshAssetNames[_outputData._meshAssetNameIndices[iMesh]];
                    int& shadingGroupIdx = meshShadingGroups[iMesh];

                    // find shader assets
                    unsigned int iMaterial = _outputData._meshAssetMaterialIndices[iMesh];
                    int meshAssetIdx = character->findMeshAssetIdx(meshName);
                    if (meshAssetIdx != -1)
                    {
                        const glm::MeshAsset& meshAsset = character->_meshAssets[meshAssetIdx];
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

                glm::Array<glm::Array<glm::Vector3>>& frameDeformedVertices = _outputData._deformedVertices[0];
                glm::Array<glm::Array<glm::Vector3>>& frameDeformedNormals = _outputData._deformedNormals[0];

                for (size_t iMesh = 0; iMesh < meshCount; ++iMesh)
                {
                    const glm::Array<glm::Vector3>& meshDeformedVertices = frameDeformedVertices[iMesh];
                    const glm::Array<glm::Vector3>& meshDeformedNormals = frameDeformedNormals[iMesh];
                    const glm::GlmString& meshName = _outputData._meshAssetNames[_outputData._meshAssetNameIndices[iMesh]];

                    size_t vertexCount = meshDeformedVertices.size();
                    if (vertexCount == 0)
                    {
                        continue;
                    }

                    polyCounts.clear();
                    polygonpointnumbers.clear();

                    GA_Offset primOffset;
                    GA_Offset vertexOffset;

                    GA_Offset pointStartOffset = gdp->appendPointBlock(vertexCount);
                    for (size_t iVertex = 0; iVertex < vertexCount; ++iVertex)
                    {
                        const glm::Vector3& meshVertex = meshDeformedVertices[iVertex];
                        gdp->setPos3(pointStartOffset + iVertex,
                                     UT_Vector3(
                                         meshVertex[0],
                                         meshVertex[1],
                                         meshVertex[2]));
                    }

                    if (_outputData._geoType == glm::crowdio::GeometryType::FBX)
                    {
                        // when fbxMesh == NULL, vertexCount == 0, so no need to check fbxMesh != NULL
                        FbxMesh* fbxMesh = _outputData._fbxCharacter->getCharacterFBXMesh(iMesh);

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

                        unsigned int meshMtlIdx = _outputData._meshAssetMaterialIndices[iMesh];

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

                        primOffset = GEO_PrimPoly::buildBlock(gdp, pointStartOffset, vertexCount, polyCounts, polygonpointnumbers.array());
                        GA_Primitive* prim = gdp->getPrimitive(primOffset);
                        vertexOffset = prim->getVertexOffset(0);

                        if (hasNormals)
                        {
                            // add normals
                            GA_Attribute* normalAttr = gdp->addNormalAttribute(GA_ATTRIB_VERTEX, GA_STORE_REAL32);
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

                        // find how many uv layers are available
                        int uvSetCount = fbxMesh->GetLayerCount(FbxLayerElement::eUV);
                        FbxLayerElementUV* uvElement = NULL;
                        for (int iUVSet = 0; iUVSet < uvSetCount; ++iUVSet)
                        {
                            glm::GlmString attrName = gdp->getStdAttributeName(GEO_ATTRIBUTE_TEXTURE, iUVSet + 1).c_str();
                            FbxLayer* layer = fbxMesh->GetLayer(fbxMesh->GetLayerTypedIndex((int)iUVSet, FbxLayerElement::eUV));
                            uvElement = layer->GetUVs();
                            bool uvsByControlPoint = uvElement->GetMappingMode() == FbxLayerElement::eByControlPoint;
                            bool uvReferenceDirect = uvElement->GetReferenceMode() == FbxLayerElement::eDirect;

                            GA_Attribute* uvAttr = gdp->addFloatTuple(GA_ATTRIB_VERTEX, attrName.c_str(), 3);
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
                    else if (_outputData._geoType == glm::crowdio::GeometryType::GCG)
                    {
                        glm::crowdio::GlmFileMeshTransform& assetFileMeshTransform = _outputData._gcgCharacter->getGeometry()._transforms[_outputData._transformIndicesInGcgFile[iMesh]];
                        glm::crowdio::GlmFileMesh& assetFileMesh = _outputData._gcgCharacter->getGeometry()._meshes[assetFileMeshTransform._meshIndex];

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

                        primOffset = GEO_PrimPoly::buildBlock(gdp, pointStartOffset, vertexCount, polyCounts, polygonpointnumbers.array());
                        GA_Primitive* prim = gdp->getPrimitive(primOffset);
                        vertexOffset = prim->getVertexOffset(0);

                        // add normals
                        GA_Attribute* normalAttr = gdp->addNormalAttribute(GA_ATTRIB_VERTEX, GA_STORE_REAL32);
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

                        if (assetFileMesh._uvSetCount > 0)
                        {
                            for (size_t iUVSet = 0; iUVSet < assetFileMesh._uvSetCount; ++iUVSet)
                            {
                                glm::GlmString attrName = "uv";
                                if (iUVSet > 0)
                                {
                                    attrName += glm::toString(iUVSet + 1);
                                }
                                GA_Attribute* uvAttr = gdp->addFloatTuple(GA_ATTRIB_VERTEX, attrName.c_str(), 3);
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
                    GA_Attribute* entityIdAttr = gdp->addTuple(GA_STORE_INT64, GA_ATTRIB_PRIMITIVE, "glmEntityId", 1);
                    GA_RWHandleID entityIdAttrHandle(entityIdAttr);

                    GA_Attribute* meshAttr = gdp->addStringTuple(GA_ATTRIB_PRIMITIVE, meshAttrName.c_str(), 1);
                    GA_RWHandleS meshAttrHandle(meshAttr);

                    GA_Attribute* materialAttr = gdp->addStringTuple(GA_ATTRIB_PRIMITIVE, GEO_STD_ATTRIB_MATERIAL, 1);
                    GA_RWHandleS materialAttrHandle(materialAttr);

                    int shadingGroupIdx = meshShadingGroups[iMesh];
                    glm::GlmString materialName = "";
                    if (shadingGroupIdx >= 0)
                    {
                        const glm::ShadingGroup& shGroup = character->_shadingGroups[shadingGroupIdx];
                        materialName = materialPath.c_str();
                        materialName.rtrim("/");
                        materialName += "/";
                        switch (materialAssignMode)
                        {
                        case GolaemMaterialAssignMode::BY_SHADING_GROUP:
                        {
                            materialName += shGroup._name;
                        }
                        break;
                        case GolaemMaterialAssignMode::BY_SURFACE_SHADER:
                        {
                            // get the surface shader
                            int shaderAssetIdx = shadingGroupToSurfaceShader[shadingGroupIdx];
                            if (shaderAssetIdx >= 0)
                            {
                                const glm::ShaderAsset& shAsset = character->_shaderAssets[shaderAssetIdx];
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
                            const glm::ShaderAttribute& shAttr = character->_shaderAttributes[shAttrIdx];
                            UT_StringHolder attrName = UT_VarEncode::encode(shAttr._name.c_str());
                            GA_Attribute* attr = NULL;
                            switch (shAttr._type)
                            {
                            case glm::ShaderAttributeType::INT:
                            {
                                attr = gdp->addIntTuple(GA_ATTRIB_PRIMITIVE, attrName, 1);
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
                                attr = gdp->addFloatTuple(GA_ATTRIB_PRIMITIVE, attrName, 1);
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
                                attr = gdp->addStringTuple(GA_ATTRIB_PRIMITIVE, attrName, 1);
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
                                attr = gdp->addFloatTuple(GA_ATTRIB_PRIMITIVE, attrName, 3);
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
                        entityIdAttrHandle.set(primOffset + iPoly, _inputData._entityId);
                        meshAttrHandle.set(primOffset + iPoly, meshName.c_str());
                        materialAttrHandle.set(primOffset + iPoly, materialName.c_str());
                    }
                }
            }
        }
    }
    break;
    default:
        break;
    }

    _noUpdateLoop = true;
    setInt(getParamName(GolaemParams::ENTITY_COUNT), 0, time, entityCount);
    _noUpdateLoop = false;

    return error();
}
