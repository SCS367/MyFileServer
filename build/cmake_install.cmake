# Install script for directory: /home/scs1/桌面/A-Tiny-Network-Library-main/My-Tiny-Network-Library-main

# Set the install prefix
IF(NOT DEFINED CMAKE_INSTALL_PREFIX)
  SET(CMAKE_INSTALL_PREFIX "/home/scs1/桌面/A-Tiny-Network-Library-main/My-Tiny-Network-Library-main")
ENDIF(NOT DEFINED CMAKE_INSTALL_PREFIX)
STRING(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
IF(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  IF(BUILD_TYPE)
    STRING(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  ELSE(BUILD_TYPE)
    SET(CMAKE_INSTALL_CONFIG_NAME "Debug")
  ENDIF(BUILD_TYPE)
  MESSAGE(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
ENDIF(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)

# Set the component getting installed.
IF(NOT CMAKE_INSTALL_COMPONENT)
  IF(COMPONENT)
    MESSAGE(STATUS "Install component: \"${COMPONENT}\"")
    SET(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  ELSE(COMPONENT)
    SET(CMAKE_INSTALL_COMPONENT)
  ENDIF(COMPONENT)
ENDIF(NOT CMAKE_INSTALL_COMPONENT)

# Install shared libraries without execute permission?
IF(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  SET(CMAKE_INSTALL_SO_NO_EXE "0")
ENDIF(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)

IF(NOT CMAKE_INSTALL_COMPONENT OR "${CMAKE_INSTALL_COMPONENT}" STREQUAL "Unspecified")
  FILE(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/tiny_network" TYPE DIRECTORY FILES "/home/scs1/桌面/A-Tiny-Network-Library-main/My-Tiny-Network-Library-main/src/" FILES_MATCHING REGEX "/[^/]*\\.h$")
ENDIF(NOT CMAKE_INSTALL_COMPONENT OR "${CMAKE_INSTALL_COMPONENT}" STREQUAL "Unspecified")

IF(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  INCLUDE("/home/scs1/桌面/A-Tiny-Network-Library-main/My-Tiny-Network-Library-main/build/example/cmake_install.cmake")
  INCLUDE("/home/scs1/桌面/A-Tiny-Network-Library-main/My-Tiny-Network-Library-main/build/src/http/cmake_install.cmake")
  INCLUDE("/home/scs1/桌面/A-Tiny-Network-Library-main/My-Tiny-Network-Library-main/build/src/logger/test/cmake_install.cmake")
  INCLUDE("/home/scs1/桌面/A-Tiny-Network-Library-main/My-Tiny-Network-Library-main/build/src/memory/test/cmake_install.cmake")
  INCLUDE("/home/scs1/桌面/A-Tiny-Network-Library-main/My-Tiny-Network-Library-main/build/src/mysql/test/cmake_install.cmake")

ENDIF(NOT CMAKE_INSTALL_LOCAL_ONLY)

IF(CMAKE_INSTALL_COMPONENT)
  SET(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
ELSE(CMAKE_INSTALL_COMPONENT)
  SET(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
ENDIF(CMAKE_INSTALL_COMPONENT)

FILE(WRITE "/home/scs1/桌面/A-Tiny-Network-Library-main/My-Tiny-Network-Library-main/build/${CMAKE_INSTALL_MANIFEST}" "")
FOREACH(file ${CMAKE_INSTALL_MANIFEST_FILES})
  FILE(APPEND "/home/scs1/桌面/A-Tiny-Network-Library-main/My-Tiny-Network-Library-main/build/${CMAKE_INSTALL_MANIFEST}" "${file}\n")
ENDFOREACH(file)
