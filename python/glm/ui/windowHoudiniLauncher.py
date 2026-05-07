# **************************************************************************
# * *
# * Copyright (C) Golaem S.A.  - All Rights Reserved.  *
# * *
# **************************************************************************
from glm.simCacheLib import simCacheLibWindow as scl
from glm.simCacheLib import simCacheLibWindowHoudiniWrapper as sclw
from glm.layout import layoutEditorUtils
from glm.layout import layoutEditorWrapper
import glm.ui.golaemAboutWindow as abt
from glm.Qtpy.Qt import QtCore, QtWidgets
import hou
import sys

usingDevkit = True
try:
    import glm.devkit as devkit
except:
    usingDevkit = False


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
    application = None
    abtUI = None
    if not QtWidgets.QApplication.instance():
        application = QtWidgets.QApplication(sys.argv)
        print("Created QApplication instance: {0}".format(application))

    # Fetch license data
    devkit.initGolaemProduct("GolaemForHoudini")
    devkit.initGolaem()
    devkit.finishGolaem()
    devkit.finishGolaemProduct()

    abtUI = abt.GolaemAboutWindow(parent=hou.qt.mainWindow(), productName="Golaem for Houdini", golaemVersion=None, baseDir=None)
    abtUI.setStyleSheet("background-color: #444444")
    abtUI.show()
    abtUI.setWindowState(abtUI.windowState() & ~QtCore.Qt.WindowMinimized | QtCore.Qt.WindowActive)
    abtUI.activateWindow()
    return abtUI


# ------------------------------------------------------------------
# LayoutEditorWindowMain
# ------------------------------------------------------------------
def LayoutEditorWindowMain(layoutFile=""):
    application = None
    layoutEditor = None
    if not QtWidgets.QApplication.instance():
        application = QtWidgets.QApplication(sys.argv)
        print("Created QApplication instance: {0}".format(application))

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
