add_library(GlobalState
	src/global_state/artifacts_location.cpp
	src/global_state/global_logger.cpp
	src/global_state/backend_options.cpp
	src/global_state/packages.cpp
	src/global_state/script_context.cpp
)

make_module(GlobalState USES Artifacts ModuleTree Packages Diagnostic QueryFramework)
