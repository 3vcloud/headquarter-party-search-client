cmake_minimum_required(VERSION 3.1)

set(CMAKE_CXX_FLAGS_DEBUG  "-g" CACHE "CMAKE_CXX_FLAGS_DEBUG" STRING FORCE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON CACHE "CMAKE_POSITION_INDEPENDENT_CODE" BOOL FORCE)
set(BUILD_SHARED_LIBS OFF CACHE "BUILD_SHARED_LIBS" BOOL FORCE)
set(CMAKE_BUILD_TYPE Debug CACHE "CMAKE_BUILD_TYPE" STRING FORCE)

set(JANSSON_BUILD_SHARED_LIBS OFF CACHE "JANSSON_BUILD_SHARED_LIBS" BOOL FORCE)
set(JANSSON_BUILD_DOCS OFF CACHE "JANSSON_BUILD_DOCS" BOOL FORCE)
set(JANSSON_STATIC_CRT ON CACHE "JANSSON_STATIC_CRT" BOOL FORCE)
set(JANSSON_EXAMPLES OFF CACHE "JANSSON_EXAMPLES" BOOL FORCE)
set(JANSSON_WITHOUT_TESTS ON CACHE "JANSSON_WITHOUT_TESTS" BOOL FORCE)

set(BUILD_TESTING_STATIC OFF CACHE "BUILD_TESTING_STATIC" BOOL FORCE)
set(BUILD_TESTING_SHARED OFF CACHE "BUILD_TESTING_SHARED" BOOL FORCE)
set(BUILD_TESTING OFF CACHE "BUILD_TESTING" BOOL FORCE)

project(
  party_search_client
  VERSION 1.0
  LANGUAGES C
)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/Headquarter)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/curl)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/jansson)


# Look for required libraries
add_library(
  party_search_client

  SHARED

  main.c
)

target_link_libraries(party_search_client PRIVATE libcurl jansson common client)

set_target_properties(common
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/"
)

set_target_properties(portal
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/"
)

set_target_properties(client
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/"
)
set_target_properties(jansson
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/"
)
set_target_properties(curl
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/"
)
set_target_properties(party_search_client
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/"
)

target_compile_definitions(party_search_client PRIVATE HEADQUARTER_BUILD_EXPORTS)

target_include_directories(
  party_search_client

  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/Headquarter/include
  ${CMAKE_BINARY_DIR}/Dependencies/jansson/include
  curl
)

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/bin/Debug/logs)

if (MSVC)
  # We disable the following warnings:
  # - C4057: nonstandard extension used: nameless struct/union
  # - C4204: nonstandard extension used: non-constant aggregate initializer
  # - C4221: nonstandard extension used: '<var>': cannot be initialized using address of automatic variable
  # - C4459: declaration of '<var>' hides global declaration
  # - C4706: assignment within conditional expression
  target_compile_options(party_search_client PRIVATE /WX /W4 /wd4201 /wd4204 /wd4244 /wd4221 /wd4295 /wd4310 /wd4018 /wd4459 /wd4100 /wd4706)
  set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT client)
endif()

