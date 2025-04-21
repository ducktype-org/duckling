# add_library(VIEW_MANAGER
#     src/dia_app/view_manager/view_manager.hpp
#     src/dia_app/view_manager/common.hpp
# )

add_executable(view_manager src/dia_app/view_manager/main.cpp)

target_link_libraries(view_manager PUBLIC JSON grpc++ Proto)

# make_module(VIEW_MANAGER USES Common JSON)

# duck_add_test(compiler view_manager_test tests/tests.cpp
# 	USES Common JSON VIEW_MANAGER)
