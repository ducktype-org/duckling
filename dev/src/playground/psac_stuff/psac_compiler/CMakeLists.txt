



add_example(psacklinger main.cpp USES pq_shared Base Init Clah Logger SystemCommand llvm_helpers)


add_example(psacklinger_test tests/psacklinger_test.cpp USES pq_shared psack_corpus Tester Base Init Clah Logger SystemCommand llvm_helpers)
