# **************************************************************************
# * *
# * Copyright (C) Golaem S.A.  - All Rights Reserved.  *
# * *
# **************************************************************************
from glm.simCacheLib import simCacheLibWindow as scl
from glm.simCacheLib import simCacheLibWindowHoudiniWrapper as sclw
from glm.layout import layoutEditorUtils
from glm.layout import layoutEditorWrapper
import glm.ui.aboutWindow as abt
import glm.ui.windowHoudiniWrapper as whw
from glm.Qtpy.Qt import QtCore, QtWidgets
import hou
import sys

usingDevkit = True
try:
    from glm.devkit import *
except:
    usingDevkit = False

class GolaemSessionInformation(object):
    def __init__(self):
        self._pluginDir = ""
        self._version = ""
        self._licenseInfo = "0;Invalid license"

glmSessionInfo = GolaemSessionInformation()

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
    global glmSimCacheLibWindowUIs
    global glmSessionInfo
    application = None
    libUI = None
    if not QtWidgets.QApplication.instance():
        application = QtWidgets.QApplication(sys.argv)
        print("Created QApplication instance: {0}".format(application))
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
    global glmSessionInfo
    application = None
    abtUI = None
    if not QtWidgets.QApplication.instance():
        application = QtWidgets.QApplication(sys.argv)
        print("Created QApplication instance: {0}".format(application))
    houWrapper = whw.WindowHoudiniWrapper()
    abtUI = abt.AboutWindow(wrapper=houWrapper, golaemVersion=glmSessionInfo._version, licenseText=glmSessionInfo._licenseInfo, productName="Golaem for Houdini")
    abtUI.setStyleSheet("background-color: #444444")
    abtUI.show()
    abtUI.setWindowState(abtUI.windowState() & ~QtCore.Qt.WindowMinimized | QtCore.Qt.WindowActive)
    abtUI.activateWindow()
    return abtUI


# ------------------------------------------------------------------
# LayoutEditorWindowMain
# ------------------------------------------------------------------
def LayoutEditorWindowMain(layoutFile=""):
    global glmSessionInfo
    application = None
    layoutEditor = None
    if not QtWidgets.QApplication.instance():
        application = QtWidgets.QApplication(sys.argv)
        print("Created QApplication instance: {0}".format(application))

    layoutWrapper = layoutEditorWrapper.getTheLayoutEditorWrapperInstance()
    layoutEditor = layoutEditorUtils.getTheLayoutEditorInstance(parentWindow=hou.qt.mainWindow(), wrapper=layoutWrapper)

    # must override background color, Houdini doesn't set this ?
    layoutEditor.editorMainWindow.setStyleSheet("background-color: #444444")
    if layoutFile:
        layoutEditor.openLayoutFile(layoutFile)
    layoutEditor.show()
    layoutEditor.editorMainWindow.setWindowState(layoutEditor.editorMainWindow.windowState() & ~QtCore.Qt.WindowMinimized | QtCore.Qt.WindowActive)
    layoutEditor.editorMainWindow.activateWindow()
    return layoutEditor

