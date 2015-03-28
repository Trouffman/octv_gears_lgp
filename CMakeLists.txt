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

## Src list
file(GLOB_RECURSE SRCLIST src/*)

## Copy firmware
add_custom_target(copy_resource_files ALL
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_LIST_DIR}/firmware ${CMAKE_CURRENT_LIST_DIR}/build
        COMMENT "Copying firmware to build dir..."
        VERBATIM
)


## Target executables
add_executable(lgp_gears ${SRCLIST})

## Linker data
target_link_libraries(lgp_gears usb-1.0)

