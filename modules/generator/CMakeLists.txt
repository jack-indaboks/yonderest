add_library(yonderest-generator STATIC src/generator.cpp)
target_link_libraries(yonderest-generator PUBLIC yonderest-engine)

if(BUILD_TESTING)
    add_executable(generator-test tests/generation.cpp)
    target_link_libraries(generator-test PRIVATE yonderest-generator)
    add_test(NAME generator COMMAND generator-test)
endif()
