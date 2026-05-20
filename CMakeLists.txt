cmake_minimum_required(VERSION 3.13)

option (GOLAEMHOUDINI_STANDALONE_BUILD "Standalone build: ON/OFF" ON)

if(GOLAEMHOUDINI_STANDALONE_BUILD)
    ############################################################
	# BEGIN Declare various macros
	############################################################
	macro( list_files FOLDER_NAME FILE_EXTENSIONS )
        set( SKIP_INCLUDE_DIR OFF )
        if( ( ${ARGC} GREATER 2 ) AND ( NOT "${ARGV2}" STREQUAL "" ) )
            set( DIRECTORY_REFERENCE "${ARGV2}/${FOLDER_NAME}" )
            if( ( ${ARGC} GREATER 3 ) AND ( "${ARGV3}" STREQUAL "SKIP_INCLUDE_DIR" ) )
                set( SKIP_INCLUDE_DIR ON )
            endif()
        else()
            set( DIRECTORY_REFERENCE "${CMAKE_CURRENT_SOURCE_DIR}/${FOLDER_NAME}" )
        endif()
        if( EXISTS ${DIRECTORY_REFERENCE} )
            if( NOT SKIP_INCLUDE_DIR )
                include_directories( ${DIRECTORY_REFERENCE} )
            endif()
            if( "${FOLDER_NAME}" STREQUAL "." )
                set( DIRECTORY_FILTER "Source Files" )
                set( DIRECTORY_LABEL "ROOT" )
            else()
                set( DIRECTORY_FILTER "Source Files/${FOLDER_NAME}" )
                string( REGEX REPLACE "/\\." "" DIRECTORY_FILTER ${DIRECTORY_FILTER} )
                string( REGEX REPLACE "\\./" "" DIRECTORY_FILTER ${DIRECTORY_FILTER} )
                string( REGEX REPLACE "/" "\\\\\\\\" DIRECTORY_FILTER ${DIRECTORY_FILTER} )
                set( DIRECTORY_LABEL "${FOLDER_NAME}" )
                string( REGEX REPLACE "/\\." "" DIRECTORY_LABEL ${DIRECTORY_LABEL} )
                string( REGEX REPLACE "\\./" "" DIRECTORY_LABEL ${DIRECTORY_LABEL} )
                string( REGEX REPLACE "/" "_" DIRECTORY_LABEL ${DIRECTORY_LABEL} )
                string( TOUPPER "${DIRECTORY_LABEL}" DIRECTORY_LABEL )
            endif()
            foreach( file_ext ${FILE_EXTENSIONS} )
                string( TOUPPER "${file_ext}" _FILE_EXT )
                unset( LIST_FILES_${DIRECTORY_LABEL}_${_FILE_EXT} )
                file( GLOB LIST_FILES_${DIRECTORY_LABEL}_${_FILE_EXT} ${DIRECTORY_REFERENCE}/*.${file_ext} )
                list( APPEND LIST_FILES "${LIST_FILES_${DIRECTORY_LABEL}_${_FILE_EXT}}" )
                list( APPEND LIST_FILES_${_FILE_EXT} "${LIST_FILES_${DIRECTORY_LABEL}_${_FILE_EXT}}" )
                source_group( "${DIRECTORY_FILTER}" FILES ${LIST_FILES_${DIRECTORY_LABEL}_${_FILE_EXT}} )
                # unset( LIST_FILES_${DIRECTORY_LABEL}_${_FILE_EXT} )
            endforeach()
        else()
        endif()

    endmacro()

    ############################################################
    # END Declare various macros
    ############################################################
    ############################################################
    # BEGIN Project
    ############################################################

    project(GolaemHoudini)

    if( NOT MSVC )
        # Needed to ensure that RPATH is set ASAP, even before copying installed libraries in multiple locations
        set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
        # set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
    endif()

    set(CMAKE_VS_INCLUDE_INSTALL_TO_DEFAULT_BUILD 1)

    # Add dependencies
    set( CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cfg/cmake")
    
    # Add dependencies
    find_package(Houdini REQUIRED)
    set(BUILD_HOUDINI ON) # to be able to use Houdini_VERSION variable in FindFbxSdk.cmake
    find_package(FbxSdk REQUIRED)

    # Golaem
    set( GOLAEMINSTALL_ROOTDIR "Golaem Install Root Dir" CACHE PATH "Golaem Install directory" )
    # check that the provided Golaem install directory is valid by checking the presence of a known file
    if( NOT EXISTS "${GOLAEMINSTALL_ROOTDIR}/devkit/include/glmCore.h" )
        message(FATAL_ERROR "The provided Golaem install directory (${GOLAEMINSTALL_ROOTDIR}) is not valid. Please set it to a valid Golaem install directory (that contains at least 'devkit/include/glmCore.h' file)." )
    endif()
    set( GOLAEMDEVKIT_INCDIR "${GOLAEMINSTALL_ROOTDIR}/devkit/include")
    set( GOLAEMDEVKIT_LIBDIR "${GOLAEMINSTALL_ROOTDIR}/devkit/lib")
    if(MSVC)
        file(GLOB GOLAEMDEVKIT_LIBS CONFIGURE_DEPENDS "${GOLAEMDEVKIT_LIBDIR}/glmCore*.lib" "${GOLAEMDEVKIT_LIBDIR}/glmCrowdIO*.lib")
    else()
        file(GLOB GOLAEMDEVKIT_LIBS CONFIGURE_DEPENDS "${GOLAEMDEVKIT_LIBDIR}/libglmCore*.so" "${GOLAEMDEVKIT_LIBDIR}/libglmCrowdIO*.so")
    endif(MSVC)

    # List Files
    list_files( "src" "h;cpp" )
    list_files( "src/utils" "h;cpp" )
    list_files( "src/procedural" "h;cpp" )
    list_files( "python/glm/ui" "py" )
    list_files( "python/glm/simCacheLib" "py" )
    list_files( "shelf" "shelf" )

    # Create a library
    add_library( ${PROJECT_NAME} SHARED ${LIST_FILES} )

    target_include_directories(${PROJECT_NAME} PRIVATE ${FBXSDK_INCDIR})
    target_include_directories(${PROJECT_NAME} PRIVATE ${GOLAEMDEVKIT_INCDIR} )
    if(MSVC)
        set_target_properties( ${PROJECT_NAME} PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD_DEBUG ON )
    endif()
    target_link_libraries( ${PROJECT_NAME} ${FBXSDK_LIBS})
    target_link_libraries(${PROJECT_NAME} ${GOLAEMDEVKIT_LIBS} )
    target_link_libraries(${PROJECT_NAME} Houdini)

    if(MSVC)
        target_compile_options(${PROJECT_NAME} PRIVATE "-DNOMINMAX" "/MP" "/nologo" "/wd4251")
    else()
        # flags needed for some usd includes (that use hash_set)
        target_compile_options(${PROJECT_NAME} PRIVATE "-Wno-deprecated")

        # follow the vfx reference platform: https://vfxplatform.com/
        if( CMAKE_COMPILER_IS_GNUCC )
            if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 7.0) # gcc 5.x 6.x
                target_compile_options(${PROJECT_NAME} PUBLIC "-std=c++14" "-D_GLIBCXX_USE_CXX11_ABI=0")
            elseif(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 11.0) # gcc 7.x 8.x 9.x 10.x
                target_compile_options(${PROJECT_NAME} PUBLIC "-std=c++17" "-D_GLIBCXX_USE_CXX11_ABI=0") 
            else() # gcc 11.x (used versions should be 11.1 at least)
                target_compile_options(${PROJECT_NAME} PUBLIC "-std=c++17" "-D_GLIBCXX_USE_CXX11_ABI=1")
            endif()
        endif()

        set_target_properties( ${PROJECT_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN/../lib:$ORIGIN/../../lib:$ORIGIN/../../../lib" )
    endif()
    if (FBXSDK_DEFINITIONS)
        target_compile_options(${PROJECT_NAME} PRIVATE ${FBXSDK_DEFINITIONS})
    endif()
    set_target_properties( ${PROJECT_NAME} PROPERTIES PREFIX "" )

    set(GOLAEM_ROOT_ICON_FILES "${GOLAEMINSTALL_ROOTDIR}/icons/buttonSimulationCacheLibrary.png")
    list(APPEND GOLAEM_ROOT_ICON_FILES "${GOLAEMINSTALL_ROOTDIR}/icons/glmDefaultIcon.png")
    list(APPEND GOLAEM_ROOT_ICON_FILES "${GOLAEMINSTALL_ROOTDIR}/icons/glmAbout.png")
    list(APPEND GOLAEM_ROOT_ICON_FILES "${GOLAEMINSTALL_ROOTDIR}/icons/SimulationCacheLibrary.png")
    list(APPEND GOLAEM_ROOT_ICON_FILES "${GOLAEMINSTALL_ROOTDIR}/icons/SimulationCacheLayout.png")
    list(APPEND GOLAEM_ROOT_ICON_FILES "${GOLAEMINSTALL_ROOTDIR}/icons/SimulationCacheProxy.png")
    list(APPEND GOLAEM_ROOT_ICON_FILES "${GOLAEMINSTALL_ROOTDIR}/icons/buttonSimulationCacheLayout.png")
    list(APPEND GOLAEM_ROOT_ICON_FILES "${GOLAEMINSTALL_ROOTDIR}/icons/W09_GOLM_about_528x90.png")

    foreach( configuration "Debug" "Release" )
        if( MSVC )
            set(GOLAEM_BINARIES_DIR_NAME "bin")
            install (TARGETS ${PROJECT_NAME} RUNTIME CONFIGURATIONS ${configuration} DESTINATION "${CMAKE_INSTALL_PREFIX}/procedurals/houdini" )
        else()
            set(GOLAEM_BINARIES_DIR_NAME "lib")
            install (TARGETS ${PROJECT_NAME} CONFIGURATIONS ${configuration} DESTINATION "${CMAKE_INSTALL_PREFIX}/procedurals/houdini" )
        endif()
        install (DIRECTORY "${GOLAEMINSTALL_ROOTDIR}/scripts/glm" CONFIGURATIONS ${configuration} DESTINATION "${CMAKE_INSTALL_PREFIX}/procedurals/houdini/python")
        foreach( icon_file ${GOLAEM_ROOT_ICON_FILES} )
            install( FILES ${icon_file} CONFIGURATIONS ${configuration} DESTINATION "${CMAKE_INSTALL_PREFIX}/procedurals/houdini/icons" )
        endforeach()
        install (DIRECTORY "${GOLAEMINSTALL_ROOTDIR}/icons/layoutToolv7" CONFIGURATIONS ${configuration} DESTINATION "${CMAKE_INSTALL_PREFIX}/procedurals/houdini/icons")
        install (DIRECTORY "${GOLAEMINSTALL_ROOTDIR}/icons/editors" CONFIGURATIONS ${configuration} DESTINATION "${CMAKE_INSTALL_PREFIX}/procedurals/houdini/icons")
        install (DIRECTORY "${GOLAEMINSTALL_ROOTDIR}/icons/libraryTool" CONFIGURATIONS ${configuration} DESTINATION "${CMAKE_INSTALL_PREFIX}/procedurals/houdini/icons")
        install (DIRECTORY "${CMAKE_SOURCE_DIR}/python/glm" CONFIGURATIONS ${configuration} DESTINATION "${CMAKE_INSTALL_PREFIX}/procedurals/houdini/python")
        install (DIRECTORY "${CMAKE_SOURCE_DIR}/shelf" CONFIGURATIONS ${configuration} DESTINATION "${CMAKE_INSTALL_PREFIX}/procedurals/houdini")
        install (DIRECTORY "${GOLAEMINSTALL_ROOTDIR}/devkit" CONFIGURATIONS ${configuration} DESTINATION "${CMAKE_INSTALL_PREFIX}")
        install (DIRECTORY "${GOLAEMINSTALL_ROOTDIR}/${GOLAEM_BINARIES_DIR_NAME}" CONFIGURATIONS ${configuration} DESTINATION "${CMAKE_INSTALL_PREFIX}")

    endforeach()
    ############################################################
    # END Project
    ############################################################
else()
    ############################################################
    # BEGIN Project
    ############################################################

    project(${TARGET_GOLAEMHOUDINI})

    # List Files
    list_files( "src" "h;cpp" )
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

    set(ROOT_ICON_FILES "${GOLAEM_ICON_DIR}/buttonSimulationCacheLibrary.png")
    list(APPEND ROOT_ICON_FILES "${GOLAEM_ICON_DIR}/glmDefaultIcon.png")
    list(APPEND ROOT_ICON_FILES "${GOLAEM_ICON_DIR}/glmAbout.png")
    list(APPEND ROOT_ICON_FILES "${GOLAEM_ICON_DIR}/SimulationCacheLibrary.png")
    list(APPEND ROOT_ICON_FILES "${GOLAEM_ICON_DIR}/SimulationCacheLayout.png")
    list(APPEND ROOT_ICON_FILES "${GOLAEM_ICON_DIR}/SimulationCacheProxy.png")
    list(APPEND ROOT_ICON_FILES "${GOLAEM_ICON_DIR}/buttonSimulationCacheLayout.png")
    list(APPEND ROOT_ICON_FILES "${GOLAEM_ICON_DIR}/W09_GOLM_about_528x90.png")

    foreach( configuration "Debug" "Release" )
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
        install( FILES "${GOLAEM_SCRIPT_DIR}/glm/ui/golaemAboutWindowHoudini.py" CONFIGURATIONS ${configuration} DESTINATION "${CROWD_INSTALL_PATH_${configuration}}/procedurals/houdini/python/glm/ui" )
        install( FILES "${CMAKE_SOURCE_DIR}/LICENSE" CONFIGURATIONS ${configuration} DESTINATION "${CROWD_INSTALL_PATH_${configuration}}/procedurals/houdini/python/glm/ui" )

        foreach( install_file ${ROOT_ICON_FILES} )
            install( FILES ${install_file} CONFIGURATIONS ${configuration} DESTINATION "${CROWD_INSTALL_PATH_${configuration}}/procedurals/houdini/icons" )
        endforeach()

        # devkit file
        install(TARGETS ${GLM_PYTHONPLUGIN_TARGET_NAME} CONFIGURATIONS ${configuration} DESTINATION "${CROWD_INSTALL_PATH_${configuration}}/procedurals/houdini/python/glm")
        install(FILES "${GOLAEM_BINARIES_DIR}/devkit.py" CONFIGURATIONS ${configuration} DESTINATION "${CROWD_INSTALL_PATH_${configuration}}/procedurals/houdini/python/glm")

    endforeach()
    ############################################################
    # END Project
    ############################################################
endif()