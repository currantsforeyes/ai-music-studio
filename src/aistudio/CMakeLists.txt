# Gate 1 AI Studio shell. It owns actions and UI-facing runtime state only.
declare_module(aistudio)

set(MODULE_SRC
    ${CMAKE_CURRENT_LIST_DIR}/aistudiomodule.cpp
    ${CMAKE_CURRENT_LIST_DIR}/aistudiomodule.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/aistudiocontroller.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/aistudiocontroller.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/assistantclient.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/assistantclient.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/aistudiouiactions.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/aistudiouiactions.h
    ${CMAKE_CURRENT_LIST_DIR}/view/aistudiostatusmodel.cpp
    ${CMAKE_CURRENT_LIST_DIR}/view/aistudiostatusmodel.h
)

set(MODULE_LINK aijobs aiproject ailibrary aicore aimodels songplan context Qt6::Network)
set(MODULE_USE_UNITY OFF)

setup_module()
