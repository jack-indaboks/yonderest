yonderest_module()

if(BUILD_TESTING)
    add_executable(generator-test tests/generation.cpp)
    target_link_libraries(generator-test PRIVATE yonderest-generator)
    add_test(NAME generator COMMAND generator-test)
endif()
