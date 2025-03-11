add_library(Listener
	src/listener/listener.hpp
)
set_target_properties(Listener PROPERTIES LINKER_LANGUAGE CXX)

make_module(Listener USES)

duck_add_test(common listener USES Listener)
