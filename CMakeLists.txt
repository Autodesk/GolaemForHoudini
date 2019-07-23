cmake_minimum_required(VERSION 3.13)

############################################################
# BEGIN Project
############################################################

project(${TARGET_GOLAEMHOUDINI})

# List Files
LIST_FILES( "src" "h;cpp" )
list_files( "src/utils" "h;cpp" )
list_files( "src/procedural" "h;cpp" )

# Create a library
add_library( ${PROJECT_NAME} SHARED ${LIST_FILES} )

string(REGEX REPLACE "^glm" "HOUDINI_" CUSTOM_PROJECT_LABEL "${PROJECT_NAME}" )

crowd_label_project( ${PROJECT_NAME} )
set_target_properties( ${PROJECT_NAME} PROPERTIES PROJECT_LABEL ${CUSTOM_PROJECT_LABEL} )

crowd_load_dependencies( ${PROJECT_NAME} )
set_target_prefix( ${PROJECT_NAME} PLUGIN )
set_target_postfix( ${PROJECT_NAME} PLUGIN )

target_link_libraries(${PROJECT_NAME} Houdini)

CROWD_INSTALL_RUNTIME_LIBRARY( ${PROJECT_NAME} "procedurals/houdini" "GolaemHoudini")
crowd_install_dependencies()

message("CMAKE_CURRENT_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}")
file( GLOB LICENSE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/licenses/*.lic )
foreach( configuration "Debug" "Release" )
    foreach( license_file ${LICENSE_FILES} )
        install( FILES ${license_file} CONFIGURATIONS ${configuration} DESTINATION "${CROWD_INSTALL_PATH_${configuration}}/procedurals/houdini" )                                                          # install licenses
    endforeach()
endforeach()