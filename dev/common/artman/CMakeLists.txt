add_library(ArtMan src/artman/artman.cpp)

make_module(ArtMan USES Base Filesystem Hashing)
