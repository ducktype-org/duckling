add_library(GlobalState
	src/global_state/artifacts_location.cpp
	src/global_state/options.cpp
)

make_module(GlobalState USES Artifacts)

