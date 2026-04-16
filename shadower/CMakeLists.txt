#
# Copyright 2013-2023 Software Radio Systems Limited
#
# This file is part of srsRAN
#
# srsRAN is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as
# published by the Free Software Foundation, either version 3 of
# the License, or (at your option) any later version.
#
# srsRAN is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU Affero General Public License for more details.
#
# A copy of the GNU Affero General Public License can be found in
# the LICENSE file in the top-level directory of this distribution
# and at http://www.gnu.org/licenses/.
#
# Override parent -std=c++14 in this subdirectory and use C++20 here.
string(REPLACE "-std=c++14" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
# Global compiler options
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# RPATH
set(CMAKE_BUILD_RPATH_USE_ORIGIN ON)
set(CMAKE_BUILD_RPATH $ORIGIN bin)

if(NOT Boost_FOUND)
  message(FATAL_ERROR "Boost required to compile srsUE and ")
endif()

include_directories(
  ${Boost_INCLUDE_DIRS}
  ${SEC_INCLUDE_DIRS}
  ${PROJECT_SOURCE_DIR}
)

link_directories(
  ${Boost_LIBRARY_DIRS}
  ${SEC_LIBRARY_DIRS}
)
option(ENABLE_UHD "Enable UHD" ON)
# UHD
if(ENABLE_UHD)
  find_package(UHD)
  if(UHD_FOUND)
    include_directories(${UHD_INCLUDE_DIRS})
    link_directories(${UHD_LIBRARY_DIRS})
  endif(UHD_FOUND)
endif(ENABLE_UHD)

if(BUILD_STATIC)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++ -static-libgcc")
endif(BUILD_STATIC)

if(RPATH)
  set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
endif(RPATH)

set(SRSUE_SOURCES srsue_phy srsue_stack srsue_upper srsue_mac srsue_rrc srslog system)
set(SRSRAN_SOURCES srsran_common srsran_mac srsran_phy srsran_radio srsran_gtpu srsran_rlc srsran_pdcp rrc_asn1 srslog support system)

set(SRSUE_SOURCES ${SRSUE_SOURCES} srsue_nr_stack srsue_rrc_nr srsue_mac_nr)
set(SRSRAN_SOURCES ${SRSRAN_SOURCES} rrc_nr_asn1 ngap_nr_asn1)
include_directories(${CMAKE_SOURCE_DIR}/hdr)

find_package(PkgConfig REQUIRED)
pkg_search_module(GLIB REQUIRED glib-2.0)

# Add the wireshark library
set(WIRESHARK_EXTERNAL_PATH /opt/wireshark)
set(WIRESHARK_BUILD_PATH ${WIRESHARK_EXTERNAL_PATH}/build)
set(WIRESHARK_RUN_PATH ${WIRESHARK_BUILD_PATH}/run)

if(EXISTS "${WIRESHARK_RUN_PATH}/libwireshark.so" AND
    EXISTS "${WIRESHARK_RUN_PATH}/libwiretap.so" AND
    EXISTS "${WIRESHARK_RUN_PATH}/libwsutil.so")
  add_library(wireshark_epan SHARED IMPORTED)
  set_property(TARGET wireshark_epan PROPERTY IMPORTED_LOCATION "${WIRESHARK_RUN_PATH}/libwireshark.so")

  add_library(wireshark_wiretap SHARED IMPORTED)
  set_property(TARGET wireshark_wiretap PROPERTY IMPORTED_LOCATION "${WIRESHARK_RUN_PATH}/libwiretap.so")

  add_library(wireshark_wsutil SHARED IMPORTED)
  set_property(TARGET wireshark_wsutil PROPERTY IMPORTED_LOCATION "${WIRESHARK_RUN_PATH}/libwsutil.so")

  set(WIRESHARK_EPAN_INCLUDES
    ${WIRESHARK_EXTERNAL_PATH}
    ${WIRESHARK_EXTERNAL_PATH}/include
    ${WIRESHARK_BUILD_PATH}
  )

  set(WIRESHARK_EPAN_ENABLED ON)
  # Ensure EPAN-enabled binaries resolve against the matching in-tree
  # Wireshark libs before the legacy bin/ symlinked copies.
  set(CMAKE_BUILD_RPATH "${WIRESHARK_RUN_PATH};$ORIGIN;bin")
  message("Wireshark EPAN backend enabled")
else()
  message(WARNING "Wireshark EPAN backend requested but build artifacts were not found under ${WIRESHARK_RUN_PATH}")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_definitions(-DDBG)
endif()

set(SHADOWER_DIR ${CMAKE_HOME_DIRECTORY}/shadower)

add_subdirectory(source)
add_subdirectory(tools)
add_subdirectory(comp)
add_subdirectory(utils)
add_subdirectory(test)
add_subdirectory(modules)

add_executable(shadower main.cc comp/scheduler.cc comp/ue_tracker.cc modules/exploit_loader.cc)
target_link_libraries(shadower source syncer exploit_loader thread_pool broadcast_worker ue_dl_worker ue_ul_worker gnb_ul_worker gnb_dl_worker apis ${Boost_LIBRARIES})
if(ENABLE_CUDA)
  target_link_libraries(shadower fft_processor ssb_cuda)
endif(ENABLE_CUDA)
