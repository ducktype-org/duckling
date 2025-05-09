add_library(ArtMan INTERFACE)

# make_module(ArtMan USES Base Filesystem)
target_link_libraries(ArtMan INTERFACE Base Filesystem)
