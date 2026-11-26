add_library(yonderest-inventory STATIC src/inventory.cpp)
target_include_directories(yonderest-inventory PUBLIC include)
target_link_libraries(yonderest-inventory PUBLIC yonderest-engine)
