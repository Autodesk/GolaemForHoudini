cmake_minimum_required(VERSION 3.13)

############################################################
# BEGIN Project
############################################################

project(${TARGET_GOLAEMHOUDINI})

# List Files
LIST_FILES( "src" "h;cpp" )
list_files( "src/utils" "h;cpp" )
list_files( "src/procedural" "h;cpp" )
list_files( "python/glm/ui" "py" )
list_files( "python/glm/simCacheLib" "py" )
list_files( "shelf" "shelf" )

# Create a library
add_library( ${PROJECT_NAME} SHARED ${LIST_FILES} )

string(REGEX REPLACE "^glm" "HOUDINI_" CUSTOM_PROJECT_LABEL "${PROJECT_NAME}" )

crowd_label_project( ${PROJECT_NAME} )
set_target_properties( ${PROJECT_NAME} PROPERTIES PROJECT_LABEL ${CUSTOM_PROJECT_LABEL} )

crowd_load_dependencies( ${PROJECT_NAME} )
set_target_prefix( ${PROJECT_NAME} PLUGIN )
set_target_postfix( ${PROJECT_NAME} PLUGIN )

if( MSVC)
    if(DEFINED GLM_EXCLUDE_INVALID_DEBUG_BUILDS) # used by build servers
        set_target_properties( ${PROJECT_NAME} PROPERTIES EXCLUDE_FROM_ALL TRUE )
        set( GLM_EXCLUDED_TARGETS_FILE_PATH "${CMAKE_BINARY_DIR}/glmExcludedDebugTargets.txt" )
        file(APPEND "${GLM_EXCLUDED_TARGETS_FILE_PATH}" "${PROJECT_NAME}\n")
    endif()
	set_target_properties( ${PROJECT_NAME} PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD_DEBUG ON )
endif()

target_link_libraries(${PROJECT_NAME} Houdini)

if(MSVC)
    # cannot compile debug / windows
	set( CROWD_INSTALL_SKIP_DEBUG ON )
endif()

# name the dll "GolaemHoudini" instead of "glmGolaemHoudini"
CROWD_INSTALL_RUNTIME_LIBRARY( ${PROJECT_NAME} "procedurals/houdini" "GolaemHoudini")

if(MSVC)
    # cannot compile debug / windows
	unset( CROWD_INSTALL_SKIP_DEBUG )
endif()

crowd_install_dependencies()

set( GOLAEM_SCRIPT_DIR ${CMAKE_SOURCE_DIR}/data/crowd/glmCrowd/scripts)
set( GOLAEM_ICON_DIR ${CMAKE_SOURCE_DIR}/data/crowd/glmCrowd/icons)

file(GLOB LAYOUT_ICON_FILES "${GOLAEM_ICON_DIR}/layoutToolv7/*" )
file(GLOB EDITORS_ICON_FILES "${GOLAEM_ICON_DIR}/editors/*" )
file(GLOB SIMCACHELIB_ICON_FILES "${GOLAEM_ICON_DIR}/libraryTool/*" )
file(GLOB GLM_SCRIPT_FILES "${GOLAEM_SCRIPT_DIR}/glm/*.py")
file(GLOB LAYOUT_SCRIPT_FILES "${GOLAEM_SCRIPT_DIR}/glm/layout/*.py" "${GOLAEM_SCRIPT_DIR}/glm/layout/*.json")
file(GLOB NODZ_SCRIPT_FILES "${GOLAEM_SCRIPT_DIR}/glm/Nodz/*.py" "${GOLAEM_SCRIPT_DIR}/glm/Nodz/*.json")
file(GLOB QTPY_SCRIPT_FILES "${GOLAEM_SCRIPT_DIR}/glm/Qtpy/*.py")
file(GLOB SIMCACHELIB_SCRIPT_FILES "${GOLAEM_SCRIPT_DIR}/glm/simCacheLib/*.py")

file(GLOB HOU_UI_SCRIPT_FILES "${CMAKE_CURRENT_SOURCE_DIR}/python/glm/ui/*.py")
file(GLOB HOU_SIMCACHELIB_SCRIPT_FILES "${CMAKE_CURRENT_SOURCE_DIR}/python/glm/simCacheLib/*.py")
file(GLOB HOU_SHELF_FILES "${CMAKE_CURRENT_SOURCE_DIR}/shelf/*.shelf")

file( GLOB LICENSE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/licenses/*.lic")

set(ROOT_ICON_FILES "${GOLAEM_ICON_DIR}/buttonSimulationCacheLibrary.png")
list(APPEND ROOT_ICON_FILES "${GOLAEM_ICON_DIR}/glmDefaultIcon.png")
list(APPEND ROOT_ICON_FILES "${GOLAEM_ICON_DIR}/glmAbout.png")
list(APPEND ROOT_ICON_FILES "${GOLAEM_ICON_DIR}/SimulationCacheLibrary.png")
list(APPEND ROOT_ICON_FILES "${GOLAEM_ICON_DIR}/SimulationCacheLayout.png")
list(APPEND ROOT_ICON_FILES "${GOLAEM_ICON_DIR}/SimulationCacheProxy.png")
list(APPEND ROOT_ICON_FILES "${GOLAEM_ICON_DIR}/buttonSimulationCacheLayout.png")
list(APPEND ROOT_ICON_FILES "${GOLAEM_ICON_DIR}/W09_GOLM_about_528x90.png")

foreach( configuration "Debug" "Release" )
    # install licenses
    foreach( install_file ${LICENSE_FILES} )
        install( FILES ${install_file} CONFIGURATIONS ${configuration} DESTINATION "${CROWD_INSTALL_PATH_${configuration}}/procedurals/houdini" )
    endforeach()

    # install layout icons
    foreach( install_file ${LAYOUT_ICON_FILES} )
        install( FILES ${install_file} CONFIGURATIONS ${configuration} DESTINATION "${CROWD_INSTALL_PATH_${configuration}}/procedurals/houdini/icons/layoutToolv7" )
    endforeach()

    foreach( install_file ${EDITORS_ICON_FILES} )
        install( FILES ${install_file} CONFIGURATIONS ${configuration} DESTINATION "${CROWD_INSTALL_PATH_${configuration}}/procedurals/houdini/icons/editors" )
    endforeach()

    foreach( install_file ${SIMCACHELIB_ICON_FILES} )
        install( FILES ${install_file} CONFIGURATIONS ${configuration} DESTINATION "${CROWD_INSTALL_PATH_${configuration}}/procedurals/houdini/icons/libraryTool" )
    endforeach()

    foreach( install_file ${GLM_SCRIPT_FILES} )
        install( FILES ${install_file} CONFIGURATIONS ${configuration} DESTINATION "${CROWD_INSTALL_PATH_${configuration}}/procedurals/houdini/python/glm" )
    endforeach()
    
    foreach( install_file ${LAYOUT_SCRIPT_FILES} )
        install( FILES ${install_file} CONFIGURATIONS ${configuration} DESTINATION "${CROWD_INSTALL_PATH_${configuration}}/procedurals/houdini/python/glm/layout" )
    endforeach()

    foreach( install_file ${NODZ_SCRIPT_FILES} )
        install( FILES ${install_file} CONFIGURATIONS ${configuration} DESTINATION "${CROWD_INSTALL_PATH_${configuration}}/procedurals/houdini/python/glm/Nodz" )
    endforeach()

    foreach( install_file ${QTPY_SCRIPT_FILES} )
        install( FILES ${install_file} CONFIGURATIONS ${configuration} DESTINATION "${CROWD_INSTALL_PATH_${configuration}}/procedurals/houdini/python/glm/Qtpy" )
    endforeach()

    foreach( install_file ${HOU_UI_SCRIPT_FILES} )
        install( FILES ${install_file} CONFIGURATIONS ${configuration} DESTINATION "${CROWD_INSTALL_PATH_${configuration}}/procedurals/houdini/python/glm/ui" )
    endforeach()

    foreach( install_file ${HOU_SIMCACHELIB_SCRIPT_FILES} )
        install( FILES ${install_file} CONFIGURATIONS ${configuration} DESTINATION "${CROWD_INSTALL_PATH_${configuration}}/procedurals/houdini/python/glm/simCacheLib" )
    endforeach()

    foreach( install_file ${HOU_SHELF_FILES} )
        install( FILES ${install_file} CONFIGURATIONS ${configuration} DESTINATION "${CROWD_INSTALL_PATH_${configuration}}/procedurals/houdini/shelf" )
    endforeach()

    foreach( install_file ${SIMCACHELIB_SCRIPT_FILES} )
        install( FILES ${install_file} CONFIGURATIONS ${configuration} DESTINATION "${CROWD_INSTALL_PATH_${configuration}}/procedurals/houdini/python/glm/simCacheLib" )
    endforeach()

    install( FILES "${GOLAEM_SCRIPT_DIR}/glm/ui/__init__.py" CONFIGURATIONS ${configuration} DESTINATION "${CROWD_INSTALL_PATH_${configuration}}/procedurals/houdini/python/glm/ui" )
    install( FILES "${GOLAEM_SCRIPT_DIR}/glm/ui/windowWrapper.py" CONFIGURATIONS ${configuration} DESTINATION "${CROWD_INSTALL_PATH_${configuration}}/procedurals/houdini/python/glm/ui" )
    install( FILES "${GOLAEM_SCRIPT_DIR}/glm/ui/golaemAboutWindow.py" CONFIGURATIONS ${configuration} DESTINATION "${CROWD_INSTALL_PATH_${configuration}}/procedurals/houdini/python/glm/ui" )
    install( FILES "${CMAKE_SOURCE_DIR}/LICENSE" CONFIGURATIONS ${configuration} DESTINATION "${CROWD_INSTALL_PATH_${configuration}}/procedurals/houdini/python/glm/ui" )

    foreach( install_file ${ROOT_ICON_FILES} )
        install( FILES ${install_file} CONFIGURATIONS ${configuration} DESTINATION "${CROWD_INSTALL_PATH_${configuration}}/procedurals/houdini/icons" )
    endforeach()

    # devkit file
    install(TARGETS ${GLM_PYTHONPLUGIN_TARGET_NAME} CONFIGURATIONS ${configuration} DESTINATION "${CROWD_INSTALL_PATH_${configuration}}/procedurals/houdini/python/glm")
    install(FILES "${GOLAEM_BINARIES_DIR}/devkit.py" CONFIGURATIONS ${configuration} DESTINATION "${CROWD_INSTALL_PATH_${configuration}}/procedurals/houdini/python/glm")

endforeach()

