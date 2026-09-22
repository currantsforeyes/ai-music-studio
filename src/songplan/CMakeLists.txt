# Normalized, provider-independent song plans and immutable revisions.
declare_module(songplan)

set(MODULE_SRC
    ${CMAKE_CURRENT_LIST_DIR}/songplan.h
    ${CMAKE_CURRENT_LIST_DIR}/songplan.cpp
    ${CMAKE_CURRENT_LIST_DIR}/songplanstore.h
    ${CMAKE_CURRENT_LIST_DIR}/songplanstore.cpp
)

set(MODULE_LINK Qt6::Core)
set(MODULE_USE_UNITY OFF)

setup_module()

if (MUSE_ENABLE_UNIT_TESTS)
    add_subdirectory(tests)
endif()