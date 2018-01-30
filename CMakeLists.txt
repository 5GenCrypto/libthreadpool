cmake_minimum_required(VERSION 3.2 FATAL_ERROR)
project(threadpool VERSION 1.0.0 LANGUAGES C)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
  if(CMAKE_C_COMPILER_VERSION VERSION_LESS "4.9.0")
    message(FATAL_ERROR "gcc version must be ≥ 4.9.0")
  endif()
endif()

include(GNUInstallDirs)

set(threadpool_SOURCES threadpool.c)
set(threadpool_HEADERS threadpool.h)

add_library(threadpool SHARED ${threadpool_SOURCES})
target_include_directories(threadpool INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:include>)

find_package(Threads REQUIRED)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic -Werror -std=gnu11 -march=native")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -pg -ggdb -O0")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")
target_link_libraries(threadpool ${CMAKE_THREAD_LIBS_INIT})

install(TARGETS threadpool LIBRARY DESTINATION lib)
install(FILES ${threadpool_HEADERS} DESTINATION include)
