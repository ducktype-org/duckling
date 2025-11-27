add_library(GlobalState
	src/global_state/artifacts_location.cpp
	src/global_state/options.cpp
	src/global_state/packages.cpp
)

make_module(GlobalState USES Artifacts ModuleTree)

