#**************************************************************************
#*                                                                        *
#*  Copyright (C) Golaem S.A. - All Rights Reserved.                      *
#*                                                                        *
#**************************************************************************

from glm.ui import windowHoudiniWrapper
import hou
import os


#**********************************************************************
#
# SimCacheLibWindowHoudiniWrapper
# Houdini wrapper for SimCacheLibWindow
#
#**********************************************************************
class SimCacheLibWindowHoudiniWrapper(windowHoudiniWrapper.WindowHoudiniWrapper):
    #******************************************************************
    # Specific
    #******************************************************************

    #------------------------------------------------------------------
    # Returns the app stylesheet
    #------------------------------------------------------------------
    def getStyleSheet(self):
        return "background-color: #444444"

    #------------------------------------------------------------------
    # Updates the item snapshot and returns it
    #------------------------------------------------------------------
    def updateItemSnapshot(self, item):
        return item

    #------------------------------------------------------------------
    # Create a sim cache proxy node, fills it from item and returns it
    #------------------------------------------------------------------
    def createSimCacheProxyFromItem(self, lib, itemIdx):
        geoNode = hou.node("/obj").createNode("geo")
        cacheProxy = None
        if geoNode:
            cacheProxy = geoNode.createNode("golaemCacheProxy")
            if cacheProxy:
                # update cache proxy parameters
                item = lib.getLibItemAt(itemIdx)
                if item.isInitialized():
                    cacheProxy.parm("glmCrowdFieldNames").set(";".join(item.crowdFields))
                    cacheProxy.parm("glmCacheName").set(item.cacheName)
                    cacheProxy.parm("glmCacheDir").set(item.cacheDir)
                    cacheProxy.parm("glmCharacterFiles").set(item.characterFiles)
                    cacheProxy.parm("glmSourceTerrain").set(item.sourceTerrain)
                    cacheProxy.parm("glmDestTerrain").set(item.destTerrain)
                    cacheProxy.parm("glmEnableLayout").set(item.enableLayout)
                    layoutFilePaths = item.layoutFile.split(";")
                    cacheProxy.parm("glmLayoutFiles").set(len(layoutFilePaths))
                    for layoutFileIndex in range(0, len(layoutFilePaths)):
                        cacheProxy.parm("glmLayoutFile" + str(layoutFileIndex + 1)).set(layoutFilePaths[layoutFileIndex])
        return cacheProxy

    #------------------------------------------------------------------
    # Updates a sim cache lib from a set of nodes and returns it
    #------------------------------------------------------------------
    def fillSimCacheLibFromProxies(self, lib, nodes):
        return lib

    #------------------------------------------------------------------
    # Return true if a button is available is this interface
    #------------------------------------------------------------------
    def isButtonAvailable(self, buttonName):
        if buttonName == "Import from selected / scene Simulation Cache Proxy" or buttonName == "Update Thumbnail from Viewport" or buttonName == "Import Simulation Cache in Scene as Multiple Proxies" or buttonName == "Import as Simulation in Scene":
            return False
        return True
