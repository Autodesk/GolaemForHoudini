#**************************************************************************
#* *
#* Copyright (C) Golaem S.A.  - All Rights Reserved.  *
#* *
#**************************************************************************
import os
import hou
from glm.Qtpy.Qt import QtWidgets, QtCore
from glm.ui import windowWrapper


#**********************************************************************
#
# WindowHoudiniWrapper
# Houdini wrapper for golaem window tools
#
#**********************************************************************
class WindowHoudiniWrapper(windowWrapper.WindowWrapper):

    def __init__(self, iconsDir=""):
        self._iconsDir = iconsDir

    #******************************************************************
    # UI
    #******************************************************************

    #------------------------------------------------------------------
    # returns the parent window
    #------------------------------------------------------------------
    def getParentWindow(self):
        return hou.qt.mainWindow()

    #------------------------------------------------------------------
    # log a message (level : info / warning / error)
    #------------------------------------------------------------------
    def log(self, logLevel, message):
        if logLevel == 'info':
            print("[INFO] ", message)
        elif logLevel == 'warning':
            print("[WARNING] ", message)
        elif logLevel == 'error':
            print("[ERROR] ", message)

    #------------------------------------------------------------------
    # returns the directory with the icons
    #------------------------------------------------------------------
    def getIconsDir(self):
        return self._iconsDir
