add_library(QueryFramework
	src/query_framework/internal/query_graph/query_graph.cpp
	src/query_framework/internal/query_graph/active_graph.cpp
	src/query_framework/internal/query_graph/query_state.cpp
	src/query_framework/internal/query_data/query_id.cpp
	src/query_framework/internal/query_graph/node_marking.cpp
	src/query_framework/internal/task_pool/task_pool.cpp
	
	src/query_framework/module_flags/module_flags.cpp

	src/query_framework/external/api.cpp
	
	src/query_framework/entry/with_context_do.cpp
	
	src/query_framework/context/context.cpp

	src/query_framework/q_stats/q_stats.cpp
	
	src/query_framework/query_errors.cpp

	src/query_framework/query_result.hpp

	src/query_framework/internal/query_metadata/metadata_storage.cpp
	src/query_framework/internal/query_metadata/metadata_registry.cpp
)

make_module(QueryFramework USES Base Diagnostic DiagnosticInteractive Timer Logger CompilerTimeStats Concurrent)

add_example(query_example examples/example.cpp USES QueryFramework)

duck_add_test(common query_framework tests/query_framework_test.cpp USES QueryFramework)
duck_add_test(common active_graph tests/active_graph_test.cpp USES QueryFramework)
duck_add_test(common concurrent_metadata_storage tests/concurrent_metadata_storage_test.cpp USES QueryFramework)
duck_add_test(common query_invalidation tests/query_invalidation_test.cpp USES QueryFramework)
duck_add_test(common task_pool tests/task_pool_test.cpp USES QueryFramework)
duck_add_test(common cycle_detection_race tests/cycle_detection_race_test.cpp USES QueryFramework)


add_example(node_idid_test src/query_framework/node_id_comp/test.cpp USES QueryFramework Base)
