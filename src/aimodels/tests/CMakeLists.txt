set(AIMODELS_TEST_SRC
    ${CMAKE_CURRENT_LIST_DIR}/main.cpp
    ${CMAKE_CURRENT_LIST_DIR}/modelsettings_tests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/promptfile_tests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/assistant_tests.cpp
)

add_executable(aimodels_tests ${AIMODELS_TEST_SRC})

get_property(_gtest_src GLOBAL PROPERTY googletest_SOURCE_DIR)
target_include_directories(aimodels_tests PRIVATE
    ${PROJECT_BINARY_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${PROJECT_SOURCE_DIR}/src
    ${MUSE_FRAMEWORK_PATH}
    ${MUSE_FRAMEWORK_PATH}/framework
    ${_gtest_src}/googletest/googletest/include
)

target_link_libraries(aimodels_tests PRIVATE
    Qt6::Core
    gmock
    aimodels
)

add_test(NAME aimodels_tests COMMAND aimodels_tests)
set_tests_properties(aimodels_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)