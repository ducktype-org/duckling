include(FetchContent)

find_library(GVC_LIBRARY gvc)
find_library(CGRAPH_LIBRARY cgraph)
find_library(CDT_LIBRARY cdt)
# The headers live in a default include path on Linux but not with a Homebrew install on macOS.
find_path(GRAPHVIZ_INCLUDE_DIR graphviz/gvc.h)

set(GRAPHVIZ_LIBRARIES_FOUND FALSE)

if(GVC_LIBRARY AND CGRAPH_LIBRARY AND CDT_LIBRARY AND GRAPHVIZ_INCLUDE_DIR)
	set(GRAPHVIZ_LIBRARIES_FOUND TRUE)

	add_library(System::gvc UNKNOWN IMPORTED GLOBAL)
	set_target_properties(System::gvc PROPERTIES
		IMPORTED_LOCATION "${GVC_LIBRARY}"
		INTERFACE_INCLUDE_DIRECTORIES "${GRAPHVIZ_INCLUDE_DIR}"
	)

	add_library(System::cgraph UNKNOWN IMPORTED GLOBAL)
	set_target_properties(System::cgraph PROPERTIES
		IMPORTED_LOCATION "${CGRAPH_LIBRARY}"
		INTERFACE_INCLUDE_DIRECTORIES "${GRAPHVIZ_INCLUDE_DIR}"
	)

	add_library(System::cdt UNKNOWN IMPORTED GLOBAL)
	set_target_properties(System::cdt PROPERTIES
		IMPORTED_LOCATION "${CDT_LIBRARY}"
		INTERFACE_INCLUDE_DIRECTORIES "${GRAPHVIZ_INCLUDE_DIR}"
	)
endif()

set(GRAPHVIZ_LIBRARIES_FOUND ${GRAPHVIZ_LIBRARIES_FOUND} PARENT_SCOPE)
