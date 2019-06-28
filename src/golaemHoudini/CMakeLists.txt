cmake_minimum_required(VERSION 3.13)

############################################################
# BEGIN Project
############################################################

project(${TARGET_GOLAEMHOUDINI})

# List Files
LIST_FILES( "." "h;cpp" )
list_files( "utils" "h;cpp" )
list_files( "procedural" "h;cpp" )

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
