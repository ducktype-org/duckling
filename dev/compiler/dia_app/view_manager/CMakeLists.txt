add_library(VIEW_MANAGER
    src/dia_app/view_manager/view_manager.hpp
    src/dia_app/view_manager/common.hpp
)

make_module(VIEW_MANAGER USES Common JSON)

duck_add_test(compiler view_manager tests/view_manager_test.cpp
	USES Common JSON VIEW_MANAGER)
