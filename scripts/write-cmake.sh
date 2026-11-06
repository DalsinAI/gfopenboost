#!/bin/sh
# Writes lib/cmake/Boost-<version>/ into an SDK folder: a CMake package with
# Boost::headers, Boost::boost and Boost::<part> for each built part, so
# find_package(Boost CONFIG COMPONENTS ...) works for the ports.
#   write-cmake.sh SDK VERSION "PARTS"
# Copyright (c) 2026 AmiCradle Team
# Distributed under the Boost Software License, Version 1.0.
set -eu
SDK=$1; V=$2; PARTS=$3
D="$SDK/lib/cmake/Boost-$V"; mkdir -p "$D"
{
cat <<CFG
# GFOpenBoost $V for AmigaOS 3 (68040). Made by scripts/write-cmake.sh.
get_filename_component(_GFOB_PREFIX "\${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(Boost_VERSION $V)
set(Boost_VERSION_STRING $V)
set(Boost_INCLUDE_DIRS "\${_GFOB_PREFIX}/include")
set(Boost_INCLUDE_DIR "\${_GFOB_PREFIX}/include")
set(Boost_LIBRARY_DIRS "\${_GFOB_PREFIX}/lib")
set(Boost_LIBRARIES "")
if(NOT TARGET Boost::headers)
  add_library(Boost::headers INTERFACE IMPORTED)
  set_target_properties(Boost::headers PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "\${_GFOB_PREFIX}/include")
  add_library(Boost::boost INTERFACE IMPORTED)
  set_target_properties(Boost::boost PROPERTIES INTERFACE_LINK_LIBRARIES Boost::headers)
  add_library(Boost::diagnostic_definitions INTERFACE IMPORTED)
  add_library(Boost::disable_autolinking INTERFACE IMPORTED)
  add_library(Boost::dynamic_linking INTERFACE IMPORTED)
endif()
CFG
for p in $PARTS; do
    deps="Boost::headers"
    case "$p" in
        thread) extra=";pthread;atomic" ;;
        atomic) extra=";atomic" ;;
        asio) extra=";socket" ;;
        timer) deps="Boost::headers;Boost::chrono" ; extra="" ;;
        locale) deps="Boost::headers;Boost::thread" ; extra="" ;;
        wserialization) deps="Boost::headers;Boost::serialization" ; extra="" ;;
        *) extra="" ;;
    esac
    U=$(echo "$p" | tr a-z A-Z)
    cat <<CFG
if(NOT TARGET Boost::$p)
  add_library(Boost::$p STATIC IMPORTED)
  set_target_properties(Boost::$p PROPERTIES
    IMPORTED_LOCATION "\${_GFOB_PREFIX}/lib/libboost_$p.a"
    INTERFACE_LINK_LIBRARIES "$deps$extra")
endif()
set(Boost_${U}_FOUND TRUE)
set(Boost_${U}_LIBRARY "\${_GFOB_PREFIX}/lib/libboost_$p.a")
set(Boost_${p}_FOUND TRUE)
CFG
done
cat <<CFG
set_target_properties(Boost::asio PROPERTIES INTERFACE_COMPILE_DEFINITIONS BOOST_ASIO_SEPARATE_COMPILATION)
foreach(_c IN LISTS Boost_FIND_COMPONENTS)
  if(TARGET Boost::\${_c})
    list(APPEND Boost_LIBRARIES Boost::\${_c})
  elseif(Boost_FIND_REQUIRED_\${_c})
    set(Boost_FOUND FALSE)
    set(Boost_NOT_FOUND_MESSAGE "GFOpenBoost has no compiled part \${_c}")
    return()
  endif()
endforeach()
set(Boost_FOUND TRUE)
CFG
} > "$D/BoostConfig.cmake"
cat > "$D/BoostConfigVersion.cmake" <<CFG
set(PACKAGE_VERSION $V)
if(PACKAGE_FIND_VERSION VERSION_GREATER PACKAGE_VERSION)
  set(PACKAGE_VERSION_COMPATIBLE FALSE)
else()
  set(PACKAGE_VERSION_COMPATIBLE TRUE)
  if(PACKAGE_FIND_VERSION STREQUAL PACKAGE_VERSION)
    set(PACKAGE_VERSION_EXACT TRUE)
  endif()
endif()
CFG
