# **************************************************************************
# * *
# * Copyright (C) Golaem S.A.  - All Rights Reserved.  *
# * *
# **************************************************************************
import os
import hou
from glm.ui import windowWrapper


# **********************************************************************
#
# WindowHoudiniWrapper
# Houdini wrapper for golaem window tools
#
# **********************************************************************
class WindowHoudiniWrapper(windowWrapper.WindowWrapper):

    # ******************************************************************
    # UI
    # ******************************************************************

    # ------------------------------------------------------------------
    # returns the parent window
    # ------------------------------------------------------------------
    def getParentWindow(self):
        return hou.qt.mainWindow()

    # ------------------------------------------------------------------
    # log a message (level : info / warning / error / debug )
    # ------------------------------------------------------------------
    def log(self, logLevel, message):
        if logLevel == "info":
            print("[INFO] ", message)
        elif logLevel == "warning":
            print("[WARNING] ", message)
        elif logLevel == "error":
            print("[ERROR] ", message)
        elif logLevel == "debug":
            print("[DEBUG] ", message)

    # ------------------------------------------------------------------
    # returns the directory with the icons
    # ------------------------------------------------------------------
    def getIconsDir(self):
        thisDirectory = os.path.dirname(os.path.realpath(__file__))
        return thisDirectory + "/../../../icons/"
