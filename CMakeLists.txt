# /****************************************************************************
# Copyright (c) 2013-2014 cocos2d-x.org
# Copyright (c) 2019-present Axmol Engine contributors (see AUTHORS.md).
#
# https://axmol.dev/
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
# ****************************************************************************/

cmake_minimum_required(VERSION 3.22...4.1)

set(APP_NAME  AxmolFighter-Editor)

project(${APP_NAME})

if(XCODE)
  set(CMAKE_XCODE_GENERATE_TOP_LEVEL_PROJECT_ONLY TRUE)
endif()

set(_is_axmol_embed FALSE)
set(_AX_USE_PREBUILT FALSE)

set(CMAKE_MODULE_PATH
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules
)

# NOTE: The order of the cmake module "include(AXGame...)" statements matters
include(AXGameEngineOptions)

set(AX_EXT_HINT OFF CACHE BOOL "The default extensions hint" FORCE)
set(AX_ENABLE_AUDIO OFF CACHE BOOL "Build audio support" FORCE)
set(AX_ENABLE_WEBSOCKET OFF CACHE BOOL "Build Websocket client based on yasio" FORCE)
set(AX_ENABLE_HTTP OFF CACHE BOOL "Build HTTP client based on yasio" FORCE)
set(AX_ENABLE_OPUS OFF CACHE BOOL "Build with opus support" FORCE)
set(AX_ENABLE_3D OFF CACHE BOOL "Build 3D support" FORCE)
set(AX_ENABLE_PHYSICS OFF CACHE BOOL "Build Physics support" FORCE)
set(AX_ENABLE_EXT_IMGUI ON CACHE BOOL "Build extension ImGui" FORCE)
set(AX_ENABLE_EXT_LUA OFF CACHE BOOL "Build lua libraries" FORCE)
set(AX_ENABLE_EXT_GUI OFF CACHE BOOL "Build extension GUI" FORCE)
set(AX_ENABLE_EXT_ASSETMANAGER OFF CACHE BOOL "Build extension asset-manager" FORCE)
set(AX_ENABLE_EXT_SPINE ON CACHE BOOL "Build extension spine" FORCE)
set(AX_ENABLE_EXT_DRAGONBONES OFF CACHE BOOL "Build extension DragonBones" FORCE)
set(AX_ENABLE_EXT_COCOSTUDIO OFF CACHE BOOL "Build extension cocostudio" FORCE)
set(AX_ENABLE_EXT_FAIRYGUI OFF CACHE BOOL "Build extension FairyGUI" FORCE)
set(AX_ENABLE_EXT_JSONDEFAULT OFF CACHE BOOL "Build extension JSONDefault" FORCE)
set(AX_ENABLE_EXT_PARTICLE3D OFF CACHE BOOL "Build extension Particle3D" FORCE)
set(AX_ENABLE_EXT_PHYSICS_NODE OFF CACHE BOOL "Build extension physics-nodes" FORCE)
set(AX_ENABLE_EXT_INSPECTOR OFF CACHE BOOL "Build extension Inspector" FORCE)
set(AX_ENABLE_EXT_SDFGEN OFF CACHE BOOL "Build extension SDFGen" FORCE)

include(AXGameEngineSetup)

# The common cross-platforms source files and header files
file(GLOB_RECURSE GAME_HEADER CONFIGURE_DEPENDS
  Source/*.h Source/*.hpp
)

file(GLOB_RECURSE GAME_SOURCE CONFIGURE_DEPENDS
  Source/*.cpp Source/*.c
)

# Spine 3.4 runtime is C; keep MSVC from compiling those files as C++.
file(GLOB SPINE34_C_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/Source/3rd/spine/spine_3_4/*.c")
set_source_files_properties(${SPINE34_C_SOURCES} PROPERTIES LANGUAGE C)

set(GAME_INC_DIRS
  "${CMAKE_CURRENT_SOURCE_DIR}/Source"
  "${CMAKE_CURRENT_SOURCE_DIR}/Source/3rd/spine" # <spine_3_4/...>
)

set(content_folder
  "${CMAKE_CURRENT_SOURCE_DIR}/Content"
)

if(APPLE)
  ax_mark_multi_resources(common_content_files RES_TO "Resources" FOLDERS ${content_folder})
elseif(WINDOWS)
  ax_mark_multi_resources(common_content_files RES_TO "Content" FOLDERS ${content_folder})
endif()

include(AXGameSourceSetup)

# mark app complie info and libs info
set(APP_SOURCES
  ${GAME_HEADER}
  ${GAME_SOURCE}
)

if(LINUX OR MACOSX OR WINDOWS)
  option(_AX_TESTS "Enable doctest" OFF)
endif()

if(_AX_TESTS)
  set(TEST_TARGET_NAME "${APP_NAME}_test")

  file(GLOB_RECURSE TEST_HEADER CONFIGURE_DEPENDS
    Tests/*.h Tests/*.hpp
  )
  list(APPEND TEST_HEADER ${GAME_HEADER})

  file(GLOB_RECURSE TEST_SOURCE CONFIGURE_DEPENDS
    Tests/*.cpp Tests/*.c
  )
  list(APPEND TEST_SOURCE ${GAME_SOURCE})

  set(TEST_TARGET_SOURCES
    ${APP_SOURCES}
    ${TEST_HEADER}
    ${TEST_SOURCE}
  )

 set(TEST_INC_DIRS
   "${CMAKE_CURRENT_SOURCE_DIR}/Tests"
   "${CMAKE_CURRENT_SOURCE_DIR}/Source"
   "${CMAKE_CURRENT_SOURCE_DIR}/Source/3rd/spine"
 )
endif()

include(AXGameTargetSetup)

# mark app resources, resource will be copy auto after mark
ax_setup_app_config(${APP_NAME})

if(_AX_TESTS)
  ax_setup_app_config(${TEST_TARGET_NAME} CONSOLE)
endif()

# Add any libraries you need to link to the project after this point
set(EDITOR_TARGETS ${APP_NAME})
if(_AX_TESTS)
  list(APPEND EDITOR_TARGETS ${TEST_TARGET_NAME})
endif()

foreach(EDITOR_TARGET ${EDITOR_TARGETS})
  target_include_directories(${EDITOR_TARGET} BEFORE PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/3rd/spine")
  target_include_directories(${EDITOR_TARGET} PRIVATE
    "${_AX_ROOT}/extensions/ImGui/src"
    "${_AX_ROOT}/extensions/ImGui/src/ImGui"
    "${_AX_ROOT}/extensions/spine/src"
    "${_AX_ROOT}/extensions/spine/runtime/include"
    "${_AX_ROOT}/3rdparty"
  )
  target_link_libraries(${EDITOR_TARGET} ImGui)
  if(TARGET spine)
    target_link_libraries(${EDITOR_TARGET} spine)
  endif()
  if(WINDOWS)
    target_link_libraries(${EDITOR_TARGET} shell32 ole32)
  endif()
endforeach()

if(TARGET axmol)
  target_compile_definitions(axmol PUBLIC AX_USE_CULLING=0)
endif()

# Default Platform-specific setup
include(AXGamePlatformSetup)

# Make sure the AXGameFinalSetup is included at the end of this file
include(AXGameFinalSetup)
