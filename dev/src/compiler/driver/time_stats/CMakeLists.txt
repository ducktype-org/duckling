add_library(CompilerTimeStats
	src/time_stats/time_stats.cpp
)

make_module(CompilerTimeStats USES Base Timer)

duck_add_test(compiler time_stats tests/time_stats_test.cpp USES CompilerTimeStats)
