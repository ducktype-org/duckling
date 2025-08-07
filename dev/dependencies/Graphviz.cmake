find_library(GVC_LIBRARY gvc)
find_library(CGRAPH_LIBRARY cgraph)
find_library(CDT_LIBRARY cdt)

set(GRAPHVIZ_LIBRARIES_FOUND FALSE)

if(GVC_LIBRARY AND CGRAPH_LIBRARY AND CDT_LIBRARY)
    set(GRAPHVIZ_LIBRARIES_FOUND TRUE)

    add_library(System::gvc UNKNOWN IMPORTED GLOBAL)
    set_target_properties(System::gvc PROPERTIES
        IMPORTED_LOCATION "${GVC_LIBRARY}"
    )

    add_library(System::cgraph UNKNOWN IMPORTED GLOBAL)
    set_target_properties(System::cgraph PROPERTIES
        IMPORTED_LOCATION "${CGRAPH_LIBRARY}"
    )

    add_library(System::cdt UNKNOWN IMPORTED GLOBAL)
    set_target_properties(System::cdt PROPERTIES
        IMPORTED_LOCATION "${CDT_LIBRARY}"
    )
endif()

set(GRAPHVIZ_LIBRARIES_FOUND ${GRAPHVIZ_LIBRARIES_FOUND} PARENT_SCOPE)
