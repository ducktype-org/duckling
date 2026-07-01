find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBFFI REQUIRED IMPORTED_TARGET GLOBAL libffi)

# To link libffi use:
# target_link_libraries(my_target PRIVATE PkgConfig::LIBFFI)
