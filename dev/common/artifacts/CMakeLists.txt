add_library(Artifacts src_private/artifacts/artifacts.cpp)

make_module(Artifacts USES Base Filesystem Hashing)
