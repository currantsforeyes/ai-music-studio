set(SONGPLAN_TEST_SRC
    ${CMAKE_CURRENT_LIST_DIR}/main.cpp
    ${CMAKE_CURRENT_LIST_DIR}/songplanstore_tests.cpp
)

add_executable(songplan_tests ${SONGPLAN_TEST_SRC})

get_property(_gtest_src GLOBAL PROPERTY googletest_SOURCE_DIR)
target_include_directories(songplan_tests PRIVATE
    ${PROJECT_BINARY_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${PROJECT_SOURCE_DIR}/src
    ${MUSE_FRAMEWORK_PATH}
    ${MUSE_FRAMEWORK_PATH}/framework
    ${_gtest_src}/googletest/googletest/include
)

target_link_libraries(songplan_tests PRIVATE
    Qt6::Core
    gmock
    songplan
)

add_test(NAME songplan_tests COMMAND songplan_tests)
set_tests_properties(songplan_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)