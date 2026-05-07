/***************************************************************************
*                                                                          *
*  Copyright (C) Golaem S.A.  All Rights Reserved.                         *
*                                                                          *
***************************************************************************/

#include "GU_PackedGolaemEntity.h"

#include "glmHoudini.h"
#include "glmHoudiniUtils.h"

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
#include <UT/UT_UI.h>
#include <UT/UT_DirUtil.h>
#include <PY/PY_Python.h>
#include <HOM/HOM_Module.h>
#include <HOM/HOM_shelves.h>
#include <HOM/HOM_qt.h>
#include <GU/GU_PackedFactory.h>
#include <GU/GU_PrimPacked.h>
#include <FBX/FBX_AllocWrapper.h>

HDK_INCLUDES_END

#include <glmCore.h>
#include <glmCrowdIO.h>
#include <glmCrowdIOUtils.h>
#include <glmSimulationCacheFactory.h>
#include <glmSimulationCacheLibrary.h>
#include <glmSimulationCacheInformation.h>
#include <glmFileDir.h>
#include <glmFileName.h>
#include <glmGolaemCharacter.h>
#include "glmADP.h"

#include "glmCrowdHoudiniPluginAPI.h"

//-----------------------------------------------------------------------------

struct GolaemParams
{
    enum Value
    {
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

//-----------------------------------------------------------------------------
static inline const char* getParamName(GolaemParams::Value value)
{
    switch (value)
    {
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
        return "glmMaterialAssignMode";
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
    bool _clearGeo;

    glm::crowdio::SimulationCacheFactory _factory;
    glm::Array<glm::PODArray<size_t>> _sortedBonesInversePerChar;
    glm::Array<glm::PODArray<int>> _sgToSsPerChar;

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

    void refreshParameters(
        fpreal time,
        bool updateCache = true,
        bool updateLayout = true,
        bool updateTerrain = true,
        bool updateCharacterFiles = true,
        bool updateDisplayMode = true);
};

//-----------------------------------------------------------------------------
PRM_Template* SOP_GolaemCacheProxy::buildTemplates()
{
    static PRM_Default defaultParam(0, "");

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
        _clearGeo = true;
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
    , _clearGeo(false)
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

    glm::Singleton<glm::HoudiniFbxData>::destroy();

    glm::crowdio::finish();
    glm::theGolaemLogger::destroy();
    glm::finishCore();
}

// register new sop
//-----------------------------------------------------------------------------
void GLM_CROWDHOUDINI_API newSopOperator(OP_OperatorTable* table)
{
    glm::initCore(); // inits logs
    glm::getLog()->_logSeverity[glm::Log::CROWD] = glm::Log::LOG_WARNING;
    glm::getLog()->_logSeverity[glm::Log::SDK] = glm::Log::LOG_ERROR;

    glm::theGolaemLogger::create();

    // plugin compatibility check
    if (UTgetHDKAPIVersion() != HDK_API_VERSION)
    {
        GLM_CROWD_TRACE_ERROR(
            "GolaemForHoudini was built against the Houdini toolkit version '"
            << HDK_API_VERSION << "', which incompatible with the current Houdini toolkit version: '" << UTgetHDKAPIVersion() << "'."
            << " This version of Houdini is not supported. Unexpected errors or crashes might occur.");
    }

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

    FBXwrapAllocators(); // use Houdini's FBX allocators (otherwise Houdini crashes when importing a FBX and Golaem is loaded)

    glm::crowdio::setupGolaemProduct("GolaemForHoudini", HDK_API_VERSION);
    glm::crowdio::init();

    if (UTisUIAvailable())
    {
        glm::crowdio::displayADPDialog("en", true, HOM().qt()._mainWindow());
    }
    else
    {
        // do not display anything in batch mode -> do not uncomment below
        // glm::crowdio::displayADPDialog("en", true);
    }

    glm::Singleton<glm::HoudiniFbxData>::create();

    UT_Exit::addExitCallback(glmDsoExit);

    table->addOperator(op);
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
    if (_clearGeo)
    {
        // clean all entities
        gdp->clearAndDestroy();
        _clearGeo = false;

        _sortedBonesInversePerChar.resize(_factory.getGolaemCharacters().size());
        _sgToSsPerChar.resize(_factory.getGolaemCharacters().size());
        for (int iChar = 0, charCount = _factory.getGolaemCharacters().sizeInt(); iChar < charCount; ++iChar)
        {
            glm::PODArray<size_t>& sortedBonesInverse = _sortedBonesInversePerChar[iChar];
            glm::PODArray<int>& shadingGroupToSurfaceShader = _sgToSsPerChar[iChar];
            sortedBonesInverse.clear();
            shadingGroupToSurfaceShader.clear();
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

            shadingGroupToSurfaceShader.resize(character->_shadingGroups.size(), -1);
            for (size_t iSg = 0, sgCount = character->_shadingGroups.size(); iSg < sgCount; ++iSg)
            {
                const glm::ShadingGroup& shadingGroup = character->_shadingGroups[iSg];
                int shaderAssetIdx = character->findShaderAsset(shadingGroup, "surface");
                if (shaderAssetIdx >= 0)
                {
                    shadingGroupToSurfaceShader[iSg] = shaderAssetIdx;
                }
            }
        }
    }

    float currentFrame = static_cast<float>(evalFloat(getParamName(GolaemParams::CURRENT_FRAME), 0, time));
    float renderPercent = static_cast<float>(evalFloat(getParamName(GolaemParams::DRAW_PERCENT), 0, time)) * 0.01f;
    short geoTag = static_cast<short>(evalInt(getParamName(GolaemParams::GEO_TAG), 0, time));
    glm::GolaemMaterialAssignMode::Value materialAssignMode = (glm::GolaemMaterialAssignMode::Value)evalInt(getParamName(GolaemParams::MATERIAL_ASSIGN_MODE), 0, time);
    UT_String materialPath;
    evalString(materialPath, getParamName(GolaemParams::MATERIAL_PATH), 0, time);

    int64_t entityCount = 0;

    GA_Size primitiveIndex = 0;
    GA_Size primCount = gdp->getNumPrimitives();

    size_t totalEntityTypesCount(0);
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
        totalEntityTypesCount += simuData->_entityTypeCount;
        const glm::Array<glm::PODArray<int>>& entityAssets = cachedSimulation.getFinalEntityAssets(currentFrame);
        const glm::ShaderAssetDataContainer* shaderDataContainer = cachedSimulation.getFinalShaderData(currentFrame, UINT32_MAX, true);

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
                GU_PrimPacked* packedPrim = static_cast<GU_PrimPacked*>(prim);
                packedEntity = static_cast<glm::GU_PackedGolaemEntity*>(packedPrim->hardenImplementation());
            }
            else
            {
                GU_PrimPacked* packedPrim = GU_PrimPacked::build(*gdp, glm::GU_PackedGolaemEntity::getTypeId());
                packedEntity = static_cast<glm::GU_PackedGolaemEntity*>(packedPrim->hardenImplementation());
                prim = packedPrim;
            }
            bool entityIsNew = packedEntity->_inputData._entityId == -1;

            // set it to -1 in case it existed but was killed
            packedEntity->_inputData._entityId = -1;

            int64_t entityId = simuData->_entityIds[iEntity];
            if (entityId < 0)
            {
                // entity was probably killed
                continue;
            }

            int32_t entityToBakeIndex = simuData->_entityToBakeIndex[iEntity];
            GLM_DEBUG_ASSERT(entityToBakeIndex >= 0);
            bool excludedEntity = frameData->_entityEnabled[entityToBakeIndex] != 1;
            if (!excludedEntity)
            {
                excludedEntity = iEntity >= maxEntities;
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
            ++entityCount;

            // set the entity id to tell it it's still valid
            packedEntity->_inputData._entityId = entityId;
            packedEntity->_isNew = entityIsNew;
            packedEntity->_inputData._frameDatas[0] = frameData;
            packedEntity->_inputData._frames[0] = (double)currentFrame;
            packedEntity->_inputData._shaderDataContainer = shaderDataContainer;
            if (entityIsNew)
            {
                //packedEntity->_inputData._dirMapRules // left empty for now
                packedEntity->_inputData._entityIndex = iEntity;
                packedEntity->_character = character;
                packedEntity->_inputData._geometryTag = geoTag;
                packedEntity->_inputData._simuData = simuData;
                packedEntity->_inputData._entityToBakeIndex = entityToBakeIndex;

                packedEntity->_inputData._characterIdx = characterIdx;
                packedEntity->_inputData._character = character;

                // compute assets if needed
                packedEntity->_inputData._assets = &entityAssets[packedEntity->_inputData._entityIndex];

                // compute the bounding box of the current entity
                glm::Vector3 halfExtents(1, 1, 1);
                size_t geoIdx = 0;
                const glm::GeometryAsset* geoAsset = character->getGeometryAsset(geoTag, geoIdx); // any LOD should have same extents !
                if (geoAsset != NULL)
                {
                    halfExtents = geoAsset->_halfExtentsYUp;
                }
                float characterScale = simuData->_scales[iEntity];
                halfExtents *= characterScale;

                packedEntity->_halfExtents = halfExtents;
                packedEntity->_sortedBonesInverse = &_sortedBonesInversePerChar[characterIdx];
                packedEntity->_displayMode = displayMode;
                packedEntity->_shadingGroupToSurfaceShader = &_sgToSsPerChar[characterIdx];
                packedEntity->_materialAssignMode = materialAssignMode;
                packedEntity->_materialPath = materialPath.c_str();
            }
            uint16_t entityType = simuData->_entityTypes[iEntity];

            uint16_t boneCount = simuData->_boneCount[entityType];
            uint32_t positionOffset = simuData->_iBoneOffsetPerEntityType[entityType] + simuData->_indexInEntityType[iEntity] * boneCount;

            float* rootPos = frameData->_bonePositions[positionOffset];

            packedEntity->_rootPos.setValues(rootPos);
            packedEntity->_updateGeo = true;
        }
    }

    _noUpdateLoop = true;
    setInt(getParamName(GolaemParams::ENTITY_COUNT), 0, time, entityCount);
    _noUpdateLoop = false;

    glm::Array<glm::GlmString> ADPattributeNames;
    glm::Array<glm::GlmString> ADPattributeValues;

    std::stringstream toChar;
    toChar << entityCount;
    ADPattributeNames.push_back("entityCount");
    ADPattributeValues.push_back(toChar.str().c_str());

    toChar.str("");
    toChar.clear();
    toChar << totalEntityTypesCount;
    ADPattributeNames.push_back("entityTypeCount");
    ADPattributeValues.push_back(toChar.str().c_str());

    toChar.str("");
    toChar.clear();
    toChar << _factory.getGolaemCharacters().size();
    ADPattributeNames.push_back("characterFilesCount");
    ADPattributeValues.push_back(toChar.str().c_str());

    size_t totalLayoutNodesCount(0);
    for (size_t iLayout = 0; iLayout < _factory.getLayoutHistoryCount(); iLayout++)
    {
        if (_factory.getLayoutHistory(iLayout))
            totalLayoutNodesCount+=_factory.getLayoutHistory(iLayout)->_layoutNodesCount;
    }
    toChar.str("");
    toChar.clear();
    toChar << totalLayoutNodesCount;
    ADPattributeNames.push_back("layoutNodesCount");
    ADPattributeValues.push_back(toChar.str().c_str());

    toChar.str("");
    toChar.clear();
    toChar << crowdFieldNames.size();
    ADPattributeNames.push_back("crowdFieldCount");
    ADPattributeValues.push_back(toChar.str().c_str());

    glm::crowdio::ADPTrackEvent("COOK", ADPattributeNames, ADPattributeValues);

    return error();
}
