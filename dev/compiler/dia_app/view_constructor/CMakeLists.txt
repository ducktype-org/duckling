add_library(ViewConstructor
    src/view_constructor/dia_parser.hpp
    src/view_constructor/display_elements.cpp
	src/view_constructor/view_constructor.hpp
)

make_module(ViewConstructor USES JSON)