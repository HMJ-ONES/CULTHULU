cmake_minimum_required(VERSION 3.16)
project(cult_ulhu CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Engine-agnostic core library (pure standard C++; Unreal binding later).
# The interactive driver has its own main() and is excluded here.
file(GLOB_RECURSE CULTULHU_SRC CONFIGURE_DEPENDS src/*.cpp)
list(FILTER CULTULHU_SRC EXCLUDE REGEX "src/driver/")
# Micro-dev fix: BetaGame (lib code, src/fuzz/) constructs AntiCheat, so
# its implementation must live in the lib — otherwise every lib consumer
# (e.g. cultulhu_fuzz) fails to link with undefined AntiCheat symbols.
list(APPEND CULTULHU_SRC src/driver/AntiCheat.cpp)
add_library(cultulhu ${CULTULHU_SRC})
target_include_directories(cultulhu PUBLIC src)

# Windows: the net layer uses Winsock2 + GetAdaptersAddresses().
if(WIN32)
    target_link_libraries(cultulhu PUBLIC ws2_32 iphlpapi)
endif()

# Playable beta driver (headless console REPL).
# AntiCheat's implementation lives in the cultulhu lib (see above).
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

add_executable(cultulhu_tests_wave25 tests/tests_wave25.cpp)
target_link_libraries(cultulhu_tests_wave25 PRIVATE cultulhu)

add_executable(cultulhu_tests_wave26 tests/tests_wave26.cpp)
target_link_libraries(cultulhu_tests_wave26 PRIVATE cultulhu)

add_executable(cultulhu_tests_wave27 tests/tests_wave27.cpp)
target_link_libraries(cultulhu_tests_wave27 PRIVATE cultulhu)

add_executable(cultulhu_tests_wave28 tests/tests_wave28.cpp)
target_link_libraries(cultulhu_tests_wave28 PRIVATE cultulhu)

add_executable(cultulhu_tests_wave29 tests/tests_wave29.cpp)
target_link_libraries(cultulhu_tests_wave29 PRIVATE cultulhu)

add_executable(cultulhu_tests_wave30 tests/tests_wave30.cpp)
target_link_libraries(cultulhu_tests_wave30 PRIVATE cultulhu)

add_executable(cultulhu_tests_wave33 tests/tests_wave33.cpp)
target_link_libraries(cultulhu_tests_wave33 PRIVATE cultulhu)

add_executable(cultulhu_tests_wave35 tests/tests_wave35.cpp)
target_link_libraries(cultulhu_tests_wave35 PRIVATE cultulhu)

add_executable(cultulhu_tests_wave36 tests/tests_wave36.cpp)
target_link_libraries(cultulhu_tests_wave36 PRIVATE cultulhu)
add_executable(cultulhu_tests_wave37 tests/tests_wave37.cpp)
target_link_libraries(cultulhu_tests_wave37 PRIVATE cultulhu)
add_executable(cultulhu_tests_wave38 tests/tests_wave38.cpp)
target_link_libraries(cultulhu_tests_wave38 PRIVATE cultulhu)

add_executable(cultulhu_tests_convenience tests/tests_convenience.cpp)
target_link_libraries(cultulhu_tests_convenience PRIVATE cultulhu)

add_executable(cultulhu_tests_wave4 tests/tests_wave4.cpp)
target_link_libraries(cultulhu_tests_wave4 PRIVATE cultulhu)

add_executable(cultulhu_tests_anim tests/tests_anim.cpp)
target_link_libraries(cultulhu_tests_anim PRIVATE cultulhu)

add_executable(cultulhu_tests_wave5b tests/tests_wave5b.cpp)
target_link_libraries(cultulhu_tests_wave5b PRIVATE cultulhu)

add_executable(cultulhu_tests_net tests/tests_net.cpp)
target_link_libraries(cultulhu_tests_net PRIVATE cultulhu)

add_executable(cultulhu_tests_wave7a tests/tests_wave7a.cpp)
target_link_libraries(cultulhu_tests_wave7a PRIVATE cultulhu)

add_executable(cultulhu_tests_wave7b tests/tests_wave7b.cpp)
target_link_libraries(cultulhu_tests_wave7b PRIVATE cultulhu)

add_executable(cultulhu_tests_wave7c tests/tests_wave7c.cpp)
target_link_libraries(cultulhu_tests_wave7c PRIVATE cultulhu)

add_executable(cultulhu_tests_wave7d tests/tests_wave7d.cpp)
target_link_libraries(cultulhu_tests_wave7d PRIVATE cultulhu)

add_executable(cultulhu_tests_wave7e tests/tests_wave7e.cpp)
target_link_libraries(cultulhu_tests_wave7e PRIVATE cultulhu)

add_executable(cultulhu_tests_sim tests/tests_sim.cpp)
target_link_libraries(cultulhu_tests_sim PRIVATE cultulhu)

add_executable(cultulhu_tests_directives_wave9 tests/tests_directives_wave9.cpp)
target_link_libraries(cultulhu_tests_directives_wave9 PRIVATE cultulhu)

add_executable(cultulhu_tests_content_wave9 tests/tests_content_wave9.cpp)
target_link_libraries(cultulhu_tests_content_wave9 PRIVATE cultulhu)

add_executable(cultulhu_tests_fuzz tests/tests_fuzz.cpp)
target_link_libraries(cultulhu_tests_fuzz PRIVATE cultulhu)

add_executable(cultulhu_tests_wave11 tests/tests_wave11.cpp)
target_link_libraries(cultulhu_tests_wave11 PRIVATE cultulhu)

add_executable(cultulhu_tests_wave12 tests/tests_wave12.cpp)
target_link_libraries(cultulhu_tests_wave12 PRIVATE cultulhu)

add_executable(cultulhu_tests_wave13 tests/tests_wave13.cpp)
target_link_libraries(cultulhu_tests_wave13 PRIVATE cultulhu)

add_executable(cultulhu_tests_wave14 tests/tests_wave14.cpp)
target_link_libraries(cultulhu_tests_wave14 PRIVATE cultulhu)

add_executable(cultulhu_tests_wave15 tests/tests_wave15.cpp)
target_link_libraries(cultulhu_tests_wave15 PRIVATE cultulhu)

add_executable(cultulhu_tests_wave16 tests/tests_wave16.cpp)
target_link_libraries(cultulhu_tests_wave16 PRIVATE cultulhu)

add_executable(cultulhu_tests_wave18 tests/tests_wave18.cpp)
target_link_libraries(cultulhu_tests_wave18 PRIVATE cultulhu)

add_executable(cultulhu_tests_wave19 tests/tests_wave19.cpp)
target_link_libraries(cultulhu_tests_wave19 PRIVATE cultulhu)

add_executable(cultulhu_tests_wave21 tests/tests_wave21.cpp)
target_link_libraries(cultulhu_tests_wave21 PRIVATE cultulhu)

# Headless balance simulator (see src/sim/ and sim_reports/analysis.md).
add_executable(cultulhu_sim src/sim/main.cpp)
target_compile_definitions(cultulhu_sim PRIVATE CULTULHU_SIM_MAIN)
target_link_libraries(cultulhu_sim PRIVATE cultulhu)

# Fuzz harness binary: cultulhu_fuzz [all|target1|2|3] [iterations] [seed].
add_executable(cultulhu_fuzz src/fuzz/fuzz_main.cpp)
target_link_libraries(cultulhu_fuzz PRIVATE cultulhu)

enable_testing()
add_test(NAME cultulhu_tests COMMAND cultulhu_tests)
add_test(NAME cultulhu_tests_wave2 COMMAND cultulhu_tests_wave2)
add_test(NAME cultulhu_tests_wave3 COMMAND cultulhu_tests_wave3)
add_test(NAME cultulhu_tests_beta COMMAND cultulhu_tests_beta)
add_test(NAME cultulhu_tests_wave25 COMMAND cultulhu_tests_wave25)
set_tests_properties(cultulhu_tests_wave25 PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME cultulhu_tests_wave26 COMMAND cultulhu_tests_wave26)
set_tests_properties(cultulhu_tests_wave26 PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME cultulhu_tests_wave27 COMMAND cultulhu_tests_wave27)
set_tests_properties(cultulhu_tests_wave27 PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME cultulhu_tests_wave28 COMMAND cultulhu_tests_wave28)
set_tests_properties(cultulhu_tests_wave28 PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME cultulhu_tests_wave29 COMMAND cultulhu_tests_wave29)
set_tests_properties(cultulhu_tests_wave29 PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME cultulhu_tests_wave30 COMMAND cultulhu_tests_wave30)
set_tests_properties(cultulhu_tests_wave30 PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME cultulhu_tests_wave33 COMMAND cultulhu_tests_wave33)
set_tests_properties(cultulhu_tests_wave33 PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME cultulhu_tests_wave35 COMMAND cultulhu_tests_wave35)
set_tests_properties(cultulhu_tests_wave35 PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME cultulhu_tests_wave36 COMMAND cultulhu_tests_wave36)
set_tests_properties(cultulhu_tests_wave36 PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME cultulhu_tests_wave37 COMMAND cultulhu_tests_wave37)
set_tests_properties(cultulhu_tests_wave37 PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME cultulhu_tests_wave38 COMMAND cultulhu_tests_wave38)
add_test(NAME cultulhu_tests_convenience COMMAND cultulhu_tests_convenience)
set_tests_properties(cultulhu_tests_wave38 PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME cultulhu_tests_wave4 COMMAND cultulhu_tests_wave4)
add_test(NAME cultulhu_tests_anim COMMAND cultulhu_tests_anim)
add_test(NAME cultulhu_tests_wave5b COMMAND cultulhu_tests_wave5b)
add_test(NAME cultulhu_tests_net COMMAND cultulhu_tests_net)
add_test(NAME cultulhu_tests_wave7a COMMAND cultulhu_tests_wave7a)
add_test(NAME cultulhu_tests_wave7b COMMAND cultulhu_tests_wave7b)
add_test(NAME cultulhu_tests_wave7c COMMAND cultulhu_tests_wave7c)
add_test(NAME cultulhu_tests_wave7d COMMAND cultulhu_tests_wave7d)
add_test(NAME cultulhu_tests_wave7e COMMAND cultulhu_tests_wave7e)
add_test(NAME cultulhu_tests_sim COMMAND cultulhu_tests_sim)
add_test(NAME cultulhu_tests_directives_wave9 COMMAND cultulhu_tests_directives_wave9)
add_test(NAME cultulhu_tests_content_wave9 COMMAND cultulhu_tests_content_wave9)
add_test(NAME cultulhu_tests_fuzz COMMAND cultulhu_tests_fuzz)
add_test(NAME cultulhu_tests_wave11 COMMAND cultulhu_tests_wave11)
add_test(NAME cultulhu_tests_wave12 COMMAND cultulhu_tests_wave12)
add_test(NAME cultulhu_tests_wave13 COMMAND cultulhu_tests_wave13)
add_test(NAME cultulhu_tests_wave14 COMMAND cultulhu_tests_wave14)
set_tests_properties(cultulhu_tests_wave14 PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME cultulhu_tests_wave15 COMMAND cultulhu_tests_wave15)
add_test(NAME cultulhu_tests_wave16 COMMAND cultulhu_tests_wave16)
add_test(NAME cultulhu_tests_wave18 COMMAND cultulhu_tests_wave18)
set_tests_properties(cultulhu_tests_wave18 PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME cultulhu_tests_wave19 COMMAND cultulhu_tests_wave19)
set_tests_properties(cultulhu_tests_wave19 PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME cultulhu_tests_wave21 COMMAND cultulhu_tests_wave21)
set_tests_properties(cultulhu_tests_wave21 PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_executable(cultulhu_tests_wave40 tests/tests_wave40.cpp)
target_link_libraries(cultulhu_tests_wave40 PRIVATE cultulhu)
add_executable(cultulhu_tests_wave42 tests/tests_wave42.cpp)
target_link_libraries(cultulhu_tests_wave42 PRIVATE cultulhu)
add_test(NAME cultulhu_tests_wave40 COMMAND cultulhu_tests_wave40)
set_tests_properties(cultulhu_tests_wave40 PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME cultulhu_tests_wave42 COMMAND cultulhu_tests_wave42)
set_tests_properties(cultulhu_tests_wave42 PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_executable(cultulhu_tests_wave43 tests/tests_wave43.cpp)
target_link_libraries(cultulhu_tests_wave43 PRIVATE cultulhu)
add_test(NAME cultulhu_tests_wave43 COMMAND cultulhu_tests_wave43)
set_tests_properties(cultulhu_tests_wave43 PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_executable(cultulhu_tests_anticheat tests/tests_anticheat.cpp)
target_link_libraries(cultulhu_tests_anticheat PRIVATE cultulhu)
add_test(NAME cultulhu_tests_anticheat COMMAND cultulhu_tests_anticheat)
set_tests_properties(cultulhu_tests_anticheat PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_executable(cultulhu_tests_wave44 tests/tests_wave44.cpp)
target_link_libraries(cultulhu_tests_wave44 PRIVATE cultulhu)
add_test(NAME cultulhu_tests_wave44 COMMAND cultulhu_tests_wave44)
set_tests_properties(cultulhu_tests_wave44 PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_executable(cultulhu_tests_wave45 tests/tests_wave45.cpp)
target_link_libraries(cultulhu_tests_wave45 PRIVATE cultulhu)
add_test(NAME cultulhu_tests_wave45 COMMAND cultulhu_tests_wave45)
set_tests_properties(cultulhu_tests_wave45 PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_executable(cultulhu_tests_micro_respawn tests/tests_micro_respawn.cpp)
target_link_libraries(cultulhu_tests_micro_respawn PRIVATE cultulhu)
add_test(NAME cultulhu_tests_micro_respawn COMMAND cultulhu_tests_micro_respawn)
set_tests_properties(cultulhu_tests_micro_respawn PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_executable(cultulhu_tests_micro_matchpoint tests/tests_micro_matchpoint.cpp)
target_link_libraries(cultulhu_tests_micro_matchpoint PRIVATE cultulhu)
add_test(NAME cultulhu_tests_micro_matchpoint COMMAND cultulhu_tests_micro_matchpoint)
set_tests_properties(cultulhu_tests_micro_matchpoint PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_executable(cultulhu_tests_micro_overtime tests/tests_micro_overtime.cpp)
target_link_libraries(cultulhu_tests_micro_overtime PRIVATE cultulhu)
add_test(NAME cultulhu_tests_micro_overtime COMMAND cultulhu_tests_micro_overtime)
set_tests_properties(cultulhu_tests_micro_overtime PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_executable(cultulhu_tests_micro_towerdown tests/tests_micro_towerdown.cpp)
target_link_libraries(cultulhu_tests_micro_towerdown PRIVATE cultulhu)
add_test(NAME cultulhu_tests_micro_towerdown COMMAND cultulhu_tests_micro_towerdown)
set_tests_properties(cultulhu_tests_micro_towerdown PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_executable(cultulhu_tests_micro_firstblood tests/tests_micro_firstblood.cpp)
target_link_libraries(cultulhu_tests_micro_firstblood PRIVATE cultulhu)
add_test(NAME cultulhu_tests_micro_firstblood COMMAND cultulhu_tests_micro_firstblood)
set_tests_properties(cultulhu_tests_micro_firstblood PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_executable(cultulhu_tests_micro_rockbreak tests/tests_micro_rockbreak.cpp)
target_link_libraries(cultulhu_tests_micro_rockbreak PRIVATE cultulhu)
add_test(NAME cultulhu_tests_micro_rockbreak COMMAND cultulhu_tests_micro_rockbreak)
set_tests_properties(cultulhu_tests_micro_rockbreak PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME cultulhu_fuzz_smoke COMMAND cultulhu_fuzz all 2000 42)
add_executable(cultulhu_tests_micro_respawned tests/tests_micro_respawned.cpp)
target_link_libraries(cultulhu_tests_micro_respawned PRIVATE cultulhu)
add_test(NAME cultulhu_tests_micro_respawned COMMAND cultulhu_tests_micro_respawned)
set_tests_properties(cultulhu_tests_micro_respawned PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_executable(cultulhu_tests_micro_botselfcast tests/tests_micro_botselfcast.cpp)
target_link_libraries(cultulhu_tests_micro_botselfcast PRIVATE cultulhu)
add_test(NAME cultulhu_tests_micro_botselfcast COMMAND cultulhu_tests_micro_botselfcast)
set_tests_properties(cultulhu_tests_micro_botselfcast PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_executable(cultulhu_tests_micro_deathcallout tests/tests_micro_deathcallout.cpp)
target_link_libraries(cultulhu_tests_micro_deathcallout PRIVATE cultulhu)
add_test(NAME cultulhu_tests_micro_deathcallout COMMAND cultulhu_tests_micro_deathcallout)
set_tests_properties(cultulhu_tests_micro_deathcallout PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_executable(cultulhu_tests_micro_botretreat tests/tests_micro_botretreat.cpp)
target_link_libraries(cultulhu_tests_micro_botretreat PRIVATE cultulhu)
add_test(NAME cultulhu_tests_micro_botretreat COMMAND cultulhu_tests_micro_botretreat)
set_tests_properties(cultulhu_tests_micro_botretreat PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_executable(cultulhu_tests_micro_mcastparse tests/tests_micro_mcastparse.cpp)
target_link_libraries(cultulhu_tests_micro_mcastparse PRIVATE cultulhu)
add_test(NAME cultulhu_tests_micro_mcastparse COMMAND cultulhu_tests_micro_mcastparse)
set_tests_properties(cultulhu_tests_micro_mcastparse PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_executable(cultulhu_tests_micro_victory tests/tests_micro_victory.cpp)
target_link_libraries(cultulhu_tests_micro_victory PRIVATE cultulhu)
add_test(NAME cultulhu_tests_micro_victory COMMAND cultulhu_tests_micro_victory)
set_tests_properties(cultulhu_tests_micro_victory PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_executable(cultulhu_tests_micro_you tests/tests_micro_you.cpp)
target_link_libraries(cultulhu_tests_micro_you PRIVATE cultulhu)
add_test(NAME cultulhu_tests_micro_you COMMAND cultulhu_tests_micro_you)
set_tests_properties(cultulhu_tests_micro_you PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_executable(cultulhu_tests_micro_goounderattack tests/tests_micro_goounderattack.cpp)
target_link_libraries(cultulhu_tests_micro_goounderattack PRIVATE cultulhu)
add_test(NAME cultulhu_tests_micro_goounderattack COMMAND cultulhu_tests_micro_goounderattack)
set_tests_properties(cultulhu_tests_micro_goounderattack PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_executable(cultulhu_tests_micro_dotgoolimit tests/tests_micro_dotgoolimit.cpp)
target_link_libraries(cultulhu_tests_micro_dotgoolimit PRIVATE cultulhu)
add_test(NAME cultulhu_tests_micro_dotgoolimit COMMAND cultulhu_tests_micro_dotgoolimit)
set_tests_properties(cultulhu_tests_micro_dotgoolimit PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_executable(cultulhu_tests_micro_unlockname tests/tests_micro_unlockname.cpp)
target_link_libraries(cultulhu_tests_micro_unlockname PRIVATE cultulhu)
add_test(NAME cultulhu_tests_micro_unlockname COMMAND cultulhu_tests_micro_unlockname)
set_tests_properties(cultulhu_tests_micro_unlockname PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_executable(cultulhu_tests_micro_ultcharged tests/tests_micro_ultcharged.cpp)
target_link_libraries(cultulhu_tests_micro_ultcharged PRIVATE cultulhu)
add_test(NAME cultulhu_tests_micro_ultcharged COMMAND cultulhu_tests_micro_ultcharged)
set_tests_properties(cultulhu_tests_micro_ultcharged PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_executable(cultulhu_tests_micro_relicclaim tests/tests_micro_relicclaim.cpp)
target_link_libraries(cultulhu_tests_micro_relicclaim PRIVATE cultulhu)
add_test(NAME cultulhu_tests_micro_relicclaim COMMAND cultulhu_tests_micro_relicclaim)
set_tests_properties(cultulhu_tests_micro_relicclaim PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
