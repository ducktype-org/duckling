add_library(VIEW_MANAGER
    src/dia_app/view_manager/view_manager.hpp
    src/dia_app/view_manager/common.hpp
)

make_module(VIEW_MANAGER USES JSON)

duck_add_test(compiler view_manager tests/view_manager_test.cpp
	USES JSON VIEW_MANAGER)
