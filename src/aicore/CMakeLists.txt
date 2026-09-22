# Shared, editor-independent AI Music Studio contracts.
declare_module(aicore)

set(MODULE_SRC
    ${CMAKE_CURRENT_LIST_DIR}/aicoretypes.h
    ${CMAKE_CURRENT_LIST_DIR}/aicoretypes.cpp
)

setup_module()