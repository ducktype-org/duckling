# package listed file into a tar archive:

rm perf_tests.tar
tar --create --verbose --file=perf_tests.tar \
    cases/              \
    cpp_setup/print.cpp \
    run_tests.sh \
    run.sh \
    binaries/    \



    