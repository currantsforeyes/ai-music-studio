# Runtime-client and job-state boundary. No provider process is launched here.
declare_module(aijobs)

set(MODULE_SRC
    ${CMAKE_CURRENT_LIST_DIR}/iruntimeclient.h
    ${CMAKE_CURRENT_LIST_DIR}/jobstore.h
    ${CMAKE_CURRENT_LIST_DIR}/jobstore.cpp
    ${CMAKE_CURRENT_LIST_DIR}/fakeruntimeclient.h
    ${CMAKE_CURRENT_LIST_DIR}/fakeruntimeclient.cpp
    ${CMAKE_CURRENT_LIST_DIR}/runtimehostsupervisor.h
    ${CMAKE_CURRENT_LIST_DIR}/runtimehostsupervisor.cpp
)

set(MODULE_LINK aicore Qt6::Core)
set(MODULE_USE_UNITY OFF)

setup_module()

if (MUSE_ENABLE_UNIT_TESTS)
    add_subdirectory(tests)
endif()

add_subdirectory(runtimehost)