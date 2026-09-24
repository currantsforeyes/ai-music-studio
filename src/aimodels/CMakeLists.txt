# Provider runtime configuration (CLI + model paths) for local AI providers.
declare_module(aimodels)

set(MODULE_SRC
    ${CMAKE_CURRENT_LIST_DIR}/modelsettings.h
    ${CMAKE_CURRENT_LIST_DIR}/modelsettings.cpp
    ${CMAKE_CURRENT_LIST_DIR}/promptfile.h
    ${CMAKE_CURRENT_LIST_DIR}/promptfile.cpp
)

set(MODULE_LINK Qt6::Core)
set(MODULE_USE_UNITY OFF)

setup_module()

if (MUSE_ENABLE_UNIT_TESTS)
    add_subdirectory(tests)
endif()