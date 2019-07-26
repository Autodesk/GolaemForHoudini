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
                cacheProxy.parm("glmCacheLibFile").set(lib.libFile)
                item = lib.getLibItemAt(itemIdx)
                if item.isInitialized():
                    cacheProxy.parm("glmCacheLibItem").set(item.itemName)
                # force reevaluating cache params
                cacheProxy.parm("glmForceCacheLibEval").set(1)
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
        if buttonName == "Import from selected / scene Simulation Cache Proxy" or buttonName == "Update Thumbnail from Viewport":
            return False
        return True
