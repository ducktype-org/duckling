add_library(GlobalState
	src/global_state/artifacts_location.cpp
)

make_module(GlobalState USES Artifacts)

