find_package(SDL3 3.4 REQUIRED CONFIG)
add_library(yonderest-display STATIC src/display.cpp)
target_link_libraries(yonderest-display PUBLIC yonderest-engine PRIVATE SDL3::SDL3)
