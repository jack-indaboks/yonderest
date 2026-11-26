add_library(yonderest-crafting STATIC src/crafting.cpp)
target_link_libraries(yonderest-crafting PUBLIC yonderest-engine PRIVATE yonderest-inventory)
