cmake_minimum_required(VERSION 3.20)
project(yonderest LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_compile_options(-Wall -Wextra -Wpedantic)
include(CTest)

add_subdirectory(engine)

# The flagship's modules with code, attached in this order: a module's hooks run before those of
# modules attached after it. Its packs are modules without code: data, and the display's art.
set(modules physics interaction inventory crafting generator controls display)
set(packs base expedition workshop art)
foreach(module ${modules})
    add_subdirectory(modules/${module})
endforeach()

# The game: the engine's program with the flagship's modules linked in.
set(declarations "")
set(calls "")
foreach(module ${modules})
    string(APPEND declarations "namespace ${module} { void attach(Host&); }\n")
    string(APPEND calls "    ${module}::attach(host);\n")
endforeach()
file(CONFIGURE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/modules.cpp CONTENT
"#include \"yonderest/host.hpp\"\n\nnamespace yonder {\n${declarations}\nvoid attach_modules(Host& host) {\n${calls}}\n\n}\n")
add_library(yonderest-modules STATIC ${CMAKE_CURRENT_BINARY_DIR}/modules.cpp)
target_link_libraries(yonderest-modules PUBLIC yonderest-engine)
foreach(module ${modules})
    target_link_libraries(yonderest-modules PRIVATE yonderest-${module})
endforeach()
add_executable(yonderest)
target_link_libraries(yonderest PRIVATE yonderest-engine-main yonderest-modules)

# The game's content, beside the program: a folder of packs.
# The build's copy is a mirror of the source tree: deletions carry over too.
set(content ${CMAKE_CURRENT_BINARY_DIR}/content)
set(content_files "")
set(mirror COMMAND ${CMAKE_COMMAND} -E rm -rf ${content})
foreach(pack ${packs})
    file(GLOB_RECURSE files CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/modules/${pack}/*)
    list(APPEND content_files ${files})
    list(APPEND mirror COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/modules/${pack} ${content}/${pack})
endforeach()
# Rewritten only when membership changes, so a removed file still triggers the mirror.
file(CONFIGURE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/content.list CONTENT "${content_files}")
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/content.stamp
    ${mirror}
    COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/content.stamp
    DEPENDS ${content_files} ${CMAKE_CURRENT_BINARY_DIR}/content.list
    COMMENT "Gathering content")
add_custom_target(content ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/content.stamp)
add_dependencies(yonderest content)

if(BUILD_TESTING)
    # Play, through the engine and the modules but without the window.
    add_executable(play-test tests/play.cpp)
    add_dependencies(play-test content)
    target_link_libraries(play-test PRIVATE yonderest-engine yonderest-physics yonderest-interaction yonderest-inventory yonderest-crafting yonderest-generator)
    add_test(NAME play COMMAND play-test ${content})

    # The whole game, driven through its window by synthetic keys.
    find_package(SDL3 3.4 REQUIRED CONFIG)
    add_executable(desktop-test tests/desktop.cpp)
    add_dependencies(desktop-test content)
    target_link_libraries(desktop-test PRIVATE yonderest-modules yonderest-physics yonderest-interaction yonderest-inventory yonderest-crafting yonderest-generator SDL3::SDL3)
    foreach(scenario "" build empty reference facing crawl reach)
        set(name desktop${scenario})
        if(scenario)
            set(name desktop-${scenario})
        endif()
        add_test(NAME ${name} COMMAND desktop-test ${content} ${scenario})
        set_tests_properties(${name} PROPERTIES TIMEOUT 20 ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software")
    endforeach()
endif()

install(TARGETS yonderest RUNTIME DESTINATION .)
install(DIRECTORY ${content} DESTINATION .)
install(FILES LICENSE README.md DESIGN.md ROADMAP.md CHANGELOG.md DESTINATION .)
