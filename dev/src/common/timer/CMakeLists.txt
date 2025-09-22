add_library(Timer
        src/timer/timer.cpp
)

make_module(Timer USES Base)

duck_add_test(common timer tests/timer_test.cpp USES Timer Base)

