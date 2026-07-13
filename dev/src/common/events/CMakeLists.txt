add_library(Events
        src/events/emitter.hpp
        src/events/emitter.cpp
)

make_module(Events USES Base)

duck_add_test(common emitter tests/emitter_test.cpp USES Base Events)
