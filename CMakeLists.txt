cmake_minimum_required(VERSION 3.31)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_COMPILER gcc)
set(CMAKE_CXX_COMPILER g++)
set(CMAKE_BUILD_TOOL make)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(COMMON_COMPILE_OPTIONS -Wall -Wextra)

option(BUILD_TESTS "Build the main.cxx file" OFF)

project(Process LANGUAGES C CXX VERSION 1.0.0)

set(BIN_NAME bin.out)
set(COMMON_LINK_LIBS)
set(COMMON_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
set(SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Impl/Process.cxx
)

set(TEST_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cxx
)

include_directories(${COMMON_INCLUDE_DIRS})

add_library(${PROJECT_NAME} SHARED ${SOURCES})
target_link_libraries(${PROJECT_NAME} PRIVATE ${COMMON_LINK_LIBS})
target_compile_options(${PROJECT_NAME} PRIVATE ${COMMON_COMPILE_OPTIONS})

if(BUILD_TESTS)
  add_executable(${BIN_NAME} ${TEST_SOURCES})
  target_compile_options(${PROJECT_NAME} PRIVATE ${COMMON_COMPILE_OPTIONS})
  target_link_libraries(${BIN_NAME} PRIVATE ${PROJECT_NAME})
  target_include_directories(${BIN_NAME} PRIVATE ${COMMON_INCLUDE_DIRS})
endif()
