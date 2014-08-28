## The minimum version of cmake that we support
cmake_minimum_required(VERSION 2.6)

## Identify the project
project(lgp_gears)

## We specify the directories where are located the include files (include and lib) with the INCLUDE_DIRECTORIES directive
include_directories(
SYSTEM
src
/usr/include/libusb-1.0
)

## Custom library directories
link_directories(
)

## Flags that will be passed as arguments to the compiler - we skip standard include and carry everything in the project treea
add_definitions(-Wall -std=c99)

## Src list
file(GLOB_RECURSE SRCLIST src/*)

## Target executables
add_executable(lgp_gears ${SRCLIST})

## Linker data
target_link_libraries(lgp_gears usb-1.0)

