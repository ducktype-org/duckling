add_library(Artifacts src_private/artifacts/artifacts.cpp)

make_module(Artifacts USES Base Filesystem Hashing Logger)

add_example(artifacts_example examples/artifacts_example.cpp
	USES Artifacts)

duck_add_test(common artifacts tests/artifacts_test.cpp USES Artifacts)
