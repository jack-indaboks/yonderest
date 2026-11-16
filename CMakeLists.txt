cmake_minimum_required(VERSION 3.20)
project(yonderest VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SDL3 3.2 REQUIRED CONFIG)
find_package(SQLite3 REQUIRED)

add_library(world src/world.cpp src/save.cpp)
target_include_directories(world PUBLIC src PRIVATE ${SQLite3_INCLUDE_DIRS})
target_link_libraries(world PRIVATE ${SQLite3_LIBRARIES})
target_compile_options(world PRIVATE -Wall -Wextra -Wpedantic)

add_library(desktop src/app.cpp)
target_link_libraries(desktop PUBLIC world SDL3::SDL3)
target_compile_options(desktop PRIVATE -Wall -Wextra -Wpedantic)

add_executable(yonderest src/main.cpp)
target_link_libraries(yonderest PRIVATE desktop)

foreach(file blocks.txt terrain.txt)
    configure_file(content/base/${file} content/base/${file} COPYONLY)
endforeach()

include(CTest)
if(BUILD_TESTING)
    add_executable(shelter-test tests/shelter.cpp)
    target_include_directories(shelter-test PRIVATE ${SQLite3_INCLUDE_DIRS})
    target_link_libraries(shelter-test PRIVATE world ${SQLite3_LIBRARIES})
    add_test(NAME shelter COMMAND shelter-test ${CMAKE_CURRENT_SOURCE_DIR}/content/base)
    find_package(Threads REQUIRED)
    add_executable(desktop-test tests/desktop.cpp)
    target_include_directories(desktop-test PRIVATE ${SQLite3_INCLUDE_DIRS})
    target_link_libraries(desktop-test PRIVATE desktop ${SQLite3_LIBRARIES} Threads::Threads)
    add_test(NAME desktop COMMAND desktop-test ${CMAKE_CURRENT_BINARY_DIR}/content)
    set_tests_properties(desktop PROPERTIES TIMEOUT 20 ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software")
endif()

install(TARGETS yonderest RUNTIME DESTINATION .)
install(DIRECTORY content DESTINATION .)
install(FILES LICENSE README.md DESIGN.md ROADMAP.md DESTINATION .)
