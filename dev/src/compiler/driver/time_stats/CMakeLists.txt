add_library(CompilerTimeStats
	src/time_stats/time_stats.cpp
)

make_module(CompilerTimeStats USES Base Timer)
