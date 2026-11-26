cmake_minimum_required(VERSION 3.20)
project(yonderest-engine VERSION 1 LANGUAGES CXX)

find_package(Threads REQUIRED)
# JsonCpp's own CMake package is missing from some distributions; pkg-config finds it everywhere.
find_package(PkgConfig REQUIRED)
pkg_check_modules(JsonCpp REQUIRED IMPORTED_TARGET GLOBAL jsoncpp)
include(cmake/YonderestModule.cmake)

add_library(yonderest-engine STATIC
    src/content.cpp src/host.cpp src/json.cpp src/platform_posix.cpp src/player.cpp src/save.cpp src/space.cpp src/value.cpp src/world.cpp)
target_compile_features(yonderest-engine PUBLIC cxx_std_20)
target_compile_options(yonderest-engine PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(yonderest-engine PUBLIC include)
target_link_libraries(yonderest-engine PRIVATE PkgConfig::JsonCpp PUBLIC Threads::Threads)
set_target_properties(yonderest-engine PROPERTIES VERSION ${PROJECT_VERSION})
yonderest_fence(yonderest-engine SDL3)

# The program a player starts. The game links it with the modules it is made of.
add_library(yonderest-engine-main OBJECT src/main.cpp)
target_link_libraries(yonderest-engine-main PRIVATE yonderest-engine)

if(BUILD_TESTING)
    add_executable(engine-test tests/saves.cpp)
    target_link_libraries(engine-test PRIVATE yonderest-engine)
    add_test(NAME engine COMMAND engine-test)
endif()
