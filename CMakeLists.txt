cmake_minimum_required(VERSION 3.14)
project(e2asm)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(E2Asm_TEST "Compile E2Asm UT suite" OFF)

set(E2ASM_SOURCE_FOLDERS core lexer parser preprocessor semantic codegen)

foreach(dir ${E2ASM_SOURCE_FOLDERS})
  file(GLOB_RECURSE sources
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/*.cpp
  )

  file(GLOB_RECURSE headers
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/*.h
  )

  list(APPEND E2ASM_SOURCES ${sources})
  list(APPEND E2ASM_HEADERS ${headers})
endforeach()

add_library(e2asm STATIC ${E2ASM_SOURCES} ${E2ASM_HEADERS})

target_compile_features(e2asm PUBLIC cxx_std_20)

target_include_directories(e2asm PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
  $<INSTALL_INTERFACE:include>
)

if(E2ASM_TEST)
  # GTest Setup
  include(FetchContent)

  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
  )
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

  FetchContent_MakeAvailable(googletest)

  enable_testing()

  set(TEST_SOURCES
    test/lexer_test.cpp
    test/parser_test.cpp
    test/semantic_test.cpp
    test/integration_test.cpp
  )

  add_executable(e2asm_ut ${TEST_SOURCES})

  target_link_libraries(e2asm_ut
    PRIVATE
    e2asm
    GTest::gtest
    GTest::gtest_main
  )

  target_include_directories(e2asm_ut PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
  )

  include(GoogleTest)
  gtest_discover_tests(e2asm_ut)

  add_test(NAME e2asm_unit_test COMMAND e2asm_ut)

  add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS e2asm_ut
  )

  add_custom_target(check_verbose
    COMMAND e2asm_ut --gtest_color=yes
    DEPENDS e2asm_ut
  )
endif()
