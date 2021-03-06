# Set project name
project(tundra)

# CMake version requirement
cmake_minimum_required(VERSION 2.4)

macro (PrintUsedEnvVariables)
    # Print out the resolved variables
    message ("\n=========== Used Environment Variables ===========\n")
    message (STATUS "TUNDRA_DEP_PATH  = " ${ENV_TUNDRA_DEP_PATH})
    message (STATUS "QTDIR            = " ${ENV_QT_DIR})
    message (STATUS "OGRE_HOME        = " ${ENV_OGRE_HOME})
endmacro ()

macro (PrintUsedBuildConfig)
    message ("\n=========== Used Build Configuration =============\n")
    message (STATUS "BUILD_SDK_ONLY            = " ${BUILD_SDK_ONLY})
    message (STATUS "BUILDING_INSTALLER        = " ${BUILDING_INSTALLER})
    message (STATUS "WINDOWS_APP               = " ${WINDOWS_APP})
    message (STATUS "ENABLE_HYDRAX             = " ${ENABLE_HYDRAX})
    message (STATUS "ENABLE_SKYX               = " ${ENABLE_SKYX})
    message (STATUS "ENABLE_OGRE_ASSET_EDITOR  = " ${ENABLE_OGRE_ASSET_EDITOR})
    message (STATUS "ENABLE_OPEN_ASSET_IMPORT  = " ${ENABLE_OPEN_ASSET_IMPORT})
    message (STATUS "ENABLE_PROFILING          = " ${ENABLE_PROFILING})
    message (STATUS "ENABLE_JS_PROFILING       = " ${ENABLE_JS_PROFILING})
    message (STATUS "ENABLE_MEMORY_LEAK_CHECKS = " ${ENABLE_MEMORY_LEAK_CHECKS})
    message (STATUS "ENABLE_SPLASH_SCREEN      = " ${ENABLE_SPLASH_SCREEN})
    message ("")
    message (STATUS "Install prefix = " ${CMAKE_INSTALL_PREFIX})
    message ("")
endmacro ()

# The AddEntityComponent macro adds the static EC lib from the given sourceFolder directory to the build.
# The componentName must be the name of the EC that will be added. E.g. AddEntityComponent(EntityComponents/EC_Highlight EC_Highlight)
# Use this macro when adding ECs to the build, since this macro tracks the set of ECs that are added, so that the individual
# modules get #defines in C++ code for each EC that is present. This allows conditionally excluding ECs from the build.
macro(AddEntityComponent componentName)
   add_subdirectory(src/EntityComponents/${componentName})
   set(${componentName}_ENABLED 1)
   set(USED_ENTITYCOMPONENTS ${USED_ENTITYCOMPONENTS} ${componentName})
endmacro()

# The AddProject takes in two parameters: projectType == Core or Application and projectName that should essentially be the folder name of the project.
macro(AddProject projectType projectName)
    set (addProjectSubdirectory 1)
    if (NOT ${projectType} STREQUAL "Core" AND NOT ${projectType} STREQUAL "Application")
        message (FATAL_ERROR "You are trying to use AddProject macro with invalid project type parameter: " ${projectType} " with project name " ${projectName})
        set (addProjectSubdirectory 0)
    endif()
    if (BUILD_SDK_ONLY AND ${projectType} STREQUAL "Application")
        message (STATUS "** Omitting Application level project " ${projectName} " because BUILD_SDK_ONLY is set to TRUE")
        set (addProjectSubdirectory 0)
    endif ()
    if (addProjectSubdirectory)
        add_subdirectory(src/${projectType}/${projectName})
    endif()
endmacro()

# Generates in the current project a #define for each EC that has been added to the build. Use this macro in your module CMakeLists.txt
# to receive information in C++ about which ECs have been added to the project. This allows supporting conditional compilation of the ECs in your module.
macro(GetEntityComponents)
   foreach(componentName ${USED_ENTITYCOMPONENTS})
      add_definitions(-D${componentName}_ENABLED)
   endforeach()
endmacro()

macro(GetOpenAssetImport)
	if (ENABLE_OPEN_ASSET_IMPORT)
		use_app_modules(OpenAssetImport)
		link_modules(OpenAssetImport)
		add_definitions(-DASSIMP_ENABLED)
	endif()
endmacro()

macro(GetOgreAssetEditor)
	if (ENABLE_OGRE_ASSET_EDITOR)
		use_app_modules(OgreAssetEditorModule)
		link_modules(OgreAssetEditorModule)
		add_definitions(-DOGREASSETEDITOR_ENABLED)
	endif()
endmacro()

# Adds the given folder_name into the source files of the current project. Use this macro when your module contains .cpp and .h files in several subdirectories.
macro(AddSourceFolder folder_name)
    file(GLOB H_FILES_IN_FOLDER_${folder_name} ${folder_name}/*.h)
    file(GLOB CPP_FILES_IN_FOLDER_${folder_name} ${folder_name}/*.cpp)
    source_group("Header Files\\${folder_name}" FILES ${H_FILES_IN_FOLDER_${folder_name}})
    source_group("Source Files\\${folder_name}" FILES ${CPP_FILES_IN_FOLDER_${folder_name}})
    set(H_FILES ${H_FILES} ${H_FILES_IN_FOLDER_${folder_name}})
    set(CPP_FILES ${CPP_FILES} ${CPP_FILES_IN_FOLDER_${folder_name}})
endmacro()

# Moves all Qt moc-compiler generated files into their own folder in the Visual Studio project. Call this once after having added all source files in the build in your module,
# but before the call to 'set (SOURCE_FILES ...)' directive.
# This macro is optional and for "conveniency" only. If you omit this call, you will have all the generated moc files in the Visual Studio project root.
# TODO: Add support for more deep folder structures. Current
#       implementation only support folders directly under project folder
macro(MocFolder)
    file(GLOB MOCS_TO_SOURCE_GROUP *.cxx */*.cxx)
    source_group("CMake Moc" FILES ${MOCS_TO_SOURCE_GROUP})
endmacro()

# Moves all Qt ui copmiler-generated .h files into their own folder in the Visual Studio project. Call this once after having added all source files in the build in your module.
# This macro is optional and for "conveniency" only. If you omit this call, you will have all the generated moc files in the Visual Studio project root.
macro(UiFolder)
    file(GLOB GENERATED_UI_H ui_*.h)
    source_group("Generated UI" FILES ${GENERATED_UI_H})
endmacro()

# Enables the use of Precompiled Headers in the project this macro is invoked in. Also adds the DEBUG_CPP_NAME to each .cpp file that specifies the name of that compilation unit. MSVC only.
macro(SetupCompileFlagsWithPCH)
    if (MSVC)
        # Label StableHeaders.cpp to create the PCH file and mark all other .cpp files to use that PCH file.
        # Add a #define DEBUG_CPP_NAME "this compilation unit name" to each compilation unit to aid in memory leak checking.
        foreach(src_file ${CPP_FILES})
            if (${src_file} MATCHES "StableHeaders.cpp$")
                set_source_files_properties(${src_file} PROPERTIES COMPILE_FLAGS "/YcStableHeaders.h")        
            else()
                get_filename_component(basename ${src_file} NAME)
                set_source_files_properties(${src_file} PROPERTIES COMPILE_FLAGS "/YuStableHeaders.h -DDEBUG_CPP_NAME=\"\\\"${basename}\"\\\"")
            endif()
        endforeach()
    endif()
endmacro()

# Sets up the compilation flags without PCH. For now just set the DEBUG_CPP_NAME to each compilation unit.
# TODO: The SetupCompileFlags and SetupCompileFlagsWithPCH macros should be merged, and the option to use PCH be passed in as a param. However,
# CMake string ops in PROPERTIES COMPILE_FLAGS gave some problems with this, so these are separate for now.
macro(SetupCompileFlags)
    if (MSVC)
        # Add a #define DEBUG_CPP_NAME "this compilation unit name" to each compilation unit to aid in memory leak checking.
        foreach(src_file ${CPP_FILES})
            if (${src_file} MATCHES "StableHeaders.cpp$")
            else()
                get_filename_component(basename ${src_file} NAME)
                set_source_files_properties(${src_file} PROPERTIES COMPILE_FLAGS "-DDEBUG_CPP_NAME=\"\\\"${basename}\"\\\"")
            endif()
        endforeach()
    endif()
endmacro()

# Set CMake library search policy
if (COMMAND cmake_policy)
    cmake_policy(SET CMP0003 NEW)
    cmake_policy(SET CMP0005 NEW)
endif()

# Detect what compiler we are using on windows
set (VC90 FALSE)
set (VC100 FALSE)
if (MSVC)
    message ("")
    message (STATUS "Detecting Visual Studio compiler")
    if (${CMAKE_GENERATOR} STREQUAL "Visual Studio 10" OR ${CMAKE_GENERATOR} STREQUAL "Visual Studio 10 Win64")
        set (VC100 TRUE)
        message (STATUS "-- VC100 detected")
    endif ()
    if (${CMAKE_GENERATOR} STREQUAL "Visual Studio 9 2008" OR ${CMAKE_GENERATOR} STREQUAL "Visual Studio 9 2008 Win64")
        set (VC90 TRUE)
        message (STATUS "-- VC90 detected")
    endif ()
    if (NOT VC90 AND NOT VC100)
        message (STATUS "-- Warning: Could not determine compiler from cmake generator name. We support 'officially' Visual Studio 2008 and 2010")
    endif ()
endif ()

# Set compiler warning levels.
# On MSVC, use warning level 4 but disable the following warnings:
# C4100 'identifier' : unreferenced formal parameter
# C4127 conditional expression is constant
# C4512 'class' : assignment operator could not be generated 
# C4505 'function' : unreferenced local function has been removed
# C4251 'identifier' : class 'type' needs to have dll-interface to be used by clients of class 'type2'
if (MSVC)
    add_definitions (/W4 /wd4100 /wd4127 /wd4512 /wd4505 /wd4251)
else ()
    add_definitions (-Wall -Wextra -Wno-unknown-pragmas -Wno-unused -Wno-reorder -Wno-switch -Wno-write-strings)
endif()

# Set Kristalli path
set(KRISTALLI_CORE_PATH ${PROJECT_BINARY_DIR}/Kristalli)

# Set CMake custom module path & include them
# The following CMake modules are required for the project to build.
set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} cmake)
include (Sagase)
include (ModuleSetup)
include (ConfigurePackages)
include (TundraInstall)
include (FindOgre)
include (FindDirectX)

# Disable unnecessary build types
set(CMAKE_CONFIGURATION_TYPES "Release;RelWithDebInfo;Debug" CACHE STRING "Configurations" FORCE)

if (MSVC)
    # In Visual Studio, use unicode character set
    add_definitions(-DUNICODE -D_UNICODE)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)

    #Define NOMINMAX in order to get rid of the evil WinDefs.h min&max defines.
    add_definitions(-DNOMINMAX)
endif()

# On all platforms, we enable PCH files by default.
add_definitions (-DPCH_ENABLED)
SET(PCH_ENABLED 1)

# Set global hardcoded install prefix. User cannot change this at the moment, untill we figure how we want to use this!
# Call the cleanup step that cleans the install prefix before every installations. This is important as module setups might change between builds.
set (CMAKE_INSTALL_PREFIX ${PROJECT_BINARY_DIR}/build)
set (TUNDRA_BIN ${PROJECT_BINARY_DIR}/bin)
setup_clean_install_step()

# This setting affects only windows. Possibility to opt out of linking agains DirecX spesific libs.
# Enabled more efficient texture blitting to Ogre using DirectX. This makes RenderSystem_Direct3D9 mandatory.
set (ENABLE_DIRECTX 1)

# Set global hardcoded install prefix. User cannot change this at the moment, untill we figure how we want to use this!
# Call the cleanup step that cleans the install prefix before every installations. This is important as module setups might change between builds.
set (CMAKE_INSTALL_PREFIX ${PROJECT_BINARY_DIR}/build)
set (TUNDRA_BIN ${PROJECT_BINARY_DIR}/bin)
setup_clean_install_step()

# Set normalized path for common environment variables
file (TO_CMAKE_PATH "$ENV{TUNDRA_DEP_PATH}" ENV_TUNDRA_DEP_PATH)
file (TO_CMAKE_PATH "$ENV{QTDIR}" ENV_QT_DIR)
file (TO_CMAKE_PATH "$ENV{OGRE_HOME}" ENV_OGRE_HOME)

# Utilize cmakes cache for the environment variables. This is needed if eg. TUNDRA_DEP_PATH
# was set on the first run of cmake, but once eg. Visual Studio auto runs cmake (after changes to any CMakeLists.txt)
# it is no longer set. This will result us in losing all the acquired dependency information that was not found with
# cmake macros find_packade, find_library etc. that automatically cache. This is more flexible for many use cases to remember
# if we once got a valid TUNDRA_DEP_PATH. If env variable TUNDRA_DEP_PATH is present, the cache is never read but instead updated.
if (NOT ENV_TUNDRA_DEP_PATH)
    set (ENV_TUNDRA_DEP_PATH ${CACHED_ENV_TUNDRA_DEP_PATH})
else ()
    set (CACHED_ENV_TUNDRA_DEP_PATH ${ENV_TUNDRA_DEP_PATH} CACHE PATH "Tundra build dependency path" FORCE)
endif ()
if (NOT ENV_QT_DIR)
    set (ENV_QT_DIR ${CACHED_ENV_QT_DIR})
else ()
    set (CACHED_ENV_QT_DIR ${ENV_QT_DIR} CACHE PATH "Tundra build Qt path" FORCE)
endif ()
if (NOT ENV_OGRE_HOME)
    set (ENV_OGRE_HOME ${CACHED_ENV_OGRE_HOME})
else ()
    set (CACHED_ENV_OGRE_HOME ${ENV_OGRE_HOME} CACHE PATH "Tundra build Ogre path" FORCE)
endif ()

# If we still havent resolved TUNDRA_DEP_PATH report a fatal error.
# ENV_QT_DIR and ENV_OGRE_HOME are not fatal if missing.
if (NOT ENV_TUNDRA_DEP_PATH)
    message(FATAL_ERROR "TUNDRA_DEP_PATH could not be resolved from a environment variable or from cmake cache!")
endif ()

message ("\n\n=========== Configuring Dependencies ===========\n")

# Find needed external libraries, abort on error
configure_boost()
configure_qt4()
configure_python()
configure_python_qt()
configure_ogre()
configure_qtpropertybrowser()
configure_openal ()
configure_ogg ()
configure_vorbis ()
configure_theora()

# This will find kNet and set the global include and library directories
use_package_knet() 

message ("\n** Adding global include and link directories")
use_package(BOOST)
use_package(OGRE)
use_package(QT4)
use_package(OPENAL)
use_package(OGG)
use_package(VORBIS)

set (CORE_MODULE_DIRECTORY ${PROJECT_BINARY_DIR}/src/Core)
include_directories (${CORE_MODULE_DIRECTORY}/Input)
include_directories (${CORE_MODULE_DIRECTORY}/Ui)
include_directories (${CORE_MODULE_DIRECTORY}/Asset)
include_directories (${CORE_MODULE_DIRECTORY}/Scene)

link_directories (${CORE_MODULE_DIRECTORY}/Input)
link_directories (${CORE_MODULE_DIRECTORY}/Ui)
link_directories (${CORE_MODULE_DIRECTORY}/Asset)
link_directories (${CORE_MODULE_DIRECTORY}/Scene)

# If the custom optional modules configuration file does not yet
# exist, create it from the template file.
if (NOT EXISTS CMakeBuildConfig.txt)
    execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/CMakeBuildConfigTemplate.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/CMakeBuildConfig.txt)
endif ()

# The following two defines are required to compile.
# These enable implicit conversions of common math types between different libraries and our internal math library.
add_definitions(-DQT_INTEROP)
add_definitions(-DIRRLICHT_INTEROP)
add_definitions(-DMATH_ENABLE_STL_SUPPORT)

# Read the set of optional modules from another file
# that is kept outside the source control. 
# To configure the set of optional modules to add to the build,
# edit CMakeBuildConfig.txt and rerun cmake.
include(CMakeBuildConfig.txt)

# Collect translation files.
file(GLOB TRANSLATIONS_FILES bin/data/translations/*.ts)

# Collect ui-files which are stored into data/
file(GLOB ui_src bin/data/ui/*.ui)

set(FILES_TO_TRANSLATE ${FILES_TO_TRANSLATE} ${ui_src})

if (UPDATE_LANGUAGE_TRANSLATIONS)
   message (STATUS "Update .ts files with new translation data.")
   update_translation_files(TRANSLATIONS_FILES)
endif()

update_qm_files(TRANSLATIONS_FILES)
message (STATUS "Language translations generated")

PrintUsedEnvVariables ()
PrintUsedBuildConfig ()
