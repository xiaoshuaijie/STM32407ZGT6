cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME 26diansai)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Disabled by default.  This controlled switch lets Ozone start one Task-3
# cycle without physical button input.  The normal vision, motor and timeout
# safety gates are unchanged.
option(OZONE_AUTOMATED_T3_TEST "Start Task 3 automatically after reset" OFF)
set(OZONE_AUTOMATED_RUN_TASK "0" CACHE STRING
    "Ozone-only closed-loop run test: 0=disabled, 3=T3, 4=T4, 5=T5, 6=T6; preserves normal task-start zeroing and safety checks")
set_property(CACHE OZONE_AUTOMATED_RUN_TASK PROPERTY STRINGS 0 3 4 5 6)
option(OZONE_AUTOMATED_IDLE_MODE_CYCLE_TEST
    "Cycle VIEW/T3/T4/T5/T6 without motor motion" OFF)
option(OZONE_RELEVEL_BEFORE_AUTOMATED_RUN
    "Ozone-only: return the motor to its existing zero before an automated task run" OFF)
set(OZONE_AUTOMATED_ZERO_RECOVERY_STEPS "0" CACHE STRING
    "Ozone-only signed count of bounded 20-degree zero shifts (-3..3; positive unwinds toward -20 degrees)")
set_property(CACHE OZONE_AUTOMATED_ZERO_RECOVERY_STEPS PROPERTY STRINGS -3 -2 -1 0 1 2 3)
set(OZONE_AUTOMATED_IDLE_MODE_SELECTIONS "0" CACHE STRING
    "Ozone-only selected-mode test: 0=VIEW, 1=T3, 2=T4, 3=T5, 4=T6")
set_property(CACHE OZONE_AUTOMATED_IDLE_MODE_SELECTIONS PROPERTY STRINGS 0 1 2 3 4)

if(NOT OZONE_AUTOMATED_IDLE_MODE_SELECTIONS MATCHES "^[0-4]$")
    message(FATAL_ERROR "OZONE_AUTOMATED_IDLE_MODE_SELECTIONS must be 0 through 4")
endif()
if(NOT OZONE_AUTOMATED_RUN_TASK MATCHES "^[03456]$")
    message(FATAL_ERROR "OZONE_AUTOMATED_RUN_TASK must be 0, 3, 4, 5, or 6")
endif()
if(OZONE_AUTOMATED_T3_TEST AND
   (NOT OZONE_AUTOMATED_RUN_TASK STREQUAL "0" OR
    NOT OZONE_AUTOMATED_IDLE_MODE_SELECTIONS STREQUAL "0" OR
    OZONE_AUTOMATED_IDLE_MODE_CYCLE_TEST))
    message(FATAL_ERROR "Choose only one Ozone task/mode test at a time")
endif()
if(NOT OZONE_AUTOMATED_RUN_TASK STREQUAL "0" AND
   (NOT OZONE_AUTOMATED_IDLE_MODE_SELECTIONS STREQUAL "0" OR
    OZONE_AUTOMATED_IDLE_MODE_CYCLE_TEST))
    message(FATAL_ERROR "Choose either a running-task test or an idle-mode test, not both")
endif()
if(OZONE_RELEVEL_BEFORE_AUTOMATED_RUN AND
   OZONE_AUTOMATED_RUN_TASK STREQUAL "0")
    message(FATAL_ERROR
        "OZONE_RELEVEL_BEFORE_AUTOMATED_RUN requires OZONE_AUTOMATED_RUN_TASK")
endif()
if(NOT OZONE_AUTOMATED_ZERO_RECOVERY_STEPS MATCHES "^-?[0-3]$")
    message(FATAL_ERROR
        "OZONE_AUTOMATED_ZERO_RECOVERY_STEPS must be -3 through 3")
endif()
if(NOT OZONE_AUTOMATED_ZERO_RECOVERY_STEPS STREQUAL "0" AND
   OZONE_AUTOMATED_RUN_TASK STREQUAL "0")
    message(FATAL_ERROR
        "OZONE_AUTOMATED_ZERO_RECOVERY_STEPS requires OZONE_AUTOMATED_RUN_TASK")
endif()
if(OZONE_AUTOMATED_IDLE_MODE_CYCLE_TEST AND
   NOT OZONE_AUTOMATED_IDLE_MODE_SELECTIONS STREQUAL "0")
    message(FATAL_ERROR "Choose one idle-mode test at a time")
endif()

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    ${CMAKE_CURRENT_SOURCE_DIR}/LCD/lcd_core.c
    ${CMAKE_CURRENT_SOURCE_DIR}/LCD/lcd_font.c
    ${CMAKE_CURRENT_SOURCE_DIR}/LCD/lcd_port.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    ${CMAKE_CURRENT_SOURCE_DIR}/LCD
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

if(OZONE_AUTOMATED_T3_TEST)
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        OZONE_AUTOMATED_T3_TEST=1)
endif()

if(NOT OZONE_AUTOMATED_RUN_TASK STREQUAL "0")
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        OZONE_AUTOMATED_RUN_TASK=${OZONE_AUTOMATED_RUN_TASK})
endif()

if(OZONE_RELEVEL_BEFORE_AUTOMATED_RUN)
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        OZONE_RELEVEL_BEFORE_AUTOMATED_RUN=1)
endif()

if(NOT OZONE_AUTOMATED_ZERO_RECOVERY_STEPS STREQUAL "0")
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        OZONE_AUTOMATED_ZERO_RECOVERY_STEPS=${OZONE_AUTOMATED_ZERO_RECOVERY_STEPS})
endif()

if(NOT OZONE_AUTOMATED_IDLE_MODE_SELECTIONS STREQUAL "0")
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        OZONE_AUTOMATED_IDLE_MODE_SELECTIONS=${OZONE_AUTOMATED_IDLE_MODE_SELECTIONS})
endif()

if(OZONE_AUTOMATED_IDLE_MODE_CYCLE_TEST)
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        OZONE_AUTOMATED_IDLE_MODE_CYCLE_TEST=1)
endif()

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)

# Add LibXR
include(${CMAKE_CURRENT_LIST_DIR}/cmake/LibXR.CMake)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/LCD/lcd_libxr_port.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/bujin_motor/zdt_x42s.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/bu_task/bu_task.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/bu_task/balance_controller.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/bu_task/operator_panel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/maxican/maxican.cpp
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/bujin_motor
    ${CMAKE_CURRENT_SOURCE_DIR}/bu_task
    ${CMAKE_CURRENT_SOURCE_DIR}/maxican
)
