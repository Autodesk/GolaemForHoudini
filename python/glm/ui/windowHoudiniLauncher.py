# **************************************************************************
# * *
# * Copyright (C) Golaem S.A.  - All Rights Reserved.  *
# * *
# **************************************************************************

# pylint: disable=C0103

import sys

from glm.simCacheLib import simCacheLibWindow as scl
from glm.simCacheLib import simCacheLibWindowHoudiniWrapper as sclw
from glm.layout import layoutEditorUtils
from glm.layout import layoutEditorWrapper
from glm.Qtpy.Qt import QtCore, QtWidgets
import hou
import glm.ui.windowHoudiniWrapper as wrapper


# **********************************************************************
#
# Launchers
#
# **********************************************************************
glmSimCacheLibWindowUIs = []


# ------------------------------------------------------------------
# SimCacheLibWindowMain
# ------------------------------------------------------------------
def SimCacheLibWindowMain():
    houdiniWrapper = wrapper.WindowHoudiniWrapper()
    global glmSimCacheLibWindowUIs
    libUI = None
    if not QtWidgets.QApplication.instance():
        application = QtWidgets.QApplication(sys.argv)
        houdiniWrapper.log("info", "Created QApplication instance: {0}".format(application))
    if not QtWidgets.QApplication.instance():
        houdiniWrapper.log("error", "No QApplication instance found. The Simulation Cache Library window cannot be displayed.")
        return None
    if len(glmSimCacheLibWindowUIs):
        libUI = glmSimCacheLibWindowUIs[0]
    else:
        houWrapper = sclw.SimCacheLibWindowHoudiniWrapper()
        libUI = scl.SimCacheLibWindow(wrapper=houWrapper)
        libUI.setStyleSheet("background-color: #444444")
        glmSimCacheLibWindowUIs.append(libUI)
    libUI.show()
    libUI.setWindowState(libUI.windowState() & ~QtCore.Qt.WindowMinimized | QtCore.Qt.WindowActive)
    libUI.activateWindow()
    return libUI


# ------------------------------------------------------------------
# AboutWindowMain
# ------------------------------------------------------------------
def AboutWindowMain():
    houdiniWrapper = wrapper.WindowHoudiniWrapper()
    try:
        import glm.ui.golaemAboutWindowHoudini as abtHoudini
    except ModuleNotFoundError:
        houdiniWrapper.log("warning", "This is a Golaem for Houdini standalone build, the about window is not available.")
        return None
    except ImportError as e:
        houdiniWrapper.log("error", f"Error importing about window: {e}")
        return None
    if not QtWidgets.QApplication.instance():
        application = QtWidgets.QApplication(sys.argv)
        houdiniWrapper.log("info", "Created QApplication instance: {0}".format(application))
    if not QtWidgets.QApplication.instance():
        houdiniWrapper.log("error", "No QApplication instance found. The About window cannot be displayed.")
        return None

    abtUI = abtHoudini.GolaemAboutWindowHoudini(parent=hou.qt.mainWindow(), golaemVersion=None, baseDir=None)
    abtUI.setStyleSheet("background-color: #444444")
    abtUI.show()
    abtUI.setWindowState(abtUI.windowState() & ~QtCore.Qt.WindowMinimized | QtCore.Qt.WindowActive)
    abtUI.activateWindow()
    return abtUI


# ------------------------------------------------------------------
# LayoutEditorWindowMain
# ------------------------------------------------------------------
def LayoutEditorWindowMain(layoutFile=""):
    houdiniWrapper = wrapper.WindowHoudiniWrapper()
    layoutEditor = None
    if not QtWidgets.QApplication.instance():
        application = QtWidgets.QApplication(sys.argv)
        houdiniWrapper.log("info", "Created QApplication instance: {0}".format(application))
    if not QtWidgets.QApplication.instance():
        houdiniWrapper.log("error", "No QApplication instance found. The Layout Editor window cannot be displayed.")
        return None

    layoutWrapper = layoutEditorWrapper.getTheLayoutEditorWrapperInstance()
    layoutEditor = layoutEditorUtils.getTheLayoutEditorInstance(parentWindow=hou.qt.mainWindow(), wrapper=layoutWrapper)

    # must override background color, Houdini doesn't set this ?
    layoutEditor.setStyleSheet("background-color: #444444")
    if layoutFile:
        layoutEditor.openLayoutFile(layoutFile)
    layoutEditor.show()
    layoutEditor.setWindowState(layoutEditor.windowState() & ~QtCore.Qt.WindowMinimized | QtCore.Qt.WindowActive)
    layoutEditor.activateWindow()
    return layoutEditor
