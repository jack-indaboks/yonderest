find_package(SDL3 3.4 REQUIRED CONFIG)
add_library(yonderest-controls STATIC src/keyboard.cpp)
target_link_libraries(yonderest-controls PUBLIC yonderest-engine PRIVATE SDL3::SDL3)
