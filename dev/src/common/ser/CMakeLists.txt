# ser is header-only. The library target exists anyway, for two reasons:
# * make_module gives it the same shape as every other module (public `src` on the
#   include path, coverage registration, clean-modules), and an INTERFACE target
#   cannot be passed to it.
# * tests are EXCLUDE_FROM_ALL, so with an INTERFACE target a plain `ninja` would
#   never compile a single ser header. src/ser/ser.cpp includes all of them.
add_library(Ser
	src/ser/ser.cpp
)

make_module(Ser USES)

duck_add_test(common ser tests/ser_test.cpp USES Ser)
