add_library(Artifacts src_private/artifacts/artifacts.cpp)

make_module(Artifacts USES Base Filesystem Hashing)

add_example(artifacts_example examples/artifacts_example.cpp
	USES Clap Base Artifacts)
