cmake_minimum_required(VERSION 3.20)
project(yonderest VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SDL3 3.4 REQUIRED CONFIG)
# JsonCpp's own CMake package is missing from some distributions; pkg-config finds it everywhere.
find_package(PkgConfig REQUIRED)
pkg_check_modules(JsonCpp REQUIRED IMPORTED_TARGET jsoncpp)

add_library(world src/json.cpp src/content.cpp src/terrain.cpp src/world.cpp src/play.cpp src/craft.cpp src/save.cpp)
target_include_directories(world PUBLIC src)
target_link_libraries(world PRIVATE PkgConfig::JsonCpp)
target_compile_options(world PRIVATE -Wall -Wextra -Wpedantic)

add_library(desktop src/app.cpp)
target_link_libraries(desktop PUBLIC world SDL3::SDL3)
target_compile_options(desktop PRIVATE -Wall -Wextra -Wpedantic)

add_executable(yonderest src/main.cpp)
target_link_libraries(yonderest PRIVATE desktop)

# The build's content is a mirror of the source tree: deletions carry over too.
# An in-source build already has its content beside the executable, and mirroring
# would delete the source itself.
file(REAL_PATH ${CMAKE_CURRENT_SOURCE_DIR} source_dir)
file(REAL_PATH ${CMAKE_CURRENT_BINARY_DIR} binary_dir)
if(source_dir STREQUAL binary_dir)
    add_custom_target(content)
else()
    file(GLOB_RECURSE content_files CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/content/*)
    # Rewritten only when membership changes, so a removed file still triggers the mirror.
    file(CONFIGURE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/content.list CONTENT "${content_files}")
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/content.stamp
        COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_CURRENT_BINARY_DIR}/content
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/content ${CMAKE_CURRENT_BINARY_DIR}/content
        COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/content.stamp
        DEPENDS ${content_files} ${CMAKE_CURRENT_BINARY_DIR}/content.list
        COMMENT "Mirroring content")
    add_custom_target(content ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/content.stamp)
endif()
add_dependencies(yonderest content)

include(CTest)
if(BUILD_TESTING)
    add_executable(saves-test tests/saves.cpp)
    target_link_libraries(saves-test PRIVATE world)
    add_test(NAME saves COMMAND saves-test ${CMAKE_CURRENT_SOURCE_DIR}/content)
    add_executable(keyboard-test tests/keyboard.cpp)
    target_link_libraries(keyboard-test PRIVATE world)
    add_test(NAME keyboard COMMAND keyboard-test ${CMAKE_CURRENT_SOURCE_DIR}/content)
    find_package(Threads REQUIRED)
    add_executable(desktop-test tests/desktop.cpp)
    add_dependencies(desktop-test content)
    target_link_libraries(desktop-test PRIVATE desktop Threads::Threads)
    add_test(NAME desktop COMMAND desktop-test ${CMAKE_CURRENT_BINARY_DIR}/content)
    add_test(NAME desktop-build COMMAND desktop-test ${CMAKE_CURRENT_BINARY_DIR}/content build)
    add_test(NAME desktop-empty COMMAND desktop-test ${CMAKE_CURRENT_BINARY_DIR}/content empty)
    add_test(NAME desktop-reference COMMAND desktop-test ${CMAKE_CURRENT_BINARY_DIR}/content reference)
    add_test(NAME desktop-facing COMMAND desktop-test ${CMAKE_CURRENT_BINARY_DIR}/content facing)
    add_test(NAME desktop-crawl COMMAND desktop-test ${CMAKE_CURRENT_BINARY_DIR}/content crawl)
    set_tests_properties(desktop desktop-build desktop-empty desktop-reference desktop-facing desktop-crawl PROPERTIES TIMEOUT 20 ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software")
endif()

install(TARGETS yonderest RUNTIME DESTINATION .)
install(DIRECTORY content DESTINATION .)
install(FILES LICENSE README.md DESIGN.md ROADMAP.md DESTINATION .)
