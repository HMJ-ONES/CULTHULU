cmake_minimum_required(VERSION 3.16)
project(cult_ulhu CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Engine-agnostic core library (pure standard C++; Unreal binding later).
# The interactive driver has its own main() and is excluded here.
file(GLOB_RECURSE CULTULHU_SRC CONFIGURE_DEPENDS src/*.cpp)
list(FILTER CULTULHU_SRC EXCLUDE REGEX "src/driver/")
add_library(cultulhu ${CULTULHU_SRC})
target_include_directories(cultulhu PUBLIC src)

# Playable beta driver (headless console REPL).
add_executable(cultulhu_play src/driver/main.cpp)
target_link_libraries(cultulhu_play PRIVATE cultulhu)

# Test binaries.
add_executable(cultulhu_tests tests/tests_main.cpp)
target_link_libraries(cultulhu_tests PRIVATE cultulhu)

add_executable(cultulhu_tests_wave2 tests/tests_wave2.cpp)
target_link_libraries(cultulhu_tests_wave2 PRIVATE cultulhu)

add_executable(cultulhu_tests_wave3 tests/tests_wave3.cpp)
target_link_libraries(cultulhu_tests_wave3 PRIVATE cultulhu)

add_executable(cultulhu_tests_beta tests/tests_beta.cpp)
target_link_libraries(cultulhu_tests_beta PRIVATE cultulhu)

enable_testing()
add_test(NAME cultulhu_tests COMMAND cultulhu_tests)
add_test(NAME cultulhu_tests_wave2 COMMAND cultulhu_tests_wave2)
add_test(NAME cultulhu_tests_wave3 COMMAND cultulhu_tests_wave3)
add_test(NAME cultulhu_tests_beta COMMAND cultulhu_tests_beta)
